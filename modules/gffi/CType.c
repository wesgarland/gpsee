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
 * Copyright (c) 2010, PageMail, Inc. All Rights Reserved.
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
 *  @file	CType.c			Implementation of the GPSEE gffi CType class.
 *					This class provides real-ctypes backing store
 *					for each JS type, instead of inferring the
 *					data.  This is important in cases where you
 *					need an in/out pointer to a native function.
 *
 *					CType is a valid ByteThing, and coercion rules
 *					match the CFunction argument rules.
 *
 *  @author	Wes Garland
 *              PageMail, Inc.
 *		wes@page.ca
 *  @date	Mar 2010
 *  @version	$Id: Memory.c,v 1.10 2010/03/06 18:17:14 wes Exp $
 *
 *  @example
 *
 *    var n = new CType(ffi.int);		// int n 	   = 0;
 *    var n = new CType(ffi.size_t, 123)	// size_t n 	   = 123;
 *    var p = new CType(ffi.pointer)		// void 	*p = NULL;
 *    var p = new CType(ffi.pointer, null)	// void 	*p = NULL;
 *    var p = new CType(ffi.pointer, "wes")	// void		*p = "wes";
 *    var m = new CType(ffi.pointer, bytething) // void		*p = &hnd->buffer;
 *
 *    var N = CType(ffi.int, 123);		// int n 	   = *(int *)123;
 *    var N = CType(ffi.int, sb);		// int n	   = *(int *)hnd->buffer;
 */
#include <gpsee.h>
#include "gffi.h"
#include <math.h>

#define CLASS_ID MODULE_ID ".CType"

JSClass *ctype_clasp = NULL;
JSObject *ctype_proto = NULL;

/** 
 *  Implements the CType constructor.
 *
 *  Constructor takes exactly one or two arguments: 
 *   - a type indicator
 *   - a value (defaults to zero if unused)
 *
 *  @param	cx	JavaScript context
 *  @param	obj	Pre-allocated CType object
 *  @param	argc	Number of arguments passed to constructor
 *  @param	argv	Arguments passed to constructor
 *  @param	rval	The new object returned to JavaScript
 *
 *  @returns 	JS_TRUE on success
 */
JSBool CType_Constructor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  ctype_handle_t	*hnd;
  void			*dummy;

  if ((argc != 1) && (argc != 2))
    return gpsee_throw(cx, CLASS_ID ".arguments.count");

  *rval = OBJECT_TO_JSVAL(obj);
 
  hnd = JS_malloc(cx, sizeof(*hnd));
  if (!hnd)
    return JS_FALSE;

  /* cleanup now solely the job of the finalizer */
  memset(hnd, 0, sizeof(*hnd));
  JS_SetPrivate(cx, obj, hnd);

  if (setupCTypeDetails(cx, argv[0], hnd, CLASS_ID ".constructor.arguments.0") == JS_FALSE)
    return JS_FALSE;

  if (hnd->length)	/* ffi.void indicator or similar have size 0 */
  {
    hnd->buffer = JS_malloc(cx, hnd->length);
    if (!hnd->buffer)
      return JS_FALSE;
  }

  hnd->memoryOwner = obj;

  if (argc == 1)
  {
    memset(hnd->buffer, 0, hnd->length);
    return JS_TRUE;
  }

  /* convert jsval to native */
  dummy=&hnd->buffer;
  return hnd->valueTo_ffiType(cx, argv[1], &dummy, (void **)&hnd->buffer, 1, CLASS_ID ".constructor");
}

/**  var a = ffi.CType(ffi.int, byteThing); */
JSBool CType_Cast(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  byteThing_handle_t	*srcHnd;
  ctype_handle_t	*newHnd;
  JSClass		*clasp;
  
  if (argc != 2)
    return gpsee_throw(cx, CLASS_ID ".cast.arguments.count");

  if (!JSVAL_IS_OBJECT(argv[1]))
  {
    if (JS_ValueToObject(cx, argv[1], &obj) == JS_FALSE)
      return JS_FALSE;
  }
  else
    obj = JSVAL_TO_OBJECT(argv[1]);

  clasp = JS_GET_CLASS(cx, obj);
  srcHnd = JS_GetPrivate(cx, obj);
  if (!srcHnd || !gpsee_isByteThingClass(cx, clasp))
  {
    const char	*className;

    if (!clasp)
      className = "Object";
    else
      className = (clasp->name && clasp->name[0]) ? clasp->name : "corrupted";

    return gpsee_throw(cx, CLASS_ID ".cast.type: %s objects are not castable to CType", className);
  }

  obj = JS_NewObject(cx, ctype_clasp, ctype_proto, NULL);
  if (!obj)
    return JS_FALSE;

  *rval = OBJECT_TO_JSVAL(obj);

  if (CType_Constructor(cx, obj, 1, argv, rval) == JS_FALSE)
    return JS_FALSE;
 
  newHnd = JS_GetPrivate(cx, obj);
  if (newHnd->buffer)	/* constructor allocates needlessly */
    JS_free(cx, newHnd->buffer);

  if (setupCTypeDetails(cx, argv[0], newHnd, CLASS_ID ".cast.arguments.0") == JS_FALSE)
    return JS_FALSE;

  if (srcHnd->length && (newHnd->length != srcHnd->length))
    return gpsee_throw(cx, CLASS_ID ".cast.size: Cannot cast %i-byte backing store to %i-byte type", newHnd->length, srcHnd->length);

  *(byteThing_handle_t *)newHnd = *(byteThing_handle_t *)srcHnd;

  return JS_TRUE;
}

/* @jazzdoc gffi.CType
 * The GFFI module's CType abstraction represents an arbitrary c type with a real
 * c type backing store rather than a coerced jsval or similar.
 *
 * @form new gffi.CType(typeIndicator)
 * This is an interface to malloc() which allocates enough memory for the CType's
 * backing store, populates the backing store from the supplied JS value, and 
 * provides a property (.value) which can be used to manipulate the data in the
 * backing store via the typical gffi coerscion rules.
 * 
 * This type is special because it can coerce to either the underlying value or
 * to a pointer, based on the needs of the user -- it can work as either a value
 * or a ByteThing.  This is the correct way to represent, for example, a pointer to int.
 *
 * @form new gffi.CType(typeIndicator, initialValue)
 * This is identical to gffi.CType(typeIndicator) except it provides an initial
 * value to populate the backing store.
 */
JSBool CType(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  if (JS_IsConstructing(cx) != JS_TRUE)
    return CType_Cast(cx, obj, argc, argv, rval);
  else
    return CType_Constructor(cx, obj, argc, argv, rval);
}

/** 
 *  CType Finalizer.
 *
 *  Called by the garbage collector, this routine release all resources
 *  used by an instance of the class when the object is collected.
 */
static void CType_Finalize(JSContext *cx, JSObject *obj)
{
  ctype_handle_t	*hnd = JS_GetPrivate(cx, obj);

  if (!hnd)
    return;

  if (hnd->buffer && (hnd->memoryOwner == obj))
    JS_free(cx, hnd->buffer);

  JS_free(cx, hnd);

  return;
}

/**
 *  Implement a non-strict equality callback for instances of CType
 *  This effectively overloads the == and != operators when the left-hand 
 *  side of the expression is a CType() object.  CType objects are ==
 *  they are the same size and their bitwise representation is the same.
 *
 *  This is very similar to a C++ reinterpret cast where the right hand side
 *  of == is cast to the same type as the left hand side.
 *
 *  @param	cx		JavaScript context
 *  @param	thisObj		The object on the left-hand side of the expression
 *  @param	v		The value of the right-hand side of the expression
 *  @param	bp		[out]	Pointer to JS_TRUE when equal, JS_FALSE otherwise
 *  @returns	JS_TRUE unless we've thrown an exception.
*/
JSBool CType_Equal(JSContext *cx, JSObject *thisObj, jsval v, JSBool *bp)
{
  ctype_handle_t *thisHnd = JS_GetPrivate(cx, thisObj);
  ctype_handle_t *thatHnd;
  JSObject	 *thatObj;

  /* The first operand (this) is guaranteed to be a Memory object.
   * Check now to see if the second operand (that) is one of either
   * an object or null */
  if (!JSVAL_IS_OBJECT(v))
  {
    *bp = JS_FALSE;
    return JS_TRUE;
  }

  /* Check for NULL */
  if (v == JSVAL_NULL)
  {
    *bp = (thisHnd->buffer == NULL) ? JS_TRUE : JS_FALSE;
    return JS_TRUE;
  }

  thatObj = JSVAL_TO_OBJECT(v);
  if (JS_GET_CLASS(cx, thatObj) != ctype_clasp)
  {
    *bp = JS_FALSE;
    return JS_TRUE;
  }

  thatHnd = JS_GetPrivate(cx, thatObj);
  if ((thisHnd && thatHnd) && (thisHnd->length == thatHnd->length) && (memcmp(thisHnd->buffer, thatHnd->buffer, thisHnd->length) == 0))
    *bp = JS_TRUE;
  else
    *bp = JS_FALSE;

  return JS_TRUE;
}

static JSBool ctype_value_setter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  ctype_handle_t	*hnd = JS_GetInstancePrivate(cx, obj, ctype_clasp, NULL);
  void			*dummy;

  if (!hnd)
    return JS_FALSE;

  dummy = &hnd->buffer;
  return hnd->valueTo_ffiType(cx, *vp, &dummy, (void **)&hnd->buffer, 1, CLASS_ID ".value.set");
}

static JSBool ctype_value_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  ctype_handle_t	*hnd = JS_GetInstancePrivate(cx, obj, ctype_clasp, NULL);

  if (!hnd)
    return JS_FALSE;

  return ffiType_toValue(cx, hnd->buffer, hnd->ffiType, vp, CLASS_ID ".value.getter");
}

static JSBool ctype_valueOf(JSContext *cx, uintN argc, jsval *vp)
{
  return ctype_value_getter(cx, JS_THIS_OBJECT(cx, vp), 0, vp);
}

/**
 *  Initialize the Memory class prototype.
 *
 *  @param	cx	Valid JS Context
 *  @param	obj	The module's exports object
 *
 *  @returns	CType.prototype
 */
JSObject *CType_InitClass(JSContext *cx, JSObject *obj, JSObject *parentProto)
{
  /** Description of this class: */
  static JSExtendedClass ctype_eclass =
  {
    {
      GPSEE_CLASS_NAME(CType),		/**< its name is CType */
      JSCLASS_HAS_PRIVATE | JSCLASS_IS_EXTENDED,	/**< private slot in use, this is really a JSExtendedClass */
      JS_PropertyStub, 			/**< addProperty stub */
      JS_PropertyStub, 			/**< deleteProperty stub */
      JS_PropertyStub,			/**< custom getProperty */
      JS_PropertyStub,			/**< setProperty stub */
      JS_EnumerateStub,			/**< enumerateProperty stub */
      JS_ResolveStub,  			/**< resolveProperty stub */
      JS_ConvertStub,  			/**< convertProperty stub */
      CType_Finalize,			/**< it has a custom finalizer */
    
      JSCLASS_NO_OPTIONAL_MEMBERS
    },					/**< JSClass		base */
    CType_Equal,			/**< JSEqualityOp 	equality */
    NULL,				/**< JSObjectOp		outerObject */
    NULL,				/**< JSObjectOp		innerObject */
    NULL,				/**< JSIteratorOp	iteratorObject */
    NULL,				/**< JSObjectOp		wrappedObject */
  };

  static JSPropertySpec ctype_props[] =
  {
    { "value",		0, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, ctype_value_getter, ctype_value_setter },
    { NULL, 0, 0, NULL, NULL }
  };

  static JSFunctionSpec ctype_methods[] = 
  {
    JS_FN("valueOf",	ctype_valueOf, 	0, JSPROP_ENUMERATE),
    JS_FS_END
  };

  GPSEE_DECLARE_BYTETHING_EXTCLASS(ctype);

  ctype_clasp = &ctype_eclass.base;

  ctype_proto =
      JS_InitClass(cx, 			/* JS context from which to derive runtime information */
		   obj, 		/* Object to use for initializing class (constructor arg?) */
		   parentProto,		/* parent_proto - Prototype object for the class */
 		   ctype_clasp, 	/* clasp - Class struct to init. Defs class for use by other API funs */
		   CType,		/* constructor function - Scope matches obj */
		   1,			/* nargs - Number of arguments for constructor (can be MAXARGS) */
		   ctype_props,	/* ps - props struct for parent_proto */
		   ctype_methods,	/* fs - functions struct for parent_proto (normal "this" methods) */
		   NULL,		/* static_ps - props struct for constructor */
		   NULL); 		/* static_fs - funcs struct for constructor (methods like Math.Abs()) */

  GPSEE_ASSERT(ctype_proto);

  return ctype_proto;
}
