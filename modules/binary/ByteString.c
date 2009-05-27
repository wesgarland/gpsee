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
 *  @version	$Id: ByteString.c,v 1.1 2009/05/27 04:51:45 wes Exp $
 *
 *  Based on https://wiki.mozilla.org/ServerJS/Binary/B
 *  Extensions:
 *  - Missing or falsy charset in constructor means to inflate/deflate
 */

static const char __attribute__((unused)) rcsid[]="$Id: ByteString.c,v 1.1 2009/05/27 04:51:45 wes Exp $";
#include "gpsee.h"
#include "binary_module.h"

static JSObject *byteString_proto;
#define CLASS_ID MODULE_ID ".ByteString"

/** Internal data structure backing a ByteString. Cast-compatible with byteThing_handle_t. */
typedef struct
{
  size_t		length;		/**< Number of characters in buffer */
  unsigned char		*buffer;	/**< Backing store */
} byteString_handle_t;

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
  byteString_handle_t	*hnd = byteString_newHandle(cx);
  
  if (!hnd)
    return NULL;

  if (stealBuffer)
  {
    hnd->buffer = (unsigned char *)buffer;
    hnd->length = length;
  }
  else
  {
    if (length)
    {
      hnd->buffer = JS_malloc(cx, length);
      hnd->length = length;

      if (!hnd->buffer)
	return NULL;
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

static JSBool byteString_findChar(JSContext *cx, uintN argc, jsval *vp, void *memchr_fn(const void *, int, size_t), const char *throwPrefix)
{
  byteString_handle_t	*hnd = JS_GetInstancePrivate(cx, JS_THIS_OBJECT(cx, vp), byteString_clasp, NULL);
  jsval			*argv = JS_ARGV(cx, vp);
  int			byte;
  jsval			bytev;
  jsdouble		start;
  jsdouble		stop;
  const unsigned char 	*found;

  switch(argc)
  {
    default:
      return gpsee_throw(cx, "%s.arguments.count", throwPrefix);
    case 3:
      stop  = argv[2];
      start = argv[1];
      bytev = argv[0];
      break;
    case 2:
      stop  = hnd->length;
      start = argv[1];
      bytev = argv[0];
    case 1:
      stop  = hnd->length;
      start = 0;
      bytev = argv[0];
  }

  if ((start < 0) || (stop  < 0))
    return gpsee_throw(cx, "%s.range.underflow", throwPrefix);

  if ((start > (size_t)start) || (stop > (size_t)stop))
    return gpsee_throw(cx, "%s.range.overflow", throwPrefix);

  if ((start > stop))
    return gpsee_throw(cx, "%s.range.negative", throwPrefix);

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
      return gpsee_throw(cx, "%s.arguments.0.length: ByteString or ByteArray must have length 1", throwPrefix);

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
      return gpsee_throw(cx, "%s.arguments.0.range", throwPrefix);
  }

  if ((byte < 0) || (byte > 255))
    return gpsee_throw(cx, "%s.arguments.0.range", throwPrefix);

  if (argc > 1 && ((size_t)start != start))
    return gpsee_throw(cx, "%s.arguments.1.range", throwPrefix);

  if (argc > 2 && ((size_t)stop != stop))
    return gpsee_throw(cx, "%s.arguments.2.range", throwPrefix);

  if ((found = memchr_fn(hnd->buffer + (size_t)start, byte, (size_t)stop)))
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
  byteString_handle_t	*hnd = JS_GetInstancePrivate(cx, JS_THIS_OBJECT(cx, vp), byteString_clasp, NULL);
  JSString		*s;
  unsigned char		*buf;
  size_t		length;
  const char		*sourceCharset;
  jsval			*argv = JS_ARGV(cx, vp);

  if (!hnd)
    return gpsee_throw(cx, CLASS_ID ".decodeToString.invalid: ByteString::decodeToString applied the wrong object type");

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

  snprintf(buf, sizeof(buf), "[object " CLASS_ID " %i]", hnd->length);
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

/** Implements ByteString::length getter.
 *
 *  @param	cx		JavaScript context
 *  @param	obj		A ByteString object
 *  @param	id		unused
 *  @param	vp		The length, on successful return.
 *  @returns	JS_TRUE on success; otherwise JS_FALSE and a pending exception is set.
 */
static JSBool ByteString_get_length(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  byteString_handle_t	*hnd = JS_GetInstancePrivate(cx, obj, byteString_clasp, NULL);
  if (!hnd)
    return gpsee_throw(cx, CLASS_ID ".length.get.invalid: property getter applied the wrong object type");

  if (((jsval)hnd->length == hnd->length) && INT_FITS_IN_JSVAL(hnd->length))
  {
    *vp = INT_TO_JSVAL(hnd->length);
    return JS_TRUE;
  }

  return JS_NewNumberValue(cx, hnd->length, vp);
}

/** Default ByteString property getter.  Used to implement to implement Array-like property lookup ([]) */
static JSBool ByteString_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  byteString_handle_t	*hnd = JS_GetInstancePrivate(cx, obj, byteString_clasp, NULL);
  char			*ch;

  if (!hnd)
    return gpsee_throw(cx, CLASS_ID ".getProperty.invalid: property getter applied the wrong object type");

  if (JSVAL_IS_INT(id))
  {
    int index = JSVAL_TO_INT(id);

    if (index < 0)
      return gpsee_throw(cx, CLASS_ID ".getProperty.range.underflow");
     
     if (index >= hnd->length)
      return gpsee_throw(cx, CLASS_ID ".getProperty.range.overflow");

    ch = hnd->buffer + index;
  }
  else
  {
    jsdouble index;

    if (index < 0)
      return gpsee_throw(cx, CLASS_ID ".getProperty.range.underflow");
     
    if (index >= hnd->length)
      return gpsee_throw(cx, CLASS_ID ".getProperty.range.overflow");

    ch = hnd->buffer + (uint64)index;
  }

  *vp = OBJECT_TO_JSVAL(byteString_fromCArray(cx, ch, 1, NULL, 0));
  return JS_TRUE;
}

JSBool ByteString_charAt(JSContext *cx, uintN argc, jsval *vp)
{
  jsval *argv = JS_ARGV(cx, vp);

  if (argc != 1)
    return gpsee_throw(cx, CLASS_ID ".byteAt.arguments.count");

  return ByteString_getProperty(cx, JS_THIS_OBJECT(cx, vp), argv[0], vp);
}

JSBool ByteString_byteAt(JSContext *cx, uintN argc, jsval *vp)
{
  jsval 		*argv = JS_ARGV(cx, vp);
  unsigned char		ch;
  byteString_handle_t	*hnd = JS_GetInstancePrivate(cx, JS_THIS_OBJECT(cx, vp), byteString_clasp, NULL);

  if (!hnd)
    return gpsee_throw(cx, CLASS_ID ".decodeToString.invalid: ByteString::decodeToString applied the wrong object type");

  if (argc != 1)
    return gpsee_throw(cx, CLASS_ID ".byteAt.arguments.count");

  if (JSVAL_IS_INT(argv[0]))
  {
    int index = JSVAL_TO_INT(argv[0]);

    if (index < 0)
      return gpsee_throw(cx, CLASS_ID ".byteAt.range.underflow");
     
     if (index >= hnd->length)
      return gpsee_throw(cx, CLASS_ID ".byteAt.range.overflow");

    ch = hnd->buffer[index];
  }
  else
  {
    jsdouble index;

    if (index < 0)
      return gpsee_throw(cx, CLASS_ID ".byteAt.range.underflow");
     
    if (index >= hnd->length)
      return gpsee_throw(cx, CLASS_ID ".byteAt.range.overflow");

    ch = hnd->buffer[(uint64)index];
  }

  JS_SET_RVAL(cx, vp, INT_TO_JSVAL(ch));
  return JS_TRUE;
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
    return gpsee_throw(cx, CLASS_ID ".constructor.notFunction: Cannot call constructor as a function!");

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

  if (!hnd)
    return;

  if (hnd->buffer)
    JS_free(cx, hnd);

  JS_free(cx, hnd);

  return;
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
    JS_FS_END
  };

  static JSPropertySpec instance_props[] = 
  {
    { "length",		0,	JSPROP_READONLY,	ByteString_get_length },
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
