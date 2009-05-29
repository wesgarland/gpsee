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
 *  @file	io_ByteArray.c 	A class for implementing Byte Arrays
 *  @author	Wes Garland
 *              PageMail, Inc.
 *		wes@page.ca
 *  @date	Jan 2008
 *  @version	$Id: ByteArray.c,v 1.1 2009/05/27 04:51:44 wes Exp $
 */

static const char __attribute__((unused)) rcsid[]="$Id: ByteArray.c,v 1.1 2009/05/27 04:51:44 wes Exp $";
#include "gpsee.h"
#include "binary_module.h"

static void	ByteArray_Finalize(JSContext *cx, JSObject *obj);
static JSBool	ByteArray_getProperty(JSContext *cx, JSObject *obj, jsval idval, jsval *vp);
static JSBool	ByteArray_setProperty(JSContext *cx, JSObject *obj, jsval idval, jsval *vp);
static void 	ByteArray_gcTrace(JSTracer *trc, JSObject *obj);

#define CLASS_ID  MODULE_ID ".byteArray"

typedef struct
{
  size_t		length;		/**< Number of characters in buffer */
  unsigned char		*buffer;	/**< Backing store */
} byteArray_handle_t;

byteArray_handle_t *byteArray_newHandle(JSContext *cx)
{
  byteArray_handle_t	*hnd;

  hnd = JS_malloc(cx, sizeof(*hnd));
  if (!hnd)
    return NULL;

  memset(hnd, 0, sizeof(*hnd));

  return hnd;
}

/**
 *  Create a bare-bonesByteArray instance out of an array of chars.
 *
 *  @param	cx		JavaScript context
 *  @param	buffer		The raw contents of the byte array; if buffer is NULL we initialize to all-zeroes
 *  @param	length		The length of the data in buf.
 *  @param	obj		New object of correct class to decorate, or NULL. If non-NULL, must match clasp.
 *  @param	stealBuffer	If non-zero, we steal the buffer rather than allocating and copying our own
 *
 *  @returns	NULL on OOM, otherwise a new ByteArray.
 */
JSObject *byteArray_fromCArray(JSContext *cx, const unsigned char *buffer, size_t length, JSObject *obj, int stealBuffer)
{
  byteArray_handle_t	*hnd = byteArray_newHandle(cx);
  
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

  GPSEE_ASSERT(!obj || (JS_GET_CLASS(cx, obj) == byteArray_clasp));

  if (!obj)
    obj = JS_NewObject(cx, byteArray_clasp, NULL, NULL);

  if (obj)
    JS_SetPrivate(cx, obj, hnd);

  return obj;
}

static JSBool ByteArray_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  byteArray_handle_t	*hnd = JS_GetInstancePrivate(cx, obj, byteArray_clasp, NULL);
  char			*ch;

  if (!hnd)
    return gpsee_throw(cx, CLASS_ID ".getProperty.invalid: property getter applied the wrong object type");

  if (JSVAL_IS_INT(id))
  {
    int index = JSVAL_TO_INT(id);

    if (index < 0)
      return gpsee_throw(cx, CLASS_ID ".getProperty.range.negative");
     
     if (index >= hnd->length)
      return gpsee_throw(cx, CLASS_ID ".getProperty.range.size: access beyond length property is prohibited.");

    ch = hnd->buffer + index;
  }
  else
  {
    jsdouble index;

    if (index < 0)
      return gpsee_throw(cx, CLASS_ID ".getProperty.range.negative");
     
    if (index >= hnd->length)
      return gpsee_throw(cx, CLASS_ID ".getProperty.range.size: access beyond length property is prohibited.");

    ch = hnd->buffer + (uint64)index;
  }

  *vp = OBJECT_TO_JSVAL(byteArray_fromCArray(cx, ch, 1, NULL, 0));
  return JS_TRUE;
}

static JSBool ByteArray_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  return JS_TRUE;	/* byteArrays are immutable */
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
static JSBool ByteArray(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  JSObject 	*instance;
  unsigned char	*buffer;
  size_t	length;
  int		stealBuffer = 0;

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

  if ((argc == 1) && JSVAL_IS_OBJECT(argv[0]))
  {
    JSObject	*o = JSVAL_TO_OBJECT(argv[0]);
    void 	*c = o ? JS_GET_CLASS(cx, o) : NULL;

    if ((c == (void *)byteArray_clasp) || (c == (void *)byteArray_clasp))
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
      stealBuffer = 1;	/* decodeString_toByteArray allocates a buffer already */
    }

    JS_LeaveLocalRootScope(cx);

    if (b == JS_TRUE)
      goto instanciate;
  }

  return gpsee_throw(cx, CLASS_ID ".constructor.arguments: invalid kind or number of arguments");

  instanciate:
  instance = byteArray_fromCArray(cx, buffer, length, obj, stealBuffer);
  if (!instance)
  {
    JS_ReportOutOfMemory(cx);
    return JS_FALSE;
  }

  *rval = OBJECT_TO_JSVAL(instance);
  return JS_TRUE;
}  

/**
 *  ByteArray trace operator. This function is called during
 *  JS garbage collection and marks the cached, converted 
 *  JSString as "in use".
 */
static void ByteArray_gcTrace(JSTracer *trc, JSObject *obj)
{
  /* byteArray_handle_t	*hnd = JS_GetPrivate(trc->context, obj); */

  /* JS_CallTracer(trc, hnd->decoded, JSTRACE_STRING); */

  return;
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
JSObject *ByteArray_InitClass(JSContext *cx, JSObject *obj, JSObject *parentProto)
{
/** Description of this class: */
  static JSClass byteArray_class = 
  {
    GPSEE_CLASS_NAME(ByteArray),		/**< its name is ByteArray */
    JSCLASS_HAS_PRIVATE | JSCLASS_MARK_IS_TRACE | JSCLASS_SHARE_ALL_PROPERTIES,			
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
    NULL, /*JS_CLASS_TRACE(ByteArray_gcTrace),*/	/**< GC Trace calls ByteArray_GCTrace */
    NULL				/**< reserveSlots */
  };

  static JSFunctionSpec instance_methods[] = 
  {
    JS_FS_END
  };

  static JSPropertySpec instance_props[] = 
  {
    { NULL, 0, 0, NULL, NULL }
  };

  JSObject *proto = 
      JS_InitClass(cx, 			/* JS context from which to derive runtime information */
		   obj, 		/* Object to use for initializing class (constructor arg?) */
		   NULL, 		/* parent_proto - Prototype object for the class */
 		   &byteArray_class,	/* clasp - Class struct to init. Defs class for use by other API funs */
		   ByteArray,		/* constructor function - Scope matches obj */
		   0,			/* nargs - Number of arguments for constructor (can be MAXARGS) */
		   instance_props,	/* ps - props struct for parent_proto */
		   instance_methods, 	/* fs - functions struct for parent_proto (normal "this" methods) */
		   NULL,		/* static_ps - props struct for constructor */
		   NULL); 		/* static_fs - funcs struct for constructor (methods like Math.Abs()) */

  GPSEE_ASSERT(proto);

  byteArray_clasp = &byteArray_class;

  return proto;
}

GPSEE_STATIC_ASSERT(offsetOf(byteThing_handle_t, length) == offsetOf(byteArray_handle_t, length));
GPSEE_STATIC_ASSERT(offsetOf(byteThing_handle_t, buffer) == offsetOf(byteArray_handle_t, buffer));
