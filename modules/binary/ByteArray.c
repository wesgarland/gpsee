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
 *  @file	ByteArray.c 	A class for implementing Byte Arrays
 *  @author	Wes Garland
 *              PageMail, Inc.
 *		wes@page.ca
 *  @date	Jan 2008
 *  @version	$Id: ByteArray.c,v 1.8 2010/12/02 21:59:42 wes Exp $
 */

static const char __attribute__((unused)) rcsid[]="$Id: ByteArray.c,v 1.8 2010/12/02 21:59:42 wes Exp $";
#include "gpsee.h"
#include "binary.h"

static void	ByteArray_Finalize(JSContext *cx, JSObject *obj);
static JSBool	ByteArray_getProperty(JSContext *cx, JSObject *obj, jsval idval, jsval *vp);
static JSBool	ByteArray_setProperty(JSContext *cx, JSObject *obj, jsval idval, jsval *vp);
static JSBool 	byteArray_requestSize(JSContext *cx, byteArray_handle_t *hnd, size_t newSize);
static JSBool 	byteArray_append(JSContext *cx, uintN argc, jsval *vp, const char * methodName);
static JSBool 	byteArray_prepend(JSContext *cx, uintN argc, jsval *vp, const char * methodName);

JSObject *byteArray_proto;
#define CLASS_ID  MODULE_ID ".ByteArray"

/** Returns a pointer to a byteArray_handle_t, or NULL on error
 *  @param      cx          Your JSContext
 *  @param      obj         ByteArray instance
 *  @param      methodName  Name of ByteArray member method (used for error reporting)
 *  @returns    A pointer to a byteArray_handle_t or NULL if an exception is thrown
 *  @throws     type
 */
inline byteArray_handle_t * byteArray_getHandle(JSContext *cx, JSObject *obj, const char const * methodName)
{
  byteArray_handle_t *hnd;
  hnd = JS_GetInstancePrivate(cx, obj, byteArray_clasp, NULL);
  if (!hnd)
  {
    JSClass *oclasp = JS_GET_CLASS(cx, obj);
    gpsee_throw(cx, CLASS_ID ".%s.type: cannot apply this method to %s-type object", methodName, oclasp?oclasp->name:"NOCLASS");
    return NULL;
  }
  return hnd;
}

/** Default ByteArray property getter.  Used to implement to implement Array-like property lookup ([]) */
static JSBool ByteArray_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  return byteThing_getProperty(cx, obj, id, vp, byteArray_clasp);
}
/** Implements ByteString.length getter */
static JSBool ByteArray_getLength(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  return byteThing_getLength(cx, obj, byteArray_clasp, id, vp);
}
/** Implements ByteString.length setter */
static JSBool ByteArray_setLength(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  size_t oldSize;
  size_t size;
  byteArray_handle_t * hnd;
  const char * errmsg;

  /* Acquire our byteArray_handle_t */
  hnd = byteArray_getHandle(cx, obj, "length.set");
  if (!hnd)
    return JS_FALSE;

  /* Coerce assignment r-value to size_t */
  if ((errmsg=byteThing_val2size(cx, *vp, &size, "length.set")))
    return gpsee_throw(cx, CLASS_ID ".length.set: %s", errmsg);

  /* Resize our byte vector */
  oldSize = hnd->length;
  if (!byteArray_requestSize(cx, hnd, size))
    return JS_FALSE;
  hnd->length = size;

  /* Initialize bytes to zero */
  if (size > oldSize)
    memset(hnd->buffer + oldSize, 0, size - oldSize);

  return JS_TRUE;
}

static JSBool ByteArray_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  const char *errmsg;
  unsigned char theByte;
  size_t index;
  byteArray_handle_t * hnd;

  /* The ByteArray getter may also be applied to ByteArray.prototype. Returning JS_TRUE without modifying the value at
   * 'vp' will allow Javascript consumers to modify the properties of ByteArray.prototype. */
  if (obj == byteArray_proto)
    return JS_TRUE;

  /* The ByteArray getter may also be applied to a property name that is not an integer. We are unconcerned with these
   * on the native/JSAPI side. To give Javascript consumers free reign to assign arbitrary non-numeric properties to
   * an instance of ByteArray, we just return JS_TRUE as in the case of application to ByteArray.prototype. Thus, if
   * we cannot get a valid 'size_t' type from the property key, we pass through by returining JS_TRUE. */

  /* Coerce index argument and do bounds checking upon it */
  if (byteThing_val2size(cx, id, &index, "setProperty"))
    return JS_TRUE;

  /* Acquire our byteArray_handle_t */
  hnd = byteArray_getHandle(cx, obj, "setProperty");
  if (!hnd)
    return JS_FALSE;

  /* Bounds checking */
  if (index >= hnd->length)
    return gpsee_throw(cx, CLASS_ID ".setter.index.range: index "GPSEE_SIZET_FMT" >= length "GPSEE_SIZET_FMT, index, hnd->length);

  /* Convert assignment argument to byte value */
  if ((errmsg=byteThing_val2byte(cx, *vp, &theByte)))
    return gpsee_throw(cx, CLASS_ID ".setter.byte.invalid: %s", errmsg);

  /* Assign byte value and return! */
  hnd->buffer[index] = theByte;
  return JS_TRUE;
}

/** 
 *  Implements the ByteArray constructor.
 *  new ByteArray(file descriptor number, mode string, [buffered boolean])
 *
 *  @param	cx	JavaScript context
 *  @param	obj	Pre-allocated ByteArray object
 *  @param	argc	Number of arguments passed to constructor
 *  @param	argv	Arguments passed to constructor
 *  @param	rval	The new object returned to JavaScript
 *
 *  @returns 	JS_TRUE on success
 */
static JSBool ByteArray_Constructor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  JSObject 		*instance;
  unsigned char		*buffer;
  size_t		length;
  int			stealBuffer = 0;

  /* ByteArray() called as function. */   
  if (JS_IsConstructing(cx) != JS_TRUE)
    return gpsee_throw(cx, CLASS_ID ".constructor.notFunction: Cannot call constructor as a function!");

  if (argc == 0)
  {
    /* Construct an empty byte array */
    length = 0;
    buffer = NULL;
    goto instanciate;
  }

  if (argc == 1)
  {
    jsdouble d;

    if (JSVAL_IS_OBJECT(argv[0]))
    {
      JSObject	*o = JSVAL_TO_OBJECT(argv[0]);
      void 	*c = o ? JS_GET_CLASS(cx, o) : NULL;

      if ((c == (void *)byteArray_clasp) || (c == (void *)byteString_clasp))
      {
        byteThing_handle_t *h = JS_GetPrivate(cx, o);

        buffer = (unsigned char *)h->buffer;
        length = h->length;
        goto instanciate;
      }

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

    else if (JS_ValueToNumber(cx, argv[0], &d) && d == (size_t)d)
    {
      length = (size_t)d;
      buffer = NULL;
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
      stealBuffer = 1;	/* decodeString_toByteArray allocates a buffer already */
    }

    JS_LeaveLocalRootScope(cx);

    if (b == JS_TRUE)
      goto instanciate;
  }

  return gpsee_throw(cx, CLASS_ID ".constructor.arguments: invalid kind or number of arguments");

  instanciate:
  instance = byteThing_fromCArray(cx, buffer, length, obj,
                                  byteArray_clasp, byteArray_proto, sizeof(byteArray_handle_t), stealBuffer);
  if (!instance)
  {
    JS_ReportOutOfMemory(cx);
    return JS_FALSE;
  }

  *rval = OBJECT_TO_JSVAL(instance);
  return JS_TRUE;
}  

/** 
 *  ByteArray Finalizer.
 *
 *  Called by the garbage collector, this routine release all resources
 *  used by an instance of the class when the object is collected: the
 *  buffered stream, the handle's memory, etc. I also closes the
 *  underlying file descriptor.
 *
 *  @param	cx	JavaScript context
 *  @param	obj	The object to finalize
 */
static void ByteArray_Finalize(JSContext *cx, JSObject *obj)
{
  byteArray_handle_t	*hnd = JS_GetPrivate(cx, obj);

  if (!hnd)
    return;

  if (hnd->buffer && (obj == hnd->memoryOwner))
    JS_free(cx, hnd->buffer);

  JS_free(cx, hnd);

  return;
}

static JSBool ByteArray_Cast(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return byteThing_Cast(cx, argc, argv, rval, byteArray_clasp, byteArray_proto, sizeof(byteArray_handle_t), CLASS_ID);
}

static JSBool ByteArray(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  /* ByteArray() called as function. */   
  if (JS_IsConstructing(cx) != JS_TRUE)
    return ByteArray_Cast(cx, obj, argc, argv, rval);
  else
    return ByteArray_Constructor(cx, obj, argc, argv, rval);
}

/** Resize a ByteArray
 *  @param    cx
 *  @param    hnd         a byteArray_handle_t
 *  @param    newSize     A number that is greater than or equal to the number of bytes you wish
 *                        to make available in hnd->buffer.
 *  @returns  JS_FALSE on OOM */
static JSBool byteArray_requestSize(JSContext *cx, byteArray_handle_t *hnd, size_t newSize)
{
  if (hnd->capacity < newSize)
  {
    size_t roundUp = 16;
    while (roundUp < newSize)
      roundUp <<= 1;

    hnd->buffer = JS_realloc(cx, hnd->buffer, roundUp);
    if (!hnd->buffer)
    {
      hnd->capacity = 0;
      JS_ReportOutOfMemory(cx);
      return JS_FALSE;
    }
    hnd->capacity = roundUp;
  }
  return JS_TRUE;
}

/** Implements ByteArray.toByteArray() */
static JSBool ByteArray_toByteArray(JSContext *cx, uintN argc, jsval *vp)
{
  return byteThing_toByteThing(cx, argc, vp, byteArray_clasp, byteArray_proto, sizeof(byteArray_handle_t), CLASS_ID ".toByteArray");
}
/** Implements ByteArray.toByteString() */
static JSBool ByteArray_toByteString(JSContext *cx, uintN argc, jsval *vp)
{
  return byteThing_toByteThing(cx, argc, vp, byteString_clasp, byteString_proto, sizeof(byteString_handle_t), CLASS_ID ".toByteString");
}
/** Implements ByteArray.decodeToString() */
static JSBool ByteArray_decodeToString(JSContext *cx, uintN argc, jsval *vp)
{
  return byteThing_decodeToString(cx, argc, vp, byteArray_clasp);
}
/** Implements ByteArray.toArray()
 *
 *  TODO this should really be ByteThing stuff, since ByteString.toArray() is identical.
 *  TODO support charset argument.
 */
static JSBool ByteArray_toArray(JSContext *cx, uintN argc, jsval *vp)
{
  byteArray_handle_t *hnd;
  JSObject *retval;

  /* Acquire our byteArray_handle_t */
  hnd = byteArray_getHandle(cx, JS_THIS_OBJECT(cx, vp), "toArray");
  if (!hnd)
    return JS_FALSE;

  /* Convert our ByteArray to a Javascript Array of Number values */
  retval = byteThing_toArray(cx, hnd->buffer, hnd->length);
  if (!retval)
    return JS_FALSE;
  
  /* Return the value */
  JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(retval));
  return JS_TRUE;
}
/** Implements ByteArray.toSource() */
static JSBool ByteArray_toSource(JSContext *cx, uintN argc, jsval *vp)
{
  return byteThing_toSource(cx, argc, vp, byteArray_clasp);
}
/** Implements ByteArray.reverse() */
static JSBool ByteArray_reverse(JSContext *cx, uintN argc, jsval *vp)
{
  size_t i, l, len;
  byteArray_handle_t * hnd;
  unsigned char * buf;
  hnd = byteArray_getHandle(cx, JS_THIS_OBJECT(cx, vp), "reverse");
  if (!hnd)
    return JS_FALSE;
  buf = hnd->buffer;
  len = hnd->length;
  for(i=0, l=len/2; i<l; i++)
  {
    unsigned char temp = buf[i];
    buf[i] = buf[len-i-1];
    buf[len-i-1] = temp;
  }
  return JS_TRUE;
}
/** Implements ByteArray.get() */
static JSBool ByteArray_get(JSContext *cx, uintN argc, jsval *vp)
{
  return byteThing_byarAt(cx, argc, vp, byteArray_clasp, NULL, "ByteArray.get");
}
/** Implements ByteArray.byteAt() */
static JSBool ByteArray_byteAt(JSContext *cx, uintN argc, jsval *vp)
{
  return byteThing_byarAt(cx, argc, vp, byteArray_clasp, byteString_clasp, "ByteArray.byteAt");
}
/** Implements ByteArray.concat() */
static JSBool ByteArray_concat(JSContext *cx, uintN argc, jsval *vp)
{
  return byteArray_append(cx, argc, vp, "concat");
}
/** Implements ByteArray.extendRight() and ByteArray.concat() */
static JSBool byteArray_append(JSContext *cx, uintN argc, jsval *vp, const char * methodName)
{
  /* Results from byteThing_val2bytes() */
  unsigned char *       buffer      = NULL;
  unsigned char *       oldBuffer   = NULL;
  size_t                len;
  JSBool                stealBuffer = JS_FALSE;
  jsval *               argv        = JS_ARGV(cx, vp);
  byteArray_handle_t *  hnd;

  /* Acquire our byteArray_handle_t */
  hnd = byteArray_getHandle(cx, JS_THIS_OBJECT(cx, vp), methodName);
  if (!hnd)
    return JS_FALSE;

  /* Convert argument vector  */
  if (!byteThing_val2bytes(cx, argv, argc, &buffer, &len, &stealBuffer, byteArray_clasp, methodName))
    return JS_FALSE;

  /* Easy case */
  if (len == 0)
    return JS_TRUE;

  /* Reallocate if necessary */
  /* Save a pointer to our internal buffer for pointer comparison later */
  oldBuffer = hnd->buffer;
  if (!byteArray_requestSize(cx, hnd, hnd->length + len))
    return JS_FALSE;

  /* Watch out for the case of myByteArray.concat(myByteArray) */
  if (oldBuffer == buffer)
    /* memcpy() any faster than memmove()? */
    memcpy(hnd->buffer + len, hnd->buffer, len);
  else
  {
    /* Copy new contents into our ByteArray's buffer at the beginning */
    memcpy(hnd->buffer + hnd->length, buffer, len);

    /* Free memory if necessary */
    if (stealBuffer)
      JS_free(cx, buffer);
  }

  /* Update our ByteArray's length */
  hnd->length += len;

  return JS_TRUE;
}
/** Implements ByteArray.pop() */
static JSBool ByteArray_pop(JSContext *cx, uintN argc, jsval *vp)
{
  byteArray_handle_t *  hnd;
  unsigned char         retval;

  hnd = byteArray_getHandle(cx, JS_THIS_OBJECT(cx, vp), "pop");
  if (!hnd)
    return JS_FALSE;

  /* Return a Number to the Javascript caller */
  retval = hnd->buffer[hnd->length-1];

  /* Resize buffer to "pop" the return value off the top */;
  if (!byteArray_requestSize(cx, hnd, hnd->length-1))
    return JS_FALSE;
  hnd->length--;

  /* Return Number */
  JS_SET_RVAL(cx, vp, INT_TO_JSVAL(retval));
  return JS_TRUE;
}
/** Implements ByteArray.push() */
static JSBool ByteArray_push(JSContext *cx, uintN argc, jsval *vp)
{
  uintN i;
  byteArray_handle_t * hnd;
  jsval * argv = JS_ARGV(cx, vp);

  hnd = byteArray_getHandle(cx, JS_THIS_OBJECT(cx, vp), "push");
  if (!hnd)
    return JS_FALSE;

  /* Resize our byte vector */
  if (!byteArray_requestSize(cx, hnd, hnd->length + argc))
    return JS_FALSE;

  /* Start appending things! */
  for (i=0; i<argc; i++)
  {
    jsdouble d;
    if (!JS_ValueToNumber(cx, argv[i], &d))
      return gpsee_throw(cx, CLASS_ID ".push.argument.%d.invalid: invalid byte value!", i);
    if (d != (unsigned char)d)
      return gpsee_throw(cx, CLASS_ID ".push.argument.%d.invalid: %lf is not a valid byte value!", i, d);
    hnd->buffer[hnd->length++] = (unsigned char)d;
  }

  return JS_TRUE;
}
/** Implements ByteArray.extendLeft() */
static JSBool ByteArray_extendLeft(JSContext *cx, uintN argc, jsval *vp)
{
  return byteArray_prepend(cx, argc, vp, "extendLeft");
}
/** Implements ByteArray.extendLeft() and ByteArray.unshift() member methods */
static JSBool byteArray_prepend(JSContext *cx, uintN argc, jsval *vp, const char * methodName)
{
  /* Results from byteThing_val2bytes() */
  unsigned char *       buffer      = NULL;
  unsigned char *       oldBuffer   = NULL;
  size_t                len;
  JSBool                stealBuffer = JS_FALSE;
  jsval *               argv        = JS_ARGV(cx, vp);
  byteArray_handle_t *  hnd;

  /* Acquire our byteArray_handle_t */
  hnd = byteArray_getHandle(cx, JS_THIS_OBJECT(cx, vp), "toArray");
  if (!hnd)
    return JS_FALSE;

  /* Convert argument vector  */
  if (!byteThing_val2bytes(cx, argv, argc, &buffer, &len, &stealBuffer, byteArray_clasp, methodName))
    return JS_FALSE;

  /* Easy case */
  if (len == 0)
    return JS_TRUE;

  /* Reallocate if necessary */
  /* Save a pointer to our internal buffer for pointer comparison later */
  oldBuffer = hnd->buffer;
  if (!byteArray_requestSize(cx, hnd, hnd->length + len))
    return JS_FALSE;

  /* Watch out for the case of myByteArray.unshift(myByteArray) */
  if (oldBuffer == buffer)
    /* memcpy() any faster than memmove()? */
    memcpy(hnd->buffer + len, hnd->buffer, hnd->length);
  else
  {
    /* Slide original contents toward the end */
    memmove(hnd->buffer + len, hnd->buffer, hnd->length);
    /* Copy new contents into our ByteArray's buffer at the beginning */
    memcpy(hnd->buffer, buffer, len);

    /* Free memory if necessary */
    if (stealBuffer)
      JS_free(cx, buffer);
  }

  /* Update our ByteArray's length */
  hnd->length += len;

  return JS_TRUE;
}
/** Implements ByteArray.extendRight() */
static JSBool ByteArray_extendRight(JSContext *cx, uintN argc, jsval *vp)
{
  return byteArray_append(cx, argc, vp, "extendRight");
}
/** Implements ByteArray.shift() */
static JSBool ByteArray_shift(JSContext *cx, uintN argc, jsval *vp)
{
  byteArray_handle_t * hnd;
  unsigned char theByte;

  /* Acquire our byteArray_handle_t */
  hnd = byteArray_getHandle(cx, JS_THIS_OBJECT(cx, vp), "toArray");
  if (!hnd)
    return JS_FALSE;

  /* If the ByteArray is empty, return undefined. This behavior corresponds with Array.shift() */
  if (!hnd->length)
  {
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
  }

  /* Save the return value */
  theByte = hnd->buffer[0];

  /* Shift one member off the low end */
  memcpy(hnd->buffer, hnd->buffer+1, hnd->length-1);

  /* Resize byte vector, reallocating if necessary */
  if (!byteArray_requestSize(cx, hnd, hnd->length - 1))
    return JS_FALSE;
  hnd->length--;

  /* Return byte */
  JS_SET_RVAL(cx, vp, INT_TO_JSVAL(theByte));
  return JS_TRUE;
}
/** Implements ByteArray.unshift() */
static JSBool ByteArray_unshift(JSContext *cx, uintN argc, jsval *vp)
{
  return byteArray_prepend(cx, argc, vp, "unshift");
}
static int compare_bytes(const void *a, const void*b);
/** Implements ByteArray.sort() */
static JSBool ByteArray_sort(JSContext *cx, uintN argc, jsval *vp)
{
  byteArray_handle_t * hnd;

  /* Acquire a pointer to our internal bytestring data */
  hnd = byteArray_getHandle(cx, JS_THIS_OBJECT(cx, vp), "sort");
  if (!hnd)
    return JS_FALSE;

  /* Quick-sort! */
  qsort(hnd->buffer, hnd->length, 1, compare_bytes);

  return JS_TRUE;
}
static int compare_bytes(const void *a, const void*b)
{
  return *(const unsigned char*)a - *(const unsigned char*)b;
}
/** Implemented both ByteArray.slice() and ByteArray.splice() instance methods */
static JSBool ByteArray_lice(JSContext *cx, uintN argc, jsval *vp, JSBool splice, const char *methodName)
{
  /* This code is pretty much identical to ByteString_substring() */
  byteArray_handle_t *  hnd;
  ssize_t               start, end, size;
  JSObject *            retval;
  
  /* Acquire a pointer to our internal bytestring data */
  hnd = byteArray_getHandle(cx, JS_THIS_OBJECT(cx, vp), methodName);
  if (!hnd)
    return JS_FALSE;
  size = hnd->length;

  /* @function
   * @name    ByteArray.splice
   * @param   start
   * @param   end       optional
   * @instancemethod
   */
  /* @function
   * @name    ByteArray.slice
   * @param   start
   * @param   end       optional
   * @instancemethod
   */
  /* Get Javascript arguments */
  if (!byteThing_arg2ssize(cx, argc, vp, &start, 0, -size, size-1, JS_FALSE, 0, byteArray_clasp, methodName))
    return JS_FALSE;

  if (!byteThing_arg2ssize(cx, argc, vp, &end,   1, -size, size, JS_TRUE, hnd->length, byteArray_clasp, methodName))
    return JS_FALSE;

  /* Negative indexes are relative to the end of ByteArray
   * This behavior corresponds with Array.slice() */
  if (start < 0)
    start = hnd->length - start;

  if (end < 0)
    end = hnd->length - end;

  /* Unlike String.substring(), Array.slice() does not appear to flip these indexes when they're reversed */
  if (end < start)
  {
    end = start;
    /* Instantiate an empty ByteArray for the return value */
    retval = byteThing_fromCArray(cx, NULL, 0, NULL,
                                  byteArray_clasp, byteArray_proto, sizeof(byteArray_handle_t), 0);
  }
  /* Instantiate a new ByteArray from a subsection of the buffer */
  else
    retval = byteThing_fromCArray(cx, hnd->buffer + start, end - start, NULL,
                                  byteArray_clasp, byteArray_proto, sizeof(byteArray_handle_t), 0);

  /* Failure; pass downstream exceptions up the call stack */
  if (!retval)
    return JS_FALSE;

  /* Do we need to remove the chunk we're returning? */
  if (splice && start != end)
  {
    size_t newSize = hnd->length - (end - start);
    memcpy(hnd->buffer + start, hnd->buffer + end, hnd->length - end);
    byteArray_requestSize(cx, hnd, newSize);
    hnd->length = newSize;
  }

  /* Return new ByteArray */
  JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(retval));
  return JS_TRUE;
}
/** Implements ByteArray.slice() */
static JSBool ByteArray_slice(JSContext *cx, uintN argc, jsval *vp)
{
  return ByteArray_lice(cx, argc, vp, JS_FALSE, "slice");
}
/** Implements ByteArray.splice() */
static JSBool ByteArray_splice(JSContext *cx, uintN argc, jsval *vp)
{
  return ByteArray_lice(cx, argc, vp, JS_TRUE, "splice");
}
/** Implements ByteArray.displace() */
static JSBool ByteArray_displace(JSContext *cx, uintN argc, jsval *vp)
{
  return gpsee_throw(cx, CLASS_ID "displace.unimplemented: this method is not yet implemented!");
}
/** Implements ByteArray.indexOf() */
static JSBool ByteArray_indexOf(JSContext *cx, uintN argc, jsval *vp)
{
  return byteThing_findChar(cx, argc, vp, memchr, "indexOf", byteArray_clasp);
}
/** Implements ByteArray.lastIndexOf() */
static JSBool ByteArray_lastIndexOf(JSContext *cx, uintN argc, jsval *vp)
{
  return byteThing_findChar(cx, argc, vp, memrchr, "lastIndexOf", byteArray_clasp);
}
/** ByteArray_xintAt() implements the ByteArray member method xintAt() */
JSBool ByteArray_xintAt(JSContext *cx, uintN argc, jsval *vp)
{
  return byteThing_intAt(cx, argc, vp, CLASS_ID);
}

/** Initializes binary.ByteArray */
JSObject *ByteArray_InitClass(JSContext *cx, JSObject *obj, JSObject *parentProto)
{
/** Description of this class: */
  static JSClass byteArray_class = 
  {
    GPSEE_CLASS_NAME(ByteArray),		/**< its name is ByteArray */
    JSCLASS_HAS_PRIVATE /* JSCLASS_SHARE_ALL_PROPERTIES breaks methods?! */,			
    //JSCLASS_HAS_PRIVATE | JSCLASS_MARK_IS_TRACE | JSCLASS_SHARE_ALL_PROPERTIES,			
    /**< 
      - Instances have private storage
      - mark API is used for js >= 1.8 GC tracing 
      - All properties are JSPROP_SHARED
      */
    JS_PropertyStub,  			/**< addProperty stub */
    JS_PropertyStub,  			/**< deleteProperty stub */
    ByteArray_getProperty,		/**< custom getProperty */
    ByteArray_setProperty,		/**< custom setProperty */
    JS_EnumerateStub, 			/**< enumerateProperty stub */
    JS_ResolveStub,   			/**< resolveProperty stub */
    JS_ConvertStub,   			/**< convertProperty stub */
    ByteArray_Finalize,			/**< it has a custom finalizer */

    /* Optional members below instead of JSCLASS_NO_OPTIONAL_MEMBERS */
    NULL,				/**< getObjectOps */
    NULL,				/**< checkAccess */
    NULL,				/**< call */
    NULL,				/**< construct */
    NULL,				/**< xdrObject */
    NULL,				/**< hasInstance */
    NULL,               /**< GC Trace */
    NULL				/**< reserveSlots */
  };

  static JSFunctionSpec instance_methods[] = 
  {
    JS_FN("toByteString",         ByteArray_toByteString,   0, 0),
    JS_FN("toByteArray",          ByteArray_toByteArray,    0, 0),
    JS_FN("toArray",              ByteArray_toArray,        0, 0),
    JS_FN("toString",             byteThing_toString,       0, 0),
    JS_FN("toSource",             ByteArray_toSource,       0, 0),
    JS_FN("decodeToString",       ByteArray_decodeToString, 0, 0),
    JS_FN("reverse",              ByteArray_reverse,        0, 0),
    JS_FN("byteAt",               ByteArray_byteAt,         0, 0),
    JS_FN("xintAt",               ByteArray_xintAt,         0, 0),
    JS_FN("get",                  ByteArray_get,            0, 0),
    JS_FN("concat",               ByteArray_concat,         0, 0),
    JS_FN("pop",                  ByteArray_pop,            0, 0),
    JS_FN("push",                 ByteArray_push,           0, 0),
    JS_FN("extendLeft",           ByteArray_extendLeft,     0, 0),
    JS_FN("extendRight",          ByteArray_extendRight,    0, 0),
    JS_FN("shift",                ByteArray_shift,          0, 0),
    JS_FN("unshift",              ByteArray_unshift,        0, 0),
    JS_FN("sort",                 ByteArray_sort,           0, 0),
    JS_FN("slice",                ByteArray_slice,          0, 0),
    JS_FN("splice",               ByteArray_splice,         0, 0),
    JS_FN("displace",             ByteArray_displace,       0, 0),
    JS_FN("indexOf",              ByteArray_indexOf,        0, 0),
    JS_FN("lastIndexOf",          ByteArray_lastIndexOf,    0, 0),
    JS_FS_END
  };

  static JSPropertySpec instance_props[] = 
  {
    { "length", 0, 0, ByteArray_getLength, ByteArray_setLength },
    { NULL, 0, 0, NULL, NULL }
  };

  JSObject *proto;

  GPSEE_DECLARE_BYTETHING_CLASS(byteArray);

  proto = 
      JS_InitClass(cx, 			/* JS context from which to derive runtime information */
		   obj, 		/* Object to use for initializing class (constructor arg?) */
		   parentProto,         /* parent_proto - parent class (ByteArray.__proto__) */
 		   &byteArray_class,	/* clasp - Class struct to init. Defs class for use by other API funs */
		   ByteArray,		/* constructor function - Scope matches obj */
		   0,			/* nargs - Number of arguments for constructor (can be MAXARGS) */
		   instance_props,	/* ps - props struct for parent_proto */
		   instance_methods, 	/* fs - functions struct for parent_proto (normal "this" methods) */
		   NULL,		/* static_ps - props struct for constructor */
		   NULL); 		/* static_fs - funcs struct for constructor (methods like Math.Abs()) */

  GPSEE_ASSERT(proto);

  byteArray_clasp = &byteArray_class;
  byteArray_proto = proto;

  return proto;
}

GPSEE_STATIC_ASSERT(offsetOf(byteThing_handle_t, length) == offsetOf(byteArray_handle_t, length));
GPSEE_STATIC_ASSERT(offsetOf(byteThing_handle_t, buffer) == offsetOf(byteArray_handle_t, buffer));
