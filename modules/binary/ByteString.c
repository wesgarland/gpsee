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
 *  @version	$Id: ByteString.c,v 1.2 2009/06/12 17:01:20 wes Exp $
 *
 *  Based on https://wiki.mozilla.org/ServerJS/Binary/B
 *  Extensions:
 *  - Missing or falsy charset in constructor means to inflate/deflate
 */

static const char __attribute__((unused)) rcsid[]="$Id: ByteString.c,v 1.2 2009/06/12 17:01:20 wes Exp $";

#if 0
#include "gpsee_config.h"
#if defined(HAVE_MEMRCHR)
# define _GNU_SOURCE
# include <string.h> /* unistd.h can poison memrchr somehow on Debian squeeze/sid, maybe others */
#endif
#endif

#include "gpsee.h"
#include <jsnum.h>
#include "binary_module.h"
#include <string.h>

static JSObject *byteString_proto;
#define CLASS_ID MODULE_ID ".ByteString"

/** Internal data structure backing a ByteString. Cast-compatible with byteThing_handle_t. */
typedef struct
{
  size_t		length;		/**< Number of characters in buffer */
  unsigned char		*buffer;	/**< Backing store */
} byteString_handle_t;


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
  if (!(bs = JS_GetInstancePrivate(cx, obj, byteString_clasp, NULL)))
  {
    gpsee_throw(cx, CLASS_ID ".%s.type: native member function applied to non-ByteString object", methodName);
    return NULL;
  }
  return bs;
}

/** Tests an index to be sure it is within a byteString_handle_t's range, and throws
 *  @param      cx          Your JSContext
 *  @param      bs          Your byteString_handle_t
 *  @param      index       Index to check against byteString_handle_t
 *  @param      methodName  Name of ByteString member method (used for error reporting)
 *  @returns    JS_TRUE on success, JS_FALSE if an exception is thrown
 *  @throws     range.underflow
 *  @throws     range.overflow
 */
inline int byteString_rangeCheck(JSContext *cx, byteString_handle_t * bs, int64 index, const char const * methodName)\
{
  if (index < 0)
  {
    gpsee_throw(cx, CLASS_ID ".%s.range.underflow: %d<0", methodName, index);
    return JS_FALSE;
  }
  if (index >= bs->length)
  {
    gpsee_throw(cx, CLASS_ID ".%s.range.overflow: %d>=%d", methodName, index, bs->length);
    return JS_FALSE;
  }
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
    if ((!JSDOUBLE_IS_FINITE(d)) || (d != (int64)d))
      return gpsee_throw(cx, CLASS_ID ".%s.arguments.integer.invalid: %lf is an invalid index", methodName, d);
    *index = (int64)d;
  }
  /* Check that this index is within our ByteString boundaries */
  return byteString_rangeCheck(cx, bs, *index, methodName);
}

/** Allocate and initialize a new byteString_handle_t. Returned handle is allocated
 *  with JS_malloc().  Caller is responsible for releasing memory; if it winds up
 *  as the private data in a ByteString instance, the finalizer will handle the
 *  clean up during garbage collection.
 *
 *  @param	cx		JavaScript context
 *  @returns	A initalized byteString_handle_t or NULL on OOM.
 */
byteString_handle_t *byteString_newHandle(JSContext *cx)
{
  byteString_handle_t	*hnd;

  hnd = JS_malloc(cx, sizeof(*hnd));
  if (!hnd)
    return NULL;

  memset(hnd, 0, sizeof(*hnd));

  return hnd;
}

/**
 *  Create a bare-bonesByteString instance out of an array of chars.
 *
 *  @param	cx		JavaScript context
 *  @param	buffer		The raw contents of the byte string; if buffer is NULL we initialize to all-zeroes
 *  @param	length		The length of the data in buf.
 *  @param	obj		New object of correct class to decorate, or NULL. If non-NULL, must match clasp.
 *  @param	stealBuffer	If non-zero, we steal the buffer rather than allocating and copying our own
 *
 *  @returns	NULL on OOM, otherwise a new ByteString.
 */
JSObject *byteString_fromCArray(JSContext *cx, const unsigned char *buffer, size_t length, JSObject *obj, int stealBuffer)
{
  /* Allocate and initialize our private ByteString handle */
  byteString_handle_t	*hnd = byteString_newHandle(cx);

  if (!hnd)
    return NULL;

  hnd->length = length;

  if (stealBuffer)
  {
    hnd->buffer = (unsigned char *)buffer;
  }
  else
  {
    if (length)
    {
      hnd->buffer = JS_malloc(cx, length);

      if (!hnd->buffer)
      {
        JS_free(cx, hnd);
	return NULL;
      }
    }

    if (buffer)
      memcpy(hnd->buffer, buffer, length);
    else
      memset(hnd->buffer, 0, length);
  }

  GPSEE_ASSERT(!obj || (JS_GET_CLASS(cx, obj) == byteString_clasp));

  if (!obj)
    obj = JS_NewObject(cx, byteString_clasp, byteString_proto, NULL);

  if (obj)
    JS_SetPrivate(cx, obj, hnd);

  return obj;
}

#if !defined(HAVE_MEMRCHR)
static void *memrchr(const void *s, int c, size_t n)
{
  const void *p;

  for (p = s + n; p != s; p--)
  {
    if (*(char *)p == c)
      return (void *)p;
  }

  return NULL;
}
#endif

/** Implements ByteString instance methods indexOf() and lastIndexOf() */
static JSBool byteString_findChar(JSContext *cx, uintN argc, jsval *vp, void *memchr_fn(const void *, int, size_t), const char const * methodName)
{
  byteString_handle_t	*hnd;
  jsval			*argv = JS_ARGV(cx, vp);
  int			byte;
  jsval			bytev;
  size_t  		start;
  size_t  		len;
  size_t                size;
  const unsigned char 	*found;

  /* Acquire our byteString_handle_t */
  if (!(hnd = byteString_getHandle(cx, JS_THIS_OBJECT(cx, vp), methodName)))
    return JS_FALSE;

  size = hnd->length;

  /* Validate argument count */
  if (argc < 1 || argc > 3)
    return gpsee_throw(cx, "%s.arguments.count", methodName);

  bytev = argv[0];

  /* Convert JS args to C args */
  if (!byteThing_arg2size(cx, argc, vp, &start, 1, 0, size-1, JS_TRUE, 0, methodName))
    return JS_FALSE;
  if (!byteThing_arg2size(cx, argc, vp, &len, 2, 1, size-len, JS_TRUE, hnd->length-start, methodName))
    return JS_FALSE;

  /* Process 'needle' argument */
  /* TODO encapsulate this type coercion? */
  if (JSVAL_IS_INT(bytev))
    byte = JSVAL_TO_INT(bytev);
  else if (JSVAL_IS_OBJECT(bytev))
  {
    JSObject		*o = JSVAL_TO_OBJECT(argv[0]);
    void 		*c = o ? JS_GET_CLASS(cx, o) : NULL;
    byteThing_handle_t *h;

    if ((c != (void *)byteString_clasp) && (c != (void *)byteArray_clasp))
      goto ToNumber;

    h = JS_GetPrivate(cx, o);
    if (h->length != 1)
      return gpsee_throw(cx, "%s.arguments.0.length: ByteString or ByteArray must have length 1", methodName);

    byte = h->buffer[0];
  }
  else /* try ToNumber */
  {
    jsdouble d;
    ToNumber:

    if (JS_ValueToNumber(cx, argv[0], &d) == JS_FALSE)
      return JS_FALSE;

    byte = d;
    if (byte != d)
      return gpsee_throw(cx, "%s.arguments.0.range", methodName);
  }

  /* Bounds checking! */
  /* TODO better */
  if (byte & ~255)
    return gpsee_throw(cx, "%s.arguments.0.range", methodName);

  /* Search for needle */
  if ((found = memchr_fn(hnd->buffer + start, byte, len)))
  {
    if (INT_FITS_IN_JSVAL(found - hnd->buffer))
      JS_SET_RVAL(cx, vp, INT_TO_JSVAL(found - hnd->buffer));
    else
    {
      jsval v;

      if (JS_NewNumberValue(cx, found - hnd->buffer, &v) == JS_FALSE)
	return JS_FALSE;

      JS_SET_RVAL(cx, vp, v);
    }
  }
  else
    JS_SET_RVAL(cx, vp, INT_TO_JSVAL(-1));
  

  return JS_TRUE;
}

/** Implements ByteString::indexOf method. Method arguments are
 *  (byte, start, stop). Byte could be a number, or single element
 *  ByteArray or ByteString. -1 indicates not found.
 */
JSBool ByteString_indexOf(JSContext *cx, uintN argc, jsval *vp)
{
  return byteString_findChar(cx, argc, vp, memchr, CLASS_ID ".indexOf");
}

/** Implements ByteString::lastIndexOf method. Method arguments are
 *  (byte, start, stop). Byte could be a number, or single element
 *  ByteArray or ByteString. -1 indicates not found.
 */
JSBool ByteString_lastIndexOf(JSContext *cx, uintN argc, jsval *vp)
{
  return byteString_findChar(cx, argc, vp, memrchr, CLASS_ID ".lastIndexOf");
}

/** Implements ByteString::decodeToString method */
JSBool ByteString_decodeToString(JSContext *cx, uintN argc, jsval *vp)
{
  byteString_handle_t	*hnd;// = JS_GetInstancePrivate(cx, JS_THIS_OBJECT(cx, vp), byteString_clasp, NULL);
  JSString		*s;
  unsigned char		*buf;
  size_t		length;
  const char		*sourceCharset;
  jsval			*argv = JS_ARGV(cx, vp);

  /* Acquire our byteString_handle_t */
  if (!(hnd = byteString_getHandle(cx, JS_THIS_OBJECT(cx, vp), "decodeToString")))
    return JS_FALSE;

  switch(argc)
  {
    case 0:
      sourceCharset = NULL;
      break;
    case 1:
      sourceCharset = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
      break;
    default:
      return gpsee_throw(cx, CLASS_ID ".decodeToString.arguments.count");
  }

  if (transcodeBuf_toBuf(cx, NULL, sourceCharset, &buf, &length, hnd->buffer, hnd->length, CLASS_ID ".decodeToString") == JS_FALSE)
    return JS_FALSE;

  s = JS_NewUCStringCopyN(cx, (jschar *)buf, length / 2);
  if (!s)
  {
    JS_ReportOutOfMemory(cx);
    return JS_FALSE;
  }

  JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(s));
  return JS_TRUE;
}

/** Implements ByteString::toString method */
JSBool ByteString_toString(JSContext *cx, uintN argc, jsval *vp)
{
  char 			buf[128];
  byteString_handle_t	*hnd = JS_GetInstancePrivate(cx, JS_THIS_OBJECT(cx, vp), byteString_clasp, NULL);
  JSString		*s;

  if (!hnd)
    return gpsee_throw(cx, CLASS_ID ".toString.invalid: ByteString::toString applied the wrong object type");

  snprintf(buf, sizeof(buf), "[object " CLASS_ID " " GPSEE_SIZET_FMT "]", hnd->length);
  s = JS_NewStringCopyZ(cx, buf);
  if (!s)
  {
    JS_ReportOutOfMemory(cx);
    return JS_FALSE;
  }

  JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(s));
  return JS_TRUE;
}

/** Implements ByteString::toByteString method. Method returns a transcoded copy 
 *  of the ByteString in a new ByteString, or the original ByteString if the
 *  encodings are equivalent.
 */
JSBool ByteString_toByteString(JSContext *cx, uintN argc, jsval *vp)
{
  const char 		*sourceCharset, *targetCharset;
  jsval			*argv = JS_ARGV(cx, vp);
  JSBool		ret;
  unsigned char		*newBuf;
  size_t		newBufLength;
  JSObject		*newByteString;
  byteString_handle_t	*hnd = JS_GetInstancePrivate(cx, JS_THIS_OBJECT(cx, vp), byteString_clasp, NULL);

  if (argc == 0)
  {
    JS_SET_RVAL(cx, vp, JS_THIS(cx, vp));
    return JS_TRUE;
  }

  if (argc != 2)
    return gpsee_throw(cx, CLASS_ID ".toByteString.arguments.count");

  if (!hnd)
    return gpsee_throw(cx, CLASS_ID ".toByteString.invalid: ByteString::toByteString applied the wrong object type");

  if (argv[0] == argv[1])
  {
    JS_SET_RVAL(cx, vp, JS_THIS(cx, vp));
    return JS_TRUE;
  }

  JS_EnterLocalRootScope(cx);
  sourceCharset = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
  targetCharset = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));

  if ((sourceCharset && targetCharset) && ((sourceCharset == targetCharset) || (strcasecmp(sourceCharset, targetCharset) == 0)))
  {
    JS_SET_RVAL(cx, vp, JS_THIS(cx, vp));
    ret = JS_TRUE;	
    goto out;
  }

  ret = transcodeBuf_toBuf(cx, targetCharset, sourceCharset, &newBuf, &newBufLength, hnd->buffer, hnd->length, CLASS_ID ".toByteString");
  if (ret == JS_FALSE)
    goto out;

  newByteString = byteString_fromCArray(cx, newBuf, newBufLength, NULL, 1);
  if (!newByteString)
  {
    (void)JS_ReportOutOfMemory(cx);
    ret = JS_FALSE;
  }
  else
  {
    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(newByteString));
  }

  out:
  JS_LeaveLocalRootScope(cx);
  return ret;
}

/** Default ByteString property getter.  Used to implement to implement Array-like property lookup ([]) */
static JSBool ByteString_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  byteString_handle_t	*hnd;
  int64                 index;

  /* Acquire our byteString_handle_t */
  if (!(hnd = byteString_getHandle(cx, obj, "getProperty")))
    return JS_FALSE;

  /* Coerce index argument and do bounds checking upon it */
  if (!byteString_retrieveAndCheckIndexArgument(cx, hnd, id, &index, "getProperty"))
    return JS_FALSE;

  /* Return a new one-length ByteString instance */
  *vp = OBJECT_TO_JSVAL(byteString_fromCArray(cx, hnd->buffer + index, 1, NULL, 0));
  return JS_TRUE;
}

/** Implements both ByteString.byteAt() and ByteString.charAt() */
JSBool byteString_byarAt(JSContext *cx, uintN argc, jsval *vp, unsigned char isChar)
{
  //jsval                 *argv = JS_ARGV(cx, vp);
  byteString_handle_t	*hnd;
  size_t                index;
  const char const      *methodName = isChar ? CLASS_ID "charAt" : CLASS_ID "byteAt";

  /* Acquire our byteString_handle_t */
  if (!(hnd = byteString_getHandle(cx, JS_THIS_OBJECT(cx, vp), methodName)))
    return JS_FALSE;

  /* Coerce argument and do bounds checking */
  if (!byteThing_arg2size(cx, argc, vp, &index, 0, 0, hnd->length-1, JS_FALSE, 0, methodName))
    return JS_FALSE;

  /* ByteString.charAt() returns a new ByteString */
  if (isChar)
  {
    JSObject *retval;

    /* Instantiate a new ByteString for the return value */
    if (!(retval = byteString_fromCArray(cx, hnd->buffer + index, 1, NULL, 0)))
      return JS_FALSE;

    /* Return the new ByteString! */
    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(retval));
  }
  /* ByteString.byteAt() returns a Number */
  else {
    JS_SET_RVAL(cx, vp, INT_TO_JSVAL(hnd->buffer[index]));
  }
  return JS_TRUE;
}
/** Implements ByteString.charAt() */
JSBool ByteString_charAt(JSContext *cx, uintN argc, jsval *vp)
{
  return byteString_byarAt(cx, argc, vp, 1);
}
/** Implements ByteString.byetAt() */
JSBool ByteString_byteAt(JSContext *cx, uintN argc, jsval *vp)
{
  return byteString_byarAt(cx, argc, vp, 0);
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
static JSBool ByteString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  JSObject 	*instance;
  unsigned char	*buffer;
  size_t	length;
  int		stealBuffer = 0;

  /* ByteString() called as function. */   
  if (JS_IsConstructing(cx) != JS_TRUE)
    return gpsee_throw(cx, CLASS_ID ".constructor.notFunction: Must call constructor with 'new'!");

  if (argc == 0)
  {
    /* Construct an empty byte string */
    length = 0;
    buffer = NULL;
    goto instanciate;
  }

  if ((argc == 1) && JSVAL_IS_OBJECT(argv[0]))
  {
    JSObject	*o = JSVAL_TO_OBJECT(argv[0]);
    void 	*c = o ? JS_GET_CLASS(cx, o) : NULL;

    if ((c == (void *)byteString_clasp) || (c == (void *)byteArray_clasp))
    {
      byteThing_handle_t *h = JS_GetPrivate(cx, o);

      buffer = (unsigned char *)h->buffer;
      length = h->length;
      goto instanciate;
    }

    if (JS_IsArrayObject(cx, o) == JS_TRUE)
    {
      if (copyJSArray_toBuf(cx, o, &buffer, &length, CLASS_ID ".constructor") != JS_TRUE)
	return JS_FALSE;

      stealBuffer = 1;	/* copyJSArray_toBuf allocates a buffer already */
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
  }

  return gpsee_throw(cx, CLASS_ID ".constructor.arguments: invalid kind or number of arguments");

  instanciate:
  instance = byteString_fromCArray(cx, buffer, length, obj, stealBuffer);
  if (!instance)
  {
    JS_ReportOutOfMemory(cx);
    return JS_FALSE;
  }

  *rval = OBJECT_TO_JSVAL(instance);
  return JS_TRUE;
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

  /* TODO should this throw an error? */
  if (!hnd)
    return;

  if (hnd->buffer)
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
  if (!(hnd = byteString_getHandle(cx, JS_THIS_OBJECT(cx, vp), "split")))
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
  if (!(retval = JS_NewArrayObject(cx, 0, NULL)))
    return JS_FALSE;

  /* Protect our array from garbage collector */
  JS_AddRoot(cx, &retval);

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
      JSObject *o = byteString_fromCArray(cx, buf + chunkstart, chunklen, NULL, 0);
      if (!o)
        return JS_FALSE;
      /* Push the new ByteArray into our result array */
      JS_AddRoot(cx, &o);
      oval = OBJECT_TO_JSVAL(o);
      success = JS_SetElement(cx, retval, chunknum++, &oval);
      JS_RemoveRoot(cx, &o);
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
  JS_RemoveRoot(cx, &retval);
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
  if (!(hnd = byteString_getHandle(cx, JS_THIS_OBJECT(cx, vp), "slice")))
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
      if (JSVAL_IS_INT(argv[1]))
        end = JSVAL_TO_INT(argv[1]);
      /* Wrap negative indexes around the end */
      if (end < 0)
        end = hnd->length + end;
      if (start < 0)
        start = hnd->length + start - 1;
      /* Range checks */
      if (start >= end)
        return
        gpsee_throw(cx, CLASS_ID ".slice.arguments.range: 'start' argument (%d) must be lesser than 'end' argument (%d)", start, end);
      /* Validate range of operation */
      /* TODO fix inaccurate end-1 error reporting */
      if (!byteString_rangeCheck(cx, hnd, start, "slice") ||
          !byteString_rangeCheck(cx, hnd, end-1, "slice"))
        return JS_FALSE;
      break;
  }


  /* Instantiate a new ByteString from a subsection of the buffer */
  retval = byteString_fromCArray(cx, hnd->buffer + start, end - start, NULL, 0);

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
  if (!(hnd = byteString_getHandle(cx, JS_THIS_OBJECT(cx, vp), "substr")))
    return JS_FALSE;

  size = hnd->length;

  /* @function
   * @name    ByteString.substr
   * @param   start
   * @param   len       optional
   */

  /* Get Javascript arguments */
  if (!byteThing_arg2size(cx, argc, vp, &start, 0, 0, size-1, JS_FALSE, 0, CLASS_ID ".substr"))
    return JS_FALSE;

  if (!byteThing_arg2size(cx, argc, vp, &len, 1, 1, size-start, JS_TRUE, hnd->length-start, CLASS_ID ".substr"))
    return JS_FALSE;

  /* Instantiate a new ByteString from a subsection of the buffer */
  if (!(retval = byteString_fromCArray(cx, hnd->buffer + start, len, NULL, 0)))
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
  if (!(hnd = byteString_getHandle(cx, JS_THIS_OBJECT(cx, vp), "substring")))
    return JS_FALSE;
  size = hnd->length;

  /* @function
   * @name    ByteString.substring
   * @param   start
   * @param   end       optional
   */
  /* Get Javascript arguments */
  if (!byteThing_arg2size(cx, argc, vp, &start, 0, 0, size-1, JS_FALSE, 0, CLASS_ID ".substring"))
    return JS_FALSE;

  if (!byteThing_arg2size(cx, argc, vp, &end,   1, 0, size-1, JS_TRUE, hnd->length-1, CLASS_ID ".substring"))
    return JS_FALSE;

  /* This behavior corresponds with String.substring() */
  if (end < start)
  {
    size_t temp = start;
    start = end;
    end = temp;
  }

  /* Instantiate a new ByteString from a subsection of the buffer */
  retval = byteString_fromCArray(cx, hnd->buffer + start, end - start, NULL, 0);

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
  const char front[] = "(new require(\"binary\").ByteString([";
  const char back[]  =                                      "]))";
  char *source_string, *c;
  unsigned char *buf;
  byteString_handle_t * hnd;
  int i, l;
  JSString *retval;

  /* Acquire a pointer to our internal bytestring data */
  if (!(hnd = byteString_getHandle(cx, JS_THIS_OBJECT(cx, vp), "toSource")))
    return JS_FALSE;
  l = hnd->length;
  buf = hnd->buffer;

  /* Allocate a string for */
  source_string = JS_malloc(cx, sizeof(front) + sizeof(back) + l*4);
  strcpy(source_string, front);
  c = source_string + sizeof(front) - 1;
  i = 0;
  do {
    c += sprintf(c, "%d", buf[i]);
  } while (++i<l && (*(c++)=','));
  strcpy(c, back);

  retval = JS_NewString(cx, source_string, strlen(source_string));
  if (retval)
  {
    JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(retval));
    return JS_TRUE;
  }

  return JS_FALSE;
}

/** Implements ByteString.length getter */
static JSBool ByteString_getLength(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  return byteThing_getLength(cx, obj, id, vp, "ByteString");
}

/**
 *  Initialize the Window class prototype.
 *
 *  @param	cx	Valid JS Context
 *  @param	obj	The Global Object
 *
 *  @returns	Window.prototype
 */
JSObject *ByteString_InitClass(JSContext *cx, JSObject *obj, JSObject *parentProto)
{
/** Description of this class: */
  static JSClass byteString_class = 
  {
    GPSEE_CLASS_NAME(ByteString),		/**< its name is ByteString */
    JSCLASS_HAS_PRIVATE /* JSCLASS_SHARE_ALL_PROPERTIES breaks methods?! */,			
    /**< 
      - Instances have private storage
      - mark API is used for js >= 1.8 GC tracing 
      - All properties are JSPROP_SHARED
      */
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
    JS_FN("toString",		ByteString_toString,		0, 0),
    JS_FN("decodeToString",	ByteString_decodeToString,	0, 0),
    JS_FN("indexOf",		ByteString_indexOf,		0, 0),
    JS_FN("lastIndexOf",	ByteString_lastIndexOf,		0, 0),
    JS_FN("charAt",		ByteString_charAt,		0, 0),
    JS_FN("byteAt",		ByteString_byteAt,		0, 0),
    JS_FN("charCodeAt",		ByteString_byteAt,		0, 0),
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

  JSObject *proto =
      JS_InitClass(cx, 			/* JS context from which to derive runtime information */
		   obj, 		/* Object to use for initializing class (constructor arg?) */
		   parentProto,		/* parent_proto - Prototype object for the class */
 		   &byteString_class,	/* clasp - Class struct to init. Defs class for use by other API funs */
		   ByteString,	/* constructor function - Scope matches obj */
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

