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
 * Copyright (c) 2009, PageMail, Inc. All Rights Reserved.
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
 * ***** END LICENSE BLOCK ***** */

/**
 *  @file	functions.c	Support code for GPSEE's gffi module which
 *				exposes C functions to JavaScript.
 *
 *  @author	Wes Garland
 *              PageMail, Inc.
 *		wes@page.ca
 *  @date	Jun 2009
 *  @version	$Id: CFunction.c,v 1.4 2009/07/28 16:43:48 wes Exp $
 */

#include <ffi.h>
#include <gpsee.h>
#include "gffi_module.h"

#define CLASS_ID MODULE_ID ".CFunction"

typedef JSBool (* valueTo_fn)(JSContext *cx, jsval v, void **avaluep, void **storagep, int argn);

typedef struct
{
  const char	*functionName;		/**< Name of the function */
  void		*fn;			/**< Function pointer to call */
  ffi_cif 	*cif;			/**< FFI call details */
  ffi_type	*rtype_abi;		/**< Return type used by FFI / native ABI */
  jsval		rtype_jsv;		/**< Return type requested by user */
  size_t	nargs;			/**< Number of arguments */
  ffi_type	**argTypes;		/**< Argument types used by FFI / native ABI */
  valueTo_fn	*argConverters;		/**< Argument converter */
} cfunction_handle_t;

size_t ffi_type_size(ffi_type *type)
{
#define ffi_type(ftype, ctype) if (type == &ffi_type_ ##ftype) return sizeof(ctype); else
#include "ffi_types.decl"
#undef ffi_type
    panic("corrupted FFI information");
}

/**
 *  By default, we assume that returned pointers are pointers to memory which does not
 *  need to be freed.  If that assumption is wrong the JS programmer can touch up the
 *  object.
 */
static JSBool ffiType_toValue(JSContext *cx, void *abi_rvalp, ffi_type *rtype_abi, jsval *rval, JSObject *thisObj)
{
  cfunction_handle_t *hnd = JS_GetInstancePrivate(cx, thisObj, cFunction_clasp, NULL);

  if (!hnd)
    return JS_FALSE;

  if (hnd->rtype_jsv == jsve_void)
    return JS_TRUE;

  if (hnd->rtype_jsv == jsve_uchar || hnd->rtype_jsv == jsve_schar)
  {
    char 	c = *(char *)abi_rvalp;
    JSString 	*str;

    str = JS_NewStringCopyN(cx, &c, 1);
    if (!str)
      return JS_FALSE;
    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;
  }

  if (rtype_abi == &ffi_type_pointer) 
  {
    void 		*ptr = *(void **)abi_rvalp;
    jsval 		argv[] = { JSVAL_TO_INT(0), JSVAL_FALSE };
    memory_handle_t	*memHnd;
    JSObject		*robj;

    if (!ptr)
    {
      *rval = JSVAL_NULL;
      return JS_TRUE;
    }

    robj = JS_NewObject(cx, memory_clasp, memory_proto, thisObj);
    if (Memory_Constructor(cx, robj, sizeof(argv) / sizeof(argv[0]), argv, rval) == JS_FALSE)
      return JS_FALSE;

    memHnd = JS_GetPrivate(cx, robj);
    GPSEE_ASSERT(memHnd);
    if (!memHnd)
      return gpsee_throw(cx, CLASS_ID ".call: impossible error processing returned Memory object");

    memHnd->buffer = ptr;
    memHnd->memoryOwner = NULL;	/* By default we mark that we don't own the memory, as we don't know how it's managed */

    return JS_TRUE;
  }

#define ffi_type(ftype, ctype) 					\
  if (rtype_abi == &ffi_type_ ##ftype)				\
  {								\
    ctype	native = *(ctype *)abi_rvalp;			\
    jsdouble 	d;						\
    								\
    if (INT_FITS_IN_JSVAL(native) && (jsint)native == native)	\
    {								\
      *rval = INT_TO_JSVAL((jsint)native);			\
      return JS_TRUE;						\
    }								\
    								\
    d = native;							\
    if (native != d)						\
      return gpsee_throw(cx, CLASS_ID ".call.return.overflow");	\
    								\
    return JS_NewNumberValue(cx, d, rval);			\
  }
#define FFI_TYPES_NUMBERS_ONLY
#include "ffi_types.decl"
#undef ffi_type
#undef FFI_TYPES_NUMBERS_ONLY

  return gpsee_throw(cx, CLASS_ID ".call.returnType.unhandled: unhandled return type");
}

static JSBool valueTo_jsint(JSContext *cx, jsval v, jsint *ip, int argn)
{
  jsdouble d;

  if (JSVAL_IS_INT(v))
  {
    *ip = JSVAL_TO_INT(v);
    return JS_TRUE;
  }

  if (JS_ValueToNumber(cx, v, &d) == JS_FALSE)
    return JS_FALSE;

  *ip = d;

  if ((jsint)d != *ip)
    return gpsee_throw(cx, CLASS_ID ".arguments.%i.range: value %g cannot be converted to an integer type", argn, d);

  return JS_TRUE;
}

/*  Arg Converter Notes
 ************************************
 *  *storagep is a pointer to temporary memory which will be freed
 *  *avaluep is a pointer to the thing we want to pass as an argument
 *  IFF *storagep points to the the thing, *avaluep = *storagep
 */

static JSBool valueTo_char(JSContext *cx, jsval v, void **avaluep, void **storagep, int argn) 
{
  jsint i;

  *storagep = JS_malloc(cx, sizeof(char));
  if (!*storagep)
    return JS_FALSE;

  *avaluep = storagep;

  if (!JSVAL_IS_INT(v))
  {
    const char *s;
    JSString		*str = JS_ValueToString(cx, v);

    if (!str && JS_IsExceptionPending(cx))
      return JS_FALSE;

    /* TODO donny says: Is this safe? */
    s = JS_GetStringBytes(str);
    if (s)
    {
      if (s[0] && !s[1])
	i = s[0];
    }
  }
  else
  {
    if (valueTo_jsint(cx, v, &i, argn) == JS_FALSE)
      return JS_FALSE;
  }

  ((char *)*storagep)[0] = i;
  if (i != ((char *)*storagep)[0])
    return gpsee_throw(cx, CLASS_ID ".arguments.%i.valueTo_char.range: %i does not fit in a char", argn, i);

  return JS_TRUE;
}

static JSBool valueTo_schar(JSContext *cx, jsval v, void **avaluep, void **storagep, int argn) 
{
  return valueTo_char(cx, v, avaluep, storagep, argn);
}

static JSBool valueTo_uchar(JSContext *cx, jsval v, void **avaluep, void **storagep, int argn) 
{
  return valueTo_char(cx, v, avaluep, storagep, argn);
}

#define ffi_type(ftype, ctype)\
static JSBool valueTo_ ##ftype(JSContext *cx, jsval v, 				\
			       void **avaluep, void **storagep, int argn)	\
{ 										\
  jsint i;									\
  ctype tgt;									\
    								                \
  if (valueTo_jsint(cx, v, &i, argn) == JS_FALSE)				\
    return JS_FALSE;								\
    								                \
  tgt = i;									\
  if (i != tgt)									\
    return gpsee_throw(cx, CLASS_ID ".arguments.%i.valueTo_" #ftype ".range"	\
		       ": %i does not fit in a%s " #ctype, argn, i, 		\
		       (#ctype[0] == 'u' ? "n" : ""));				\
    								                \
  *storagep = JS_malloc(cx, sizeof(tgt));					\
  if (!*storagep)								\
    return JS_FALSE;								\
    								                \
  *avaluep = *storagep;								\
  *(ctype *)*storagep = tgt;							\
    								                \
  return JS_TRUE; 								\
}

#define FFI_TYPES_INTEGERS_ONLY
#include "ffi_types.decl"
#undef FFI_TYPES_INTEGERS_ONLY
#undef ffi_type

static JSBool valueTo_void(JSContext *cx, jsval v, void **avaluep, void **storagep, int argn) 
{ 
  return gpsee_throw(cx, CLASS_ID ".call.valueTo_void: cannot convert argument %i to void", argn);
}

static JSBool valueTo_float(JSContext *cx, jsval v, void **avaluep, void **storagep, int argn) 
{ 
  jsdouble d;

  if (JS_ValueToNumber(cx, v, &d) == JS_FALSE)
    return JS_FALSE;

  *storagep = JS_malloc(cx, sizeof(float));
  if (!*storagep)
  {
    JS_ReportOutOfMemory(cx);
    return JS_FALSE;
  }

  *avaluep = *storagep;
  *(float *)*storagep = (float)d;

  return JS_TRUE; 
}

static JSBool valueTo_double(JSContext *cx, jsval v, void **avaluep, void **storagep, int argn) 
{ 
  *storagep = JS_malloc(cx, sizeof(jsdouble));

  if (!*storagep)
    return JS_FALSE;

  *avaluep = *storagep;

  return JS_ValueToNumber(cx, v, (jsdouble *)*storagep);
}

static JSBool valueTo_pointer(JSContext *cx, jsval v, void **avaluep, void **storagep, int argn) 
{ 
  JSString 		*str;

  if (JSVAL_IS_STRING(v))
  {
    str = JSVAL_TO_STRING(v);
    goto stringPointer;
  }

  if (JSVAL_IS_OBJECT(v))
  {
    memory_handle_t	*hnd;

    JSObject *obj = JSVAL_TO_OBJECT(v);

    hnd = gpsee_getInstancePrivate(cx, obj, memory_clasp, mutableStruct_clasp, immutableStruct_clasp);
    if (hnd)
    {
      *avaluep = &hnd->buffer;
      return JS_TRUE;
    }
  }

  if (JSVAL_IS_VOID(v))
    return gpsee_throw(cx, CLASS_ID ".call.argument.%i.invalid: cannot convert a void argument to a pointer", argn);

  if (v == JSVAL_TRUE || v == JSVAL_FALSE)
    return gpsee_throw(cx, CLASS_ID ".call.argument.%i.invalid: cannot convert a bool argument to a pointer", argn);

  if (JSVAL_IS_NUMBER(v))
    return gpsee_throw(cx, CLASS_ID ".call.argument.%i.invalid: cannot convert a numeric argument to a pointer", argn);

  if (JSVAL_IS_NULL(v))
  {
    *storagep = NULL;
    *avaluep = storagep;
    return JS_TRUE;
  }

  str = JS_ValueToString(cx , v);
  if (!str)
    return JS_FALSE;
  v = STRING_TO_JSVAL(str);

  stringPointer:
  *storagep = JS_strdup(cx, JS_GetStringBytes(str));
  if (!*storagep)
    return JS_FALSE;

  *avaluep = storagep;

  return JS_TRUE; 
}

static JSBool valueTo_longdouble(JSContext *cx, jsval v, void **avaluep, void **storagep, int argn) 
{ 
  jsdouble d;

  *storagep = JS_malloc(cx, sizeof(long double));

  if (!*storagep)
    return JS_FALSE;

  if (JS_ValueToNumber(cx, v, &d) == JS_FALSE)
    return JS_FALSE;

  *(long double *)storagep = d;
  *avaluep = storagep;

  return JS_TRUE; 
}

JSBool cFunction_call(JSContext *cx, uintN argc, jsval *vp)
{
  cfunction_handle_t 	*hnd;
  void 			*rvaluep = NULL;
  void 			**avalues = NULL;
  void			**storage = NULL;
  size_t		i;
  JSBool		ret = JS_TRUE;
  JSObject		*obj = JS_THIS_OBJECT(cx, vp);
  jsval			*argv = JS_ARGV(cx, vp);
  jsrefcount		depth;

  if (!obj)
    goto oom;

  hnd = JS_GetInstancePrivate(cx, obj, cFunction_clasp, NULL);
  if (!hnd)
    return gpsee_throw(cx, CLASS_ID ".call.invalid: unable to locate function call details");

  if (hnd->nargs != argc)
    return gpsee_throw(cx, CLASS_ID ".call.arguments.count: Expected %i arguments, received %i", hnd->nargs, argc);

  if (hnd->rtype_abi != &ffi_type_void)
  {
    rvaluep = JS_malloc(cx, ffi_type_size(hnd->rtype_abi));
    if (!rvaluep)
      goto oom;
  }

  avalues = JS_malloc(cx, sizeof(avalues[0]) * hnd->nargs);
  if (!avalues)
    goto oom;

  storage = JS_malloc(cx, sizeof(storage[0]) * hnd->nargs);
  if (!storage)
    goto oom;

  memset(storage, 0, sizeof(storage[0]) * hnd->nargs);

  for (i = 0 ; i < hnd->nargs; i++)
  {
    if (hnd->argConverters[i](cx, argv[i], avalues + i, storage + i, i + 1) == JS_FALSE)
      goto throwout;
  }

  depth = JS_SuspendRequest(cx);
  ffi_call(hnd->cif, hnd->fn, rvaluep, avalues);
  JS_ResumeRequest(cx, depth);
  ret = ffiType_toValue(cx, rvaluep, hnd->rtype_abi, &JS_RVAL(cx, vp), obj);
  goto out;

  oom:
  JS_ReportOutOfMemory(cx);

  throwout:
  ret = JS_FALSE;

  out:
  if (rvaluep)
    JS_free(cx, rvaluep);

  if (avalues)
    JS_free(cx, avalues);

  if (storage)
  {
    for (i=0; i < hnd->nargs; i++)
      if (*(storage + i))
	JS_free(cx, *(storage + i));
    JS_free(cx, storage);
  }

  return ret;
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
JSBool CFunction(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  cfunction_handle_t	*hnd;
  ffi_status		status;
  int			i;
  library_handle_t      *libhnd = NULL;
  JSObject              *libobj;

  if (JS_IsConstructing(cx) != JS_TRUE)
    return gpsee_throw(cx, CLASS_ID ".constructor.notFunction: Must call constructor with 'new'!");

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
    if (!libhnd)
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
    /* No Library instance. Try RTLD_DEFAULT. See dlopen(3) for details. */
    hnd->fn = dlsym(RTLD_DEFAULT, hnd->functionName);

dlsymOver:
  /* Throw an exception on dlsym() failure */
  if (!hnd->fn)
    return gpsee_throw(cx, CLASS_ID ".constructor.%s.notFound: no such function in %s",
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
  for (i=0; i < hnd->nargs;  i++)
  {
#define ffi_type(type, junk) 			\
    if (argv[2 + i] == jsve_ ## type) 		\
    {						\
      hnd->argTypes[i] = &ffi_type_ ## type; 	\
      hnd->argConverters[i] = valueTo_ ## type;	\
      continue; 				\
    }
#include "ffi_types.decl"
#undef ffi_type
    return gpsee_throw(cx, CLASS_ID ".constructor.argument.%i: Invalid value specified: should be FFI type indicator", 3 + i);
  }

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

  if (JS_DefineFunction(cx, obj, "call", (JSNative)cFunction_call, 0, JSPROP_PERMANENT | JSPROP_READONLY | JSFUN_FAST_NATIVE) == NULL)
    return gpsee_throw(cx, CLASS_ID ".constructor.call.add: unable to create call method");

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
  cfunction_handle_t	*hnd = JS_GetPrivate(cx, obj);

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
    JS_FS_END
  };

  static JSPropertySpec instance_props[] = 
  {
    { NULL, 0, 0, NULL, NULL }
  };

  JSObject *proto;
  
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

static  __attribute__((unused)) void CFunction_debug_dump()
{
#define ffi_type(ftype, ctype) printf("FFI type %s is like C type %s and represented by jsve_%s, %i\n", #ftype, #ctype, #ftype, jsve_ ## ftype);
#include "ffi_types.decl"
#undef ffi_type
  return;
}
