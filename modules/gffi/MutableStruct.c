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
 *  @file	mutable_struct.c	Implementation of the GPSEE gffi MutableStruct class.
 *					This class immediately reflects reads and writes
 *					to/from a backing-store C structure.
 *  @author	Wes Garland
 *              PageMail, Inc.
 *		wes@page.ca
 *  @date	Jun 2009
 *  @version	$Id: MutableStruct.c,v 1.11 2011/12/05 19:13:37 wes Exp $
 *
 *  @todo       Struct and member lookup are linear traversal; should sort them
 *		and bsearch or similar.
 */

#include <gpsee.h>
#include "gffi.h"

#define CLASS_ID MODULE_ID ".MutableStruct"

static JSBool MutableStruct_getProperty(JSContext *cx, JSObject *obj, jsval propId, jsval *vp)
{
  const char 		*name;
  int			memberId;
  struct_handle_t	*hnd;

  if (!(hnd  = JS_GetInstancePrivate(cx, obj, mutableStruct_clasp, JS_ARGV(cx, vp))))
    return JS_FALSE;

  if (!JSVAL_IS_STRING(propId))
  {
    JSString *str = JS_ValueToString(cx, propId);
    if (!str)
      return JS_FALSE;
    propId = STRING_TO_JSVAL(str);
  }

  name = JS_GetStringBytes(JSVAL_TO_STRING(propId));
  memberId = struct_findMemberIndex(hnd->descriptor, name);
  if (memberId == -1)
    return JS_TRUE;

  switch(hnd->descriptor->members[memberId].type)
  {
    case selt_integer:
      return struct_getInteger(cx, obj, memberId, vp, CLASS_ID);

    case selt_array:
      return struct_getArray(cx, obj, memberId, vp, CLASS_ID);

    case selt_string:
      return struct_getString(cx, obj, memberId, vp, CLASS_ID);

    case selt_pointer:
      return struct_getPointer(cx, obj, memberId, vp, CLASS_ID);

    default:
      GPSEE_NOT_REACHED("enum exhausted");
  }

  return JS_TRUE;
}

static JSBool MutableStruct_setProperty(JSContext *cx, JSObject *obj, jsval propId, jsval *vp)
{
  const char 		*name;
  int			memberId;
  struct_handle_t	*hnd;

  if (!(hnd  = JS_GetInstancePrivate(cx, obj, mutableStruct_clasp, JS_ARGV(cx, vp))))
    return JS_FALSE;

  if (!JSVAL_IS_STRING(propId))
  {
    JSString *str = JS_ValueToString(cx, propId);
    if (!str)
      return JS_FALSE;
    propId = STRING_TO_JSVAL(str);
  }

  name = JS_GetStringBytes(JSVAL_TO_STRING(propId));
  memberId = struct_findMemberIndex(hnd->descriptor, name);
  if (memberId == -1)
    return gpsee_throw(cx, CLASS_ID ".setProperty.noSuchProperty: class does not support duck-typing");

  switch(hnd->descriptor->members[memberId].type)
  {
    case selt_integer:
      return struct_setInteger(cx, obj, memberId, vp, CLASS_ID);

    case selt_array:
      return struct_setArray(cx, obj, memberId, vp, CLASS_ID);

    case selt_string:
      return struct_setString(cx, obj, memberId, vp, CLASS_ID);

    case selt_pointer:
      return struct_setPointer(cx, obj, memberId, vp, CLASS_ID);

    default:
      GPSEE_NOT_REACHED("enum exhausted");
  }

  return JS_TRUE;
}

/** 
 *  Implements the MutableStruct constructor.
 *  new MutableStruct("structType").
 *  FUTURE: new MutableStruct([{name: "fieldName", type: "fieldType", offset: fieldOffset, size: fieldSize},...]
 *
 *  @param	cx	JavaScript context
 *  @param	obj	Pre-allocated MutableStruct object
 *  @param	argc	Number of arguments passed to constructor
 *  @param	argv	Arguments passed to constructor
 *  @param	rval	The new object returned to JavaScript
 *
 *  @returns 	JS_TRUE on success
 */
JSBool MutableStruct_Constructor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  struct_handle_t	*hnd;

  if (argc != 1)
    return gpsee_throw(cx, CLASS_ID ".arguments.count");

  hnd = JS_malloc(cx, sizeof(*hnd));
  if (!hnd)
    return JS_FALSE;

  /* cleanup now solely the job of the finalizer */
  memset(hnd, 0, sizeof(*hnd));
  JS_SetPrivate(cx, obj, hnd);
  hnd->memoryOwner = obj;

  if (!JSVAL_IS_STRING(argv[0]))
  {
    JSString *structName = JS_ValueToString(cx, argv[0]);
    if (!structName)
      return JS_FALSE;
    argv[0] = STRING_TO_JSVAL(structName);
  }

  hnd->descriptor = struct_findShape(JS_GetStringBytes(JSVAL_TO_STRING(argv[0])));
  if (!hnd->descriptor)
    return gpsee_throw(cx, CLASS_ID ".constructor.unknown: Struct '%s' is unknown to this build of GPSEE", JS_GetStringBytes(JSVAL_TO_STRING(argv[0])));

  hnd->length = hnd->descriptor->size;
  if (hnd->length)
  {
    hnd->buffer = JS_malloc(cx, hnd->length);
    if (!hnd->descriptor)
      return JS_FALSE;

    memset(hnd->buffer, 0, hnd->length);
  }

  return JS_TRUE;
}

/** Struct casts require struct name and castee as arguments
 * 
 *  @example var s = ffi.MutableStruct("struct stat", byteThing);
 */
JSBool MutableStruct_Cast(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  byteThing_handle_t	*srcHnd;
  struct_handle_t	*newHnd;
  JSClass 		*clasp;
  JSObject              *mutableStruct_proto;

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
  if (gpsee_isByteThingClass(cx, clasp))
    srcHnd = JS_GetPrivate(cx, obj);
  else
    srcHnd = NULL;

  if (!srcHnd)
  {
    const char	*className;

    if (!clasp)
      className = "Object";
    else
      className = (clasp->name && clasp->name[0]) ? clasp->name : "corrupted";

    return gpsee_throw(cx, CLASS_ID ".cast.type: %s objects are not castable to MutableStruct", className);
  }

  if (gpsee_getModuleData(cx, mutableStruct_clasp, (void **)&mutableStruct_proto, CLASS_ID ".cast") == JS_FALSE)
    return JS_FALSE;

  obj = JS_NewObject(cx, mutableStruct_clasp, mutableStruct_proto, srcHnd->memoryOwner);
  if (!obj)
    return JS_FALSE;

  *rval = OBJECT_TO_JSVAL(obj);

  if (MutableStruct_Constructor(cx, obj, 1, argv, rval) == JS_FALSE)
    return JS_FALSE;

  newHnd = JS_GetPrivate(cx, obj);
  if (newHnd->buffer)	/* constructor allocates needlessly */
    JS_free(cx, newHnd->buffer);

  *(byteThing_handle_t *)newHnd = *(byteThing_handle_t *)srcHnd;

  return JS_TRUE;
}


/** 
 *  MutableStruct Finalizer.
 *
 *  Called by the garbage collector, this routine release all resources
 *  used by an instance of the class when the object is collected.
 *
 *  @param	cx	JavaScript context
 *  @param	obj	The object to finalize
 */
static void MutableStruct_Finalize(JSContext *cx, JSObject *obj)
{
  struct_handle_t	*hnd = JS_GetPrivate(cx, obj);

  if (!hnd)
    return;

  if (hnd->buffer && (hnd->memoryOwner == obj))
    JS_free(cx, hnd->buffer);

  JS_free(cx, hnd);

  return;
}

/* @jazzdoc gffi.MutableStruct
 * TODO document
 */
JSBool MutableStruct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  /* MutableStruct() called as function. */   
  if (JS_IsConstructing(cx) != JS_TRUE)
    return MutableStruct_Cast(cx, obj, argc, argv, rval);
  else
    return MutableStruct_Constructor(cx, obj, argc, argv, rval);
}


JSClass *mutableStruct_clasp = NULL;
JSClass *immutableStruct_clasp = NULL;          /**< @todo Implement immutable structs */

/**
 *  Initialize the MutableStruct class prototype.
 *
 *  @param	cx	Valid JS Context
 *  @param	obj	The module's exports object
 *
 *  @returns	MutableStruct.prototype
 */
JSObject *MutableStruct_InitClass(JSContext *cx, JSObject *obj, JSObject *parentProto)
{
  JSObject *mutableStruct_proto;

  /** Description of this class: */
  static JSClass mutableStruct_class =
  {
    GPSEE_CLASS_NAME(MutableStruct),	/**< its name is MutableStruct */
    JSCLASS_HAS_PRIVATE,		/**< private slot in use */
    JS_PropertyStub,  			/**< addProperty stub */
    JS_PropertyStub,  			/**< deleteProperty stub */
    MutableStruct_getProperty,		/**< custom getProperty */
    MutableStruct_setProperty,		/**< setProperty stub */
    JS_EnumerateStub, 			/**< enumerateProperty stub */
    JS_ResolveStub,   			/**< resolveProperty stub */
    JS_ConvertStub,   			/**< convertProperty stub */
    MutableStruct_Finalize,		/**< it has a custom finalizer */

    JSCLASS_NO_OPTIONAL_MEMBERS
  };

#define mutableStruct_handle_t struct_handle_t
  GPSEE_DECLARE_BYTETHING_CLASS(mutableStruct);

  mutableStruct_clasp = &mutableStruct_class;
  immutableStruct_clasp = mutableStruct_clasp; //xxxwg

  mutableStruct_proto =
      JS_InitClass(cx, 			/* JS context from which to derive runtime information */
		   obj, 		/* Object to use for initializing class (constructor arg?) */
		   parentProto,		/* parent_proto - Prototype object for the class */
 		   mutableStruct_clasp, /* clasp - Class struct to init. Defs class for use by other API funs */
		   MutableStruct,	/* constructor function - Scope matches obj */
		   1,			/* nargs - Number of arguments for constructor (can be MAXARGS) */
		   NULL,		/* ps - props struct for parent_proto */
		   NULL, 		/* fs - functions struct for parent_proto (normal "this" methods) */
		   NULL,		/* static_ps - props struct for constructor */
		   NULL); 		/* static_fs - funcs struct for constructor (methods like Math.Abs()) */

  if (gpsee_setModuleData(cx, mutableStruct_clasp, mutableStruct_proto) == JS_FALSE)
    return NULL;

  GPSEE_ASSERT(mutableStruct_proto);

  return mutableStruct_proto;
}
