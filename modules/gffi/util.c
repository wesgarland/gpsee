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
 *  @file	util.c		General support code for GPSEE's gffi module
 *
 *  @author	Wes Garland
 *              PageMail, Inc.
 *		wes@page.ca
 *  @date	Sep 2009
 *  @version	$Id: util.c,v 1.3 2010/03/26 00:19:33 wes Exp $
 */

#include <gpsee.h>
#include "gffi.h"

/** Return a generic pointer back to JS for use in comparisons.
 *  Pointers in JavaScript are just hexadecimal numbers encoded with String.  
 *  Can't do numbers / valueof because there are 2^64 pointers in 64-bit 
 *  land and only 2^53 integer doubles.
 *
 *  @param	cx	JavaScript context to use
 *  @param 	vp	Returned pointer for JS consumption
 *  @param	pointer	Pointer to encode
 *  @returns	JS_FALSE if an exception has been thrown
 */
JSBool pointer_toString(JSContext *cx, void *pointer, jsval *vp)
{
  JSString		*str;
  char			ptrbuf[1 + 2 + (sizeof(void *) * 2) + 1];	/* @ 0x hex_number NUL */

  snprintf(ptrbuf, sizeof(ptrbuf), "@" GPSEE_PTR_FMT, pointer);

  str = JS_NewStringCopyZ(cx, ptrbuf);
  if (!str)
    return JS_FALSE;

  *vp = STRING_TO_JSVAL(str);

  return JS_TRUE;
}

/** Decode a JS pointer string back to a generic pointer. Intended for
 *  use in comparison operations and nothing more.
 *
 *  @param	cx		JavaScript context to use
 *  @param 	v		Encoded pointer, must come from pointer_toString()
 *  @param	pointer_p	Memory in which to return the decoded pointer
 *  @returns	JS_FALSE if an exception has been thrown
 */
JSBool pointer_fromString(JSContext *cx, jsval v, void **pointer_p, const char *throwLabel)
{
  char			*str;

  str = JS_GetStringBytes(JS_ValueToString(cx, v));
  if ((sscanf(str, "@" GPSEE_PTR_FMT, pointer_p) != 1))
    return gpsee_throw(cx, "%s.pointer.invalid", throwLabel);

  return JS_TRUE;
}

/** Return the number of bytes required to store a particular ffi_type.
 *
 *  @param	ffi_type     	The type
 *  @returns the number of bytes, or panic
 */
size_t ffi_type_size(ffi_type *type)
{
#define ffi_type(ftype, ctype) if (type == &ffi_type_ ##ftype) return sizeof(ctype); else
#define FFI_TYPES_SIZED_ONLY
#include "ffi_types.decl"
#undef  FFI_TYPES_SIZED_ONLY
#undef ffi_type
    panic("corrupted FFI information");
}

/** Get the number of bytes require to store the indicated type.
 * 
 *  @param	tival		A type indicator
 *  @param	size		[out] the size
 *  @param	throwPrefix	Prefix for any exceptions thrown
 *
 *  @returns	JS_TRUE on success, JS_FALSE if throws
 */
JSBool ctypeStorageSize(JSContext *cx, jsval tival, size_t *size, const char *throwPrefix)
{
  switch(tival)
  {
#define FFI_TYPES_SIZED_ONLY
#define ffi_type(ftype, ctype) case jsve_ ## ftype: *size = sizeof(ctype); break; 
#include "ffi_types.decl"
#undef ffi_type
#undef FFI_TYPES_SIZED_ONLY
    default:
      return gpsee_throw(cx, "%s.typeStorageSize.typeIndicator.invalid", throwPrefix);
  }
  
  return JS_TRUE;
}

/**
 *  Convert an ffi type to a JS type, e.g. as we process a CFunction's return value.
 *
 *  By default, we assume that returned pointers are pointers to memory which does not
 *  need to be freed.  If that assumption is wrong the JS programmer can touch up the
 *  object with the finalizeWith() feature.
 *
 *  @param cx           The JavaScript context
 *  @param abi_rvalp    A pointer to the returned value
 *  @param rval         [out] The converted value as a jsval
 *  @param throwPrefix  Prefix for exceptions (e.g. CLASS_ID)
 */
JSBool ffiType_toValue(JSContext *cx, void *abi_rvalp, ffi_type *rtype_abi, jsval *rval, const char *throwPrefix)
{
  if (rtype_abi == &ffi_type_pointer) 
  {
    void 		*ptr = *(void **)abi_rvalp;
    jsval 		argv[] = { INT_TO_JSVAL(0), JSVAL_FALSE };
    memory_handle_t	*memHnd;
    JSObject		*robj;
    JSObject            *memory_proto;

    if (!ptr)
    {
      *rval = JSVAL_NULL;
      return JS_TRUE;
    }

    if (gpsee_getModuleData(cx, memory_clasp, (void **)&memory_proto, throwPrefix) == JS_FALSE)
      return JS_FALSE;

    robj = JS_NewObject(cx, memory_clasp, memory_proto, NULL);
    if (Memory_Constructor(cx, robj, sizeof(argv) / sizeof(argv[0]), argv, rval) == JS_FALSE)
      return JS_FALSE;

    memHnd = JS_GetPrivate(cx, robj);
    GPSEE_ASSERT(memHnd);
    if (!memHnd)
      return gpsee_throw(cx, "%s: impossible error processing returned Memory object", throwPrefix);

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
      return gpsee_throw(cx, "%s.return.overflow", throwPrefix);\
    								\
    return JS_NewNumberValue(cx, d, rval);			\
  }
#define FFI_TYPES_NUMBERS_ONLY
#include "ffi_types.decl"
#undef ffi_type
#undef FFI_TYPES_NUMBERS_ONLY

  if (rtype_abi == &ffi_type_void)
  {
    *rval = JSVAL_VOID;
    return JS_TRUE;
  }

  return gpsee_throw(cx, "%s.returnType.unhandled: unhandled return type", throwPrefix);
}

/** Not an arg converter - poor naming */
static JSBool valueTo_jsint(JSContext *cx, jsval v, jsint *ip, int argn, const char *throwPrefix)
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
    return gpsee_throw(cx, "%s.arguments.%i.range: value %g cannot be converted to an integer type", throwPrefix, argn, d);

  return JS_TRUE;
}

/*  Arg Converter Notes
 ************************************
 *  *storagep is a pointer to temporary memory which will be freed
 *  *avaluep is a pointer to the thing we want to pass as an argument
 *  IFF *storagep points to the the thing, *avaluep = *storagep
 */
static JSBool valueTo_char(JSContext *cx, jsval v, void **avaluep, void **storagep, int argn, const char *throwPrefix) 
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

    s = JS_GetStringBytes(str);
    if (s)
    {
      if (s[0] && !s[1])
	i = s[0];
    }
  }
  else
  {
    if (valueTo_jsint(cx, v, &i, argn, throwPrefix) == JS_FALSE)
      return JS_FALSE;
  }

  ((char *)*storagep)[0] = i;
  if (i != ((char *)*storagep)[0])
    return gpsee_throw(cx, "%s.arguments.%i.valueTo_char.range: %i does not fit in a char", throwPrefix, argn, i);

  return JS_TRUE;
}

static JSBool valueTo_schar(JSContext *cx, jsval v, void **avaluep, void **storagep, int argn, const char *throwPrefix) 
{
  return valueTo_char(cx, v, avaluep, storagep, argn, throwPrefix);
}

static JSBool valueTo_uchar(JSContext *cx, jsval v, void **avaluep, void **storagep, int argn, const char *throwPrefix) 
{
  return valueTo_char(cx, v, avaluep, storagep, argn, throwPrefix);
}

#define ffi_type(ftype, ctype)\
static JSBool valueTo_ ##ftype(JSContext *cx, jsval v, 				\
			       void **avaluep, void **storagep, int argn,       \
                               const char *throwPrefix)                         \
{ 										\
  jsint i;									\
  ctype tgt;									\
    								                \
  if (valueTo_jsint(cx, v, &i, argn, throwPrefix) == JS_FALSE)			\
    return JS_FALSE;								\
    								                \
  tgt = i;									\
  if (i != tgt)									\
    return gpsee_throw(cx, "%s.arguments.%i.valueTo_" #ftype ".range"	        \
		       ": %i does not fit in a%s " #ctype, throwPrefix, argn, i,\
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

static JSBool valueTo_void(JSContext *cx, jsval v, void **avaluep, void **storagep, int argn, const char *throwPrefix) 
{ 
  return gpsee_throw(cx, "%s.valueTo_void: cannot convert argument %i to void", throwPrefix, argn);
}

static JSBool valueTo_float(JSContext *cx, jsval v, void **avaluep, void **storagep, int argn, const char *throwPrefix)
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

static JSBool valueTo_double(JSContext *cx, jsval v, void **avaluep, void **storagep, int argn, const char *throwPrefix)
{ 
  *storagep = JS_malloc(cx, sizeof(jsdouble));

  if (!*storagep)
    return JS_FALSE;

  *avaluep = *storagep;

  return JS_ValueToNumber(cx, v, (jsdouble *)*storagep);
}

static JSBool valueTo_pointer(JSContext *cx, jsval v, void **avaluep, void **storagep, int argn, const char *throwPrefix)
{ 
  JSString 		*str;

  if (JSVAL_IS_STRING(v))
  {
    str = JSVAL_TO_STRING(v);
    goto stringPointer;
  }

  if (JSVAL_IS_NULL(v))
  {
    *storagep = NULL;
    *avaluep = storagep;
    return JS_TRUE;
  }

  if (JSVAL_IS_OBJECT(v))
  {
    JSObject *obj = JSVAL_TO_OBJECT(v);
    if (gpsee_isByteThing(cx, obj))
    {
      memory_handle_t *hnd = JS_GetPrivate(cx, obj);
      if (hnd) {
        *avaluep = &hnd->buffer;
        return JS_TRUE;
      }
    }
  }

  if (JSVAL_IS_VOID(v))
    return gpsee_throw(cx, "%s.argument.%i.invalid: cannot convert undefined argument to a pointer", throwPrefix, argn);

  if (v == JSVAL_TRUE || v == JSVAL_FALSE)
    return gpsee_throw(cx, "%s.argument.%i.invalid: cannot convert a bool argument to a pointer", throwPrefix, argn);

  if (JSVAL_IS_NUMBER(v))
    return gpsee_throw(cx, "%s.argument.%i.invalid: cannot convert a numeric argument to a pointer", throwPrefix, argn);

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

static JSBool valueTo_longdouble(JSContext *cx, jsval v, void **avaluep, void **storagep, int argn, const char *throwPrefix) 
{ 
  jsdouble d;
  long double	*storagep_ld;

  *storagep = storagep_ld = JS_malloc(cx, sizeof(long double));

  if (!*storagep)
    return JS_FALSE;

  if (JS_ValueToNumber(cx, v, &d) == JS_FALSE)
    return JS_FALSE;

  *storagep_ld = d;
  *avaluep = storagep;

  return JS_TRUE; 
}

/** Set up the argument converters for a CFunction, intended to be done as the CFunction is instanciated.
 *  
 *  @param cx              The JavaScript context
 *  @param typeIndicators  An array of typeIndicators (exactly hnd->nargs long)
 *  @param hnd             CFunction handle, with all memory allocated and hnd->nargs initialized. 
 *                         The handle will be modfied to reflect the type and converted of each
 *                         argument.
 *  @returns JS_TRUE on success or JS_FALSE on exception
 */
JSBool setupCFunctionArgumentConverters(JSContext *cx, jsval *typeIndicators, cFunction_handle_t *hnd, const char *throwPrefix)
{
  int i;

  /* Sort out argument types */
  for (i=0; i < hnd->nargs;  i++)
  {
#define ffi_type(type, junk) 			\
    if (typeIndicators[i] == jsve_ ## type) 		\
    {						\
      hnd->argTypes[i] = &ffi_type_ ## type; 	\
      hnd->argConverters[i] = valueTo_ ## type;	\
      continue; 				\
    }
#include "ffi_types.decl"
#undef ffi_type
    return gpsee_throw(cx, "%s.constructor.typeIndicator.%i: Invalid value specified: should be FFI type indicator", throwPrefix, 1 + i);
  }

  return JS_TRUE;
}

/** Setup up the data size, arguments converter, and ffi_type pointer for a CType instance. 
 *  @param cx              The JavaScript context
 *  @param typeIndicator   A type indicator (e.g. jsve_int)
 *  @param hnd             A CType handle. The handle will be modfied to reflect the details of this particular CType.
 *                         argument.
 *  @returns JS_TRUE on success or JS_FALSE on exception
 */
JSBool setupCTypeDetails(JSContext *cx, jsval typeIndicator, ctype_handle_t *hnd, const char *throwPrefix)
{
  switch(typeIndicator)
  {
#define FFI_TYPES_SIZED_ONLY
#define ffi_type(ftype, ctype) 			\
    case jsve_ ## ftype: 			\
      hnd->length = sizeof(ctype); 		\
      hnd->valueTo_ffiType = valueTo_ ## ftype;	\
      hnd->ffiType = &ffi_type_ ## ftype; 	\
      break;					
#include "ffi_types.decl"
#undef ffi_type
#undef FFI_TYPES_SIZED_ONLY
    default:
      return gpsee_throw(cx, "%s: invalid type indicator", throwPrefix);
  }

  return JS_TRUE;
}


