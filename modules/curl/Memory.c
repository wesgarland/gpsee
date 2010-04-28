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
 *  @file   Memory.c        Implementation of the GPSEE gffi Memory class.
 *                  This class provides a handle and allocator for
 *                  heap; we also support void pointers by using
 *                  0-byte heap segments at specific addresses.
 *
 *  @author Wes Garland
 *              PageMail, Inc.
 *      wes@page.ca
 *  @date   Jul 2009
 *  @version    $Id: Memory.c,v 1.9 2009/09/22 04:38:38 wes Exp $
 */

#include "Memory.h"
#include <math.h>

JSClass *memory_clasp = NULL;
JSObject *memory_proto = NULL;

#define MODULE_ID GPSEE_GLOBAL_NAMESPACE_NAME ".module.ca.page.curl"
#define CLASS_ID MODULE_ID ".Memory"


/** Return a generic pointer back to JS for use in comparisons.
 *  Pointers in JavaScript are just hexadecimal numbers encoded with String.
 *  Can't do numbers / valueof because there are 2^64 pointers in 64-bit
 *  land and only 2^53 integer doubles.
 *
 *  ORIGINALLY IN gffi/util.c
 *
 *  @param      cx      JavaScript context to use
 *  @param      vp      Returned pointer for JS consumption
 *  @param      pointer Pointer to encode
 *  @returns    JS_FALSE if an exception has been thrown
 */
JSBool pointer_toString(JSContext *cx, void *pointer, jsval *vp)
{
  JSString              *str;
  char                  ptrbuf[1 + 2 + (sizeof(void *) * 2) + 1];       /* @ 0x hex_number NUL */

  snprintf(ptrbuf, sizeof(ptrbuf), "@" GPSEE_PTR_FMT, pointer);

  str = JS_NewStringCopyZ(cx, ptrbuf);
  if (!str)
    return JS_FALSE;

  *vp = STRING_TO_JSVAL(str);

  return JS_TRUE;
}

/**
 *  Implements Memory.prototype.asString -- a method to take a Memory object,
 *  treat it as a char * and be decode toString() following the regular JSAPI
 *  C Strings rules.
 *
 *  asString can take one argument: a length. If length is -1, use strlen;
 *  otherwise use at most length bytes. If length is not defined, we use either
 *     - buffer->length when !hnd->memoryOwner != this, or
 *     - strlen
 */
/* @jazzdoc gffi.Memory.asString()
 * This is an interface to JS_NewStringCopyN().
 *
 * @form (instance of Memory).asString()
 * This form of the asString() instance method infers the length of memory to be consumed, and the Javascript String returned,
 * on its own. It will use strlen() to accomplish this if it does not already "know" its own length.
 *
 * @form (instance of Memory).asString(-1)
 * Like the above form, but use of strlen() is forced.
 *
 * @form (instance of Memory).asString(length >= 0)
 * As the first form, but assuming the specified length.
 */
static JSBool memory_asString(JSContext *cx, uintN argc, jsval *vp)
{
  byteThing_handle_t   *hnd;
  JSObject      *obj = JS_THIS_OBJECT(cx, vp);
  JSString      *str;
  size_t        length;
  jsval         *argv = JS_ARGV(cx, vp);

  if (!obj)
    return JS_FALSE;

  hnd = JS_GetInstancePrivate(cx, obj, memory_clasp, NULL);
  if (!hnd)
    return JS_FALSE;

  if (!hnd->buffer)
    {
      *vp = JSVAL_NULL;
      return JS_TRUE;
    }

  length = hnd->length;

  str = JS_NewStringCopyN(cx, (char *)hnd->buffer, length);
  if (!str)
    return JS_FALSE;

  *vp = STRING_TO_JSVAL(str);
  return JS_TRUE;
}

/**
 *  Implements Memory.prototype.toString -- a method to take a Memory object,
 *  and return the address of the backing store encoded in a JavaScript String,
 *  so that JS programmers can compare pointers.
 */
static JSBool memory_toString(JSContext *cx, uintN argc, jsval *vp)
{
  byteThing_handle_t   *hnd;
  JSObject      *thisObj = JS_THIS_OBJECT(cx, vp);

  if (!thisObj)
    return JS_FALSE;

  hnd = JS_GetInstancePrivate(cx, thisObj, memory_clasp, NULL);
  if (!hnd)
    return JS_FALSE;

  return pointer_toString(cx, hnd->buffer, vp);
}


/**
 * Size of memory region in use, not necessary how much is allocated.
 */
static JSBool memory_size_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  byteThing_handle_t   *hnd = JS_GetInstancePrivate(cx, obj, memory_clasp, NULL);
  jsdouble      d;

  if (!hnd)
    return JS_FALSE;

  if (INT_FITS_IN_JSVAL(hnd->length))
    {
      *vp = INT_TO_JSVAL(hnd->length);
      return JS_TRUE;
    }

  d = hnd->length;
  if (hnd->length != d)
    return gpsee_throw(cx, CLASS_ID ".size.getter.overflow");

  return JS_NewNumberValue(cx, d, vp);
}

/**
 *  Implements the Memory constructor.
 *
 *  Constructor takes exactly one or two arguments:
 *   - number of bytes to allocate on heap.
 *   - whether the memory pointed to by this object is to
 *     be freed when the JS Object is finalzed.
 *
 *  This object may be used by C methods to keep handles on
 *  unknown amounts of memory. For this, simply construct the
 *  object with a 0 byte argument and set the pointer value
 *  by manipulating the data in the private slot.
 *
 *  @param  cx  JavaScript context
 *  @param  obj Pre-allocated Memory object
 *  @param  argc    Number of arguments passed to constructor
 *  @param  argv    Arguments passed to constructor
 *  @param  rval    The new object returned to JavaScript
 *
 *  @returns    JS_TRUE on success
 */
JSBool Memory_Constructor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  byteThing_handle_t   *hnd;

  if ((argc != 1) && (argc != 2))
    return gpsee_throw(cx, CLASS_ID ".arguments.count");

  *rval = OBJECT_TO_JSVAL(obj);

  hnd = JS_malloc(cx, sizeof(*hnd));
  if (!hnd)
    return JS_FALSE;

  /* cleanup now solely the job of the finalizer */
  memset(hnd, 0, sizeof(*hnd));
  JS_SetPrivate(cx, obj, hnd);

  if (JSVAL_IS_INT(argv[0]))
    {
      hnd->length = JSVAL_TO_INT(argv[0]);
    }
  else
    {
      jsdouble d;

      if (JS_ValueToNumber(cx, argv[0], &d) == JS_FALSE)
        return JS_FALSE;

      hnd->length = d;
      if (d != hnd->length)
        return gpsee_throw(cx, CLASS_ID ".constructor.size: %1.2g is not a valid memory size", d);
    }

  if (hnd->length)
    {
      hnd->buffer = JS_malloc(cx, hnd->length);
      if (!hnd->buffer)
        return JS_FALSE;

      hnd->memoryOwner = obj;
      memset(hnd->buffer, 0, hnd->length);
    }

  if (argc == 2) /* Allow JS programmer to override sanity */
    {
      if (argv[1] != JSVAL_TRUE && argv[1] != JSVAL_FALSE)
        {
          JSBool b;

          if (JS_ValueToBoolean(cx, argv[1], &b) == JSVAL_FALSE)
            return JS_FALSE;
          else
            argv[1] = (b == JS_TRUE) ? JSVAL_TRUE : JSVAL_FALSE;
        }

      if (argv[1] == JSVAL_TRUE)
        hnd->memoryOwner = obj;
      else
        hnd->memoryOwner = NULL;
    }

  return JS_TRUE;
}

/* @jazzdoc gffi.Memory
 * The GFFI module's Memory abstraction represents a pointer, analogous to the void* C type.
 *
 * @form new gffi.Memory(size:Number)
 * This is an interface to malloc(). The returned Memory instance is the "owner" of the memory allocated this way. See
 * gffi.Memory.prototype.ownMemory for more information on memory ownership in GFFI.
 *
 * @form gffi.Memory((instance of Memory-like object))
 * This is analogous to a cast operation and can be used on objects like gffi.MutableStruct, binary.ByteArray,
 * and binary.ByteString. The returned Memory object holds the same address as the one held by the argument given.
 */
JSBool Memory(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  if (JS_IsConstructing(cx) != JS_TRUE)
    return Memory_Cast(cx, obj, argc, argv, rval);
  else
    return Memory_Constructor(cx, obj, argc, argv, rval);
}

/**
 *  Memory Finalizer.
 *
 *  Called by the garbage collector, this routine release all resources
 *  used by an instance of the class when the object is collected.
 */
static void Memory_Finalize(JSContext *cx, JSObject *obj)
{
  byteThing_handle_t   *hnd = JS_GetPrivate(cx, obj);

  if (!hnd)
    return;

  if (hnd->buffer && (hnd->memoryOwner == obj))
    JS_free(cx, hnd->buffer);

  JS_free(cx, hnd);

  return;
}

/**
 *  Initialize the Memory class prototype.
 *
 *  @param  cx  Valid JS Context
 *  @param  obj The module's exports object
 *
 *  @returns    Memory.prototype
 */
JSObject *Memory_InitClass(JSContext *cx, JSObject *obj)
{
  /** Description of this class: */
  static JSClass byteThing_class =
    {
        GPSEE_CLASS_NAME(Memory),     /**< its name is Memory */
        JSCLASS_HAS_PRIVATE,
        JS_PropertyStub,          /**< addProperty stub */
        JS_PropertyStub,          /**< deleteProperty stub */
        JS_PropertyStub,          /**< custom getProperty */
        JS_PropertyStub,          /**< setProperty stub */
        JS_EnumerateStub,         /**< enumerateProperty stub */
        JS_ResolveStub,           /**< resolveProperty stub */
        JS_ConvertStub,           /**< convertProperty stub */
        Memory_Finalize,          /**< it has a custom finalizer */
        /* Optional members below instead of JSCLASS_NO_OPTIONAL_MEMBERS */
        NULL,                     /**< getObjectOps */
        NULL,                     /**< checkAccess */
        NULL,                     /**< call */
        NULL,                     /**< construct */
        NULL,                     /**< xdrObject */
        NULL,                     /**< hasInstance */
        NULL,                     /**< GC Trace */
        NULL                      /**< reserveSlots */
    };

  static JSPropertySpec memory_props[] =
    {
      { "size",       0, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, memory_size_getter, JS_PropertyStub },
      { NULL, 0, 0, NULL, NULL }
    };

  static JSFunctionSpec memory_methods[] =
    {
      JS_FN("toString",   memory_toString,    0, JSPROP_ENUMERATE),
      JS_FN("asString",   memory_asString,    0, JSPROP_ENUMERATE),
      JS_FS_END
    };

  GPSEE_DECLARE_BYTETHING_CLASS(byteThing);

  memory_clasp = &byteThing_class;

  memory_proto =
    JS_InitClass(cx,              /* JS context from which to derive runtime information */
                 obj,             /* Object to use for initializing class (constructor arg?) */
                 NULL,            /* parent_proto - Prototype object for the class */
                 memory_clasp,    /* clasp - Class struct to init. Defs class for use by other API funs */
                 NULL,            /* constructor function - Scope matches obj */
                 0,               /* nargs - Number of arguments for constructor (can be MAXARGS) */
                 memory_props,    /* ps - props struct for parent_proto */
                 memory_methods,  /* fs - functions struct for parent_proto (normal "this" methods) */
                 NULL,            /* static_ps - props struct for constructor */
                 NULL);           /* static_fs - funcs struct for constructor (methods like Math.Abs()) */

  GPSEE_ASSERT(memory_proto);

  return memory_proto;
}

/**
 *  Create a new ByteString or ByteArray instance from a provided buffer.
 *
 *  @param  cx          JavaScript context
 *  @param  buffer      The raw contents of the byte string; if buffer is NULL we initialize to all-zeroes
 *  @param  length      The length of the data in buf.
 *
 *  @returns    NULL on OOM, otherwise a new ByteString.
 */
JSObject *byteThing_fromCArray(JSContext *cx, const unsigned char *buffer, size_t length)
{
  /* Allocate and initialize our private ByteThing handle */
  byteThing_handle_t  *hnd;

  hnd = (byteThing_handle_t*) JS_malloc(cx, sizeof(byteThing_handle_t));
  if (!hnd)
    return NULL;

  memset(hnd, 0, sizeof(byteThing_handle_t));
  hnd->length = length;

  if (length) {
    hnd->buffer = JS_malloc(cx, length);
    if (!hnd->buffer) {
      JS_free(cx, hnd);
      return NULL;
    }
  }

  if (buffer)
    memcpy(hnd->buffer, buffer, length);

  //    GPSEE_ASSERT(!obj || (JS_GET_CLASS(cx, obj) ==  memory_clasp));

  JSObject* obj = JS_NewObject(cx, memory_clasp, memory_proto, NULL);

  if (obj) {
    JS_SetPrivate(cx, obj, hnd);
    hnd->memoryOwner = obj;
  }

  return obj;
}
