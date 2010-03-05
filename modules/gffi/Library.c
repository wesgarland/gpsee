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
 *  @file	functions.c	Support code for GPSEE's gffi module which
 *				exposes C functions to JavaScript.
 *
 *  @author	Donny Viszneki
 *              PageMail, Inc.
 *		donny.viszneki@gmail.com
 *  @date	Jul 2009
 *  @version	$Id: Library.c,v 1.2 2009/09/14 21:22:50 wes Exp $
 */

#include <gpsee.h>
#include <ffi.h>
#include "gffi.h"

#define CLASS_ID MODULE_ID ".Library"

JSClass *library_clasp;

/* library_handle_t::flags */
#define LIBRARY_DLCLOSE 1
#define LIBRARY_FREENAME 2
/**
 *  Implements the Library constructor representing DSOs loaded for FFI binding.
 *
 *  @param      cx      Valid JS Context
 *  @param      obj     Pre-allocated Library object
 *  @param      argc    Number of arguments passed to constructor
 *  @param      argv    Arguments passed to constructor
 *  @param      rval    The new object returned to Javascript
 *
 *  @returns    JS_TRUE on success
 */
static JSBool Library(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  library_handle_t    *hnd;

  /* Require we are constructing a new object instance with 'new' */
  if (!JS_IsConstructing(cx))
    return gpsee_throw(cx, CLASS_ID ".constructor.notFunction: Must call constructor with 'new'!");

  /* @jazzdoc gffi.Library
   *
   * Libraries represent a symbol resolution scope which comes in the form of either a dlopen(3)'d DSO or a notional
   * symbol resolution scope such as RTLD_DEFAULT. (Note that RTLD_DEFAULT has special behavior in GFFI.)
   *
   * @form new gffi.Library()
   * @form new gffi.Library(gffi.rtldDefault)
   *
   * This form of Library instantiation yields a library representing the special GFFI RTLD_DEFAULT symbol resolution
   * scope. When a new CFunction is instantiated from this instance, a symbol is first looked up using dlsym(3) using
   * the RTLD_DEFAULT resolution scope, meaning that any symbol linked by the cstdlib dynamic linker will be found.
   * Please see gffi.CFunction for more information.
   * 
   * @form new gffi.Library(filename)
   *
   * This form of Library instantiation yields a library representing the symbols contained in the DSO referenced by
   * the argument 'filename'. Please see gffi.CFunction for more information.
   */

  /* Require a single argument from caller */
  if (argc != 1)
    return gpsee_throw(cx, CLASS_ID ".arguments.count: An argument is required!");

  /* Allocate our instance private struct */
  hnd = JS_malloc(cx, sizeof(*hnd));
  if (!hnd)
  {
    JS_ReportOutOfMemory(cx);
    return JS_FALSE;
  }
  /* Initialize our instance private struct */
  memset(hnd, 0, sizeof(*hnd));
  /* Associate the instance private with our new JSObject */
  JS_SetPrivate(cx, obj, hnd);

  /* Handle dlopen() calls that don't actually open a new DSO */
  switch(argv[0])
  {
    /* We have cases for so-called "pseudo-handles" which are DSO handle values (void* type) which are special */
    case JSVAL_VOID:
    case jsve_rtldDefault:
      hnd->dlHandle = RTLD_DEFAULT;
      hnd->name =  (char *)"RTLD_DEFAULT";
      break;

#if defined(RTLD_SELF)
    case jsve_rtldSelf:
      hnd->dlHandle = RTLD_SELF;
      hnd->name = (char *)"RTLD_SELF";
      break;
#endif

#if defined(RTLD_PROBE)
    case jsve_rtldProbe:
      hnd->dlHandle = RTLD_PROBE;
      hnd->name = (char *)"RTLD_PROBE";
      break;
#endif

    /* Open a real DSO */
    default:
      /* Retrieve the name of the DSO */
      hnd->name = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
      /* Open the DSO */
      hnd->dlHandle = dlopen(hnd->name, RTLD_LAZY);
      if (!hnd->dlHandle)
        return gpsee_throw(cx, CLASS_ID ".error: Could not dlopen(\"%s\", RTLD_LAZY) (%m)", hnd->name);
      /* Remember to dlclose() later */
      hnd->flags |= LIBRARY_DLCLOSE;
      break;
  }

  if (!CFunction_InitClass(cx, obj, CFunction_proto))
    return gpsee_throw(cx, CLASS_ID ".error: could not CFunction_InitClass()");

  return JS_TRUE;
}
/** Library finalizer.
 *
 *  @param    cx    Valid JS Context
 *  @param    obj   Library instance
 */
static void Library_Finalize(JSContext *cx, JSObject *obj)
{
  library_handle_t *hnd;

  /* Fetch our instance private */
  hnd = JS_GetPrivate(cx, obj);
  if (!hnd)
    return;

  if (hnd->flags & LIBRARY_DLCLOSE)
    dlclose(hnd->dlHandle);
  if (hnd->flags & LIBRARY_FREENAME)
    JS_free(cx, hnd->name);
  JS_free(cx, hnd);
}
/**
 *  Initalize the Library JSClass and prototype.
 *
 *  @param    cx            Valid JS Context
 *  @param    obj           The module's exports object.
 *  @param    parentProto   Prototype from which Library should inherit (always NULL?)
 *
 *  @returns  Library.prototype
 */
JSObject *Library_InitClass(JSContext *cx, JSObject *obj, JSObject *parentProto)
{
  /** Description of this class */
  static JSClass library_class =
  {
    GPSEE_CLASS_NAME(Library),      /**< its name is Library */
    JSCLASS_HAS_PRIVATE,            /**< private slot in use */
    JS_PropertyStub,                /**< addProperty stub */
    JS_PropertyStub,                /**< deleteProperty stub */
    JS_PropertyStub,                /**< getProperty stub */
    JS_PropertyStub,                /**< setProperty stub */
    JS_EnumerateStub,               /**< enumerateProperty stub */
    JS_ResolveStub,                 /**< resolveProperty stub */
    JS_ConvertStub,                 /**< convertProperty stub */
    Library_Finalize,               /**< finalizer */

    JSCLASS_NO_OPTIONAL_MEMBERS
  };
  static JSFunctionSpec instance_methods[] =
  {
    JS_FS_END
  };
  static JSPropertySpec instance_props[] =
  {
    {NULL,0,0,NULL,NULL}
  };
  JSObject *proto;
  library_clasp = &library_class;
  /* Initialize Library JSClass and prototype object */
  proto =
      JS_InitClass(cx,                  /* JS context from which to derive runtime information */
                   obj,                 /* Object to use for initializing class (constructor arg?) */
                   parentProto,         /* parent_proto - Prototype object for the class */
                   library_clasp,       /* clasp - Class struct to init. Defs class for use by other API funs */
                   Library,             /* constructor function - Scope matches obj */
                   1,                   /* nargs - Number of arguments for constructor (can be MAXARGS) */
                   instance_props,      /* ps - props struct for parent_proto */
                   instance_methods,    /* fs - functions struct for parent_proto (normal "this" methods) */
                   NULL,                /* static_ps - props struct for constructor */
                   NULL);               /* static_fs - funcs struct for constructor (methods like Math.Abs()) */
  GPSEE_ASSERT(proto);
  return proto;
}

