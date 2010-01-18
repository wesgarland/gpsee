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
 *  @file       WillFinalize.c  Support code for GPSEE's gffi module which
 *                              provides a JSClass with a __finalizeWith__()
 *                              member that accepts CFunction invocations.
 *
 *  @author     Donny Viszneki
 *              PageMail, Inc.
 *              hdon@page.ca
 *  @date       Sep 2009
 *  @version    $Id$
 */

#include <gpsee.h>
#include <ffi.h>
#include "gffi_module.h"

#define CLASS_ID MODULE_ID ".WillFinalize"

/* @jazzdoc gffi.WillFinalize
 *
 * GFFI provides the Javascript programmer with new powers and new responsibilities. Javascript programs cannot provide
 * finalizers/destructors to clean up after objects get garbage collected. This JSClass bypasses this limitation in a
 * manner suited specifically to the GFFI module. The goal of WillFinalize is to clean up non-Javascript resources when
 * they become unreachable by Javascript code. This goal is congruent with Spidermonkey's limitation that JSClass finalizers
 * may not invoke Javascript functionality. This means that the WillFinalize facility can only be used to invoke CFunctions.
 *
 * You will probably never instantiate deal with WillFinalize directly. Instead, gffi.CFunction.prototype.call() returns
 * values with instance methods which will do that for you. Of course, primitive values can't have JSClasses, so we box
 * them with gffi.BoxedPrimitive. If ever in the future gffi.CFunction.prototype.call() can return a non-primitive type,
 * then the 'finalizer' and 'finalizeWith' properties are simply tacked onto the return value. You can bypass all of this
 * by using gffi.CFunction.prototype.unboxedCall().
 *
 * You will want to utilize the power of WillFinalize through the gffi.BoxedPrimitive.prototype.finalizeWith() instance method.
 * That method will create an instance of WillFinalize if it does not already exist in the 'finalizer' property of the
 * BoxedPrimitive. As long as this is the only reference to the WillFinalize instance, it will get cleaned up at the same time
 * that the BoxedPrimitive does.
 *
 * @form new gffi.WillFinalize(void)
 *
 * See gffi.WillFinalize.prototype.finalizeWith() for more usage information.
 */
/* @jazzdoc gffi.WillFinalize.prototype.finalizeWith()
 *
 * This instance method registers a finalizer for this WillFinalize instance.
 *
 * @form instance.finalizeWith(CFunction instance, arguments...);
 *
 * When 'instance' is garbage collected, its JSClass finalizer will execute the given CFunction, passing the arguments given
 * after it as though they had been provided to the CFunction.prototype.call() instance method of the CFunction instance
 * given as an argument.
 *
 * *IMPORTANT* CFunctions do some magic things to turn your Javascript arguments into typical C-language arguments. The 
 * arguments you provide here will be evaluated at the time you invoke this instance method, not at the time the finalizer
 * gets executed!
 */
JSClass *WillFinalize_clasp;

/**
 *  Implements the WillFinalize constructor.
 *
 *  The WillFinalize Javascript constructor accepts zero arguments.
 *
 *  @param    cx
 *  @param    obj
 *  @param    argc
 *  @param    argv
 *  @param    rval
 *
 *  @returns  JS_TRUE on success
 */
static JSBool WillFinalize(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  /* Javascript programmer must invoke WillFinalize as a constructor */
  if (JS_IsConstructing(cx) != JS_TRUE)
    return gpsee_throw(cx, CLASS_ID ".constructor.notFunction: Must call constructor with 'new'!");

  return JS_TRUE;
}

static JSBool WillFinalize_FinalizeWith(JSContext *cx, uintN argc, jsval *vp)
{
  cFunction_closure_t *clos, *closOld;
  JSObject *cfuncObj = NULL;
  JSObject *thisObj = JS_THIS_OBJECT(cx, vp);
  jsval *argv = JS_ARGV(cx, vp);

  /* Currently we only support one finalizer CFunction closure, so we will check for it here. */
  closOld = (cFunction_closure_t*) JS_GetInstancePrivate(cx, thisObj, WillFinalize_clasp, NULL);
  if (closOld)
    gpsee_log(SLOG_WARNING, CLASS_ID ".finalizeWith.contention: a finalizer has already installed!");
  /* TODO support multiple finalizers? */

  /* Prepare a CFunction for calling. The product of this intermediate step will be pointed to by 'clos.'
   * FinalizeWith() argv begins with a CFunction instance and is followed by arguments for the invocation of said
   * CFunction. So we'll get a JSObject for the CFunction instance, and then supply cFunction_prepare() with what's
   * left of the argument vector. */

  /* Grab 'cfuncObj' from 'argv' */
  if (!JSVAL_IS_OBJECT(argv[0]))
    return gpsee_throw(cx, CLASS_ID ".finalizeWith.arguments.0.invalidType: Arguments must begin with a CFunction"
                                    " instance, followed by a valid argument list.");
  cfuncObj = JSVAL_TO_OBJECT(argv[0]);

  /* Prepare 'clos' */
  if (!cFunction_prepare(cx, cfuncObj, argc-1, argv+1, &clos, CLASS_ID ".call"))
    return JS_FALSE;

  /* Associate 'clos' to our WillFinalize instance */
  if (clos)
  {
    JS_SetPrivate(cx, thisObj, clos);
    if (closOld)
      cFunction_closure_free(cx, closOld);
  }

  return JS_TRUE;
}
/** This function implements an API for objects to be finalized on-demand. The finalizer is executed and removed when
 *  this is called. */
static JSBool WillFinalize_RunFinalizer(JSContext *cx, uintN argc, jsval *vp)
{
  cFunction_closure_t *clos;
  JSObject *thisObj = JS_THIS_OBJECT(cx, vp);

  /* Currently we only support one finalizer CFunction closure, so we will check for it here. */
  clos = (cFunction_closure_t*) JS_GetInstancePrivate(cx, thisObj, WillFinalize_clasp, NULL);
  if (clos)
    cFunction_closure_call(cx, clos);

  /* Remove the finalizer now, maybe the user will add another one, who knows, but we shouldn't default to double-calling it */
  JS_SetPrivate(cx, thisObj, NULL);
  return JS_TRUE;
}
static void WillFinalize_Finalize(JSContext *cx, JSObject *obj)
{
  /* If we have private data, it is a cFunction_closure_t waiting to be called */
  cFunction_closure_t *clos = JS_GetInstancePrivate(cx, obj, WillFinalize_clasp, NULL);
  if (!clos)
    return;

  cFunction_closure_call(cx, clos);
}

/**
 *  Initialize the WillFinalize JSClass and add its constructor to the JS object graph.
 *
 *  @param      cx            Valid JS Context
 *  @param      obj           The object that will be given a property referencing the WillFinalize constructor.
 *  @param      parentProto   Grandparent prototype of WillFinalize instances
 *
 *  @returns    WillFinalize.prototype
 */
JSObject *WillFinalize_InitClass(JSContext *cx, JSObject *obj, JSObject *parentProto)
{
  static JSClass WillFinalize_class =
  {
    GPSEE_CLASS_NAME(WillFinalize),     /**< name */
    JSCLASS_HAS_PRIVATE,                /**< private slot in use */
    JS_PropertyStub,                    /**< addProperty stub */
    JS_PropertyStub,                    /**< deleteProperty stub */
    JS_PropertyStub,                    /**< getProperty stub */
    JS_PropertyStub,                    /**< setProperty stub */
    JS_EnumerateStub,                   /**< enumerateProperty stub */
    JS_ResolveStub,                     /**< resolveProperty stub */
    JS_ConvertStub,                     /**< convertProperty stub */
    WillFinalize_Finalize,              /**< it has a custom finalizer */

    JSCLASS_NO_OPTIONAL_MEMBERS
  };
  WillFinalize_clasp = &WillFinalize_class;

  static JSFunctionSpec instance_methods[] = 
  {
    JS_FN("finalizeWith", WillFinalize_FinalizeWith,      0, 0),
    JS_FN("runFinalizer", WillFinalize_RunFinalizer,      0, 0),
    JS_FS_END
  };

  JSObject *proto =
      JS_InitClass(cx,                  /* JS context from which to derive runtime information */
                   obj,                 /* Object to use for initializing class (constructor arg?) */
                   parentProto,         /* parent_proto - Prototype object for the class */
                   WillFinalize_clasp,  /* clasp - Class struct to init. Defs class for use by other API funs */
                   WillFinalize,        /* constructor function - Scope matches obj */
                   0,                   /* nargs - Number of arguments for constructor (can be MAXARGS) */
                   NULL,                /* ps - props struct for parent_proto */
                   instance_methods,    /* fs - functions struct for parent_proto (normal "this" methods) */
                   NULL,                /* static_ps - props struct for constructor */
                   NULL);               /* static_fs - funcs struct for constructor (methods like Math.Abs()) */

  GPSEE_ASSERT(proto);
  return proto;
}

