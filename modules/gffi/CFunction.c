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
 * Copyright (c) 2009 PageMail, Inc. All Rights Reserved.
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
 *  @file	CFunction.c	Support code for GPSEE's gffi module which
 *				exposes C functions to JavaScript.
 *
 *  @author	Wes Garland
 *              PageMail, Inc.
 *		wes@page.ca
 *  @date	Jun 2009
 *  @version	$Id: CFunction.c,v 1.14 2010/04/14 00:38:31 wes Exp $
 */

#include <gpsee.h>
#include <ffi.h>
#include "gffi.h"

#define CLASS_ID MODULE_ID ".CFunction"

static JSBool cFunction_jsapiCall_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  cFunction_handle_t *hnd = JS_GetInstancePrivate(cx, obj, cFunction_clasp, NULL);
  if (!hnd)
    return JS_FALSE;

  *vp = hnd->noSuspend ? JSVAL_TRUE : JSVAL_FALSE;

  return JS_TRUE;
}

static JSBool cFunction_jsapiCall_setter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  cFunction_handle_t *hnd = JS_GetInstancePrivate(cx, obj, cFunction_clasp, NULL);

  if (!hnd)
    return JS_FALSE;

  if (*vp == JSVAL_TRUE)
    hnd->noSuspend = 1;
  else if (*vp == JSVAL_FALSE)
    hnd->noSuspend = 0;
  else
    return gpsee_throw(cx, CLASS_ID ".jsapiCall: assignment of non-boolean value");

  return JS_TRUE;
}

/* cFunction_closure_free() frees a cFunction_closure_t instance and any members that might have been allocated during
 * a call to cFunction_prepare(). The first member of that struct, a pointer to a cFunction_handle_t, is not freed.
 *
 * @param     cx
 * @param     clos      Pointer to that which is to be freed.
 */
void cFunction_closure_free(JSContext *cx, cFunction_closure_t *clos)
{
  if (clos->rvaluep)
    JS_free(cx, clos->rvaluep);
  if (clos->avalues)
    JS_free(cx, clos->avalues);
  if (clos->storage)
  {
    int i, l;
    for (i=0, l=clos->hnd->nargs; i<l; i++)
      if (clos->storage[i])
        JS_free(cx, clos->storage[i]);
    JS_free(cx, clos->storage);
  }
  JS_free(cx, clos);
}

/* cFunction_prepare() prepares a cFunction_closure_t, an intermediate stage between evaluating a CFunction's argument
 * vector and making the call.
 *
 * @param     cx
 * @param     obj         CFunction instance to be prepared for invocation
 * @param     argc        Length of argv
 * @param     argv        Argument vector to be prepared for invocation
 * @param     clospp      Outvar cFunction_closure_t
 * @param     throwPrefix
 *
 * @returns   JS_TRUE on success
 */
JSBool cFunction_prepare(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, cFunction_closure_t **clospp,
                                const char *throwPrefix)
{
  cFunction_handle_t    *hnd;
  cFunction_closure_t   *clos;
  size_t                i;

  /* Fetch our CFunction private data struct */
  hnd = JS_GetInstancePrivate(cx, obj, cFunction_clasp, NULL);
  if (!hnd)
    return gpsee_throw(cx, "%s.invalid: unable to locate function call details", throwPrefix);

  /* Verify arg count */
  if (hnd->nargs != argc)
    return gpsee_throw(cx, "%s.arguments.count: Expected " GPSEE_SIZET_FMT " arguments for %s, received %i", 
                       throwPrefix, hnd->nargs, hnd->functionName, argc);

  /* Allocate the struct representing a prepared FFI call */
  clos = JS_malloc(cx, sizeof(cFunction_closure_t));
  if (!clos)
    return JS_FALSE;
  memset(clos, 0, sizeof(*clos));
  clos->hnd = hnd;

  if (hnd->nargs)
  {
    clos->avalues = JS_malloc(cx, sizeof(clos->avalues[0]) * hnd->nargs);
    if (!clos->avalues)
      goto fail;

    clos->storage = JS_malloc(cx, sizeof(clos->storage[0]) * hnd->nargs);
    if (!clos->storage)
      goto fail;

    memset(clos->storage, 0, sizeof(clos->storage[0]) * hnd->nargs);
  }

  if (hnd->rtype_abi != &ffi_type_void)
  {
    clos->rvaluep = JS_malloc(cx, ffi_type_size(hnd->rtype_abi));
    if (!clos->rvaluep)
      goto fail;
  }

  /* Convert jsval argv to C ABI values */
  for (i = 0 ; i < hnd->nargs; i++)
  {
    if (hnd->argConverters[i](cx, argv[i], clos->avalues + i, clos->storage + i, i + 1, throwPrefix) == JS_FALSE)
      goto fail;
  }

  /* Return the prepared FFI call */
  *clospp = clos;
  return JS_TRUE;

fail:
  cFunction_closure_free(cx, clos);
  return JS_FALSE;
}

/* @jazzdoc gffi.CFunction.prototype.call()
 * Invokes a foreign function. Obsolete, can now invoke CFunction objects as functions.
 *
 * @form CFunctionInstance.call(arguments[])
 * The exact form of invocation depends on the instantiation of the CFunction. Please see gffi.CFunction for more
 * information.
 *
 * @returns an instance of gffi.WillFinalize
 */
static JSBool cFunction_call(JSContext *cx, uintN argc, jsval *vp)
{
  JSBool                ret;
  jsrefcount            depth = 0;
  cFunction_closure_t   *clos;
  JSObject              *obj = JS_THIS_OBJECT(cx, vp);
  jsval                 *argv = JS_ARGV(cx, vp);

  /* Prepare the FFI call */
  if (!cFunction_prepare(cx, obj, argc, argv, &clos, CLASS_ID ".call"))
    return JS_FALSE;

  /* Make the call */
  if (!clos->hnd->noSuspend)
    depth = JS_SuspendRequest(cx);
  ffi_call(clos->hnd->cif, clos->hnd->fn, clos->rvaluep, clos->avalues);
  if (!clos->hnd->noSuspend)
    JS_ResumeRequest(cx, depth);
  ret = ffiType_toValue(cx, clos->rvaluep, clos->hnd->rtype_abi, &JS_RVAL(cx, vp), CLASS_ID ".call");

  /* Clean up */
  cFunction_closure_free(cx, clos);

  return ret;
}

/**
 *  JSNative for use by JSClass.call to replace F.call() with F()
 */
static JSBool CFunction_call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  JSBool                ret;
  jsrefcount            depth = 0;
  cFunction_closure_t   *clos;
  
  obj = JSVAL_TO_OBJECT(JS_ARGV_CALLEE(argv));

  /* Prepare the FFI call */
  if (!cFunction_prepare(cx, obj, argc, argv, &clos, CLASS_ID ".invoke"))
    return JS_FALSE;

  /* Make the call */
  if (!clos->hnd->noSuspend)
    depth = JS_SuspendRequest(cx);
  ffi_call(clos->hnd->cif, clos->hnd->fn, clos->rvaluep, clos->avalues);
  if (!clos->hnd->noSuspend)
    JS_ResumeRequest(cx, depth);
  ret = ffiType_toValue(cx, clos->rvaluep, clos->hnd->rtype_abi, rval, CLASS_ID ".invoke");

  /* Clean up */
  cFunction_closure_free(cx, clos);

  if (ret == JS_FALSE)
    goto out;

  /* Box if necessary so we can finalize */
  switch(*rval)
  {
    case JSVAL_NULL:
    case JSVAL_VOID:
      goto out;
  }

  /* Box primitive values so that they can be garbage collected, and support a finalizeWith() instance method */

  /*  - make a new object
   *  - add valueOf method which returns what rval is right now  (ffiType_toValue result)
   *  - add finalizeWith method which sets up finalizer
   *    - finalizeWith can use JSClass call op if we need to preserve private slot, other wise
   *      it can be a OBJECT_TO_JSVAL(JSFunction *)
   *  - add destroy method which calls then removes finalizer    
   *  - set rval to new object
   */

  out:
  return ret;
}

/* cFunction_closure_call() calls and frees a prepared CFunction call represented by a cFunction_closure_t instance
 * created by a call to cFunction_prepare(), which in turn is intended to be called by 
 *
 * @param     cx
 * @param     clos    CFunction closure to call
 */
void cFunction_closure_call(JSContext *cx, cFunction_closure_t *clos)
{
  /* Make the call */
  ffi_call(clos->hnd->cif, clos->hnd->fn, clos->rvaluep, clos->avalues);
  /* Clean up */
  cFunction_closure_free(cx, clos);
}

/** 
 *  Implements the CFunction constructor.
 *
 *  Constructor is variadic,
 *  argv[0] = return type
 *  argv[1] = C function name
 *  argv[2...] = Argument types
 *
 *  @param	cx	JavaScript context
 *  @param	obj	Pre-allocated CFunction object
 *  @param	argc	Number of arguments passed to constructor
 *  @param	argv	Arguments passed to constructor
 *  @param	rval	The new object returned to JavaScript
 *
 *  @returns 	JS_TRUE on success
 */
static JSBool CFunction(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  cFunction_handle_t	*hnd;
  ffi_status		status;
  library_handle_t      *libhnd = NULL;
  JSObject              *libobj;

  if (JS_IsConstructing(cx) != JS_TRUE)
    return gpsee_throw(cx, CLASS_ID ".constructor.notFunction: Must call constructor with 'new'!");

  /* @jazzdoc gffi.CFunction
   *
   * CFunctions represent a C ABI function that has been linked to Javascript with the GFFI module. There are two ways
   * of instantiating them, each way affecting symbol resolution semantics differently.
   *
   * @form new gffi.CFunction(rtype, symbol, argtypes[])
   * Instantiating a new CFunction from the generic constructor gffi.CFunction provides an interface to a function
   * looked up by the dynamic linker without looking in a specific library. As some APIs can be particularly difficult
   * to resolve at runtime with a mercurial dynamic linker (particularly problematic is accessing syscalls under Linux)
   * there is a compile-time mechanism for building trampolines to those APIs. To add or modify the list of trampolines
   * available, recompile after making the necessary changes to:
   *
   * gpsee/modules/gffi/function_aliases.incl
   *
   * The format for entries in this file comes in two forms:
   *
   * function(<return_type>,<function_name>,(<argument_list_types_and_names>),(<argument_list_names_only>))
   * voidfunction(<function_name>,(<argument_list_types_and_names>),(<argument_list_names_only>))
   *
   * The redundancy of this form is to accommodate simple implementation in the C preprocessor. To serve as an example,
   * here are stat(2) and fstat(2) declared as ordinary C prototypes, and again as GFFI function aliases:
   *
   * stat(2)
   *    C:    int stat(const char *path, struct stat *buf);
   *    GFFI: function(int, stat, (const char *path, struct stat *buf), (path, buf))
   *
   * fstat(2)
   *    C:    int fstat(int fd, struct stat *buf);
   *    GFFI: function(int, fstat, (int fd, struct stat *buf), (fd, buf))
   *
   * @form new LibraryInstance.CFunction(rtype, symbol, argtypes[])
   * Instantiating a new CFunction from the Library instance method CFunction provies an interface to a function
   * looked up by the dynamic linker in the library associated with the Library instance that CFunction constructor
   * is associated with.
   *
   * Constructor vs. instance method contention:
   *
   * There is semantic contention in the Javascript language for constructor functions and instance methods.
   * The magic variable 'this' cannot refer both to the Library instance from which the constructor was referenced
   * and simultaneously to the CFunction instance which is being constructed. GFFI cleverly uses Spidermonkey's
   * notion of a "parent" to overcome this limitation. The "parent" of the CFunction constructor will either be the
   * Library instance from which it came, or the gffi module itself. For this reason, multiple references to the
   * CFunction constructor are not interchangeable, and you will not find a reference to the CFunction constructor
   * in Library.prototype as you might expect to find since it is a Library instance method.
   */
  if (argc < 2)
    return gpsee_throw(cx, CLASS_ID ".arguments.count: Must have at least a function name and a return value");

  /* Allocate our cfunction_handle_t */
  hnd = JS_malloc(cx, sizeof(*hnd));
  if (!hnd)
    return JS_FALSE;
  /* All clean-up beyond this point is solely the responsibility of the finalizer, including deallocation */
  /* Initialize our cfunction_handle_t */
  memset(hnd, 0, sizeof(*hnd));
  /* Associate our cfunction_handle_t with our JSObject */
  JS_SetPrivate(cx, obj, hnd);
  /* Fetch and store the function name argument */
  hnd->functionName = JS_strdup(cx, JS_GetStringBytes(JS_ValueToString(cx, argv[1])));
  /* The JSAPI "parent" object will probably be a Library instance, which contains our DSO handle from dlopen() */
  libobj = JS_GetParent(cx, obj);
  /* Was our "parent" a Library instance? */
  if (libobj && JS_InstanceOf(cx, libobj, library_clasp, NULL))
  {
    jsval libval;
    /* Fetch our library_handle_t from the Library instance JSObject */
    libhnd = JS_GetPrivate(cx, libobj);
    /* If this constructor is not a property of a Library instance, or if that Library instance is jsut an RTLD_DEFAULT
     * Library, then we should branch to the rltdDefault code path */
    if (!libhnd || libhnd->dlHandle == RTLD_DEFAULT)
      goto rtldDefault;
    /* Fetch the function pointer from the DSO */
    hnd->fn = dlsym(libhnd->dlHandle, hnd->functionName);
    /* Attach our source library so the GC can't free it until we're already gone */
    libval = OBJECT_TO_JSVAL(libobj);
    if (!JS_SetProperty(cx, obj, "library", &libval))
      return gpsee_throw(cx, CLASS_ID ".constructor.internalerror: could not JS_SetProperty() my library!");
    goto dlsymOver;
  }

rtldDefault:
  if (!(hnd->fn = findPreDefFunction(hnd->functionName)))
  {
    /* No Library instance. Try RTLD_DEFAULT. See dlopen(3) for details. */
    hnd->fn = dlsym(RTLD_DEFAULT, hnd->functionName);
    /* To overcome difficult linkage scenarios (so far, this only includes macros and weak symbols) upon
     * dlsym(RTLD_DEFAULT) failure, we try for a gffi alias. */
    if (!hnd->fn)
    {
      /* TODO what is a good value here? */
      char functionName[1024];
      int n;
      /* Mangle the function name (prepend "gffi_alias_") */
      n = snprintf(functionName, sizeof functionName, "gffi_alias_%s", hnd->functionName);
      GPSEE_ASSERT(n >= 0);
      if (n >= sizeof functionName)
        gpsee_log(cx, SLOG_WARNING, "gffi alias for \"%s\" exceeds " GPSEE_SIZET_FMT " characters", hnd->functionName, sizeof functionName);
      else
        /* Try dlsym() again with the new mangled name! */
        hnd->fn = dlsym(RTLD_DEFAULT, functionName);
    }
  }

dlsymOver:
  /* Throw an exception on dlsym() failure */
  if (!hnd->fn)
    return gpsee_throw(cx, CLASS_ID ".constructor.notFound: no function named \"%s\" in %s",
		       hnd->functionName, libhnd?libhnd->name:"RTLD_DEFAULT");

  hnd->nargs		= argc - 2;
  hnd->cif 		= JS_malloc(cx, sizeof(*hnd->cif));
  if (hnd->nargs)
  {
    hnd->argTypes 	= JS_malloc(cx, sizeof(hnd->argTypes[0]) * hnd->nargs);
    hnd->argConverters	= JS_malloc(cx, sizeof(hnd->argConverters[0]) * hnd->nargs);
  }

  /* TODO cleanup */
  if (!hnd->functionName || !hnd->cif || (hnd->nargs && (!hnd->argTypes || !hnd->argConverters)) || !hnd->functionName)
    return JS_FALSE;

  /* Sort out return type */
#define ffi_type(type, junk) if (argv[0] == jsve_ ## type) { hnd->rtype_abi = &ffi_type_ ## type; } else
#include "ffi_types.decl"
#undef ffi_type
  return gpsee_throw(cx, CLASS_ID ".constructor.argument.1: Invalid value specified for return value; should be FFI type indicator");

  /* Sort out argument types */
  if (setupCFunctionArgumentConverters(cx, argv + 2, hnd, CLASS_ID) == JS_FALSE)
    return JS_FALSE;

  status = ffi_prep_cif(hnd->cif, FFI_DEFAULT_ABI, hnd->nargs, hnd->rtype_abi, hnd->argTypes);

  switch(status)
  {
    case FFI_OK:
      break;
    case FFI_BAD_TYPEDEF:
      return gpsee_throw(cx, CLASS_ID ".constructor.prep_cif.typedef: Bad typedef");
    case FFI_BAD_ABI:
      return gpsee_throw(cx, CLASS_ID ".constructor.prep_cif.abi: Bad ABI");
    default:
      return gpsee_throw(cx, CLASS_ID ".constructor.prep_cif.status.%i: Invalid status", (int)status);
  }

  return JS_TRUE;
}

/** 
 *  CFunction Finalizer.
 *
 *  Called by the garbage collector, this routine release all resources
 *  used by an instance of the class when the object is collected: the
 *  buffered stream, the handle's memory, etc. I also closes the
 *  underlying file descriptor.
 *
 *  @param	cx	JavaScript context
 *  @param	obj	The object to finalize
 */
static void CFunction_Finalize(JSContext *cx, JSObject *obj)
{
  cFunction_handle_t	*hnd = JS_GetPrivate(cx, obj);

  if (!hnd)
    return;

  if (hnd->cif)
    JS_free(cx, hnd->cif);

  if (hnd->argTypes)
    JS_free(cx, hnd->argTypes);

  if (hnd->argConverters)
    JS_free(cx, hnd->argConverters);

  if (hnd->functionName)
    JS_free(cx, (void *)hnd->functionName);

  JS_free(cx, hnd);

  return;
}

/* Some interfaces are difficult to dlsym(). Sometimes it's because they're macros, sometimes it's because they're weak
 * symbols and the linker likes to be difficult about it. In any case, we can create trampolines to them here, with a
 * little name mangling, and a little preplanning. */
#define function(rtype, name, argdecl, argv) \
  rtype gffi_alias_ ## name argdecl { return name argv; }
#define voidfunction(name, argdecl, argv) \
  void  gffi_alias_ ## name argdecl { name argv; }
#include "function_aliases.incl"
#undef function

/** Implements CFunction.toString() */
JSBool cFunction_toString(JSContext *cx, uintN argc, jsval *vp)
{
  JSObject *self;
  cFunction_handle_t *hnd;
  JSString *str;
  char buf[256];

  /* Fetch our 'this' JSObject */
  self = JS_THIS_OBJECT(cx, vp);
  if (!self)
    return JS_FALSE;

  /* Fetch our private data */
  hnd = JS_GetInstancePrivate(cx, self, cFunction_clasp, NULL);
  if (!hnd)
    return gpsee_throw(cx, CLASS_ID "prototype.toString applied to non-CFunction object");
    /* TODO handle CFunction prototype object differently? */

  /* Assemble the our string description */
  snprintf(buf, sizeof(buf), "[" CLASS_ID " \"%s\" %s]", hnd->functionName, hnd->noSuspend?"fast":"slow");
  /* Make a JSString from our C string */
  str = JS_NewStringCopyZ(cx, buf);
  if (!str)
    return JS_FALSE;

  /* Return the JSString */
  JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(str));

  /* Success */
  return JS_TRUE;
}

JSClass *cFunction_clasp;

/**
 *  Initialize the CFunction class prototype.
 *
 *  @param	cx	Valid JS Context
 *  @param	obj	The module's exports object
 *
 *  @returns	CFunction.prototype
 */
JSObject *CFunction_InitClass(JSContext *cx, JSObject *obj, JSObject *parentProto)
{
  /** Description of this class: */
  static JSClass cFunction_class =
  {
    GPSEE_CLASS_NAME(CFunction),	/**< its name is CFunction */
    JSCLASS_HAS_PRIVATE,		/**< private slot in use */
    JS_PropertyStub,  			/**< addProperty stub */
    JS_PropertyStub,  			/**< deleteProperty stub */
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub, 			/**< enumerateProperty stub */
    JS_ResolveStub,   			/**< resolveProperty stub */
    JS_ConvertStub,   			/**< convertProperty stub */
    CFunction_Finalize,		/**< it has a custom finalizer */

    JSCLASS_NO_OPTIONAL_MEMBERS
  };

  static JSFunctionSpec instance_methods[] = 
  {
    JS_FN("unboxedCall",  cFunction_call,      0, 0),
    JS_FN("toString",     cFunction_toString,  0, 0),
    JS_FS_END
  };

  static JSPropertySpec instance_props[] = 
  {
    { "jsapiCall", 	0, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, cFunction_jsapiCall_getter, cFunction_jsapiCall_setter },
    { NULL, 0, 0, NULL, NULL }
  };

  JSObject *proto;

  cFunction_class.call = CFunction_call;
  cFunction_clasp = &cFunction_class;

  proto =
      JS_InitClass(cx, 			/* JS context from which to derive runtime information */
		   obj, 		/* Object to use for initializing class (constructor arg?) */
		   parentProto,		/* parent_proto - Prototype object for the class */
 		   cFunction_clasp, 	/* clasp - Class struct to init. Defs class for use by other API funs */
		   CFunction,		/* constructor function - Scope matches obj */
		   0,			/* nargs - Number of arguments for constructor (can be MAXARGS) */
		   instance_props,	/* ps - props struct for parent_proto */
		   instance_methods, 	/* fs - functions struct for parent_proto (normal "this" methods) */
		   NULL,		/* static_ps - props struct for constructor */
		   NULL); 		/* static_fs - funcs struct for constructor (methods like Math.Abs()) */

  GPSEE_ASSERT(proto);

  return proto;
}

static  __attribute__((unused)) void CFunction_debug_dump(JSContext *cx)
{
#define ffi_type(ftype, ctype) gpsee_printf(cx, "FFI type %s is like C type %s and represented by jsve_%s, %i\n", #ftype, #ctype, #ftype, jsve_ ## ftype);
#include "ffi_types.decl"
#undef ffi_type
  return;
}
