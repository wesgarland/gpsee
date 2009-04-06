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
 *  @version	$Id: gpsee_modules.c,v 1.2 2009/04/01 22:30:55 wes Exp $
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
 *  - object:	Marked during module initialization, afterwards by assignment in JavaScript
 *  - scope:	Marked when object is NULL; otherwise by virtue of object.parent
 * 
 ************

 - exports stays rooted until no fini function
 - scope stays rooted until no exports
 - scope "owns" module handle, but cannot depend on exports
 - exports cannot depend on scope
 */

static const char __attribute__((unused)) rcsid[]="$Id: gpsee_modules.c,v 1.2 2009/04/01 22:30:55 wes Exp $:";

#define _GPSEE_INTERNALS
#include "gpsee.h"
#include "gpsee_private.h"
#include <dlfcn.h>

#define moduleShortName(a)	strstr(a, gpsee_basename(getenv("PWD")?:"trunk")) ?: a

#if 1
  #define dprintf(a...) do { if (gpsee_verbosity(0) > 2) printf("> "), printf(a); } while(0)
#else
  #define dprintf(a...) while(0) printf(a)
#endif

#ifdef GPSEE_DEBUG_BUILD
# define RTLD_mode	RTLD_NOW
#else
# define RTLD_mode	RTLD_LAZY
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

static const char *moduleNotFoundErrorMessage = "module not found";
static void finalizeModuleScope(JSContext *cx, JSObject *moduleObject);
static void finalizeModuleObject(JSContext *cx, JSObject *moduleObject);
static void setModuleHandle_forScope(JSContext *cx, JSObject *moduleScope, moduleHandle_t *module);
static moduleHandle_t *getModuleHandle_fromScope(JSContext *cx, JSObject *moduleScope);
static void markModuleUnused(JSContext *cx, moduleHandle_t *module);

static JSClass module_scope_class = 	/* private member is reserved by gpsee - holds module handle */
{
  GPSEE_GLOBAL_NAMESPACE_NAME ".module.Scope",
  JSCLASS_HAS_PRIVATE,
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
  finalizeModuleObject,
  JSCLASS_NO_OPTIONAL_MEMBERS
};  

typedef enum 
{ 
  mhf_loaded	= 1 << 0,
} moduleHandle_flags_t;

struct moduleHandle
{
  const char 		*cname;		/**< Name of the module, used to generate load path */
  const char		*id;		/**< Unique ID describing the module, generated by module init */
  void 			*DSOHnd;	/**< Handle to dlopen() shared object for DSO modules */
  moduleInit_fn		init;		/**< Function to run when module done loading */
  moduleFini_fn		fini;		/**< Function to run just before module is unloaded */
  JSObject		*object;	/**< JavaScript object holding module contents (require/loadModule return). 
					     Private slot reserved for module's use. */
  JSObject		*scope;		/**< Scope object for JavaScript modules; preserved only from load->init */
  JSScript		*script;	/**< Script object for JavaScript modules; preserved only from load->init */
  JSObject		*scrobj;	/**< GC thing for script, used by GC callback */

  char			*idStorage;	/**< Storage for module id, should come from malloc if used */
  moduleHandle_flags_t	flags;		/**< Special attributes of module; bit-field */
  size_t		slot;		/**< Index of this handle into jsi->modules */

  moduleHandle_t	*next;		/**< Used when treating as a linked list node, i.e. during DSO unload */
};

/** Retrieve the parent module based on the scopedObject of the current call.
 *  Safe to run inside a finalizer.
 */
static JSObject *findModuleScope(JSContext *cx, JSObject *scopedObject)
{
  JSObject		*obj;

  /* Find module scope by walking up scope chain */
  for (obj = scopedObject; obj != NULL; obj = JS_GetParent(cx, obj))
  {
    if (JS_GET_CLASS(cx, obj) == &module_scope_class)
      return obj;
  }

  /* No module scope found, must be global */
  return NULL;
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

/** Determine the canonical name for a module.  Path name of parent module 
 *  is concatenated with moduleName (which may contain slashes etc) to form
 *  a fully-qualified filename.  The directory component of this fully-qualified
 *  filename is canonicalized to remove .. references, symbolic links, and so on
 *  All directories in the path must exist and be readable for this routine to 
 *  succeed. It does not care about the end filename's existence.
 *
 *  @param parentModule		Parent module's handle, or NULL if none (or to force libexec dir)
 *  @param moduleName		Name of the module we're interested in; may contain slashses, relative ..s etc
 *  @param cnBuf	        Buffer to used to generate the result; suggest PATH_MAX bytes.
 *  @param cnBuf_size		Size of cnBuf's memory allocation.
 *  @returns 			A string containing a caonical directory which exists and is readable, or NULL.
 */
static const char *generateCanonicalName(const char *moduleDir, const char *moduleName, char *cnBuf, size_t cnBuf_size)
{
  char			tmpBuf[PATH_MAX];
  int			i;
  char			*s;
  const char		*cs;

  s = gpsee_cpystrn(tmpBuf, moduleDir, sizeof(tmpBuf));
  if (s == tmpBuf + sizeof(tmpBuf))
    return NULL;	/* overrun */

  /* tmpBuf now has parent module's path in it */
  if (gpsee_catstrn(tmpBuf, "/", sizeof(tmpBuf)) == 0)
    return NULL;

  if (gpsee_catstrn(tmpBuf, moduleName, sizeof(tmpBuf)) == 0)
    return NULL;

  /* tmpBuf now has full module name in it, less type-specifying extensions, plus .. and so forth */
  s = strrchr(tmpBuf, '/');
  *s = (char)0;

  /* tmpBuf is directory-only: resolve .., links, etc into cnBuf */
  i = gpsee_resolvepath(tmpBuf, cnBuf, cnBuf_size);
  if ((i >= cnBuf_size - 1) || (i <= 0))
    return NULL;
  else
    cnBuf[i] = (char)0;

  /* append module basename to cnBuf & we're done */
  cs = gpsee_basename(moduleName);
  if (cs[0] == '.')
  {
    if (cs[1] == (char)0)
      return NULL;

    if ((cs[1] == '.') && cs[2] == (char)0)
      return NULL;
  }

  if (gpsee_catstrn(cnBuf, "/", cnBuf_size) == 0)
    return NULL;

  if (gpsee_catstrn(cnBuf, cs, cnBuf_size) == 0)
    return NULL;

  return cnBuf;
}

/**
 *  Retrieve the existing module object (exports) for a given  
 *  scope, or create a new one. 
 *
 *  This object is effectively a container for the  properties of the module,
 *  and is what loadModule() and require() return to the JS programmer.
 *
 *  The module object will be created with an unused private slot.
 *  This slot is for use by the module's Fini code, based on data
 *  the module's Init code puts there.
 *
 *  @param	cx 		The JS context to use
 *  @param	moduleScope	The module scope we need the module object for.
 *  @return	A new, valid, module object or NULL. Module object is unrooted, 
 *		and thus should be assigned to a rooted address immediately.
 */
static JSObject *acquireModuleObject(JSContext *cx, JSObject *moduleScope)
{
  JSObject 		*moduleObject;
  moduleHandle_t	*module;

  if (JS_GetClass(cx, moduleScope) != &module_scope_class)
    return NULL;

  if ((module = getModuleHandle_fromScope(cx, moduleScope)) && module->object)
    return module->object;

  if (JS_EnterLocalRootScope(cx) != JS_TRUE)
    return NULL;
 
  moduleObject = JS_NewObject(cx, &module_exports_class, NULL, moduleScope);
  if (moduleObject)
  {
    jsval v = OBJECT_TO_JSVAL(moduleObject);

    if (JS_DefineProperty(cx, moduleScope, "exports", v, NULL, NULL, JSPROP_ENUMERATE | JSPROP_PERMANENT) != JS_TRUE)
      moduleObject = NULL;
  }

  JS_LeaveLocalRootScope(cx);

  return moduleObject;
}

/**
 *  Create an object to act as the scope for the module. The module's
 *  exports object will be a property of this object, along with
 *  module-global private variables.
 *
 *  The private slot for this object is reserved for (and contains) the module handle.
 *
 *  @returns	NULL on failure, or an object which is in unrooted
 *		but in the recently-created slot. 
 */
static JSObject *newModuleScope(JSContext *cx, JSObject *parentObject, moduleHandle_t *module)
{
  JSObject 	*moduleScope;

  if (!module)
    return NULL;

  if (JS_EnterLocalRootScope(cx) != JS_TRUE)
    return NULL;

  moduleScope = JS_NewObject(cx, &module_scope_class, NULL, parentObject); 
  if (!moduleScope)
    return NULL;

/*
 *  if (JS_DefineProperty(cx, moduleScope, "exports", JSVAL_NULL, NULL, NULL,
 *			JSPROP_ENUMERATE | JSPROP_PERMANENT) != JS_TRUE)
 *    return NULL;
 */

  if (JS_DefineFunction(cx, moduleScope, "require", gpsee_loadModule, 0, 0) == NULL)
    return NULL;

  if (JS_DefineProperty(cx, moduleScope, "moduleName", 
			STRING_TO_JSVAL(JS_NewStringCopyZ(cx, module->cname)), NULL, NULL, 
			JSPROP_READONLY | JSPROP_PERMANENT) != JS_TRUE)
    return NULL;
  
  setModuleHandle_forScope(cx, moduleScope, module);

  JS_LeaveLocalRootScopeWithResult(cx, OBJECT_TO_JSVAL(moduleScope));

  return moduleScope;
}

/** Find existing or create new module handle.
 *
 *  Module handles are used to 
 *  - track shutdown requirements over the lifetime of  the interpreter
 *  - track module objects so they don't get garbage collected
 *  - enforce { parentModulePath / moduleName } singleton-ness.
 *
 *  Note that the module objects in here can't be made GC roots, as the
 *  whole structure is subject reallocation.  Hence gpsee_moduleGCCallback().
 *
 *  Any module handle returned from this routine is guaranteed to have a cname, so that it
 *  can be identified.
 *
 *  @param cx			JavaScript context
 *  @param parentObject		Object to use to find parentModuleScope when creating handle
 *  @param moduleName		Name of the module we're interested in; may contain slashses, relative ..s etc
 *  @returns 			A module handle, possibly one already initialized.
 *
 *  @note	A special mode of this function allows it find an empty slot if moduleName is NULL. This mode
 *		is for loading program modules; program module loaders are expected to provide module->cname
 *		themselves.
 */
static moduleHandle_t *acquireModuleHandle(JSContext *cx, const char *cname, JSObject *parentObject)
{
  moduleHandle_t	*module, **moduleSlot = NULL;
  gpsee_interpreter_t 	*jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  size_t		i;

  /* Scan for free slots and/or cname repetition (modules are singletons) */
  for (i=0; i < jsi->modules_len; i++)
  {
    module = jsi->modules[i];

    if (module && cname && module->cname && (strcmp(module->cname, cname) == 0))
      return module;	/* seen this one already */

    if (!moduleSlot && !module)
      moduleSlot = jsi->modules + i; /* don't break, singleton check still! */
  }
  
  if (!moduleSlot)	/* No free slots, allocate some more */
  {
    size_t incr;

#ifdef GPSEE_DEBUG_BUILD
    incr = 1;
#else
    incr = max(max(modules_len / 2, 64), 4);
#endif

    jsi->modules = realloc(jsi->modules, sizeof(jsi->modules[0]) * (jsi->modules_len + incr));
    if (!jsi->modules)
      panic(GPSEE_GLOBAL_NAMESPACE_NAME ".modules.acquireHandle: Out of memory in " __FILE__);

    moduleSlot = jsi->modules + jsi->modules_len;

    jsi->modules_len += incr;
  }

  *moduleSlot = calloc(1, sizeof(**moduleSlot));
  module = *moduleSlot;
  module->slot = moduleSlot - jsi->modules;

  dprintf("allocated new module slot for %s in slot %i at 0x%p\n", 
	  cname ? moduleShortName(cname) : "program module", moduleSlot - jsi->modules, module);

  if (cname)
  {
    module->cname = strdup(cname);
  }
  {
    module->scope = newModuleScope(cx, parentObject, module);
    module->object = acquireModuleObject(cx, module->scope);

    if (!module->scope || !module->object)
    {
      markModuleUnused(cx, module);
      return NULL;
    }

    if (!module->object)
    {
      markModuleUnused(cx, module);
      return NULL;
    }
  }

  return module;
}

/** Marking a module unused makes it eligible for immediate garbage 
 *  collection. Safe to call from a finalizer.
 */
static void markModuleUnused(JSContext *cx, moduleHandle_t *module)
{
  dprintf("Marking module at 0x%p as unused\n", module);

  if (module->flags & ~mhf_loaded && module->object && module->fini)
    module->fini(cx, module->object);

  module->scope  = NULL;
  module->scrobj = NULL;
  module->object = NULL;
  module->fini	 = NULL;

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
static moduleHandle_t *finalizeModuleHandle(moduleHandle_t *module)
{
  moduleHandle_t *next = module->next;

  dprintf("Finalizing module handle at 0x%p\n", module);
  if (module->cname)
    free((char *)module->cname);

  if (module->idStorage)
    free(module->idStorage);

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

static void finalizeModuleObject(JSContext *cx, JSObject *moduleObject)
{
  gpsee_interpreter_t 	*jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  size_t		i;

  dprintf("begin finalizing module exports\n");

  /** Module exports are finalized by "forgetting" them */
  for (i=0; i < jsi->modules_len; i++)
  {
    if (!jsi->modules[i])
      continue;

    if (jsi->modules[i] && jsi->modules[i]->object == moduleObject)
    {
      jsi->modules[i]->object = NULL;
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
 *   - Exports object is in fact module->object
 *
 *   @param	cx		JS Context
 *   @param	module		Pre-populated moduleHandle
 *   @param	filename	Fully-qualified path to the file to load
 *
 *   @returns	NULL if the module was found,
 *		moduleNotFoundErrorMessage if the module was not found,
 *		or a descriptive error string if the module was found but could not load.
 */
static const char *loadJSModule(JSContext *cx, moduleHandle_t *module, const char *filename)
{
  module->script = JS_CompileFile(cx, module->scope, filename);
  if (!module->script)
    return "unable to compile module";

  module->scrobj = JS_NewScriptObject(cx, module->script);
  if (!module->scrobj)
    return "unable to create module's script object";

  return NULL; /* no error = success*/
}

/**
 *   Load a DSO module. These modules are linked in at run-time, so we compute the 
 *   addresses of the symbols with dlsym().
 *
 *   @param	cx			JS Context
 *   @returns	NULL if the module was found,
 *		moduleNotFoundErrorMessage if the module was not found,
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

static moduleHandle_t *loadDiskModule(JSContext *cx, moduleHandle_t *parentModule, const char *moduleName, const char **errorMessage_p)
{ 
  gpsee_interpreter_t 	*jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  char			pmBuf[PATH_MAX];
  char			fnBuf[PATH_MAX];
  char			cnBuf[PATH_MAX];
  const	char 		*libexec_dir = gpsee_libexecDir();
  const char		*modulePath[] = { NULL, libexec_dir };
  moduleHandle_t	*module = NULL;
  int			retval, i;
  const char		*cname;

  if (moduleName[0] == '/')
  {
    *errorMessage_p = "rooted modules not supported";
    goto fail;
  }

  if (isRelativePath(moduleName))
    modulePath[0] = parentModule ? gpsee_dirname(parentModule->cname, pmBuf, sizeof(pmBuf)) : NULL;
  else
    modulePath[0] = jsi->programModule_dir;

  dprintf("First module path entry is %s\n", modulePath[0]);

  for (i = 0; i < (sizeof(modulePath) / sizeof(modulePath[0])); i++)
  {
    if (!modulePath[i] || !modulePath[i][0])
      continue;

    cname = generateCanonicalName(modulePath[i], moduleName, cnBuf, sizeof(cnBuf));
    if (!cname)
      continue;

    if (!checkJail(cname, libexec_dir) && !checkJail(cname, jsi->moduleJail))
    {
      *errorMessage_p = "attempted jail-break";
      goto fail;
    }

    /* Try native and native+script */
    retval = snprintf(fnBuf, sizeof(fnBuf), "%s_module." DSO_EXTENSION, cname);
    if (retval <= 0 || retval >= sizeof(fnBuf))
    {
      *errorMessage_p = "path buffer overrun";
      goto fail;
    }

    if (access(fnBuf, F_OK) == 0)
    {
      module = acquireModuleHandle(cx, cname, parentModule ? parentModule->scope : NULL);
      if (module->flags & mhf_loaded)
	return module;
	
      *errorMessage_p = loadDSOModule(cx, module, fnBuf);
      if (*errorMessage_p)
	goto fail;

      /* Native loaded, try monkey-patch */
      retval = snprintf(fnBuf, sizeof(fnBuf), "%s_module.js", cname);
      if (retval <= 0 || retval >= sizeof(fnBuf))
      {
	*errorMessage_p = "path buffer overrun";
	goto fail;
      }

      if (access(fnBuf, F_OK) == 0)
      {
	if ((*errorMessage_p = loadJSModule(cx, module, fnBuf)))
	  goto fail;
      }

      return module;
    }

    /* Try plain script module */
    retval = snprintf(fnBuf, sizeof(fnBuf), "%s.js", cname);
    if (retval <= 0 || retval >= sizeof(fnBuf))
    {
      *errorMessage_p = "path buffer overrun";
      goto fail;
    }
    if (access(fnBuf, F_OK) == 0)
    {
      module = acquireModuleHandle(cx, cname, parentModule ? parentModule->scope : NULL);
      module->cname = strdup(cname);
      *errorMessage_p = loadJSModule(cx, module, fnBuf);
      if (*errorMessage_p)
	goto fail;

      return module;
    }
  }

  *errorMessage_p = moduleNotFoundErrorMessage;

  fail:
  if (module)
    markModuleUnused(cx, module);

  return NULL;
}

/**
 *   Load an internal module. These modules are linked in at compile-time, so we can
 *   compute the addresses of the symbols with some precompiler magic.
 *
 *   Note that internal modules do NOT take pathing into account
 *
 *   @param	cx	JS Context
 *   @param	module	pre-populated moduleHandle
 *   @returns	NULL if the module was found,
 *		moduleNotFoundErrorMessage if the module was not found,
 *		or a descriptive error string if the module was found but could not load.
 */
static moduleHandle_t *loadInternalModule(JSContext *cx, moduleHandle_t *parentModule, const char *moduleName, const char **errorMessage_p)
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

  if (strchr(moduleName, '/'))
  {
    *errorMessage_p = moduleNotFoundErrorMessage;
    return NULL;
  }

  module = acquireModuleHandle(cx, moduleName, parentModule ? parentModule->scope : NULL);
  if (module->flags & mhf_loaded)
  {
    dprintf("no reload internal singleton %s\n", moduleShortName(moduleName));
    return module;
  }

  for (i=0; i < (sizeof internalModules/sizeof internalModules[0]); i++)
  {
    if (strcmp(internalModules[i].name , moduleName) == 0)
    {
      module->init = internalModules[i].init;
      module->fini = internalModules[i].fini;
      dprintf("module %s is internal\n", moduleName);
      return module;
    }
  }

  markModuleUnused(cx, module);
  dprintf("module %s is not internal\n", moduleName);
  *errorMessage_p = moduleNotFoundErrorMessage;
  return NULL;
}

/** Run module initialization routines and add moduleID property to module scope.
 * 
 *  @returns module ptr on success, NULL on failure. Some failures may be the result of JavaScript exceptions.
 * 
 *  @note Returns module handle for a reason! This code can reallocate the backing module array. 
 *        Module on the way in may be at a different address on the way out.
 */
static moduleHandle_t *initializeModule(JSContext *cx, moduleHandle_t *module)
{
  JSObject	*moduleScope = module->scope;

  if (module->init)
  {
    const char *id;

    id = module->init(cx, module->object); /* realloc hazard */
    module = getModuleHandle_fromScope(cx, moduleScope);
    GPSEE_ASSERT(module != NULL);
    module->id = id;
  }

  if (module->script)
  {
    jsval 	v = OBJECT_TO_JSVAL(module->object);
    JSBool	b;

    if (JS_SetProperty(cx, module->scope, "exports", &v) != JS_TRUE)
      return NULL;

    b = JS_ExecuteScript(cx, module->scope, module->script, &v); /* realloc hazard */
    module = getModuleHandle_fromScope(cx, moduleScope);
    GPSEE_ASSERT(module != NULL);

    module->script = NULL;	/* No longer needed */
    module->scrobj = NULL;	/* No longer needed */

    if (b != JS_TRUE)	/* uncaught exception */
      return NULL;

    if (!module->id) /* Don't override id if JS is ~ DSO monkey-patch */
    {
      if (JS_GetProperty(cx, module->scope, "moduleID", &v) == JS_TRUE)		
      {
        /* JS module specified global ID, use that. */
	module->idStorage = strdup(JS_GetStringBytes(JS_ValueToString(cx, v) ?: JS_InternString(cx, "")));
      }
      else					
      {
	JSObject *parentScope = findModuleScope(cx, JS_GetParent(cx, module->scope));

	if (parentScope)
	{
	  /* By definition, parent module has valid moduleID, tack basename(cname) on to that */

	  const char *bname = gpsee_basename(module->cname);
	  const char *parentID = getModuleHandle_fromScope(cx, parentScope)->id;

	  module->idStorage = malloc(strlen(parentID) + 1 + strlen(bname) + 1);
	  sprintf(module->idStorage, "%s.%s",  parentID, bname);
	}
	else
	{
	  /* Last-ditch: calculate module ID based on cname */
	  const char 	*ld = gpsee_libexecDir();
	  size_t	ld_len = strlen(ld);
	  char 		*slash;	
	  const char 	*fmt;
	  const char	*adjcname;
	    
	  if ((strncmp(module->cname, ld, ld_len) == 0) && (module->cname[ld_len] == '/'))
	  {
	    fmt = GPSEE_GLOBAL_NAMESPACE_NAME ".module.libexec.%s";
	    adjcname = module->cname + strlen(ld) + 1;
	  }
	  else
	  {
	    fmt = GPSEE_GLOBAL_NAMESPACE_NAME ".module.abspath.%s";
	    adjcname = module->cname + 1;
	  }

	  module->idStorage = malloc(strlen(fmt) + strlen(adjcname));
	  sprintf(module->idStorage, fmt, adjcname);

	  while ((slash = strchr(module->idStorage, '/')))
	    *slash = '.';
	}
      }

      module->id = module->idStorage;
    }
  }

  return module;
}

/** Implements JavaScript loadModule() (require) function.
 *  First argument is the module name.
 *  Second argument (optional) is the module file's filename.
 *
 *  We try internal, DSO, JS modules in that order.
 */
JSBool gpsee_loadModule(JSContext *cx, JSObject *parentObject, uintN argc, jsval *argv, jsval *rval)
{
  JSObject 		*parentModuleScope = findModuleScope(cx, parentObject);
  moduleHandle_t	*parentModule = parentModuleScope ? getModuleHandle_fromScope(cx, parentModuleScope) : NULL;
  const char		*moduleName;
  const char		*errorMessage = "unknown error";
  moduleHandle_t	*module, *m;

  if ((argc != 1) && (argc != 2))
    return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".loadModule.argument.count");

  moduleName = JS_GetStringBytes(JS_ValueToString(cx, argv[0]) ?: JS_InternString(cx, ""));
  if (!moduleName || !moduleName[0])
    return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".loadModule.invalidName: Module name must be at least one character long");

  dprintf("loading module %s\n", moduleShortName(moduleName));

  module = loadInternalModule(cx, parentModule, moduleName, &errorMessage);
  if (!module)
  {
    if (errorMessage != moduleNotFoundErrorMessage)
      return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".loadModule.internal.fatal: %s, loading module %s", errorMessage, moduleName);

    module = loadDiskModule(cx, parentModule, moduleName, &errorMessage);
    if (!module)
    {
      if (JS_IsExceptionPending(cx))	/* Propogate throw from monkey-patch */
	return JS_FALSE;

      return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".loadModule.disk.fatal: %s, loading module %s", errorMessage, moduleName);
    }
  }

  if (module->flags & mhf_loaded)
  {
    /* modules are singletons */
    *rval = OBJECT_TO_JSVAL(module->object);
    dprintf("no reload singleton %s\n", moduleShortName(moduleName));
    return JS_TRUE;
  }

  module->flags |= mhf_loaded;

  if (!module->init && !module->script)
  {
    markModuleUnused(cx, module);

    if (module->DSOHnd)
      return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".loadModule.init.notFound: "
			 "Module lacks initialization function (%s_InitModule)", moduleName);

    return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".loadModule.init.notFound: "
		       "There is no mechanism to initialize module %s!", moduleName);
  }

  dprintf("Initializing module at 0x%p\n", module);
  if ((m = initializeModule(cx, module)))
    module = m;
  dprintf("Initialized module now at 0x%p\n", m);

  if (!m || !module->id)
  {
    markModuleUnused(cx, module);
    if (JS_IsExceptionPending(cx))
      return JS_FALSE;

    return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".loadModule.init: Unable to initialize module '%s'", moduleName);
  }

  if (JS_TRUE != JS_DefineProperty(cx, module->scope, "moduleID", 
				       STRING_TO_JSVAL(JS_NewStringCopyZ(cx, module->id)), NULL, NULL,
				       JSPROP_READONLY | JSPROP_PERMANENT))
  {
    markModuleUnused(cx, module);
    return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".loadModule.init.moduleID: Unable to assign moduleID %s to module '%s'",
		       module->id, moduleName);
  }

  *rval = OBJECT_TO_JSVAL(module->object);

  return JS_TRUE;
}

/** Run a program as if it were a module. Interface differs from gpsee_loadModule()
 *  to reflect things like that the source of the program module may be stdin rather
 *  than a module file, may not be in the libexec dir, etc.
 *
 *  It is possible to have more than one program module in the module list at once,
 *  provided we can generate reasoanble canonical name for each one.
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
  int			i;
  const char		*errorMessage = NULL;
  gpsee_interpreter_t 	*jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  char			*s;

  module = acquireModuleHandle(cx, NULL, jsi->globalObj);
  if (!module)
  {
    errorMessage = "could not allocate module handle";
    goto fail;
  }

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
  if ((i >= sizeof(cnBuf) - 1) || (i <= 0))
    return "buffer overrun";
  else
    cnBuf[i] = (char)0;

  if ((s = strrchr(cnBuf, '.')))
  {
    if (!strchr(s, '/'))
      *s = (char)0;
  }

  module->cname = strdup(cnBuf);
  if (!module->cname)
    panic("Out of memory");

  /* jsi->programModule_dir = gpsee_dirname(module->cname, cnBuf, sizeof(cnBuf)); */
  jsi->programModule_dir = cnBuf;
  s = strrchr(cnBuf, '/');
  if (!s || (s == cnBuf))
    return "could not determine program module dir";
  else
    *s = (char)0;

  dprintf("Program module root is %s\n", jsi->programModule_dir);
  dprintf("compiling program module %s\n", moduleShortName(module->cname));

  if (scriptFile)
    module->script = JS_CompileFileHandle(cx, module->scope, scriptFilename, scriptFile);  
  else
    module->script = JS_CompileFile(cx, module->scope, scriptFilename);
  if (!module->script)
  {
    errorMessage = "could not compile script";
    goto fail;
  }

  module->scrobj = JS_NewScriptObject(cx, module->script);
  if (!module->scrobj)
  {
    errorMessage = "unable to create module's script object";
    goto fail;
  }

  if (initializeModule(cx, module))
  {
    dprintf("done running program module %s\n", moduleShortName(module->cname));
    goto good;
  }
  else
  {
    errorMessage = "module initialization failed";
  }

  fail:
  dprintf("failed running program module %s\n", module ? moduleShortName(module->cname) : "(null)");

  if (JS_IsExceptionPending(cx) || gpsee_verbosity(0))
    gpsee_log(SLOG_NOTICE, "Failed loading program module '%s': %s", scriptFilename, errorMessage);
  else
    gpsee_log(SLOG_EMERG, "Failed loading program module '%s': %s", scriptFilename, errorMessage);

  if (!errorMessage)
    errorMessage = "unknown failure";

  good:
  jsi->programModule_dir = NULL;
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
      jsi->unreachableModule_llist = finalizeModuleHandle(jsi->unreachableModule_llist);
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
      JS_MarkGCThing(cx, module->object, module->id, NULL);

    /* Modules which have exports should not have their scopes collected */
    if (module->scope && (module->object || !(module->flags & mhf_loaded)))
      JS_MarkGCThing(cx, module->scope, module->id, NULL);

    /* Scrobj is a temporary place to root script modules between 
     * compilation and execution.
     */
    if (module->scrobj)
      JS_MarkGCThing(cx, module->scrobj, module->id, NULL);
  }

  return JS_TRUE;
}

void gpsee_initializeModuleSystem(JSContext *cx)
{
  gpsee_interpreter_t 	*jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));

  jsi->moduleJail = rc_value(rc, "gpsee_module_jail");

  dprintf("Initializing module system; jail starts at %s\n", jsi->moduleJail ?: "/");

  JS_SetGCCallback(cx, moduleGCCallback);
}

void gpsee_shutdownModuleSystem(JSContext *cx)
{
  gpsee_interpreter_t 	*jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  size_t		i;

  dprintf("Shutting down module system\n");

  for (i=0; i < jsi->modules_len; i++)
  {  
    moduleHandle_t *module;

    if ((module = jsi->modules[i]) == NULL)
      continue;

    dprintf("Fini'ing module at 0x%p\n", module);

    if (module->fini)
    {
      module->fini(cx, module->object);
      module->fini = NULL;
    }

    markModuleUnused(cx, module);
  }
}

/** Retrieve a module object based on module ID. 
 *  Generally speaking, needing this means you've made a design error
 *  in your native module. Provided to ease importing of foreign
 *  natives which think calling JS_GetGlobalObject() is a good idea.
 *  (e.g. moz jsfile.c)
 */
JSObject *gpsee_findModuleObject_byID(JSContext *cx, const char *moduleID)
{
  gpsee_interpreter_t 	*jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  size_t		i;

  for (i=0; i < jsi->modules_len; i++)
  {  
    moduleHandle_t *module;

    if ((module = jsi->modules[i]) == NULL)
      continue;

    if (module->id && strcmp(module->id, moduleID) == 0)
      return module->object;
  }

  return NULL;
}
