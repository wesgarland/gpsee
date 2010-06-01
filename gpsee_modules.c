/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Initial Developer of the Original Code is PageMail, Inc.
 *
 * Portions created by the Initial Developer are 
 * Copyright (c) 2007-2010, PageMail, Inc. All Rights Reserved.
 *
 * Contributor(s): 
 * 
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** 
 */

/**
 *  @author	Wes Garland, PageMail, Inc., wes@page.ca
 *  @version	$Id: gpsee_modules.c,v 1.32 2010/04/22 12:43:29 wes Exp $
 *  @date	March 2009
 *  @file	gpsee_modules.c		GPSEE module load, unload, and management code
 *					for native, script, and blended modules.
 *
 * <b>Design Notes</b>
 *
 * Module handles are a private (to this file) data structure which describe everything
 * worth knowing about a module -- it's scope (var object), init and fini functions,
 * exports object, etc.
 *
 * Module handles are stored in a splay tree, realm->modules. Splay trees are
 * automatically balanced binary search trees that slightly re-balance on 
 * every read to improve to locality of reference, moving recent search results
 * closer to the head.
 *
 * Extra GC roots, provided by moduleGCCallback(), are available in each
 * module handle. They are:
 *  - exports:	Marked during module initialization, afterwards rooted by scope or calling script
 *  - scrobj:	Marked during module initialization, afterwards not needed
 *  - scope:	Marked when object is NULL; otherwise by virtue of object.parent
 ************
 *
 * Terminology
 * 
 *  moduleScope:  	Nearly equivalent to the var object of a closure which wraps the module
 *  parentModule: 	The module calling require(); usually this is the program module
 *  exports: 		An object which holds the return value of parentModule's require() call
 *  GPSEE module path:  The first place non-(internal|relative) modules are searched for; libexec dir etc.
 */

static const char __attribute__((unused)) rcsid[]="$Id: gpsee_modules.c,v 1.32 2010/04/22 12:43:29 wes Exp $:";

#define _GPSEE_INTERNALS
#include "gpsee.h"
#include "gpsee_private.h"
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "./freebsd_tree.h"

#if defined(GPSEE_DEBUG_BUILD)
# define RTLD_mode	RTLD_NOW
static int dpDepthSpaces=0;
static const char *spaces(void)
{
  static char buf[1024];
  memset(buf, 32, dpDepthSpaces * 2);
  buf[dpDepthSpaces * 2] = (char)0;
  return buf;
}
# define dpDepth(a) 		do { if (a > 0) dprintf("{\n"); dpDepthSpaces += a; if (a < 0) dprintf("}\n"); } while (0)
# define dprintf(a...) 		do { if (gpsee_verbosity(0) > GPSEE_MODULE_DEBUG_VERBOSITY) gpsee_printf(cx, "modules\t> %s", spaces()), gpsee_printf(cx, a); } while(0)
# define moduleShortName(a)	strstr(a, gpsee_basename(getenv("PWD")?:"trunk")) ?: a
#else
# define RTLD_mode	RTLD_LAZY
# define dprintf(a...) 		while(0) gpsee_printf(cx, a)
# define dpDepth(a) 		while(0)
# define moduleShortName(a) 	a
#endif

GPSEE_STATIC_ASSERT(sizeof(void *) >= sizeof(size_t));

extern rc_list rc;

typedef const char * (* moduleInit_fn)(JSContext *, JSObject *);	/**< Module initializer function type */
typedef JSBool (* moduleFini_fn)(JSContext *, JSObject *);		/**< Module finisher function type */
typedef struct
{
  moduleHandle_t        *module;
  gpsee_realm_t         *realm;
} moduleScopeInfo_t;

/* Generate Init/Fini function prototypes for all internal modules */
#define InternalModule(a) const char *a ## _InitModule(JSContext *, JSObject *);\
                          JSBool a ## _FiniModule(JSContext *, JSObject *);
#include "modules.h"
#undef InternalModule

/** Entry in a module path linked list. Completes forward declaration
 *  found in gpsee.h
 */
struct modulePathEntry
{
  const char			*dir;	/**< Directory to search for modules */
  struct modulePathEntry	*next;	/**< Next element in the list, or NULL for the last */
};

/** Type describing a module loader function pointer. These functions can load and
 *  initialize a module during loadDiskModule.
 */
typedef JSBool(*moduleLoader_t)(JSContext *, moduleHandle_t *, const char *);

static void finalizeModuleScope(JSContext *cx, JSObject *exports);
static JSBool setModuleScopeInfo(JSContext *cx, JSObject *moduleScope, moduleHandle_t *module, gpsee_realm_t *realm);
static moduleScopeInfo_t *getModuleScopeInfo(JSContext *cx, JSObject *moduleScope);
static void markModuleUnused(JSContext *cx, moduleHandle_t *module);

/** Module-scope getter which retrieves properties from the true global */
static JSBool getGlobalProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  gpsee_realm_t 	*realm = gpsee_getRealm(cx);

  return realm ? JS_GetPropertyById(cx, realm->globalObject, id, vp) : JS_FALSE;
}

/** Module-scope setter which sets properties on the true global */
static JSBool setGlobalProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  gpsee_realm_t 	*realm = gpsee_getRealm(cx);

  return realm ? JS_SetPropertyById(cx, realm->globalObject, id, vp) : JS_FALSE;
}

/** Module-scope resolver which resolves properties on the scope by looking them
 *  up on the true global. Any properties which are found are defined on the module
 *  scope with a setter and getter which look back to the true global.
 */
static JSBool resolveGlobalProperty(JSContext *cx, JSObject *obj, jsval id, uintN flags, JSObject **objp)
{
  gpsee_realm_t         *realm = gpsee_getRealm(cx);
  jsval			v;

  if (flags & JSRESOLVE_DECLARING)	/* Don't walk up scope to global when making new vars */
  {
    return JS_TRUE;
  }

  if (!realm || JS_GetPropertyById(cx, realm->globalObject, id, &v) == JS_FALSE)
    return JS_FALSE;

  if (v == JSVAL_VOID)
    return JS_TRUE;

  if (JS_DefinePropertyById(cx, obj, id, v, getGlobalProperty, setGlobalProperty, 0) == JS_FALSE)
    return JS_FALSE;

  /** @todo	Add JS_GetPropertyAttributesById() and JS_SetPropertyAttributesById() to JSAPI,
   *            re-write this block to use them, and eliminate the JSVAL_IS_STRING(id) condition,
   *            which is actually semantically incorrect but reasonable in practice.
   */
  if (JSVAL_IS_STRING(id))
  {
    char 	*s = JS_GetStringBytes(JSVAL_TO_STRING(id));
    JSBool	found;
    uintN	attrs;
    
    if (JS_GetPropertyAttributes(cx, obj, s, &attrs, &found) == JS_FALSE)
      return JS_FALSE;

    if (found)
    {
      if (JS_SetPropertyAttributes(cx, obj, s, attrs, &found) == JS_FALSE)
	return JS_FALSE;

      GPSEE_ASSERT(found);
    }
  }

  *objp = obj;

  return JS_TRUE;
}

static JSClass module_scope_class = 	/* private member is reserved by gpsee - holds module handle */
{
  GPSEE_GLOBAL_NAMESPACE_NAME ".module.Scope",
  JSCLASS_HAS_PRIVATE | JSCLASS_GLOBAL_FLAGS | JSCLASS_NEW_RESOLVE,
  JS_PropertyStub,  
  JS_PropertyStub,  
  JS_PropertyStub,
  JS_PropertyStub,
  JS_EnumerateStub, 
  (JSResolveOp)resolveGlobalProperty,
  JS_ConvertStub,   
  finalizeModuleScope,
  JSCLASS_NO_OPTIONAL_MEMBERS
};  

static JSClass module_exports_class = 		/* private member - do not touch - for use by native module */
{
  GPSEE_GLOBAL_NAMESPACE_NAME ".module.Exports",
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,  
  JS_PropertyStub,  
  JS_PropertyStub,  
  JS_PropertyStub,
  JS_EnumerateStub, 
  JS_ResolveStub,   
  JS_ConvertStub,   
  JS_FinalizeStub,
  JSCLASS_NO_OPTIONAL_MEMBERS
};  

typedef enum 
{ 
  mhf_loaded	= 1 << 0,		/**< Indicates that the module has been successfully loaded */
} moduleHandle_flags_t;

/** Structure describing a module. Completes forward declaration in gpsee.h */
struct moduleHandle
{
  const char 		*cname;		/**< Name of the module; canonical, unique */
  void 			*DSOHnd;	/**< Handle to dlopen() shared object for DSO modules */
  moduleInit_fn		init;		/**< Function to run when module done loading */
  moduleFini_fn		fini;		/**< Function to run just before module is unloaded */
  JSObject		*exports;	/**< JavaScript object holding module contents (require/loadModule return). 
					     Private slot reserved for module's use. */
  JSObject		*scope;		/**< Scope object for JavaScript modules; preserved only from load->init */
  JSScript              *script;        /**< Script object for JavaScript modules; preserved only from
                                             load -> JS_ExecuteScript() */
  JSObject		*scrobj;	/**< GC thing for script, used by GC callback */

  moduleHandle_flags_t	flags;		/**< Special attributes of module; bit-field */

  moduleHandle_t	*next;		/**< Used when treating as a linked list node, i.e. during DSO unload */
  SPLAY_ENTRY(moduleHandle)	entry;	/**< Tree data */
};

/** Compare two module pointers to see if they have the same canonical name */
static int moduleCName_strcmp(struct moduleHandle *module1, struct moduleHandle *module2)
{
  return strcmp(module1->cname, module2->cname);
}
/** Declare splay tree support functions for splay tree type moduleMemo */
SPLAY_HEAD(moduleMemo, moduleHandle);
SPLAY_PROTOTYPE(moduleMemo, moduleHandle, entry, moduleCName_strcmp)
SPLAY_GENERATE(moduleMemo, moduleHandle, entry, moduleCName_strcmp)

/**
 *  requireLock / requireUnlock form a simple re-entrant lockless "mutex", optimized for 
 *  the case where there are no collisions. These locks are per-runtime.
 *
 *  The requireLockThread holds the thread ID of the thread currrently holding this lock, or
 *  JSVAL_NULL (which is an invalid value for PRThread).  
 *
 *  requireLockThread is only read/written via CAS operations, forcing a full memory barrier 
 *  at the start of a locking operation. requireLockDepth may only be read or written by a 
 *  thread which holds the lock.  When the lock is released, the cas memory barrier insures 
 *  that the next lock-holder gets an initial depth of zero.
 */
static void requireLock(JSContext *cx)
{
  dpDepth(+1);
#if defined(JS_THREADSAFE)
  gpsee_interpreter_t 	*jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  PRThread		*thisThread = PR_GetCurrentThread();

  if (jsval_CompareAndSwap((jsval *)&jsi->requireLockThread, (jsval)thisThread, (jsval)thisThread) == JS_TRUE)  /* membar */
    goto haveLock;

  do
  {
    jsrefcount depth;

    if (jsval_CompareAndSwap((jsval *)&jsi->requireLockThread, JSVAL_NULL, (jsval)thisThread) == JS_TRUE)
      goto haveLock;

    depth = JS_SuspendRequest(cx);
    PR_Sleep(PR_INTERVAL_NO_WAIT);
    JS_ResumeRequest(cx, depth);
  } while(1);

  haveLock:
  jsi->requireLockDepth++;
#endif
  return;
}

/**
 *  @see requireLock()
 */
static void requireUnlock(JSContext *cx)
{
#if defined(JS_THREADSAFE)
  gpsee_interpreter_t 	*jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  PRThread		*thisThread = PR_GetCurrentThread();

  GPSEE_ASSERT(jsi->requireLockDepth);
  GPSEE_ASSERT(jsi->requireLockThread == thisThread);

  jsi->requireLockDepth--;
  if (jsi->requireLockDepth == 0)
  {
    if (jsval_CompareAndSwap((jsval *)&jsi->requireLockThread, (jsval)thisThread, JSVAL_NULL) != JS_TRUE) /* membar */
    {
      GPSEE_NOT_REACHED("bug in require-locking code");
    }
 }
#endif
  dpDepth(-1);
  return;
}

/**
 *  Create a new module object (exports)
 *
 *  This object is effectively a container for the properties of the module,
 *  and is what require() returns to the JS programmer.
 *
 *  The exports object will be created with an unused private slot.
 *  This slot is for use by the module's Fini code, based on data
 *  the module's Init code puts there.
 *
 *  @param	cx 		The JS context to use
 *  @param	moduleScope	The scope for the module we are creating the exports object for. 
 *
 *  @return	A new, valid, module object or NULL if an exception has been
 *		thrown.
 *
 *  @note	Module object is unrooted, and thus should be assigned to a
 *		rooted address immediately. 
 *		
 */
static JSObject *newModuleExports(JSContext *cx, JSObject *moduleScope)
{
  JSObject 		*exports;

  exports = JS_NewObject(cx, &module_exports_class, NULL, moduleScope);
  dprintf("Created new exports at %p for module at scope %p\n", exports, moduleScope);
  return exports;
}

/**
 *  Initialize a "fresh" module scope ("global") to have all the correct properties 
 *  and methods. This includes a require function which "knows" its path information
 *  by virtue of the module handle stored in reserved slot 0.
 *
 *  If the module global is not the realm's global ("super-global"), we also copy through
 *  the standard classes.
 *
 *  @param	cx			Current JavaScript context
 *  @param	module			Module handle for the module whose scope we are initializing.
 *					Handle must remain valid for the lifetime of this scope. Only
 *					the cname member is used by this function.
 *  @param	moduleScope		Scope object to initialize
 *  @param	isVolatileScope		If isVolatileScope is JS_TRUE, this module scope will be created
 *					with "volatile" objects; that is, properties like require, exports,
 *					and so can be deleted or modified in ways they normally could not.
 *					This mode allows us to declare a special module scope before the
 *					program module loads that shares the global object with the program
 *					module, but has had its module-special properties (like require and
 *					exports) replaced by the time the program module runs.
 *
 *  @note	This function also sets module->exports
 *
 *  @returns	JS_TRUE or JS_FALSE if an exception was thrown
 */
static JSBool initializeModuleScope(JSContext *cx, moduleHandle_t *module, JSObject *moduleScope, JSBool isVolatileScope)
{
  JSFunction 		*require;
  gpsee_realm_t         *realm = gpsee_getRealm(cx);
  JSObject      	*modDotModObj;
  JSString      	*moduleId;
  int			jsProp_permanentReadOnly;
  int			jsProp_permanent;

  if (!realm)
    return JS_FALSE;

  if (isVolatileScope == JS_TRUE)
  {
    jsProp_permanent = 0;
    jsProp_permanentReadOnly = 0;
  }
  else
  {
    jsProp_permanent = JSPROP_PERMANENT;
    jsProp_permanentReadOnly = JSPROP_PERMANENT | JSPROP_READONLY;
  }

  dprintf("Initializing module scope for module %s at %p\n", moduleShortName(module->cname), module);
  dpDepth(+1);

  GPSEE_ASSERT(module->exports == NULL || isVolatileScope);
  GPSEE_ASSERT(JS_GET_CLASS(cx, moduleScope)->flags & JSCLASS_IS_GLOBAL);
  GPSEE_ASSERT(JS_GET_CLASS(cx, moduleScope)->flags & JSCLASS_HAS_PRIVATE);

  if (setModuleScopeInfo(cx, moduleScope, module, realm) == JS_FALSE)
    return JS_FALSE;

  if (moduleScope != realm->globalObject)
  {
    JSProtoKey	key;

    /** Get the cached class prototypes sorted out in advance. Not guaranteed tracemonkey-future-proof. 
     *  Almost certainly requires eager standard class initialization on the true global.
     */
    for (key = 0; key < JSProto_LIMIT; key++)
    {
      jsval v;

      if (JS_GetReservedSlot(cx, realm->globalObject, key, &v) == JS_FALSE)
	return JS_FALSE;

      if (JS_SetReservedSlot(cx, moduleScope, key, v) == JS_FALSE)
	return JS_FALSE;
    }

    if (JS_DefineProperty(cx, moduleScope, "undefined", JSVAL_VOID, NULL, NULL,
			  JSPROP_ENUMERATE | jsProp_permanent) == JS_FALSE)
      goto fail;
  }

  /* Define the basic requirements for what constitutes a CommonJS module:
   *  - require
   *  - require.paths
   *  - exports
   *  - module.id
   */
  require = JS_DefineFunction(cx, moduleScope, "require", gpsee_loadModule, 1, JSPROP_ENUMERATE | jsProp_permanentReadOnly);
  if (!require)
    goto fail;

  if (!realm->userModulePath)
  {
    /* This is the first module scope we've initialized; set up the storage
     * required for the user module path (require.paths). This path is global
     * across ALL contexts in the realm.
     */
    JS_AddNamedRoot(cx, &realm->userModulePath, "User Module Path");
    realm->userModulePath = JS_NewArrayObject(cx, 0, NULL);
    if (!realm->userModulePath)
      goto fail;
  }

  if (JS_DefineProperty(cx, (JSObject *)require, "paths", 
			OBJECT_TO_JSVAL(realm->userModulePath), NULL, NULL,
			JSPROP_ENUMERATE | jsProp_permanentReadOnly) != JS_TRUE)
    goto fail;

  if (JS_SetReservedSlot(cx, (JSObject *)require, 0, PRIVATE_TO_JSVAL(module)) == JS_FALSE)
    goto fail;

  module->exports = newModuleExports(cx, moduleScope);
  if (JS_DefineProperty(cx, moduleScope, "exports", 
			OBJECT_TO_JSVAL(module->exports), NULL, NULL,
			JSPROP_ENUMERATE | jsProp_permanentReadOnly) != JS_TRUE)
    goto fail;

  modDotModObj = JS_NewObject(cx, NULL, NULL, moduleScope);
  if (!modDotModObj)
    goto fail;

  if (JS_DefineProperty(cx, moduleScope, "module", OBJECT_TO_JSVAL(modDotModObj), NULL, NULL,
			JSPROP_ENUMERATE | jsProp_permanentReadOnly) == JS_FALSE)
    goto fail;

  /* Add 'id' property to 'module' property of module scope. */
  moduleId = JS_NewStringCopyZ(cx, module->cname);
  if (!moduleId)
    goto fail;

  if (JS_DefineProperty(cx, modDotModObj, "id", STRING_TO_JSVAL(moduleId), NULL, NULL,
		    JSPROP_ENUMERATE | jsProp_permanentReadOnly) == JS_FALSE)
    goto fail;

  dpDepth(-1);
  return JS_TRUE;

  fail:
  dpDepth(-1);
  return JS_FALSE;
}

/**
 *  Create an object to act as the global object for the module. The module's
 *  exports object will be a property of this object, along with
 *  module-scope variables.
 *
 *  The private slot for this object is reserved for (and contains) the module handle.
 *
 *  @param cx             JavaScript context
 *  @param      module    The handle describing the module we are building
 *  @returns	NULL on failure, or an object which is in unrooted
 *		but in the recently-created slot. 
 */
static JSObject *newModuleScope(JSContext *cx, moduleHandle_t *module)
{
  gpsee_realm_t         *realm = gpsee_getRealm(cx);
  JSObject 		*moduleScope;

  GPSEE_ASSERT(module);

  if (!realm || JS_EnterLocalRootScope(cx) != JS_TRUE)
    return NULL;

  dpDepth(+1);

  moduleScope = JS_NewObjectWithGivenProto(cx, &module_scope_class, JS_GetPrototype(cx, realm->globalObject), NULL);
  if (!moduleScope)
      goto fail;

  if (JS_SetParent(cx, moduleScope, JS_GetParent(cx, realm->globalObject)) == JS_FALSE)
    goto fail;

  if (initializeModuleScope(cx, module, moduleScope, JS_FALSE) == JS_FALSE)
    goto fail;

  JS_LeaveLocalRootScopeWithResult(cx, OBJECT_TO_JSVAL(moduleScope));

  dpDepth(-1);
  return moduleScope;

  fail:
  JS_LeaveLocalRootScope(cx);
  dpDepth(-1);
  return NULL;
}

static moduleHandle_t *findModuleHandle(gpsee_realm_t *realm, const char *cname)
{
  moduleHandle_t tmp;

  tmp.cname = cname;

  return SPLAY_FIND(moduleMemo, realm->modules, &tmp);
}

/** Find existing or create new module handle.
 *
 *  Module handles are used to 
 *  - track shutdown requirements over the lifetime of the realm
 *  - track module objects so they don't get garbage collected
 *
 *  Any module handle returned from this routine is guaranteed to have a cname, so that it
 *  can be identified.
 *
 *  @param      cx	        JavaScript context
 *  @param      cname           Canonical name of the module we're interested in; may contain slashses, relative ..s etc,
 *                              or name an internal module.
 *  @param     	moduleScope	Scope object to be used instead of a brand-new one if we need to create a module handle.
 *  @returns 			A module handle, possibly one already initialized, or NULL if an exception has been thrown.
 */
static moduleHandle_t *acquireModuleHandle(JSContext *cx, const char *cname, JSObject *moduleScope)
{
  moduleHandle_t	*module = NULL;
  gpsee_realm_t         *realm = gpsee_getRealm(cx);

  GPSEE_ASSERT(cname != NULL);

  dprintf("Acquiring module handle for %s\n", cname);
  dpDepth(+1);

  if (!realm)
    goto fail;

  module = findModuleHandle(realm, cname);
  if (module)
  {
    dprintf("Returning used module handle at %p with scope %p and exports %p\n", module, module->scope, module->exports);
    goto success;	/* seen this one already */
  }

  module = calloc(sizeof *module, 1);
  if (!module)
  {
    JS_ReportOutOfMemory(cx);
    goto fail;
  }

  dprintf("Allocated new module node for %s in memo tree at 0x%p\n", 
	  cname ? moduleShortName(cname) : "program module", module);

  module->cname = JS_strdup(cx, cname);

  if (!moduleScope)
  {
    dprintf("Creating new module scope\n");
    module->scope = newModuleScope(cx, module);
    if (!module->scope)
    {
      dprintf("Could not create new module scope\n");
      goto fail;
    }
  }
  else
  {
    dprintf("Using module scope at %p\n", moduleScope);
    module->scope = moduleScope;
    GPSEE_ASSERT(module->scope);
  }
  
  SPLAY_INSERT(moduleMemo, realm->modules, module);	/* module->scope becomes a root here */
  dprintf("Memoized module at %p with scope %p\n", module, module->scope);

  success:
  dpDepth(-1);
  dprintf("Module handle is at %p\n", module);
  return module;

  fail:
  dpDepth(-1);
  if (module)
    markModuleUnused(cx, module);
  return NULL;
}

/** Marking a module unused makes it eligible for immediate garbage 
 *  collection. Safe to call from a finalizer.
 */
static void markModuleUnused(JSContext *cx, moduleHandle_t *module)
{
  dpDepth(+1);
  dprintf("Marking module at %p (%s) as unused\n", module, module->cname?:"no name");

  if (module->flags & ~mhf_loaded && module->exports && module->fini)
    module->fini(cx, module->exports);

  module->scope   = NULL;
  module->scrobj  = NULL;
  module->exports = NULL;
  module->fini	  = NULL;

  module->flags	 &= ~mhf_loaded;
  dpDepth(-1);
}

/**
 *  Return resources used by a module handle to the OS or other
 *  underlying allocator.  
 * 
 *  This routine is intended be called from the garbage collector
 *  callback after all finalizable JS gcthings have been finalized.
 *  This is because some of those gcthings might need C resources
 *  for finalization.  The canonical example is that finalizing a
 *  prototype object for a class which has its clasp in a DSO. It
 *  is imperative that the DSO stay around until AFTER the prototype
 *  is finalized, but there is no direct way to know that since 
 *  finalization order is random.
 *
 *  @param	cx	JavaScript context
 *  @param	module	Module handle to finalize
 *  @returns	module->next
 */
static moduleHandle_t *finalizeModuleHandle(JSContext *cx, moduleHandle_t *module)
{
  moduleHandle_t *next = module->next;

  dprintf("Finalizing module handle at 0x%p\n", module);
  if (module->cname)
    JS_free(cx, (char *)module->cname);

  if (module->DSOHnd)
  {
    dpDepth(+1);
    dprintf("unloading DSO at 0x%p for module 0x%p\n", module->DSOHnd, module);
    dpDepth(-1);
    dlclose(module->DSOHnd);
  }

#if defined(GPSEE_DEBUG_BUILD)
  memset(module, 0xba, sizeof(*module));
#endif    

  free(module);

  return next;
}

/** 
 *  Release all resources allocated during the creation of the handle, 
 *  including the handle itself.
 *
 *  Note that "release" in this case means "schedule for finalization";
 *  the finalization actually returns the resources to the OS or other
 *  underlying resources allocators. 
 *
 *  This routine should only be called from the scope finalizer. Once
 *  a module has been 'released', it is removed from the realm->modules
 *  memo, meaning that reloading the same module will result in a new
 *  instance even if the module handle has not yet been finalized.
 *
 *  Once the module is out of the realm->modules memo, it is inserted
 *  into the realm->unreachableModules_llist linked list.  This list is
 *  traversed at the very end of the garbaged collector cycle, 
 *  finalizing all modules handles in the list as it prunes the list.
 *  This two-phase garbage collection process is necessary because
 *  the code to finalize the module scope may in fact get dlclose()d
 *  during finalization of the module handle.
 *
 *  @param	cx	JavaScript context handle
 *  @param	module	Handle to destroy
 */
static void releaseModuleHandle(JSContext *cx, gpsee_realm_t *realm, moduleHandle_t *module)
{
  dprintf("Releasing module at 0x%p\n", module);

  SPLAY_REMOVE(moduleMemo, realm->modules, module);
  markModuleUnused(cx, module);

  /* Actually release OS resources after everything on JS
   * side has been finalized. This is especially important
   * for DSO modules, as dlclosing() before JS object finalizer
   * has read clasp is disastrous 
   */
  module->next = realm->unreachableModule_llist;
  realm->unreachableModule_llist = module;
}

/**
 *  Retrieve the module handle associated with the scope.
 *  Safe to run in a finalizer.
 */
static moduleScopeInfo_t *getModuleScopeInfo(JSContext *cx, JSObject *moduleScope)
{
  dprintf("Retrieving info for module with scope 0x%p\n", moduleScope);

  GPSEE_ASSERT(JS_GET_CLASS(cx, moduleScope) == &module_scope_class || JS_GET_CLASS(cx, moduleScope) == gpsee_getGlobalClass());

  return JS_GetPrivate(cx, moduleScope);
}

/**
 *  Note the module handle associated with the scope.
 *  Safe to run in a finalizer.
 */
static JSBool setModuleScopeInfo(JSContext *cx, JSObject *moduleScope, moduleHandle_t *module, gpsee_realm_t *realm)
{
  moduleScopeInfo_t     *hnd;

  dprintf("Noting module with scope 0x%p\n", moduleScope);
  GPSEE_ASSERT(realm == gpsee_getRealm(cx));
  GPSEE_ASSERT(JS_GET_CLASS(cx, moduleScope) == &module_scope_class || JS_GET_CLASS(cx, moduleScope) == gpsee_getGlobalClass());

  hnd = JS_malloc(cx, sizeof(*hnd));
  if (!hnd)
    return JS_FALSE;

  memset(hnd, 0, sizeof(*hnd));
  hnd->module = module;
  hnd->realm = realm;
  JS_SetPrivate(cx, moduleScope, hnd);

  return JS_TRUE;
}

/**
 *  Attempt to extract the current GPSEE realm from the given moduleScope.
 *  Passing NULL for the moduleScope instructs this function to use 
 *  the context's current global object.  This function will not cause
 *  a JS exception to be thrown. The current global object is normally the module's
 *  scope during module load/initialization, and the program module's scope
 *  otherwise.
 *
 *  @param      cx              The current JS context (belonging to the current realm)
 *  @param      moduleScope     The scope to search for the realm in, or NULL to select the global object.
 *  @returns    The realm or NUL.
 *
 *  @note       This routine is called by gpsee_getRealm().
 */
gpsee_realm_t *gpsee_getModuleScopeRealm(JSContext *cx, JSObject *moduleScope)
{
  moduleScopeInfo_t     *hnd;
  JSClass               *class;

  if (!moduleScope)
    moduleScope = JS_GetGlobalObject(cx);

  if (!moduleScope)
    return NULL;

  class = JS_GET_CLASS(cx, moduleScope);
  if (class != &module_scope_class && class != gpsee_getGlobalClass())
    return NULL;

  hnd = JS_GetPrivate(cx, moduleScope);
  if (!hnd)
    return NULL;

  return hnd->realm;
}

static void finalizeModuleScope(JSContext *cx, JSObject *moduleScope)
{
  moduleScopeInfo_t     *hnd;

  dprintf("begin finalizing module scope\n");
  dpDepth(+1);

  hnd = getModuleScopeInfo(cx, moduleScope);
  GPSEE_ASSERT(hnd && hnd->module);
  GPSEE_ASSERT(hnd->module->fini == NULL);      /* Should not be finalizing if fini handler is unrun */

  dprintf("module is %s, %p\n", moduleShortName(hnd->module->cname), hnd->module);
  releaseModuleHandle(cx, hnd->realm, hnd->module);
  JS_free(cx, hnd);

  dpDepth(-1);
  dprintf("done finalizing module scope\n");
}

/**
 *  Load a JavaScript-language module. These modules conform to the ServerJS Securable Modules
 *  proposal at https://wiki.mozilla.org/ServerJS/Modules/SecurableModules.
 *
 *  Namely,
 *   - JS is executed in a non-global scope
 *   - JS modifies "exports" object in that scope
 *   - Exports object is in fact module->exports
 *
 *   @param	cx		JS Context
 *   @param	module		Pre-populated moduleHandle
 *   @param	filename	Fully-qualified path to the file to load
 *
 *   @returns	JSBool
 */
static JSBool loadJSModule(JSContext *cx, moduleHandle_t *module, const char *filename)
{
  dprintf("loadJSModule(\"%s\")\n", filename);
  dpDepth(+1);

  if (!gpsee_compileScript(cx, filename, NULL, NULL, &module->script,
      module->scope, &module->scrobj))
  {
    dprintf("module %s compilation failed\n", moduleShortName(filename));
    dpDepth(-1);
    return JS_FALSE;
  }

  dprintf("module %s compiled okay\n", moduleShortName(filename));
  dpDepth(-1);
  return JS_TRUE;
}

/**
 *   Load a DSO module. These modules are linked in at run-time, so we compute the 
 *   addresses of the symbols with dlsym().
 *
 *   @param	cx			JS Context
 *   @returns	JSBool
 */
static JSBool loadDSOModule(JSContext *cx, moduleHandle_t *module, const char *filename)
{
  jsrefcount 	depth;

  depth = JS_SuspendRequest(cx);
  module->DSOHnd = dlopen(filename, RTLD_mode);
  JS_ResumeRequest(cx, depth);

  if (!module->DSOHnd)
  {
    return gpsee_throw(cx, "dlopen() error: %s", dlerror());
  }
  else
  {
    char	symbol[strlen(module->cname) + 11 + 1];
    const char 	*bname = gpsee_basename(module->cname);

    sprintf(symbol, "%s_InitModule", bname);
    module->init = dlsym(module->DSOHnd, symbol);
      
    sprintf(symbol, "%s_FiniModule", bname);
    module->fini = dlsym(module->DSOHnd, symbol);
  }

  return JS_TRUE;
}

/**  Check to insure a full path is a child of a jail path.
 *  @param	fullPath	The full path we're checking
 *  @param	jailpath	The jail path we're checking against, or NULL for "anything goes".
 *  @returns	fullPath if the path is inside the jail; NULL otherwise.
 */
static const char *checkJail(const char *fullPath, const char *jailPath)
{
  size_t	jailLen = jailPath ? strlen(jailPath) : 0;

  if (!jailPath)
    return fullPath;

  if ((strncmp(fullPath, jailPath, jailLen) != 0) || fullPath[jailLen] != '/')
    return NULL;

  return fullPath;
}

static int isRelativePath(const char *path)
{
  if (path[0] != '.')
    return 0;

  if (path[1] == '/')
    return 1;

  if ((path[1] == '.') && (path[2] == '/'))
    return 1;

  return 0;
}

/** 
 *  Turn a JavaScript array in a module path linked list.
 *
 *  @param	cx		Current JavaScript context
 *  @param	obj		Scope-Object containing the array (e.g. global)
 *  @param	pathName	Expression yielding the array (e.g. "require.paths")
 *  @param	modulePath_p	[out]	The list or NULL (for an empy list) upon successful return
 *  
 *  @returns	JS_FALSE when a JS exception is thrown during processing
 */
static JSBool JSArray_toModulePath(JSContext *cx, JSObject *arrObj, modulePathEntry_t *modulePath_p)
{
  jsval			v;
  JSString		*jsstr;
  jsuint		idx, arrlen;
  modulePathEntry_t	modulePath, pathEl;
  const char		*dir;

  if (JS_IsArrayObject(cx, arrObj) != JS_TRUE)
  {
    *modulePath_p = NULL;
    return JS_TRUE;
  }

  if (JS_GetArrayLength(cx, arrObj, &arrlen) == JS_FALSE)
    return JS_FALSE;

  if (arrlen == 0)
  {
    *modulePath_p = NULL;
    return JS_TRUE;
  }

  modulePath = JS_malloc(cx, sizeof(*modulePath) * arrlen);
  pathEl = NULL;

  for (idx = 0; idx < arrlen; idx++)
  {
    if (JS_GetElement(cx, arrObj, idx, &v) == JS_FALSE)
    {
      JS_free(cx, modulePath);
      return JS_FALSE;
    }

    if (v == JSVAL_VOID)
      continue;

    if (JSVAL_IS_STRING(v))
      jsstr = JSVAL_TO_STRING(v);
    else
      jsstr = JS_ValueToString(cx, v);

    dir = JS_EncodeString(cx, jsstr);
    if (!dir || !dir[0])
    {
      if (dir)
	JS_free(cx, (void *)dir);
      continue;
    }

    if (!pathEl)
      pathEl = modulePath;
    else
      pathEl = pathEl->next = modulePath + idx + 1;

    pathEl->dir = dir;
  }
  pathEl->next = NULL;

  *modulePath_p = modulePath;
  return JS_TRUE;
}

/** Free a module path generated from JSArray_toModulePath. 
 *  Takes into account malloc short cuts used to create it in the first place.
 */
static void freeModulePath_fromJSArray(JSContext *cx, modulePathEntry_t modulePath)
{
  modulePathEntry_t pathEl;

  if (!modulePath)
    return;

  for (pathEl = modulePath; pathEl; pathEl = pathEl->next)
    if (pathEl->dir)
      JS_free(cx, (char *)pathEl->dir);

  JS_free(cx, modulePath);
  return;
}

/** 
 *  Load a disk module from a specific directory.
 *  Modules outside of the module jail will not be loaded under any circumstance.
 *
 *  Modules can be loaded with multiple loaders, designated by multiple extensions,
 *  in the extensions and loaders arrays. All possible loaders for a given module 
 *  name in a given directory will be tried in the order they are defined. Successfully
 *  loading a module, without any loader returning a failure message, counts as loading
 *  the module.
 *
 *  @param	realm		Current GPSEE realm
 *  @param	cx		Current JS context
 *  @param	moduleName	Name of the module (argument to require)
 *  @param	directory	Directory in which to find the module
 *  @param	module_p    	[out]	Module handle, if found. May represet a previously-loaded module. 
 *				      	If not found, *module_p is NULL.
 *
 *  @returns	JS_TRUE on success, or JS_FALSE if an exception was thrown.
 */
static JSBool loadDiskModule_inDir(gpsee_realm_t *realm, JSContext *cx, const char *moduleName, const char *directory,
				   moduleHandle_t **module_p)
{
  const char    **ext_p, *extensions[]  = { DSO_EXTENSION, "js", NULL };
  moduleLoader_t  loaders[]             = {loadDSOModule, loadJSModule};
  moduleHandle_t  *module               = NULL;
  char			fnBuf[PATH_MAX];

  for (ext_p = extensions; *ext_p; ext_p++)
  {
    if (snprintf(fnBuf, sizeof(fnBuf), "%s/%s.%s", directory, moduleName, *ext_p) >= sizeof(fnBuf))
    {
      GPSEE_NOT_REACHED("path buffer overrun");
      continue;
    }

    dprintf("%s() trying filename '%s'\n", __func__, fnBuf);
    if (access(fnBuf, F_OK) == 0)
    {
      JSBool success;
      char   cnBuf[PATH_MAX];
      char   *s;

      if (!module)
      {
        int i = gpsee_resolvepath(fnBuf, cnBuf, sizeof(cnBuf));
        if (i == -1)
          return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".loadModule.disk.canonicalize: "
              "Error canonicalizing '%s' (%m)", fnBuf);
        if (i >= sizeof(cnBuf))
          return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".loadModule.disk.canonicalize.overflow: "
              "Error canonicalizing '%s' (buffer overflow)", fnBuf);

        if (!checkJail(cnBuf, realm->moduleJail))
          break;

        if ((s = strchr(cnBuf, '.')))
          if (strcmp(s + 1, *ext_p) == 0)
            *s = (char)0;

        module = acquireModuleHandle(cx, cnBuf, NULL);
        if (!module)
          return JS_FALSE;
        if (module->flags & mhf_loaded)	/* Saw this module previously but cache missed: different relative name? */
          break;
      }

      success = loaders[ext_p - extensions](cx, module, fnBuf);
      if (!success)
      {
        if (module) 
          markModuleUnused(cx, module);
        *module_p = NULL;
        return JS_FALSE;
      }
    }
  }

  *module_p = module;
  return JS_TRUE;
}

/** Iterate over a modulePath, try to find an appropriate module on disk, and load it.
 *
 *  @param	realm		Current GPSEE Realm
 *  @param	cx		Current JS context
 *  @param	moduleName	Name of the module (argument to require)
 *  @param	modulePath	Linked list of module paths to try, in order
 *  @param	module_p    	[out]	Module handle, if found. May represet a previously-loaded module. 
 *				      	If not found, *module_p is set to NULL.
 *
 *  @returns	JS_TRUE on success, or JS_FALSE if an exception was thrown.
 */
static JSBool loadDiskModule_onPath(gpsee_realm_t *realm, JSContext *cx, const char *moduleName, modulePathEntry_t modulePath, 
				     moduleHandle_t **module_p)
{
  modulePathEntry_t	pathEl;

  dpDepth(+1);

  for (pathEl = modulePath,	*module_p = NULL;
       pathEl && 		*module_p == NULL; 
       pathEl = pathEl->next)
  {
    if (loadDiskModule_inDir(realm, cx, moduleName, pathEl->dir, module_p) == JS_FALSE)
    {
      dpDepth(-1);
      return JS_FALSE;
    }
  }

  dpDepth(-1);
  return JS_TRUE;
}

/** Load a module from the local filesystem. This function will search for and load DSO and/or Javascript modules.
 *  @param	cx		Current JS context
 *  @param	parentModule	Module that defined the require() function which is loading the new module.
 *  @param	moduleName	Name of the module (argument to require)
 *  @param	module_p    	[out]	Module handle, if found. May represet a previously-loaded module. 
 *				      	If not found, *module_p is set to NULL.
 *  @returns JS_FALSE and sets an exception when there is an error. Failing to load a module is an error, even if 
 *           it fails because it does not exist.
 */
static JSBool loadDiskModule(JSContext *cx, moduleHandle_t *parentModule,  const char *moduleName, moduleHandle_t **module_p)
{ 
  gpsee_realm_t         *realm = gpsee_getRealm(cx);
  modulePathEntry_t	requirePaths;

  *module_p = NULL;

  /* Handle relative module names */
  if (isRelativePath(moduleName))
  {
    const char	*currentModulePath;
    char	pmBuf[PATH_MAX];

    currentModulePath = gpsee_dirname(parentModule->cname, pmBuf, sizeof(pmBuf));

    if (loadDiskModule_inDir(realm, cx, moduleName, currentModulePath, module_p) == JS_FALSE)
      return JS_FALSE;
    if (!*module_p)
      return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".loadModule.disk: Error loading relative module '%s': "
			 "module not found in '%s'", moduleName, currentModulePath);
    return JS_TRUE;
  }

  /* Handle absolute module names */
  if (moduleName[0] == '/')
  {
    if (loadDiskModule_inDir(realm, cx, moduleName, "", module_p) == JS_FALSE)
      return JS_FALSE;
    if (!*module_p)
      return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".loadModule.disk: Error loading absolute module '%s': module not found",
			 moduleName);
  }

  /* Search GPSEE module path */
  if (loadDiskModule_onPath(realm, cx, moduleName, realm->modulePath, module_p) == JS_FALSE)
    return JS_FALSE;
  if (*module_p)
    return JS_TRUE;

  /* Search require.paths */
  if (realm->userModulePath)
  {
    if (JSArray_toModulePath(cx, realm->userModulePath, &requirePaths) == JS_FALSE)
      return JS_FALSE;
    if (requirePaths)
    {
      JSBool b = loadDiskModule_onPath(realm, cx, moduleName, requirePaths, module_p);
      freeModulePath_fromJSArray(cx, requirePaths);
      if (b == JS_FALSE)
        return JS_FALSE;
      if (*module_p)
        return JS_TRUE;
    }
  }

  return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".loadModule.disk: Error loading top-level module '%s': module not found", 
		     moduleName);
}

/**
 *   Load an internal module. These modules are linked in at compile-time, so we can
 *   compute the addresses of the symbols with some precompiler magic.
 *
 *   Note that internal modules do NOT take pathing into account
 *
 *   @param	cx	JS Context
 *   @param	module	pre-populated moduleHandle
 *   @returns	JS_FALSE if an exception was thrown, JS_TRUE other. 
 *		If the module was found and JS_TRUE is returned, *module_p will not be NULL.
 */
static JSBool loadInternalModule(JSContext *cx, const char *moduleName, moduleHandle_t **module_p)
{
  static struct 
  {
    const char 		*name;
    moduleInit_fn	init;
    moduleFini_fn	fini;
  }
  internalModules[] = {
#define InternalModule(a) 	{ #a, a ## _InitModule, a ## _FiniModule },
#include "modules.h"
#undef InternalModule
  };

  unsigned int 		i;
  moduleHandle_t	*module;

  *module_p = NULL;

  /* First, see if the module is actually in the list of internal modules */
  for (i=0; i < (sizeof internalModules/sizeof internalModules[0]); i++)
    if (strcmp(internalModules[i].name , moduleName) == 0)
      break;
  if (i == (sizeof internalModules/sizeof internalModules[0]))
    return JS_TRUE;	/* internal module not found */

  module = acquireModuleHandle(cx, moduleName, NULL);
  if (module->flags & mhf_loaded)
  {
    dprintf("no reload internal singleton %s\n", moduleShortName(moduleName));
    *module_p = module;
    return JS_TRUE;
  }

  for (i=0; i < (sizeof internalModules/sizeof internalModules[0]); i++)
  {
    if (strcmp(internalModules[i].name , moduleName) == 0)
    {
      module->init = internalModules[i].init;
      module->fini = internalModules[i].fini;
      dprintf("module %s is internal\n", moduleName);
      *module_p = module;
      return JS_TRUE;
    }
  }

  markModuleUnused(cx, module);
  dprintf("module %s is not internal\n", moduleName);

  return JS_TRUE;
}

/** Run module initialization routines and add moduleID property to module scope.
 * 
 *  @returns JS_TRUE  on success, JS_FALSE if an exception was thrown
 */
static JSBool initializeModule(JSContext *cx, moduleHandle_t *module)
{
  gpsee_realm_t         *realm = gpsee_getRealm(cx);

  dprintf("initializeModule(%s)\n", module->cname);

  if (module->init)
  {
    const char *id;

    id = module->init(cx, module->exports);

    dprintf("Initialized native module with internal id = %s\n", id);
  }

  if (module->script)
  {
    JSScript    *script;
    jsval       dummyval;
    JSBool	b;

    /* Once we begin evaluating module code, that module code must never begin evaluating again. Setting module->script
     * to NULL signifies that the module code has already begun (and possibly finished) evaluating its module code.
     * Note that this code block is guarded by if(module->script). Note also that module->flags & dhf_loaded is used
     * elsewhere to short-circuit the module initialization process, simply producing the existing module object.
     */
    script = module->script;
    module->script = NULL;
    JS_SetGlobalObject(cx, module->scope);
    b = JS_ExecuteScript(cx, module->scope, script, &dummyval);
    JS_SetGlobalObject(cx, realm->globalObject);

    module->scrobj	 = NULL;	/* No longer needed */

    if (b == JS_FALSE)
    {
      /** @todo handle system-exit exceptions here? */
      dprintf("module %s at %p threw an exception during initialization\n", moduleShortName(module->cname), module);
      return JS_FALSE;
    }
  }

  return JS_TRUE;
}

/** Implements the CommonJS require() function, allowing JS inclusive-or native module types.
 *  First argument is the module name.
 */
JSBool gpsee_loadModule(JSContext *cx, JSObject *thisObject, uintN argc, jsval *argv, jsval *rval)
{
  const char		*moduleName;
  moduleHandle_t	*module;
  moduleHandle_t        *parentModule;
  jsval			v;
  JSObject		*require_fn = JSVAL_TO_OBJECT(argv[-2]);

  if (argc != 1)
    return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".loadModule.argument.count");

  moduleName = JS_GetStringBytes(JS_ValueToString(cx, argv[0]) ?: JS_InternString(cx, ""));
  if (!moduleName || !moduleName[0])
    return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".loadModule.invalidName: Module name must be at least one character long");

  dprintf("loading module %s\n", moduleShortName(moduleName));

  if (JS_GetReservedSlot(cx, require_fn, 0, &v) == JS_FALSE)
    return JS_FALSE;
  parentModule = JSVAL_TO_PRIVATE(v);

  requireLock(cx);

  if (loadInternalModule(cx, moduleName, &module) == JS_FALSE)
  {
    requireUnlock(cx);
    return JS_FALSE;
  }

  if (!module)
  {
    if (loadDiskModule(cx, parentModule, moduleName, &module) == JS_FALSE)
    {
      requireUnlock(cx);
      return JS_FALSE;
    }

    GPSEE_ASSERT(module);
  }

  if (module->flags & mhf_loaded)
  {
    /* modules are singletons */
    dprintf("no reload singleton %s\n", moduleShortName(moduleName));
    requireUnlock(cx);
    *rval = OBJECT_TO_JSVAL(module->exports);
    return JS_TRUE;
  }

  module->flags |= mhf_loaded;

  if (!module->init && !module->script)
  {
    markModuleUnused(cx, module);
    requireUnlock(cx);

    if (module->DSOHnd)
      return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".loadModule.init.notFound: "
			 "Module lacks initialization function (%s_InitModule)", moduleName);

    return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".loadModule.init.notFound: "
		       "There is no mechanism to initialize module %s!", moduleName);
  }

  dprintf("Initializing module at 0x%p\n", module);
  dpDepth(+1);

  if (initializeModule(cx, module) == JS_FALSE)
  {
    markModuleUnused(cx, module);
    requireUnlock(cx);
    GPSEE_ASSERT(JS_IsExceptionPending(cx));
    dpDepth(-1);
    return JS_FALSE;
  }

  requireUnlock(cx);
  *rval = OBJECT_TO_JSVAL(module->exports);

  dpDepth(-1);
  return JS_TRUE;
}

#define RUNPROERR GPSEE_GLOBAL_NAMESPACE_NAME ".runProgramModule: error running %s: "
/** Run a program as if it were a module. Interface differs from gpsee_loadModule()
 *  to reflect things like that the source of the program module may be stdin rather
 *  than a module file, may not be in the libexec dir, etc.
 *
 *  It is possible to have more than one program module in the module list at once,
 *  provided we can generate reasonable canonical names for each one.
 *
 *  Note that only JavaScript modules can be program modules.
 *
 *  @param      cx              JavaScript context for execution
 *  @param      scriptFilename  This specifies the file to open if neither scriptCode nor ScriptFile are non-null. If
 *                              either of those are non-null, this filename is still handed off to Spidermonkey and is
 *                              useful in qualifying warnings, error messages, or for identifying code via debugger.
 *  @param      scriptCode      If non-null, points to actual Javascript source code.
 *  @param      scriptFile      Stream positioned where we can start reading the script, or NULL
 *
 *  @returns    JS_TRUE on success, otherwise JS_FALSE
 */
JSBool gpsee_runProgramModule(JSContext *cx, const char *scriptFilename, const char *scriptCode, FILE *scriptFile,
                              char * const script_argv[], char * const script_environ[])
{
  moduleHandle_t 	*module = NULL;
  char			cnBuf[PATH_MAX];
  char			fnBuf[PATH_MAX];
  int			i;
  gpsee_realm_t         *realm = gpsee_getRealm(cx);
  char			*s;
  JSBool                b;

  if (scriptFilename[0] == '/')
  {
    i = gpsee_resolvepath(scriptFilename, cnBuf, sizeof(cnBuf));
  }
  else
  {
    char	tmpBuf[PATH_MAX];
    
    if (getcwd(tmpBuf, sizeof(tmpBuf)) == NULL)
      return gpsee_throw(cx, RUNPROERR "getcwd() error: %m", scriptFilename);

    if (gpsee_catstrn(tmpBuf, "/", sizeof(tmpBuf)) == 0)
      return gpsee_throw(cx, RUNPROERR "filename buffer overrun", scriptFilename);

    if (gpsee_catstrn(tmpBuf, scriptFilename, sizeof(tmpBuf)) == 0)
      return gpsee_throw(cx, RUNPROERR "filename buffer overrun", scriptFilename);

    i = gpsee_resolvepath(tmpBuf, cnBuf, sizeof(cnBuf));
  }
  if ((i >= sizeof(cnBuf)) || (i == -1))
    return gpsee_throw(cx, RUNPROERR "filename buffer overrun", scriptFilename);
  else
    cnBuf[i] = (char)0;

  dprintf("Running program module \"%s\"\n", scriptFilename);
  dpDepth(+1);

  /* Before mangling cnBuf, let's copy it to fnBuf to be used in gpsee_compileScript() */
  strcpy(fnBuf, cnBuf);

  /* If we were supplied a FILE* from which to get our source code, we want to ensure that the scriptFilename
   * still refers to this file. This check may be unnecessary or redundant, but it will help keep knots out of our
   * hair. TODO we should fix this check if we ever support symbolic filenames like "stdin" or URIs like
   * "http://what/ever.js" (in fact we should probably use something like "sym://" for symbolic filenames to simplify
   * things, should this ever be added.
   */
  if (scriptFile)
  {
    /* Ensure that the supplied script file stream matches the file name */
    struct stat sb1, sb2;

    if (stat(fnBuf, &sb1))
      return gpsee_throw(cx, RUNPROERR "stat(\"%s\") error: %m", scriptFilename, fnBuf);

    if (fstat(fileno(scriptFile), &sb2))
      return gpsee_throw(cx, RUNPROERR "fstat(file descriptor %d, allegedly \"%s\") error: %m", scriptFilename,
                             fileno(scriptFile), fnBuf);

    if ((sb1.st_ino != sb2.st_ino) || (sb1.st_dev != sb2.st_dev))
      return gpsee_throw(cx, RUNPROERR "file moved? \"%s\" stat inode/dev %d/%d doesn't match file descriptor"
                             " %d inode/dev %d/%d", scriptFilename,
                             fnBuf, (int)sb1.st_ino, (int)sb1.st_dev,
                             fileno(scriptFile), (int)sb2.st_ino, (int)sb2.st_dev);
  }

  /* If we are no supplied an open FILE* ('scriptFile') or literal script code ('scriptCode') then we should try to
   * open the file from 'scriptFilename'
   */
  if (!scriptFile && !scriptCode)
  {
    scriptFile = fopen(scriptFilename, "r");
    if (!scriptFile)
      return gpsee_throw(cx, RUNPROERR "fopen() error %m", scriptFilename);
  }

  /* The "cname" argument to acquireModuleHandle() names a module, not a file, so let's remove the .js extension */
  if ((s = strrchr(cnBuf, '.')))
  {
    if (strcmp(s, ".js") == 0)
      *s = (char)0;
  }

  /* realm->mutable->programModuleDir stores a copy of the program module's directory, 
   * which we can use to generate nicer error output, or posssible program-relative
   * resource names.
   */
  gpsee_enterAutoMonitor(cx, &realm->monitors.programModuleDir);
  if (realm->mutable.programModuleDir)
    JS_free(cx, (char *)realm->mutable.programModuleDir);
  i = strlen(cnBuf) + 1;
  realm->mutable.programModuleDir = gpsee_dirname(cnBuf, JS_malloc(cx, i), i);
  if (!realm->monitors.programModuleDir)
  {
    gpsee_leaveAutoMonitor(realm->monitors.programModuleDir);
    goto fail;
  }
  gpsee_leaveAutoMonitor(realm->monitors.programModuleDir);

  module = acquireModuleHandle(cx, cnBuf, realm->globalObject);
  if (!module)
  {
    dprintf("Could not acquire module handle for program module\n");
    goto fail;
  }

  if (initializeModuleScope(cx, module, realm->globalObject, JS_FALSE) == JS_FALSE)
  {
    dprintf("Could not initialize module scope for module at %p\n", module);
    goto fail;
  }

  dprintf("Program module root is %s\n", module->cname);
  dprintf("compiling program module %s\n", moduleShortName(module->cname));

  JS_SetGlobalObject(cx, module->scope);
  if (!gpsee_compileScript(cx, fnBuf, scriptFile, NULL, &module->script,
      module->scope, &module->scrobj))
  {
    goto fail;
  }

  gpsee_enterAutoMonitor(cx, &realm->monitors.programModule);
  realm->mutable.programModule = module;
  gpsee_leaveAutoMonitor(realm->monitors.programModule);

  /* Enable 'mhf_loaded' flag before calling initializeModule() */
  module->flags |= mhf_loaded;	

  JS_SetGlobalObject(cx, realm->globalObject);

  if (script_argv)
    gpsee_createJSArray_fromVector(cx, realm->globalObject, "arguments", script_argv);

  if (script_environ)
    gpsee_createJSArray_fromVector(cx, realm->globalObject, "environ", script_environ);

  /* For the CommonJS system module's "args" property */
  gpsee_enterAutoMonitor(cx, &realm->monitors.script_argv);
  realm->mutable.script_argv = script_argv;
  gpsee_leaveAutoMonitor(realm->monitors.script_argv);

  /* Run the program (initializing the module also runs the script code) */
  b = initializeModule(cx, module);

  gpsee_enterAutoMonitor(cx, &realm->monitors.script_argv);
  realm->mutable.script_argv = NULL;  /* We do not know anything about script_argv's lifetime */
  gpsee_leaveAutoMonitor(realm->monitors.script_argv);

  if (b == JS_FALSE)
    goto fail;

  return JS_TRUE;

  fail:

  dprintf("failed running program module %s\n", module ? moduleShortName(module->cname) : "(null)");

  gpsee_enterAutoMonitor(cx, &realm->monitors.programModule);
  realm->mutable.programModule = NULL;
  gpsee_leaveAutoMonitor(realm->monitors.programModule);

  if (module)
    markModuleUnused(cx, module);

  dpDepth(-1);
 
  return JS_FALSE;
}

/** Callback from the garbage collector, which marks all
 *  module objects so that they are not collected before
 *  their Fini functions are called.
 */
static JSBool moduleGCCallback(JSContext *cx, JSGCStatus status)
{
  gpsee_realm_t         *realm = gpsee_getRealm(cx);
  gpsee_interpreter_t   *jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  moduleHandle_t	*module;

  /* Finalize all modules on the unreachable list now that main GC has finished */
  if (status == JSGC_FINALIZE_END)
  {
    while (realm->unreachableModule_llist)
      realm->unreachableModule_llist = finalizeModuleHandle(cx, realm->unreachableModule_llist);
  }

  if (status != JSGC_MARK_END)
    return JS_TRUE;

#warning Need general purpose GC callback hook - calling IO hook code from module GC handler
  {
    size_t i;

    for (i = 0; i < jsi->user_io_hooks_len; i++)
    {
      if (jsi->user_io_hooks[i].input != JSVAL_VOID)
        JS_MarkGCThing(cx, (void *)jsi->user_io_hooks[i].input, "input hook", NULL);
      if (jsi->user_io_hooks[i].output != JSVAL_VOID)
        JS_MarkGCThing(cx, (void *)jsi->user_io_hooks[i].output, "output hook", NULL);
    }
  }

  dprintf("Adding roots from GC Callback\n");
  dpDepth(+1);

  SPLAY_FOREACH(module, moduleMemo, realm->modules)
  {
    dprintf("GC Callback considering module %s at %p\n", moduleShortName(module->cname), module);
    /* Modules which have an un-run fini method, or are not loaded, 
     * should not have their exports object collected.
     */
    if (module->exports && (!(module->flags & mhf_loaded) || module->fini))
    {
      dprintf("  keeping exports at %p alive\n", module->exports);
      JS_MarkGCThing(cx, module->exports, module->cname, NULL);
    }

    /* Modules which are not loaded should not have their scopes collected */
    if (module->scope && !(module->flags & mhf_loaded))
    {
      dprintf("  keeping scope at %p alive\n", module->scope);
      JS_MarkGCThing(cx, module->scope, module->cname, NULL);
    }

    /* Scrobj is a temporary place to root script modules between 
     * compilation and execution.
     */
    if (module->scrobj)
    {
      dprintf("  keeping scrobj at %p alive\n", module->scrobj);
      JS_MarkGCThing(cx, module->scrobj, module->cname, NULL);
    }
  }

  dpDepth(-1);
  return JS_TRUE;
}

/** Determine the libexec directory, which is the default directory in which
 *  to find modules. Libexec dir is generated first via the environment, if
 *  not found the rc file, and finally via the GPSEE_LIBEXEC_DIR define passed
 *  from the Makefile.
 *
 *  returns	A pointer to the libexec dir pathname
 */
static const char *libexecDir(void)
{
  const char *s = getenv("GPSEE_LIBEXEC_DIR");

  if (!s)
    s = rc_default_value(rc, "gpsee_libexec_dir", DEFAULT_LIBEXEC_DIR);

  return s;
}

/**
 *  Module system bootstapping code.  Modules are singletons and memoized
 *  per GPSEE Realm, but may span contexts when multithreaded. 
 * 
 *  This routine is intended to be called during realm-creation. As such, it
 *  is the caller's responsibility to insure it is only called once per realm.
 *
 *  @param      realm   The GPSEE Realm to which we are adding module capability
 *  @param      cx      The current JavaScript context; must belong to the passed realm.
 *
 *  @returns    JS_TRUE on success; JS_FALSE if we have thrown an exception.
 */
JSBool gpsee_initializeModuleSystem(gpsee_realm_t *realm, JSContext *cx)
{
  char			*envpath = getenv("GPSEE_PATH");
  char			*path;
  modulePathEntry_t	pathEl;

  /* Initialize basic module system data structures */
  realm->moduleJail = rc_value(rc, "gpsee_module_jail");
  dprintf("Initializing module system; jail starts at %s\n", realm->moduleJail ?: "/");
  dpDepth(+1);

  JS_SetGCCallback(cx, moduleGCCallback);

  realm->modulePath = JS_malloc(cx, sizeof(*realm->modulePath));
  if (!realm->modulePath)
    goto fail;
  memset(realm->modulePath, 0, sizeof(*realm->modulePath));

  realm->modules = malloc(sizeof *realm->modules);
  SPLAY_INIT(realm->modules);

  /* Populate the GPSEE module path */
  realm->modulePath->dir = JS_strdup(cx, libexecDir());
  if (envpath)
  {
    envpath = JS_strdup(cx, envpath);
    if (!envpath)
      goto fail;

    for (pathEl = realm->modulePath, path = strtok(envpath, ":"); path; pathEl = pathEl->next, path = strtok(NULL, ":"))
    {
      pathEl->next = JS_malloc(cx, sizeof(*pathEl));
      memset(pathEl->next, 0, sizeof(*pathEl->next));
      pathEl->next->dir = JS_strdup(cx, path);
    }

    JS_free(cx, envpath);
  }

  realm->moduleData = gpsee_ds_create(JS_GetRuntimePrivate(JS_GetRuntime(cx)), 5);

  dpDepth(-1);
  return JS_TRUE;

  fail:
  dpDepth(-1);
  return JS_FALSE;
}

/**
 *  Shut downt he module system for the given, cleaning up 
 *  all module-related resources which can be cleaned up at
 *  this time. gpsee_moduleSystemClean() will clean up the
 *  remaining resources.
 */
void gpsee_shutdownModuleSystem(gpsee_realm_t *realm, JSContext *cx)
{
  modulePathEntry_t	node, nextNode;
  moduleHandle_t	*module;

  dprintf("Shutting down module system\n");
  dpDepth(+1);

  /* Clean up modules by traversing the module memo tree */
  SPLAY_FOREACH(module, moduleMemo, realm->modules)
  {
    dprintf("Fini'ing module at 0x%p\n", module);
    if (module->fini)
    {
      module->fini(cx, module->exports);
      module->fini = NULL;
    }

    markModuleUnused(cx, module);
  }

  /* Clean up module paths */
  if (realm->userModulePath)
  {
    JS_RemoveRoot(cx, &realm->userModulePath);

    for (node = realm->modulePath; node; node = nextNode)
    {
      nextNode = node->next;
      JS_free(cx, (char *)node->dir);
    }

    JS_free(cx, realm->modulePath);
  }

  dpDepth(-1);
}

/**
 * Adjust an arbitrary global object to look like a program module so
 * that require() can run.  This is intended to hook GPSEE into unusual
 * environments where the concept of "program module" doesn't really apply, 
 * but we want to be able to use the module facilities nevertheless.
 *
 * This is also used to provide the preload module environment which is 
 * eventually overwritten by runProgramModule, if that's in use.
 *
 * @param       cx      JavaScript context
 * @param       glob    Object to modulize
 * @param       label   Label describing the object, used to generate a module cname, or NULL
 * @param       id      Number describing the object instance, used to generate a module cname, or 0
 *
 * @note        The pair (label, id) must be unique across the JS Runtime. If id is unspecified, the
 *              value of glob's pointer will be used.  If this happens, it is imperative that no 
 *              other global object be created at that address. The best way to do that is to never
 *              collect the initial one.
 *
 * @note        label may not being with the / character, otherwise the module memo may get confused
 *              and think this pseudo-anon-program module is in fact a real module that exists on disk.
 *
 * @returns     JS_TRUE on success;
 */
JSBool gpsee_modulizeGlobal(JSContext *cx, JSObject *glob, const char *label, size_t id)
{
  moduleHandle_t	*module;
  JSBool                b = JS_FALSE;
  char                  *cname;
  size_t                cnameLen;

  if (!label)
    label = "internal";
  if (label[0] == '/')
    return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".modulizeGlobal: invalid label");

  if (!id)
    id = (size_t)glob;

  dprintf("Modulizing global for %s module\n");
  dpDepth(+1);

  cnameLen = snprintf(NULL, 0, "__%s:" GPSEE_SIZET_FMT "__", label, id);
  cname = JS_malloc(cx, cnameLen + 1);
  if (!cname)
    goto out;
  sprintf(cname, "__%s:" GPSEE_SIZET_FMT "__", label, id);
  module = acquireModuleHandle(cx, cname, glob);
  if (!module)
  {
    dprintf("Could not acquire module handle for modulization of %s module\n", label);
    goto out;
  }

  if (initializeModuleScope(cx, module, glob, JS_TRUE) == JS_FALSE)
  {
    dprintf("Could not initialize module scope for module at %p\n", module);
    goto out;
  }

  b = JS_TRUE;

  out:
  if (cname)
    JS_free(cx, cname);
  dpDepth(-1);

  return b;
}

/** Perform final clean-up on module system which must be done
 *  after all the JavaScript objects have been finalized and
 *  the realm's contexts have all been destroyed. 
 *
 *  @note	This is why the module memo tree is allocated 
 *		with malloc rather than JS_malloc.
 */
void gpsee_moduleSystemCleanup(gpsee_realm_t *realm)
{
  moduleHandle_t	*module, *nextModule;

  for (module = SPLAY_MIN(moduleMemo, realm->modules);
       module != NULL;
       module = nextModule)
  {
    nextModule = SPLAY_NEXT(moduleMemo, realm->modules, module);
    SPLAY_REMOVE(moduleMemo, realm->modules, module);
    free(module);
  }
  
  free(realm->modules);
}

const char *gpsee_getModuleCName(moduleHandle_t *module)
{
  return module ? module->cname : NULL;
}

/**
 *  Find the GPSEE Data Store for the current realm which 
 *  exists to allow modules to store arbitrary memos. The
 *  Data store index is a unique pointer. This data store
 *  allows us to avoid using global variables in DSO modules
 *  to store information which may not be shared across
 *  across runtimes. 
 *
 *  By convention, the class pointer (which IS safe to share)
 *  is used as an index in this data store to look up the
 *  prototype generated by JS_InitClass() for the current
 *  context. These prototypes are expected to have a lifetime
 *  matching the visibility of the class pointer.
 *
 *  @param      cx                      Any context in the current realm.
 *  @param      dataStore_p     [out]   The module data store if we are successful.
 *  @returns    JS_TRUE on success, JS_FALSE if we threw an exception.
 *
 *  @note       While the module data store can be queried 
 *              safely in multithreaded coded due to internal
 *              synchronization, the lifetime of the value
 *              is defined by the code putting the data into
 *              the data store.  If the data is volatile, 
 *              a monitor surround use of the table and the
 *              extracted data should be used to insure consistency.
 */
JSBool gpsee_getModuleDataStore(JSContext *cx, gpsee_dataStore_t *dataStore_p)
{
  gpsee_realm_t *realm = gpsee_getRealm(cx);

  if (!realm)
    return JS_FALSE;

  if (!realm->moduleData)
    return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".realm.getModuleDataStore.notFound: module data store missing for realm " GPSEE_PTR_FMT "!", realm);

  *dataStore_p = realm->moduleData;
  return JS_TRUE;
}

/**
 *  Extract data from the module data store for the current realm, matching 
 *  the passed key. This is a helper function; using lower-level gpsee_dataStore_t
 *  APIs to access the data store does not violate the API.
 *
 *  @see gpsee_getModuleDataStore()
 *  @see gpsee_ds_get()
 *
 *  @param      cx                      Any context in the current realm.
 *  @param      key                     The describing the data to get
 *  @param      *value_p        [out]   The stored value
 *  @param      throwPrefix             Exception message prefix if we throw because the data was not found (or the found data was NULL).
 *  @returns    JS_TRUE on success, JS_FALSE if we threw an exception.
 */
JSBool gpsee_getModuleData(JSContext *cx, const void *key, void **data_p, const char *throwPrefix)
{
  gpsee_dataStore_t     ds;

  if (gpsee_getModuleDataStore(cx, &ds) == JS_FALSE)
    return JS_FALSE;

  *data_p = gpsee_ds_get(ds, key);
  if (!*data_p)
    return gpsee_throw(cx, "%s.moduleData.notFound: Data for key " GPSEE_PTR_FMT " was not found.", throwPrefix, key);

  return JS_TRUE;
}

/**
 *  Add/change data in the module data store for the current realm, matching 
 *  the passed key. This is a helper function; using lower-level gpsee_dataStore_t
 *  APIs to access the data store does not violate the API.
 *
 *  @see gpsee_getModuleDataStore()
 *  @see gpsee_ds_set()
 *
 *  @param      cx                      Any context in the current realm.
 *  @param      key                     The describing the data to get
 *  @param      value                   The value to store
 *  @param      throwPrefix             Exception message prefix if we throw because the data was not found (or the found data was NULL).
 *  @returns    JS_TRUE on success, JS_FALSE if we threw an exception.
 */
JSBool gpsee_setModuleData(JSContext *cx, const void *key, void *data)
{
  gpsee_dataStore_t     ds;

  if (gpsee_getModuleDataStore(cx, &ds) == JS_FALSE)
    return JS_FALSE;

  return gpsee_ds_put(ds, key, data);
}

