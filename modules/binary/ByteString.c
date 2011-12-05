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
 * Copyright (c) 2007, PageMail, Inc. All Rights Reserved.
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
 *  @file	ByteString.c 	A class for implementing Byte Strings
 *  @author	Wes Garland
 *              PageMail, Inc.
 *		wes@page.ca
 *  @date	Jan 2008
 *  @version	$Id: ByteString.c,v 1.11 2011/12/05 19:13:37 wes Exp $
 *
 *  Based on https://wiki.mozilla.org/ServerJS/Binary/B
 *  Extensions:
 *  - Missing or falsy charset in constructor means to inflate/deflate
 */

static const char __attribute__((unused)) rcsid[]="$Id: ByteString.c,v 1.11 2011/12/05 19:13:37 wes Exp $";
#include "gpsee.h"
#include "binary.h"

JSObject *byteString_proto;
#define CLASS_ID MODULE_ID ".ByteString"

/** Returns a pointer to a byteString_handle_t, or NULL on error
 *  @param      cx          Your JSContext
 *  @param      obj         ByteString instance
 *  @param      methodName  Name of ByteString member method (used for error reporting)
 *  @returns    A pointer to a byteString_handle_t or NULL if an exception is thrown
 *  @throws     type
 */
inline byteString_handle_t * byteString_getHandle(JSContext *cx, JSObject *obj, const char const * methodName)
{
  byteString_handle_t *bs;
  bs = JS_GetInstancePrivate(cx, obj, byteString_clasp, NULL);
  if (!bs)
  {
    gpsee_throw(cx, CLASS_ID ".%s.type: native member function applied to non-ByteString object", methodName);
    return NULL;
  }
  return bs;
}

GPSEE_STATIC_ASSERT(sizeof(int64) == sizeof(long long int));

/** Tests an index to be sure it is within a byteString_handle_t's range, and throws
 *  @param      cx          Your JSContext
 *  @param      bs          Your byteString_handle_t
 *  @param      index       Index to check against byteString_handle_t
 *  @param      methodName  Name of ByteString member method (used for error reporting)
 *  @returns    JS_TRUE on success, JS_FALSE if an exception is thrown
 *  @throws     range.underflow
 *  @throws     range.overflow
 */
int byteString_rangeCheck(JSContext *cx, byteString_handle_t * bs, int64 index, const char const * methodName)\
{
  if (index < 0)
    return gpsee_throw(cx, CLASS_ID ".%s.range.underflow: " GPSEE_INT64_FMT "<0", methodName, (long long int) index);

  if (index >= bs->length)
    return gpsee_throw(cx, CLASS_ID ".%s.range.overflow: " GPSEE_INT64_FMT ">=" GPSEE_SIZET_FMT, methodName, (long long int) index, bs->length);

  return JS_TRUE;
}

/** Tries to coerce a jsval into an int64 for indexing into a ByteString
 *  @param    cx          Your JSContext
 *  @param    bs          Your byteString_handle_t
 *  @param    val         Your jsval to be converted and stored in 'index'
 *  @param    index       (out-variable) Address at which to store the coerced jsval
 *  @param    methodName  Name of ByteString member method (used for error reporting)
 *  @returns  JS_TRUE on success, JS_FALSE if an exception is thrown
 *  @throws   arguments.integer.invalid
 *  @throws   arguments.invalidType
 *  @throws   range.underflow
 *  @throws   range.overflow
 */
/* TODO int64 is not the greatest way to do this, i think, but for the moment, it works nicely */
inline int byteString_retrieveAndCheckIndexArgument(JSContext *cx, byteString_handle_t *bs, jsval val, int64 * index, const char const * methodName)
{
  /* The simplest scenario: the argument is a jsint */
  if (JSVAL_IS_INT(val))
  {
    *index = (int64)JSVAL_TO_INT(val);
    return JS_TRUE;
  }
  else {
    /* If it is not an int, it might be something else, which might be coerceable to jsdouble (actually JS_ValueToNumber() tries for a jsint, too) */
    jsdouble d;
    if (!JS_ValueToNumber(cx, val, &d))
      return gpsee_throw(cx, CLASS_ID ".%s.arguments.invalidType: Number required", methodName);
    /* Avoid NaN et al, and ensure the floating-point value survives a conversion cast to int intact */
    if (d != (int64)d)
      return gpsee_throw(cx, CLASS_ID ".%s.arguments.integer.invalid: %lf is an invalid index", methodName, d);
    *index = (int64)d;
  }
  /* Check that this index is within our ByteString boundaries */
  return byteString_rangeCheck(cx, bs, *index, methodName);
}

/** Implements ByteString::indexOf method. Method arguments are
 *  (byte, start, stop). Byte could be a number, or single element
 *  ByteArray or ByteString. -1 indicates not found.
 */
JSBool ByteString_indexOf(JSContext *cx, uintN argc, jsval *vp)
{
  return byteThing_findChar(cx, argc, vp, memchr, "indexOf", byteString_clasp);
}

/** Implements ByteString::lastIndexOf method. Method arguments are
 *  (byte, start, stop). Byte could be a number, or single element
 *  ByteArray or ByteString. -1 indicates not found.
 */
static JSBool ByteString_lastIndexOf(JSContext *cx, uintN argc, jsval *vp)
{
  return byteThing_findChar(cx, argc, vp, memrchr, "lastIndexOf", byteString_clasp);
}

/** Implements ByteString::decodeToString method */
static JSBool ByteString_decodeToString(JSContext *cx, uintN argc, jsval *vp)
{
  return byteThing_decodeToString(cx, argc, vp, byteString_clasp);
}

/** Implements ByteString.toByteString() */
static JSBool ByteString_toByteString(JSContext *cx, uintN argc, jsval *vp)
{
  return byteThing_toByteThing(cx, argc, vp, byteString_clasp, byteString_proto, sizeof(byteString_handle_t), CLASS_ID ".toByteString");
}
/** Implements ByteString.toByteArray() */
static JSBool ByteString_toByteArray(JSContext *cx, uintN argc, jsval *vp)
{
  return byteThing_toByteThing(cx, argc, vp, byteArray_clasp, byteArray_proto, sizeof(byteArray_handle_t), CLASS_ID ".toByteArray");
}

/* TODO this function is now identical to the ByteArray getter */
/** Default ByteString property getter.  Used to implement to implement Array-like property lookup ([]) */
static JSBool ByteString_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  return byteThing_getProperty(cx, obj, id, vp, byteString_clasp);
}

/** Implements ByteString.charAt() */
JSBool ByteString_charAt(JSContext *cx, uintN argc, jsval *vp)
{
  return byteThing_byarAt(cx, argc, vp, byteString_clasp, byteString_clasp, "ByteString.charAt");
}
/** Implements ByteString.byteAt() */
JSBool ByteString_byteAt(JSContext *cx, uintN argc, jsval *vp)
{
  return byteThing_byarAt(cx, argc, vp, byteString_clasp, byteString_clasp, "ByteString.byteAt");
}
/** Implements ByteString.charCodeAt() */
JSBool ByteString_charCodeAt(JSContext *cx, uintN argc, jsval *vp)
{
  return byteThing_byarAt(cx, argc, vp, byteString_clasp, NULL, "ByteString.charCodeAt");
}
/** Implements ByteString.get() */
JSBool ByteString_get(JSContext *cx, uintN argc, jsval *vp)
{
  return byteThing_byarAt(cx, argc, vp, byteString_clasp, NULL, "ByteString.get");
}

/** 
 *  Implements the ByteString constructor.
 *  new ByteString(file descriptor number, mode string, [buffered boolean])
 *
 *  @param	cx	JavaScript context
 *  @param	obj	Pre-allocated ByteString object
 *  @param	argc	Number of arguments passed to constructor
 *  @param	argv	Arguments passed to constructor
 *  @param	rval	The new object returned to JavaScript
 *
 *  @returns 	JS_TRUE on success
 */
static JSBool ByteString_Constructor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  JSObject 	*instance;        /* Constructor return value */
  unsigned char	*buffer = NULL;   /* Outvar to copyJSArray_toBuf() and transcodeString_toBuf() */
  size_t	length;           /* Outvar to copyJSArray_toBuf() and transcodeString_toBuf() */
  int		stealBuffer = 0;  /* whether or not we want a further private constructor function to make its own copy */

  byteArray_handle_t 	*hnd;

  /* This case is easy */
  if (argc == 0)
  {
    length = 0;
    buffer = NULL;
    goto instanciate;
  }

  if ((argc == 1) && JSVAL_IS_OBJECT(argv[0]))
  {
    JSObject	*o = JSVAL_TO_OBJECT(argv[0]);
    void 	*c = o ? JS_GET_CLASS(cx, o) : NULL;

    /* Copy constructor! */
    if ((c == (void *)byteString_clasp) || (c == (void *)byteArray_clasp))
    {
      byteThing_handle_t *h = JS_GetPrivate(cx, o);

      buffer = (unsigned char *)h->buffer;
      length = h->length;
      goto instanciate;
    }

    /* Construct from an array of byte Number values */
    if (JS_IsArrayObject(cx, o) == JS_TRUE)
    {
      /* Setting 'buffer' to NULL indicates that we want copyJSArray_toBuf() to allocate
       * a byte vector for us and return it through 'buffer' */
      buffer = NULL;
      stealBuffer = 1;

      if (copyJSArray_toBuf(cx, o, 0, &buffer, &length, NULL, CLASS_ID ".constructor") != JS_TRUE)
	return JS_FALSE;

      goto instanciate;
    }
  }

  if (argc == 1 || argc == 2)
  {
    JSString 	*string;
    JSString 	*charset;
    JSBool	b;

    JS_EnterLocalRootScope(cx);
 
    string = JS_ValueToString(cx, argv[0]);
    if ((argc == 2) && !gpsee_isFalsy(cx, argv[1]))
      charset  = JS_ValueToString(cx, argv[1]);
    else
      charset = NULL;

    if (!string)
      b = JS_FALSE;
    else
    {
      b = transcodeString_toBuf(cx, string, charset ? JS_GetStringBytes(charset) : NULL, &buffer, &length, CLASS_ID ".constructor");
      stealBuffer = 1;	/* decodeString_toByteString allocates a buffer already */
    }

    JS_LeaveLocalRootScope(cx);

    if (b == JS_TRUE)
      goto instanciate;
    return JS_FALSE;
  }

  return gpsee_throw(cx, CLASS_ID ".constructor.arguments: invalid kind or number of arguments");

  instanciate:
  instance = byteThing_fromCArray(cx, buffer, length, obj,
                                  byteString_clasp, byteString_proto, sizeof(byteString_handle_t), stealBuffer);
  if (!instance)
    return JS_FALSE;

  hnd = JS_GetPrivate(cx, instance);
  hnd->btFlags |= bt_immutable;

  *rval = OBJECT_TO_JSVAL(instance);
  return JS_TRUE;
}  

static JSBool ByteString_Cast(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return byteThing_Cast(cx, argc, argv, rval, byteString_clasp, byteString_proto, sizeof(byteString_handle_t), CLASS_ID);
}

static JSBool ByteString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  /* ByteString() called as function. */   
  if (JS_IsConstructing(cx) != JS_TRUE)
    return ByteString_Cast(cx, obj, argc, argv, rval);
  else
    return ByteString_Constructor(cx, obj, argc, argv, rval);
}

/** 
 *  ByteString Finalizer.
 *
 *  Called by the garbage collector, this routine release all resources
 *  used by an instance of the class when the object is collected: the
 *  buffered stream, the handle's memory, etc. I also closes the
 *  underlying file descriptor.
 *
 *  @param	cx	JavaScript context
 *  @param	obj	The object to finalize
 */
static void ByteString_Finalize(JSContext *cx, JSObject *obj)
{
  byteString_handle_t	*hnd = JS_GetPrivate(cx, obj);

  if (!hnd)
    return;

  if (hnd->buffer && (hnd->memoryOwner == obj))
    JS_free(cx, hnd->buffer);

  JS_free(cx, hnd);

  return;
}

/** ByteString_split() implements the ByteString member method split() */
JSBool ByteString_split(JSContext *cx, uintN argc, jsval *vp)
{
  jsval 		*argv = JS_ARGV(cx, vp);
  byteString_handle_t	*hnd;
  JSObject              *retval;
  int                   i, l, chunkstart, chunklen, chunknum;
  unsigned char         *buf, delimiter;
  jsdouble              delimiter_temp;
  /* TODO support more delimiter types than scalar int */

  /* Acquire a pointer to our internal bytestring data */
  hnd = byteString_getHandle(cx, JS_THIS_OBJECT(cx, vp), "split");
  if (!hnd)
    return JS_FALSE;
  buf = hnd->buffer;

  /* @function
   * @name    ByteString.split
   * @param   delimiter
   * @param   options   optional
   */
  /* TODO support 'options' */

  if (argc < 1 || argc > 2)
    return gpsee_throw(cx, CLASS_ID ".split.arguments.count");

  /* Type coercion for long delimiters */
  if (JSVAL_IS_OBJECT(argv[0]))
  {
    /* TODO support long delimiters */
    return gpsee_throw(cx, CLASS_ID ".split.arguments.unimplemented");
  }

  /* Type coercion for scalar delimiters */
  if (!JS_ValueToNumber(cx, argv[0], &delimiter_temp))
    return gpsee_throw(cx, CLASS_ID ".split.arguments.type.invalid");
  delimiter = (unsigned char)delimiter_temp;
  if (delimiter_temp != delimiter)
    return gpsee_throw(cx, CLASS_ID ".split.arguments.type.invalid: %lf invalid byte value", delimiter_temp);

  /* Instantiate an array to hold our results */
  retval = JS_NewArrayObject(cx, 0, NULL);
  if (!retval)
    return JS_FALSE;

  /* Protect our array from garbage collector */
  JS_AddObjectRoot(cx, &retval);

  /* Iterate through ByteArray splitting it up by 'delimiter' */
  chunkstart = 0;
  chunklen = 0;
  chunknum = 0;
  for (i=0, l=hnd->length, chunkstart=0, chunklen=0; i<=l; i++)
  {
    /* Split! */
    if (((i==l) && (chunklen>0)) || (buf[i] == delimiter))
    {
      JSBool success;
      jsval oval;
      /* Instantiate new ByteArray */
      JSObject *o = byteThing_fromCArray(cx, buf + chunkstart, chunklen, NULL,
                                         byteString_clasp, byteString_proto, sizeof(byteString_handle_t), 0);
      if (!o)
        return JS_FALSE;
      /* Push the new ByteArray into our result array */
      JS_AddObjectRoot(cx, &o);
      oval = OBJECT_TO_JSVAL(o);
      success = JS_SetElement(cx, retval, chunknum++, &oval);
      JS_RemoveObjectRoot(cx, &o);
      if (!success)
        return JS_FALSE;
      /* Start on new chunk! */
      chunkstart = i+1;
      chunklen = 0;
    } else {
      chunklen++;
    }
  }

  /* Return the array object containing results */
  JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(retval));
  /* Un-GC-protect */
  JS_RemoveObjectRoot(cx, &retval);
  /* Success! */
  return JS_TRUE;
}
/** ByteString_slice() implements the ByteString member method slice() */
JSBool ByteString_slice(JSContext *cx, uintN argc, jsval *vp)
{
  jsval 		*argv = JS_ARGV(cx, vp);
  byteString_handle_t	*hnd;
  int64                 start, end;
  JSObject              *retval;
  
  /* Acquire a pointer to our internal bytestring data */
  hnd = byteString_getHandle(cx, JS_THIS_OBJECT(cx, vp), "slice");
  if (!hnd)
    return JS_FALSE;

  /* @function
   * @name    ByteString.slice
   * @param   start
   * @param   end       optional
   */
  switch (argc)
  {
    default:
      return gpsee_throw(cx, CLASS_ID ".slice.arguments.count");
    case 1:
      /* TODO to number! */
      if (JSVAL_IS_INT(argv[0]))
        start = JSVAL_TO_INT(argv[0]);
      else
	return gpsee_throw(cx, CLASS_ID ".slice,arguments.0.type: must specify an actual number");
      end = hnd->length;
      /* Wrap negative indexes around the end */
      if (start < 0)
        start = hnd->length + start - 1;
      /* Range checks */
      if (!byteString_rangeCheck(cx, hnd, start, "slice"))
        return JS_FALSE;
      break;
    case 2:
      /* TODO to number! */
      if (JSVAL_IS_INT(argv[0]))
        start = JSVAL_TO_INT(argv[0]);
      else
	return gpsee_throw(cx, CLASS_ID ".slice,arguments.0.type: must specify an actual number");

      if (JSVAL_IS_INT(argv[1]))
        end = JSVAL_TO_INT(argv[1]);
      else
	return gpsee_throw(cx, CLASS_ID ".slice,arguments.1.type: must specify an actual number");

      /* Wrap negative indexes around the end */
      if (end < 0)
        end = hnd->length + end;
      if (start < 0)
        start = hnd->length + start - 1;
      /* Range checks */
      if (start >= end)
        return
        gpsee_throw(cx, CLASS_ID ".slice.arguments.range: 'start' argument ("
		    GPSEE_INT64_FMT ") must be lesser than 'end' argument ("
		    GPSEE_INT64_FMT ")", (long long int) start, (long long int) end);
      /* Validate range of operation */
      /* TODO fix inaccurate end-1 error reporting */
      if (!byteString_rangeCheck(cx, hnd, start, "slice") ||
          !byteString_rangeCheck(cx, hnd, end-1, "slice"))
        return JS_FALSE;
      break;
  }


  /* Instantiate a new ByteString from a subsection of the buffer */
  retval = byteThing_fromCArray(cx, hnd->buffer + start, end - start, NULL,
                                byteString_clasp, byteString_proto, sizeof(byteString_handle_t), 0);

  /* Success! */
  if (retval)
  {
    /* Return new ByteString */
    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(retval));
    return JS_TRUE;
  }

  /* Failure; pass downstream exceptions up the call stack */
  return JS_FALSE;
}
/** ByteString_substr() implements the ByteString member method substr() */
JSBool ByteString_substr(JSContext *cx, uintN argc, jsval *vp)
{
  byteString_handle_t	*hnd;
  size_t                start, len, size;
  JSObject              *retval;
  
  /* Acquire a pointer to our internal bytestring data */
  hnd = byteString_getHandle(cx, JS_THIS_OBJECT(cx, vp), "substr");
  if (!hnd)
    return JS_FALSE;

  size = hnd->length;

  /* @function
   * @name    ByteString.substr
   * @param   start
   * @param   len       optional
   */

  /* Get Javascript arguments */
  if (!byteThing_arg2size(cx, argc, vp, &start, 0, 0, size-1, JS_FALSE, 0, byteString_clasp, "substr"))
    return JS_FALSE;

  if (!byteThing_arg2size(cx, argc, vp, &len, 1, 1, size-start, JS_TRUE, hnd->length-start, byteString_clasp, "substr"))
    return JS_FALSE;

  /* Instantiate a new ByteString from a subsection of the buffer */
  retval = byteThing_fromCArray(cx, hnd->buffer + start, len, NULL,
                                byteString_clasp, byteString_proto, sizeof(byteString_handle_t), 0);
  if (!retval)
    return JS_FALSE;

  /* Return new ByteString */
  JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(retval));
  return JS_TRUE;
}
/** ByteString_substring() implements the ByteString member method substring() */
JSBool ByteString_substring(JSContext *cx, uintN argc, jsval *vp)
{
  byteString_handle_t	*hnd;
  size_t                start, end, size;
  JSObject              *retval;
  
  /* Acquire a pointer to our internal bytestring data */
  hnd = byteString_getHandle(cx, JS_THIS_OBJECT(cx, vp), "substring");
  if (!hnd)
    return JS_FALSE;
  size = hnd->length;

  /* @function
   * @name    ByteString.substring
   * @param   start
   * @param   end       optional
   */
  /* Get Javascript arguments */
  if (!byteThing_arg2size(cx, argc, vp, &start, 0, 0, size-1, JS_FALSE, 0, byteString_clasp, "substring"))
    return JS_FALSE;

  if (!byteThing_arg2size(cx, argc, vp, &end,   1, 0, size-1, JS_TRUE, hnd->length-1, byteString_clasp, "substring"))
    return JS_FALSE;

  /* This behavior corresponds with String.substring() */
  if (end < start)
  {
    size_t temp = start;
    start = end;
    end = temp;
  }

  /* Instantiate a new ByteString from a subsection of the buffer */
  retval = byteThing_fromCArray(cx, hnd->buffer + start, end - start, NULL,
                                byteString_clasp, byteString_proto, sizeof(byteString_handle_t), 0);

  /* Success! */
  if (retval)
  {
    /* Return new ByteString */
    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(retval));
    return JS_TRUE;
  }

  /* Failure; pass downstream exceptions up the call stack */
  return JS_FALSE;
}
/** ByteString_toSource() implements the ByteString member method toSource() */
JSBool ByteString_toSource(JSContext *cx, uintN argc, jsval *vp)
{
  return byteThing_toSource(cx, argc, vp, byteString_clasp);
}
/** ByteString_xintAt() implements the ByteString member method xintAt() */
JSBool ByteString_xintAt(JSContext *cx, uintN argc, jsval *vp)
{
  return byteThing_intAt(cx, argc, vp, CLASS_ID);
}

/** Implements ByteString.length getter */
static JSBool ByteString_getLength(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  return byteThing_getLength(cx, obj, byteString_clasp, id, vp);
}

/** Initializes binary.ByteString */
JSObject *ByteString_InitClass(JSContext *cx, JSObject *obj, JSObject *parentProto)
{
/** Description of this class: */
  static JSClass byteString_class = 
  {
    GPSEE_CLASS_NAME(ByteString),		/**< its name is ByteString */
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  			/**< addProperty stub */
    JS_PropertyStub,  			/**< deleteProperty stub */
    ByteString_getProperty,		/**< custom getProperty */
    JS_PropertyStub, 			/**< setProperty stub */
    JS_EnumerateStub, 			/**< enumerateProperty stub */
    JS_ResolveStub,   			/**< resolveProperty stub */
    JS_ConvertStub,   			/**< convertProperty stub */
    ByteString_Finalize,		/**< it has a custom finalizer */

    JSCLASS_NO_OPTIONAL_MEMBERS
  };

  static JSFunctionSpec instance_methods[] = 
  {
    JS_FN("toByteString", 	ByteString_toByteString, 	0, 0),
    JS_FN("toByteArray",  	ByteString_toByteArray,  	0, 0),
    JS_FN("toString",		byteThing_toString,		0, 0),
    JS_FN("decodeToString",	ByteString_decodeToString,	0, 0),
    JS_FN("indexOf",		ByteString_indexOf,		0, 0),
    JS_FN("lastIndexOf",	ByteString_lastIndexOf,		0, 0),
    JS_FN("charAt",		ByteString_charAt,		0, 0),
    JS_FN("byteAt",		ByteString_byteAt,		0, 0),
    JS_FN("xintAt",             ByteString_xintAt,              0, 0),
    JS_FN("charCodeAt",		ByteString_charCodeAt,          0, 0),
    JS_FN("get",        	ByteString_get,   		0, 0),
    JS_FN("split",              ByteString_split,               0, 0),
    JS_FN("slice",              ByteString_slice,               0, 0),
    JS_FN("substr",             ByteString_substr,              0, 0),
    JS_FN("substring",          ByteString_substring,           0, 0),
    JS_FN("toSource",           ByteString_toSource,            0, 0),
    JS_FS_END
  };

  static JSPropertySpec instance_props[] = 
  {
    { "length",		0,	JSPROP_READONLY,	ByteString_getLength },
    { NULL, 0, 0, NULL, NULL }
  };

  JSObject *proto;

  GPSEE_DECLARE_BYTETHING_CLASS(byteString);

  proto = 
      JS_InitClass(cx, 			/* JS context from which to derive runtime information */
		   obj, 		/* Object to use for initializing class (constructor arg?) */
		   parentProto,		/* parent_proto - parent class (ByteString.__proto__) */
 		   &byteString_class,	/* clasp - Class struct to init. Defs class for use by other API funs */
		   ByteString,		/* constructor function - Scope matches obj */
		   0,			/* nargs - Number of arguments for constructor (can be MAXARGS) */
		   instance_props,	/* ps - props struct for parent_proto */
		   instance_methods, 	/* fs - functions struct for parent_proto (normal "this" methods) */
		   NULL,		/* static_ps - props struct for constructor */
		   NULL); 		/* static_fs - funcs struct for constructor (methods like Math.Abs()) */

  GPSEE_ASSERT(proto);
  byteString_proto = proto;

  byteString_clasp = &byteString_class;
  return proto;
}

GPSEE_STATIC_ASSERT(offsetOf(byteThing_handle_t, length) == offsetOf(byteString_handle_t, length));
GPSEE_STATIC_ASSERT(offsetOf(byteThing_handle_t, buffer) == offsetOf(byteString_handle_t, buffer));

