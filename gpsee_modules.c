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
 * Copyright (c) 2007-2009, PageMail, Inc. All Rights Reserved.
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
 *  @version	$Id: gpsee_modules.c,v 1.19 2010/02/14 13:17:54 wes Exp $
 *  @date	March 2009
 *  @file	gpsee_modules.c		GPSEE module load, unload, and management code for
 *					native, script, and blended modules.
 *
 * <b>Design Notes (warning, rather out of date)</b>
 *
 * Module handles are stored in a dynamically-realloc()d array, jsi->modules.
 * This means you cannot store a reference to a module handle unless you can
 * guarantee that the address of the entire array will not change when you're
 * not looking.
 *
 * - Running ANY JavaScript code can move the modules array
 * - Running any function which can call acquireModuleHandle() can move the 
 *   modules array. 
 * - Corrolary: calling module->init() and module->script may move jsi->modules.
 *   Best way to get around that is to re-calculate module immediately after any
 *   calls like that.
 * - Module objects and scopes will not move, as their allocation is managed 
 *   by JavaScript garbage collector
 * - The module scope object stores an offset to the module handle from the start 
 *   of the modules array in its private data
 *
 * Extra GC roots, provided by moduleGCCallback(), are available in each
 * module handle. They are:
 *  - scrobj:	Marked during module initialization, afterwards not needed
 *  - exports:	Marked during module initialization, afterwards by assignment in JavaScript
 *  - scope:	Marked when object is NULL; otherwise by virtue of object.parent
 * 
 ************
 *
 * Terminology
 * 
 *  moduleScope:  	Nearly equivalent to the var object of a closure which wraps the module
 *  parentModule: 	The module calling require(); usually this is the program module
 *  exports: 		An object which holds the return value of parentModule's require() call
 *  GPSEE module path:  The first place non-(internal|relative) modules are searched for; libexec dir etc.
 *
 ************
 * - exports stays rooted until no fini function
 * - scope stays rooted until no exports
 * - scope "owns" module handle, but cannot depend on exports
 * - exports cannot depend on scope
 */

static const char __attribute__((unused)) rcsid[]="$Id: gpsee_modules.c,v 1.19 2010/02/14 13:17:54 wes Exp $:";

#define _GPSEE_INTERNALS
#include "gpsee.h"
#include "gpsee_private.h"
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define moduleShortName(a)	strstr(a, gpsee_basename(getenv("PWD")?:"trunk")) ?: a

#if defined(GPSEE_DEBUG_BUILD)
# define RTLD_mode	RTLD_NOW
# define dprintf(a...) do { if (gpsee_verbosity(0) > 2) printf("> "), printf(a); } while(0)
#else
# define RTLD_mode	RTLD_LAZY
# define dprintf(a...) while(0) printf(a)
#endif

GPSEE_STATIC_ASSERT(sizeof(void *) >= sizeof(size_t));

extern rc_list rc;

typedef const char * (* moduleInit_fn)(JSContext *, JSObject *);	/**< Module initializer function type */
typedef JSBool (* moduleFini_fn)(JSContext *, JSObject *);		/**< Module finisher function type */

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
typedef const char *(* moduleLoader_t)(JSContext *, moduleHandle_t *, const char *);

static void finalizeModuleScope(JSContext *cx, JSObject *exports);
static void finalizeModuleExports(JSContext *cx, JSObject *exports);
static void setModuleHandle_forScope(JSContext *cx, JSObject *moduleScope, moduleHandle_t *module);
static moduleHandle_t *getModuleHandle_fromScope(JSContext *cx, JSObject *moduleScope);
static void markModuleUnused(JSContext *cx, moduleHandle_t *module);

static JSClass module_scope_class = 	/* private member is reserved by gpsee - holds module handle */
{
  GPSEE_GLOBAL_NAMESPACE_NAME ".module.Scope",
  JSCLASS_HAS_PRIVATE | JSCLASS_GLOBAL_FLAGS,
  JS_PropertyStub,  
  JS_PropertyStub,  
  JS_PropertyStub,  
  JS_PropertyStub,
  JS_EnumerateStub, 
  JS_ResolveStub,   
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
  finalizeModuleExports,
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
  size_t		slot;		/**< Index of this handle into jsi->modules */

  moduleHandle_t	*next;		/**< Used when treating as a linked list node, i.e. during DSO unload */
};

/**
 *  requireLock / requireUnlock form a simple re-entrant lockless "mutex", optimized for 
 *  the case where there are no collisions. These locks are per-runtime.
 *
 *  The requireLockThread holds the thread ID of the thread currrently holding this lock, or
 *  NULL (which is an invalid value for PRThread).  
 *
 *  requireLockThread is only read/written via CAS operations, forcing a full memory barrier 
 *  at the start of a locking operation. requireLockDepth may only be read or written by a 
 *  thread which holds the lock.  When the lock is released, the cas memory barrier insures 
 *  that the next lock-holder gets an initial depth of zero.
 */
static void requireLock(JSContext *cx)
{
#if defined(JS_THREADSAFE)
  gpsee_interpreter_t 	*jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  PRThread		*thisThread = PR_GetCurrentThread();

  if (jsval_CompareAndSwap((jsval *)&jsi->requireLockThread, (jsval)thisThread, (jsval)thisThread) == JS_TRUE)  /* membar */
    goto haveLock;

  do
  {
    jsrefcount depth;

    if (jsval_CompareAndSwap((jsval *)&jsi->requireLockThread, NULL, (jsval)thisThread) == JS_TRUE)
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
    if (jsval_CompareAndSwap((jsval *)&jsi->requireLockThread, (jsval)thisThread, NULL) != JS_TRUE) /* membar */
    {
      GPSEE_NOT_REACHED("bug in require-locking code");
    }
 }
#endif
  return;
}

/** Determine the libexec directory, which is the default directory in which
 *  to find modules. Libexec  dir is generated first via the environment, if
 *  not found the rc file, and finally via the GPSEE_LIBEXEC_DIR define passed
 *  from the Makefile.
 *
 *  returns	A pointer to the libexec dir pathname
 */
const char *gpsee_libexecDir(void)
{
  const char *s = getenv("GPSEE_LIBEXEC_DIR");

  if (!s)
    s = rc_default_value(rc, "gpsee_libexec_dir", DEFAULT_LIBEXEC_DIR);

  return s;
}

/**
 *  Create a new module object (exports)
 *
 *  This object is effectively a container for the  properties of the module,
 *  and is what loadModule() and require() return to the JS programmer.
 *
 *  The module object will be created with an unused private slot.
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
  if (exports)
  {
    if (JS_DefineProperty(cx, moduleScope, "exports", OBJECT_TO_JSVAL(exports), NULL, NULL, 
			  JSPROP_ENUMERATE | JSPROP_PERMANENT) != JS_TRUE)
      exports = NULL;
  }

  return exports;
}

/**
 *  Initialize a "fresh" module scope ("global") to have all the correct properties 
 *  and methods. This includes a require function which "knows" its path information
 *  by virtue of the module handle stored in private slot 0.
 *
 *  If the module global is not the interpreter's global, we also copy through
 *  the standard classes.
 *
 *  @param	cx			Current JavaScript context
 *  @param	module			Module handle for the module whose scope we are initializing.
 *					Handle must remain valid for the lifetime of this scope. Only
 *					the cname member is used by this function.
 *  @param	moduleScope		Scope object to initialize
 *
 *  @note	This function also sets module->exports
 *
 *  @returns	JS_TRUE or JS_FALSE if an exception was thrown
 */
static JSBool initializeModuleScope(JSContext *cx, moduleHandle_t *module, JSObject *moduleScope)
{
  JSFunction 		*require;
  gpsee_interpreter_t 	*jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  JSObject      	*modDotModObj;
  JSString      	*moduleId;

  GPSEE_ASSERT(module->exports == NULL || moduleScope == jsi->globalObj);
  GPSEE_ASSERT(JS_GET_CLASS(cx, moduleScope) == &module_scope_class || moduleScope == jsi->globalObj);

  setModuleHandle_forScope(cx, moduleScope, module);

  require = JS_DefineFunction(cx, moduleScope, "require", gpsee_loadModule, 1, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT);
  if (!require)
    return JS_FALSE;

  if (JS_DefineProperty(cx, (JSObject *)require, "paths", 
			OBJECT_TO_JSVAL(jsi->userModulePath), NULL, NULL,
			JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT) != JS_TRUE)
  {	
    return JS_FALSE;
  }

  if (JS_SetReservedSlot(cx, (JSObject *)require, 0, PRIVATE_TO_JSVAL(module)) == JS_FALSE)
    return JS_FALSE;

  module->exports = newModuleExports(cx, moduleScope);
  if (JS_DefineProperty(cx, moduleScope, "exports", 
			OBJECT_TO_JSVAL(module->exports), NULL, NULL,
			JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT) != JS_TRUE)
  {
    return JS_FALSE;
  }

  modDotModObj = JS_NewObject(cx, NULL, NULL, moduleScope);
  if (!modDotModObj)
    return JS_FALSE;

  if (JS_DefineProperty(cx, moduleScope, "module", OBJECT_TO_JSVAL(modDotModObj), NULL, NULL,
			JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY) == JS_FALSE)
  {
    return JS_FALSE;
  }

  /* Add 'id' property to 'module' property of module scope. */
  moduleId = JS_NewStringCopyZ(cx, module->cname);
  if (!moduleId)
    return JS_FALSE;

  if (JS_DefineProperty(cx, modDotModObj, "id", STRING_TO_JSVAL(moduleId), NULL, NULL,
		    JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY) == JS_FALSE)
  {
    return JS_FALSE;
  }

  /** XXX copy through standard classes */

  return JS_TRUE;
}

/**
 *  Create an object to act as the global object for the module. The module's
 *  exports object will be a property of this object, along with
 *  module-global private variables.
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
  JSObject 	*moduleScope;

  GPSEE_ASSERT(module);

  if (JS_EnterLocalRootScope(cx) != JS_TRUE)
    return NULL;

  moduleScope = JS_NewObject(cx, &module_scope_class, NULL, NULL);
  if (!moduleScope)
      goto errout;
  
  if (initializeModuleScope(cx, module, moduleScope) == JS_FALSE)
    goto errout;

  JS_LeaveLocalRootScopeWithResult(cx, OBJECT_TO_JSVAL(moduleScope));

  return moduleScope;

  errout:
  JS_LeaveLocalRootScope(cx);
  return NULL;
}

/** Find existing or create new module handle.
 *
 *  Module handles are used to 
 *  - track shutdown requirements over the lifetime of  the interpreter
 *  - track module objects so they don't get garbage collected
 *
 *  Note that the module objects in here can't be made GC roots, as the
 *  whole structure is subject reallocation. Hence moduleGCCallback().
 *
 *  Any module handle returned from this routine is guaranteed to have a cname, so that it
 *  can be identified.
 *
 *  @param cx			JavaScript context
 *  @param cname                Canonical name of the module we're interested in; may contain slashses, relative ..s etc,
 *                              or name an internal module.
 *  @param     	moduleScope	Scope object to be used instead of a brand-new one if we need to create a module handle.
 *  @returns 			A module handle, possibly one already initialized, or NULL if an exception has been thrown.
 */
static moduleHandle_t *acquireModuleHandle(JSContext *cx, const char *cname, JSObject *moduleScope)
{
  moduleHandle_t	*module, **moduleSlot = NULL;
  gpsee_interpreter_t 	*jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  size_t		i;

  GPSEE_ASSERT(cname != NULL);
  dprintf("Acquiring module handle for %s\n", cname);

  /* Scan for free slots and/or cname repetition (modules are singletons) */
  for (i=0; i < jsi->modules_len; i++)
  {
    module = jsi->modules[i];

    if (module && module->cname && (strcmp(module->cname, cname) == 0))
    {
      dprintf("Returning used module handled at %p\n", module);
      return module;	/* seen this one already */
    }

    if (!moduleSlot && !module)
      moduleSlot = jsi->modules + i; /* don't break, singleton check still! */
  }
  
  if (!moduleSlot)	/* No free slots, allocate some more */
  {
    moduleHandle_t **modulesRealloc;
    size_t incr;

    /* Grow the pointer array jsi->modules by a healthy increment */
    incr = max(jsi->modules_len / 2, 64);

    /* Reallocate pointer array */
    modulesRealloc = JS_realloc(cx, jsi->modules, sizeof(jsi->modules[0]) * (jsi->modules_len + incr));
    if (!modulesRealloc)
      panic(GPSEE_GLOBAL_NAMESPACE_NAME ".modules.acquireHandle: Out of memory in " __FILE__);

    /* Initialize new pointer array members to NULL */
    memset(modulesRealloc + jsi->modules_len, 0, incr * sizeof(*modulesRealloc));

    /* Implement the change in address caused by reallocation */
    moduleSlot = modulesRealloc + jsi->modules_len;
    jsi->modules = modulesRealloc;
    jsi->modules_len += incr;
  }

  /* Allocate the new moduleHandle_t, inserting its pointer into jsi->modules */
  *moduleSlot = calloc(1, sizeof(**moduleSlot));
  if (!*moduleSlot)
  {
    JS_ReportOutOfMemory(cx);
    return NULL;
  }
  module = *moduleSlot;
  module->slot = moduleSlot - jsi->modules;

  dprintf("Allocated new module slot for %s in slot %ld at 0x%p\n", 
	  cname ? moduleShortName(cname) : "program module", (long int)(moduleSlot - jsi->modules), module);

  if (cname)
  {
    module->cname = JS_strdup(cx, cname);
  }

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

  dprintf("Module handle is at %p\n", module);
  return module;

fail:
  markModuleUnused(cx, module);
  return NULL;
}

/** Marking a module unused makes it eligible for immediate garbage 
 *  collection. Safe to call from a finalizer.
 */
static void markModuleUnused(JSContext *cx, moduleHandle_t *module)
{
  dprintf("Marking module at %p (%s) as unused\n", module, module->cname?:"no name");

  if (module->flags & ~mhf_loaded && module->exports && module->fini)
    module->fini(cx, module->exports);

  module->scope   = NULL;
  module->scrobj  = NULL;
  module->exports = NULL;
  module->fini	  = NULL;

  module->flags	 &= ~mhf_loaded;
}

/** Forget a module handle, as far as GPSEE is concerned, so that it is
 *  not reused, e.g. during module load etc.  This is an important
 *  precursor to module unloading.
 *
 *  Is safe to call in a finalizer.
 */
static void forgetModuleHandle(JSContext *cx, moduleHandle_t *module)
{
  gpsee_interpreter_t 	*jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));

  dprintf("Forgetting module at 0x%p\n", module);
  if (jsi->modules[module->slot] == module)
    jsi->modules[module->slot] = NULL;
}

/**
 *  Return resources used by a module handle to the OS or other
 *  underlying allocator.  
 * 
 *  This routine is intended be called from the garbage collector
 *  callback afterall finalizable JS gcthings have been finalized.
 *  This is because some of those gcthings might need C resources
 *  for finalization.  The canonical example is that finalizing a
 *  prototype object for a class which has its clasp in a DSO. It
 *  is imperative that the DSO stay around until AFTER the prototype
 *  is finalized, but there is no direct way to know that since 
 *  finalization order is random.
 */
static moduleHandle_t *finalizeModuleHandle(JSContext *cx, moduleHandle_t *module)
{
  moduleHandle_t *next = module->next;

  dprintf("Finalizing module handle at 0x%p\n", module);
  if (module->cname)
    JS_free(cx, (char *)module->cname);

  if (module->DSOHnd)
  {
    dprintf("unloading DSO at 0x%p for module 0x%p\n", module->DSOHnd, module);
    dlclose(module->DSOHnd);
  }

#if defined(GPSEE_DEBUG_BUILD)
  memset(module, 0xba, sizeof(*module));
#endif    

  free(module);

  return next;
}

/** 
 *  Release all memory allocated during the creation of the handle,
 *  with the exception of the array slot itself.  We allow this to 'leak'
 *  since it's small and will be reused soon - on next module load.
 *
 *  This routine does no clean-up of its slot in the module list.
 *
 *  Note that "release" in this case means "schedule for finalization";
 *  the finalization actually returns the resources to the OS or other
 *  underlying resources allocators. Calling this routine twice on 
 *  the same module will cause problems. Specifically, it will cause
 *  the GC callback to double-free or free garbage.  As such it is
 *  recommended to ONLY call this routine from the scope finalizer.
 *
 *  @param	cx	JavaScript context handle
 *  @param	jsi	Script interpreter handle
 *  @param	module	Handle to destroy
 */
static void releaseModuleHandle(JSContext *cx, moduleHandle_t *module)
{
  gpsee_interpreter_t 	*jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));

  dprintf("Releasing module at 0x%p\n", module);

  forgetModuleHandle(cx, module);
  markModuleUnused(cx, module);

  /* Actually release OS resources after everything on JS
   * side has been finalized. This is especially important
   * for DSO modules, as dlclosing() before JS object finalizer
   * has read clasp is disastrous 
   */
  module->next = jsi->unreachableModule_llist;
  jsi->unreachableModule_llist = module;
}

/**
 *  Retrieve the module handle associated with the scope.
 *  Safe to run in a finalizer.
 */
static moduleHandle_t *getModuleHandle_fromScope(JSContext *cx, JSObject *moduleScope)
{
  dprintf("Retrieving module with scope 0x%p\n", moduleScope);
  return JS_GetPrivate(cx, moduleScope);
}

/**
 *  Note the module handle associated with the scope.
 *  Safe to run in a finalizer.
 */
static void setModuleHandle_forScope(JSContext *cx, JSObject *moduleScope, moduleHandle_t *module)
{
  dprintf("Noting module with scope 0x%p\n", moduleScope);
  JS_SetPrivate(cx, moduleScope, module);
}

static void finalizeModuleScope(JSContext *cx, JSObject *moduleScope)
{
  moduleHandle_t	*module;
  dprintf("begin finalizing module scope\n");

  module = getModuleHandle_fromScope(cx, moduleScope);
  GPSEE_ASSERT(module != NULL);
  GPSEE_ASSERT(module->fini == NULL);

  releaseModuleHandle(cx, module);

  dprintf("done finalizing module scope\n");
}

static void finalizeModuleExports(JSContext *cx, JSObject *exports)
{
  gpsee_interpreter_t 	*jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  size_t		i;

  dprintf("begin finalizing module exports\n");

  /** Module exports are finalized by "forgetting" them */
  for (i=0; i < jsi->modules_len; i++)
  {
    if (!jsi->modules[i])
      continue;

    if (jsi->modules[i] && jsi->modules[i]->exports == exports)
    {
      jsi->modules[i]->exports = NULL;
      break;
    }
  }

  dprintf("done finalizing module exports\n");
  return;
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
 *   @returns	NULL if the module was found,
 *		or a descriptive error string if the module was found but could not load.
 */
static const char *loadJSModule(JSContext *cx, moduleHandle_t *module, const char *filename)
{
  const char *errorMessage;
  dprintf("loadJSModule(\"%s\")\n", filename);

  if (gpsee_compileScript(cx, filename, NULL, &module->script,
      module->scope, &module->scrobj, &errorMessage))
    return errorMessage;

  return NULL;
}

/**
 *   Load a DSO module. These modules are linked in at run-time, so we compute the 
 *   addresses of the symbols with dlsym().
 *
 *   @param	cx			JS Context
 *   @returns	NULL if the module was found,
 *		or a descriptive error string if the module was found but could not load.
 */
static const char *loadDSOModule(JSContext *cx, moduleHandle_t *module, const char *filename)
{
  jsrefcount 	depth;

  depth = JS_SuspendRequest(cx);
  module->DSOHnd = dlopen(filename, RTLD_mode);
  JS_ResumeRequest(cx, depth);

  if (!module->DSOHnd)
  {
    return dlerror();
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

  return NULL;
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
JSBool JSArray_toModulePath(JSContext *cx, JSObject *arrObj, modulePathEntry_t *modulePath_p)
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
      continue;

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
void freeModulePath_fromJSArray(JSContext *cx, modulePathEntry_t modulePath)
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
 *  @param	jsi		Current GPSEE context
 *  @param	cx		Current JS context
 *  @param	moduleName	Name of the module (argument to require)
 *  @param	directory	Directory in which to find the module
 *  @param	module_p    	[out]	Module handle, if found. May represet a previously-loaded module. 
 *				      	If not found, *module_p is NULL.
 *
 *  @returns	JS_TRUE on success, or JS_FALSE if an exception was thrown.
 */
static JSBool loadDiskModule_inDir(gpsee_interpreter_t *jsi, JSContext *cx, const char *moduleName, const char *directory,
				    moduleHandle_t **module_p)
{
  const char		**ext_p, *extensions[] 	= { DSO_EXTENSION,	"js", 		NULL };
  moduleLoader_t	loaders[] 		= { loadDSOModule, 	loadJSModule};
  moduleHandle_t	*module = NULL;
  char			fnBuf[PATH_MAX];

  for (ext_p = extensions; *ext_p; ext_p++)
  {
    if (snprintf(fnBuf, sizeof(fnBuf), "%s/%s.%s", directory, moduleName, *ext_p) >= sizeof(fnBuf))
    {
      GPSEE_ASSERT("path buffer overrun");
      continue;
    }

    dprintf("%s() trying filename '%s'\n", __func__, fnBuf);
    if (access(fnBuf, F_OK) == 0)
    {
      const char 	*errmsg;
      char		cnBuf[PATH_MAX];
      char		*s;

      if (!module)
      {
	int i = gpsee_resolvepath(fnBuf, cnBuf, sizeof(cnBuf));
	if (i == -1)
	  return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".loadModule.disk.canonicalize: "
			     "Error canonicalizing '%s' (%m)", fnBuf);
	if (i >= sizeof(cnBuf))
	  return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".loadModule.disk.canonicalize.overflow: "
			     "Error canonicalizing '%s' (buffer overflow)", fnBuf);

	if (!checkJail(cnBuf, jsi->moduleJail))
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

      errmsg = loaders[ext_p - extensions](cx, module, fnBuf);
      if (errmsg)
      {
	if (module)
	  markModuleUnused(cx, module);
	*module_p = NULL;
	return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".loadModule.disk: %s", errmsg);
      }
    }
  }

  *module_p = module;
  return JS_TRUE;
}

/** Iterate over a modulePath, try to find an appropriate module on disk, and load it.
 *
 *  @param	jsi		Current GPSEE context
 *  @param	cx		Current JS context
 *  @param	moduleName	Name of the module (argument to require)
 *  @param	modulePath	Linked list of module paths to try, in order
 *  @param	module_p    	[out]	Module handle, if found. May represet a previously-loaded module. 
 *				      	If not found, *module_p is set to NULL.
 *
 *  @returns	JS_TRUE on success, or JS_FALSE if an exception was thrown.
 */
static JSBool loadDiskModule_onPath(gpsee_interpreter_t *jsi, JSContext *cx, const char *moduleName, modulePathEntry_t modulePath, 
				     moduleHandle_t **module_p)
{
  modulePathEntry_t	pathEl;

  for (pathEl = modulePath,	*module_p = NULL;
       pathEl && 		*module_p == NULL; 
       pathEl = pathEl->next)
  {
    if (loadDiskModule_inDir(jsi, cx, moduleName, pathEl->dir, module_p) == JS_FALSE)
      return JS_FALSE;
  }

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
  gpsee_interpreter_t 	*jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  modulePathEntry_t	requirePaths;

  *module_p = NULL;

  /* Handle relative module names */
  if (isRelativePath(moduleName))
  {
    const char	*currentModulePath;
    char	pmBuf[PATH_MAX];

    currentModulePath = gpsee_dirname(parentModule->cname, pmBuf, sizeof(pmBuf));

    if (loadDiskModule_inDir(jsi, cx, moduleName, currentModulePath, module_p) == JS_FALSE)
      return JS_FALSE;
    if (!*module_p)
      return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".loadModule.disk: Error loading relative module '%s': "
			 "module not found in '%s'", moduleName, currentModulePath);
    return JS_TRUE;
  }

  /* Handle absolute module names */
  if (moduleName[0] == '/')
  {
    if (loadDiskModule_inDir(jsi, cx, moduleName, "", module_p) == JS_FALSE)
      return JS_FALSE;
    if (!*module_p)
      return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".loadModule.disk: Error loading absolute module '%s': module not found",
			 moduleName);
  }

  /* Search GPSEE module path */
  if (loadDiskModule_onPath(jsi, cx, moduleName, jsi->modulePath, module_p) == JS_FALSE)
    return JS_FALSE;
  if (*module_p)
    return JS_TRUE;

  /* Search require.paths */
  if (JSArray_toModulePath(cx, jsi->userModulePath, &requirePaths) == JS_FALSE)
    return JS_FALSE;
  if (requirePaths)
  {
    JSBool b = loadDiskModule_onPath(jsi, cx, moduleName, requirePaths, module_p);
    freeModulePath_fromJSArray(cx, requirePaths);
    if (b == JS_FALSE)
      return JS_FALSE;
    if (*module_p)
      return JS_TRUE;
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
 *  @returns module ptr on success, NULL on failure. Some failures may be the result of JavaScript exceptions.
 * 
 *  @note Returns module handle for a reason! This code can reallocate the backing module array. 
 *        Module on the way in may be at a different address on the way out.
 */
static moduleHandle_t *initializeModule(JSContext *cx, moduleHandle_t *module, const char **errorMessage_p)
{
  JSObject		*moduleScope = module->scope;

  dprintf("initializeModule(%s)\n", module->cname);

  if (module->init)
  {
    const char *id;

    id = module->init(cx, module->exports); /* realloc hazard */
    module = getModuleHandle_fromScope(cx, moduleScope);
    GPSEE_ASSERT(module != NULL);
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
    b = JS_ExecuteScript(cx, module->scope, script, &dummyval); /* realloc hazard */
    module = getModuleHandle_fromScope(cx, moduleScope);

    module->scrobj = NULL;	/* No longer needed */
  }

  return module;
}

/** Implements the CommonJS require() function, allowing JS inclusive-or native module types.
 *  First argument is the module name.
 */
JSBool gpsee_loadModule(JSContext *cx, JSObject *thisObject, uintN argc, jsval *argv, jsval *rval)
{
  const char		*moduleName;
  const char		*errorMessage = "unknown error";
  moduleHandle_t	*module, *m;
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
    requireUnlock(cx);
    *rval = OBJECT_TO_JSVAL(module->exports);
    dprintf("no reload singleton %s\n", moduleShortName(moduleName));
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
  if ((m = initializeModule(cx, module, &errorMessage)))
    module = m;
  dprintf("Initialized module now at 0x%p\n", m);

  if (!m)
  {
    markModuleUnused(cx, module);
    requireUnlock(cx);
    if (JS_IsExceptionPending(cx))
      return JS_FALSE;

    return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".loadModule.init: Unable to initialize module '%s'", moduleName);
  }

  requireUnlock(cx);
  *rval = OBJECT_TO_JSVAL(module->exports);

  return JS_TRUE;
}

/** Run a program as if it were a module. Interface differs from gpsee_loadModule()
 *  to reflect things like that the source of the program module may be stdin rather
 *  than a module file, may not be in the libexec dir, etc.
 *
 *  It is possible to have more than one program module in the module list at once,
 *  provided we can generate reasonable canonical names for each one.
 *
 *  Note that only JavaScript modules can be program modules.
 *
 *  @param 	cx		JavaScript context for execution
 *  @param	scriptFilename	Filename where we can find the script
 *  @param	scriptFile	Stream positioned where we can start reading the script, or NULL
 *
 *  @returns	NULL on success, error string on failure
 */
const char *gpsee_runProgramModule(JSContext *cx, const char *scriptFilename, FILE *scriptFile)
{
  moduleHandle_t 	*module;
  char			cnBuf[PATH_MAX];
  char			fnBuf[PATH_MAX];
  int			i;
  const char		*errorMessage = NULL;
  gpsee_interpreter_t 	*jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  char			*s;

  if (scriptFilename[0] == '/')
  {
    i = gpsee_resolvepath(scriptFilename, cnBuf, sizeof(cnBuf));
  }
  else
  {
    char	tmpBuf[PATH_MAX];
    
    if (getcwd(tmpBuf, sizeof(tmpBuf)) == NULL)
      return "getcwd failure";

    if (gpsee_catstrn(tmpBuf, "/", sizeof(tmpBuf)) == 0)
      return "buffer overrun";

    if (gpsee_catstrn(tmpBuf, scriptFilename, sizeof(tmpBuf)) == 0)
      return "buffer overrun";

    i = gpsee_resolvepath(tmpBuf, cnBuf, sizeof(cnBuf));
  }
  if ((i >= sizeof(cnBuf)) || (i == -1))
    return "buffer overrun";
  else
    cnBuf[i] = (char)0;

  if (scriptFile)
  {
    /* Insure that the supplied script file stream matches the file name */
    int 	e = 0;
    struct stat sb1, sb2;

    e = stat(cnBuf, &sb1);
    if (!e)
      e = fstat(fileno(scriptFile), &sb2);

    if (e)
      gpsee_log(SLOG_ERR, "Unable to stat program module %s: %m", cnBuf);
    else
    {
      if ((sb1.st_ino != sb2.st_ino) || (sb1.st_dev != sb2.st_dev))
	gpsee_log(SLOG_ERR, "Program module does not have a valid canonical name");
    }
  }

  /* Before mangling cnBuf, let's copy it to fnBuf to be used in gpsee_compileScript() */
  strcpy(fnBuf, cnBuf);

  /* The "cname" argument to acquireModuleHandle() names a module, not a file, so let's remove the .js extension */
  if ((s = strrchr(cnBuf, '.')))
  {
    if (strcmp(s, ".js") == 0)
      *s = (char)0;
  }

  module = acquireModuleHandle(cx, cnBuf, jsi->globalObj);
  if (!module)
  {
    dprintf("Could not acquire module handle for program module\n");
    errorMessage = "could not allocate module handle";
    goto fail;
  }

  if (initializeModuleScope(cx, module, jsi->globalObj) == JS_FALSE)
  {
    dprintf("Could not initialize module scope for module at %p\n", module);
    goto fail;
  }

  dprintf("Program module root is %s\n", module->cname);
  dprintf("compiling program module %s\n", moduleShortName(module->cname));

  if (gpsee_compileScript(cx, fnBuf, scriptFile, &module->script,
      module->scope, &module->scrobj, &errorMessage))
    goto fail;

  /* Enable 'mhf_loaded' flag before calling initializeModule() */
  module->flags |= mhf_loaded;	

  if (initializeModule(cx, module, &errorMessage))
  {
    dprintf("done running program module %s\n", moduleShortName(module->cname));
    goto good;
  }

  /* More serious errors get handled here in "fail:" but twe fall through to "good:" where
   * additional and/or less serious error reporting facilities exist.
   */
  fail:
  GPSEE_ASSERT(errorMessage);
  if (!errorMessage)
    errorMessage = "unknown failure";

  dprintf("failed running program module %s\n", module ? moduleShortName(module->cname) : "(null)");
  gpsee_log(gpsee_verbosity(0) ? SLOG_NOTICE : SLOG_EMERG, "Failed loading program module '%s': %s", scriptFilename, errorMessage);

  good:

  /* If there is a pending exception, we'll report it here */
  if (JS_IsExceptionPending(cx))
  {
    int exitCode;
    /* Is this a formal SystemExit? Assume all error reporting has been performed by the application. */
    exitCode = gpsee_getExceptionExitCode(cx);
    if (exitCode >= 0)
    {
      jsi->exitCode = exitCode;
      jsi->exitType = et_finished;
    }
    /* Any other type of program exception will be reported here */
    else
    {
      /* @todo where should we publish this stuff to? */
      gpsee_reportUncaughtException(cx, JSVAL_NULL, stderr, NULL, 0);
      jsi->exitType = et_exception;
    }
  }

  if (module)
    markModuleUnused(cx, module);
 
  return errorMessage; /* NULL == no failures */
}

/** Callback from the garbage collector, which marks all
 *  module objects so that they are not collected before
 *  their Fini functions are called.
 */
static JSBool moduleGCCallback(JSContext *cx, JSGCStatus status)
{
  gpsee_interpreter_t   *jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  size_t		i;

  if (status == JSGC_FINALIZE_END)
  {
    while (jsi->unreachableModule_llist)
      jsi->unreachableModule_llist = finalizeModuleHandle(cx, jsi->unreachableModule_llist);
  }

  if (status != JSGC_MARK_END)
    return JS_TRUE;

  for (i=0; i < jsi->modules_len; i++)
  {  
    moduleHandle_t *module;

    if ((module = jsi->modules[i]) == NULL)
      continue;

    /* Modules which have an un-run fini method, or are not loaded, 
     * should not have their exports object collected.
     */
    if (!(module->flags & mhf_loaded) || module->fini)
      JS_MarkGCThing(cx, module->exports, module->cname, NULL);

    /* Modules which have exports should not have their scopes collected */
    if (module->scope && (module->exports || !(module->flags & mhf_loaded)))
      JS_MarkGCThing(cx, module->scope, module->cname, NULL);

    /* Scrobj is a temporary place to root script modules between 
     * compilation and execution.
     */
    if (module->scrobj)
      JS_MarkGCThing(cx, module->scrobj, module->cname, NULL);
  }

  return JS_TRUE;
}

JSBool gpsee_initializeModuleSystem(gpsee_interpreter_t *jsi, JSContext *cx)
{
  char			*envpath = getenv("GPSEE_PATH");
  char			*path;
  modulePathEntry_t	pathEl;
  moduleHandle_t	*module;

  jsi->moduleJail = rc_value(rc, "gpsee_module_jail");
  dprintf("Initializing module system; jail starts at %s\n", jsi->moduleJail ?: "/");
  JS_SetGCCallback(cx, moduleGCCallback);

  jsi->modulePath = JS_malloc(cx, sizeof(*jsi->modulePath));
  if (!jsi->modulePath)
    return JS_FALSE;
  memset(jsi->modulePath, 0, sizeof(*jsi->modulePath));

  /* Populate the GPSEE module path */
  jsi->modulePath->dir = JS_strdup(cx, gpsee_libexecDir());
  if (envpath)
  {
    envpath = JS_strdup(cx, envpath);
    if (!envpath)
      return JS_FALSE;

    for (pathEl = jsi->modulePath, path = strtok(envpath, ":"); path; pathEl = pathEl->next, path = strtok(NULL, ":"))
    {
      pathEl->next = JS_malloc(cx, sizeof(*pathEl));
      memset(pathEl->next, 0, sizeof(*pathEl->next));
      pathEl->next->dir = path;
    }
  }

  JS_AddNamedRoot(cx, &jsi->userModulePath, "User Module Path");
  jsi->userModulePath = JS_NewArrayObject(cx, 0, NULL);
  if (!jsi->userModulePath)
    return JS_FALSE;

  module = acquireModuleHandle(cx, "__preload__", jsi->globalObj);
  if (!module)
  {
    dprintf("Could not acquire module handle for preload code\n");
    return JS_FALSE;
  }

  if (initializeModuleScope(cx, module, jsi->globalObj) == JS_FALSE)
  {
    dprintf("Could not initialize module scope for module at %p\n", module);
    return JS_FALSE;
  }

  return JS_TRUE;
}

void gpsee_shutdownModuleSystem(gpsee_interpreter_t *jsi, JSContext *cx)
{
  size_t		i;
  modulePathEntry_t	node, nextNode;

  dprintf("Shutting down module system\n");

  for (i=0; i < jsi->modules_len; i++)
  {  
    moduleHandle_t *module;

    if ((module = jsi->modules[i]) == NULL)
      continue;

    dprintf("Fini'ing module at 0x%p\n", module);

    if (module->fini)
    {
      module->fini(cx, module->exports);
      module->fini = NULL;
    }

    markModuleUnused(cx, module);
  }

  JS_RemoveRoot(cx, &jsi->userModulePath);

  for (node = jsi->modulePath; node; node = nextNode)
  {
    nextNode = node->next;
    JS_free(cx, (char *)node->dir);
  }

  JS_free(cx, jsi->modulePath);
}
