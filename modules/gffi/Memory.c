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
 *  @file	Memory.c		Implementation of the GPSEE gffi Memory class.
 *					This class provides a handle and allocator for 
 * 					heap; we also support void pointers by using
 *					0-byte heap segments at specific addresses.
 *
 *  @author	Wes Garland
 *              PageMail, Inc.
 *		wes@page.ca
 *  @date	Jul 2009
 *  @version	$Id: Memory.c,v 1.2 2009/07/24 21:17:32 wes Exp $
 */

#include <gpsee.h>
#include "gffi_module.h"

#define CLASS_ID MODULE_ID ".Memory"

JSBool Memory(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  /* Memory() called as function. */   
  if (JS_IsConstructing(cx) != JS_TRUE)
    return gpsee_throw(cx, CLASS_ID ".constructor.notFunction: Must call constructor with 'new'!");

  return Memory_Constructor(cx, obj, argc, argv, rval);
}

static JSBool memory_size_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  memory_handle_t	*hnd = JS_GetInstancePrivate(cx, obj, memory_clasp, NULL);
  jsdouble		d;

  if (!hnd)
    return JS_FALSE;

  if (INT_FITS_IN_JSVAL(hnd->length))
  {
    *vp = INT_TO_JSVAL(hnd->length);
    return JS_TRUE;
  }

  d = hnd->length;
  if (hnd->length != d)
    return gpsee_throw(cx, CLASS_ID, ".size.getter.overflow");

  return JS_NewNumberValue(cx, d, vp);
}

static JSBool memory_size_setter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  size_t		newLength;
  void			*newBuffer;
  memory_handle_t	*hnd = JS_GetInstancePrivate(cx, obj, memory_clasp, NULL);

  if (!hnd)
    return JS_FALSE;

  if (hnd->ownMemory == JSVAL_FALSE)
    return gpsee_throw(cx, CLASS_ID ".size.setter.notOwnMemory: cannot realloc memory we did not allocate");

  if (JSVAL_IS_INT(*vp))
    newLength = JSVAL_TO_INT(*vp);
  else
  {
    jsdouble d;

    if (JS_ValueToNumber(cx, *vp, &d) != JS_TRUE)
      return JS_FALSE;

    newLength = d;
    if (d != newLength)
      return gpsee_throw(cx, CLASS_ID, ".size.setter.overflow");
  }

  newBuffer = JS_realloc(cx, hnd->buffer, newLength);
  if (!newBuffer && newLength)
    return JS_FALSE;

  hnd->buffer = newBuffer;
  hnd->length = newLength;

  return JS_TRUE;
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
 *  @param	cx	JavaScript context
 *  @param	obj	Pre-allocated Memory object
 *  @param	argc	Number of arguments passed to constructor
 *  @param	argv	Arguments passed to constructor
 *  @param	rval	The new object returned to JavaScript
 *
 *  @returns 	JS_TRUE on success
 */
JSBool Memory_Constructor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  memory_handle_t	*hnd;

  if ((argc != 1) && (argc != 2))
    return gpsee_throw(cx, CLASS_ID ".arguments.count");

  *rval = OBJECT_TO_JSVAL(obj);
 
  hnd = JS_malloc(cx, sizeof(*hnd));
  if (!hnd)
  {
    JS_ReportOutOfMemory(cx);
    return JS_FALSE;
  }
  else
  {
    /* cleanup now solely the job of the finalizer */
    memset(hnd, 0, sizeof(*hnd));
    JS_SetPrivate(cx, obj, hnd);
    hnd->ownMemory = JSVAL_TRUE;
  }

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
    {
      JS_ReportOutOfMemory(cx);
      return JS_FALSE;
    }

    memset(hnd->buffer, 0, hnd->length);
  }

  if (argc == 2)
  {
    if ((argv[1] == JSVAL_TRUE) || argv[1] == JSVAL_FALSE)
      hnd->ownMemory = argv[1];
    else
    {
      if (JS_ValueToBoolean(cx, argv[1], &hnd->ownMemory) == JS_FALSE)
	return JS_FALSE;
    }
  }

  return JS_TRUE;
}

/** 
 *  Memory Finalizer.
 *
 *  Called by the garbage collector, this routine release all resources
 *  used by an instance of the class when the object is collected.
 */
static void Memory_Finalize(JSContext *cx, JSObject *obj)
{
  memory_handle_t	*hnd = JS_GetPrivate(cx, obj);

  if (!hnd)
    return;

  if (hnd->buffer && (hnd->ownMemory == JSVAL_TRUE))
    JS_free(cx, hnd->buffer);

  JS_free(cx, hnd);

  return;
}

JSClass *memory_clasp = NULL;

/**
 *  Initialize the Memory class prototype.
 *
 *  @param	cx	Valid JS Context
 *  @param	obj	The module's exports object
 *
 *  @returns	Memory.prototype
 */
JSObject *Memory_InitClass(JSContext *cx, JSObject *obj, JSObject *parentProto)
{
  /** Description of this class: */
  static JSClass memory_class =
  {
    GPSEE_CLASS_NAME(Memory),		/**< its name is Memory */
    JSCLASS_HAS_PRIVATE,		/**< private slot in use */
    JS_PropertyStub,  			/**< addProperty stub */
    JS_PropertyStub,  			/**< deleteProperty stub */
    JS_PropertyStub,			/**< custom getProperty */
    JS_PropertyStub,			/**< setProperty stub */
    JS_EnumerateStub, 			/**< enumerateProperty stub */
    JS_ResolveStub,   			/**< resolveProperty stub */
    JS_ConvertStub,   			/**< convertProperty stub */
    Memory_Finalize,			/**< it has a custom finalizer */

    JSCLASS_NO_OPTIONAL_MEMBERS
  };

  static JSPropertySpec memory_props[] =
  {
    { "size",	0, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, memory_size_getter, memory_size_setter },
    { NULL, 0, 0, NULL, NULL }
  };

  JSObject *proto;

  memory_clasp = &memory_class;

  proto =
      JS_InitClass(cx, 			/* JS context from which to derive runtime information */
		   obj, 		/* Object to use for initializing class (constructor arg?) */
		   parentProto,		/* parent_proto - Prototype object for the class */
 		   memory_clasp, 	/* clasp - Class struct to init. Defs class for use by other API funs */
		   Memory,		/* constructor function - Scope matches obj */
		   1,			/* nargs - Number of arguments for constructor (can be MAXARGS) */
		   memory_props,	/* ps - props struct for parent_proto */
		   NULL, 		/* fs - functions struct for parent_proto (normal "this" methods) */
		   NULL,		/* static_ps - props struct for constructor */
		   NULL); 		/* static_fs - funcs struct for constructor (methods like Math.Abs()) */

  GPSEE_ASSERT(proto);

  return proto;
}




