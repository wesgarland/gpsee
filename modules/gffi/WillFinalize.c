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
 *  @version    $Id: WillFinalize.c,v 1.2 2010/06/14 22:12:00 wes Exp $
 */

#include <gpsee.h>
#include <ffi.h>
#include "gffi.h"

#if defined(GPSEE_DEBUG_BUILD)
# define dprintf(a...) do { if (gpsee_verbosity(0) > 2) gpsee_printf(cx, "gpsee\t> "), gpsee_printf(cx, a); } while(0)
#else
# define dprintf(a...) while(0) gpsee_printf(cx, a)
#endif

/** A list of WillFinalize instances which have not yet been finalized;
 *  these instances have C-side "weak" references to CFunction instances
 *  which must be kept alive by the garbage collector.  The key of the
 *  elements in the list is the WillFinalize instance; the value is the
 *  CFunction instance (both JSObject *).
 */
static gpsee_dataStore_t pendingFinalizers;

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

/** FinalizeWith constructor. 
 *  
 *  Accepts N+1 arguments, where N is the number of arguments to the C function 
 *  that gets invoked during the finalization of this object.
 */
static JSBool WillFinalize_FinalizeWith(JSContext *cx, uintN argc, jsval *vp)
{
  cFunction_closure_t   *clos;
  cFunction_handle_t    *hnd;
  JSObject              *cfunObj = NULL;
  JSObject              *thisObj = JS_THIS_OBJECT(cx, vp);
  jsval                 *argv = JS_ARGV(cx, vp);

  return JS_TRUE;
#warning XXX WillFinalize_FinalizeWith disabled

  if (!thisObj)
    return JS_FALSE;

  if (JS_GetPrivate(cx, thisObj))
    return gpsee_throw(cx, CLASS_ID ".finalizeWith.contention: a finalizer has already installed!");

  /* Prepare a CFunction for calling. The product of this intermediate step will be pointed to by 'clos.'
   * FinalizeWith() argv begins with a CFunction instance and is followed by arguments for the invocation of said
   * CFunction. So we'll get a JSObject for the CFunction instance, and then supply cFunction_prepare() with what's
   * left of the argument vector.
   */

  /* Grab 'cfunObj' from 'argv' */
  if (!JSVAL_IS_OBJECT(argv[0]))
    return gpsee_throw(cx, CLASS_ID ".finalizeWith.arguments.0.invalidType: Arguments must begin with a CFunction"
                                    " instance, followed by a valid argument list.");
  cfunObj = JSVAL_TO_OBJECT(argv[0]);

  /* Prepare 'clos' */
  if (!cFunction_prepare(cx, cfunObj, argc-1, argv+1, &clos, &hnd, CLASS_ID ".call"))
    return JS_FALSE;

  /* Associate 'clos' to our WillFinalize instance */
  GPSEE_ASSERT(clos);
  if (clos)
  {
    JS_SetPrivate(cx, thisObj, clos);
    gpsee_ds_put(pendingFinalizers, thisObj, cfunObj);
  }

  return JS_TRUE;
}


/* cFunction_closure_use() calls and frees a prepared CFunction call represented by a
 * cFunction_closure_t instance, created by a call to cFunction_prepare()
 *
 * @param     cx
 * @param     WFObject          WillFinalize object which holds the closure to use
 *                              in its private slot.
 * @param       throwPrefix     Prefix to use for thrown errors, or NULL to not throw
 *                              when returning JS_FALSE.
 * @returns JS_TRUE on success
 */
static JSBool cFunction_closure_use(JSContext *cx, JSObject *WFObject, const char *throwPrefix)
{
  cFunction_closure_t   *clos = JS_GetInstancePrivate(cx, WFObject, WillFinalize_clasp, NULL);
  cFunction_handle_t    *hnd;
  
  if (!clos)
    return JS_TRUE;     /* Closure has previously been used */

  hnd = JS_GetInstancePrivate(cx, clos->cfunObj, cFunction_clasp, NULL); 
  GPSEE_ASSERT(hnd);
  if (!hnd)
    return throwPrefix ? gpsee_throw(cx, "%s: Missing CFunction data in cFunction_closure!", throwPrefix) : JS_FALSE;

  ffi_call(hnd->cif, hnd->fn, clos->rvaluep, clos->avalues);
  gpsee_ds_remove(pendingFinalizers, WFObject);
  cFunction_closure_free(cx, clos);
  JS_SetPrivate(cx, WFObject, NULL);

  return JS_TRUE;
}

/** This function implements an API for objects to be finalized on-demand. The finalizer is executed and removed when
 *  this is called. */
static JSBool WillFinalize_RunFinalizer(JSContext *cx, uintN argc, jsval *vp)
{
  JSObject              *thisObj = JS_THIS_OBJECT(cx, vp);

  if (!thisObj)
    return JS_FALSE;

  dprintf("%s %p\n", __func__, thisObj);

  return cFunction_closure_use(cx, thisObj, CLASS_ID ".destroy");
}

static void WillFinalize_Finalize(JSContext *cx, JSObject *thisObj)
{
  dprintf("%s %p\n", __func__, thisObj);

  (void)cFunction_closure_use(cx, thisObj, NULL);
}

static JSBool markObject(JSContext *cx, const void *key, void *value, void *_private)
{
  JSObject              *WFObject = (JSObject *)key;
  JSObject              *cfunObj = value;
  cFunction_closure_t   *clos = JS_GetInstancePrivate(cx, WFObject, WillFinalize_clasp, NULL);
  cFunction_handle_t    *hnd;

  GPSEE_ASSERT(clos);
  if (!clos)
    return JS_TRUE;     /* Closure has previously been used */

  hnd = JS_GetInstancePrivate(cx, clos->cfunObj, cFunction_clasp, NULL); 
  GPSEE_ASSERT(hnd);

  JS_MarkGCThing(cx, cfunObj, hnd->functionName, NULL);
  dprintf("marking %p for %p, closure %p (%s)\n", WFObject, cfunObj, clos, hnd->functionName);

  return JS_TRUE;
}

/** A callback which runs during garbage-collection that marks the  
 *  objects memoized by pendingFinalizers as "still alive", whether or
 *  not they are reachable from the rooted object graphs, because there
 *  are cFunction_closure finalizers have not yet been run which
 *  depend on them.   (i.e. a cFunction_closure invoking fclose()
 *  will depend on an instance of CFunction that exposes fclose()).
 *
 *  @param      cx      Any context in the current runtime
 *  @param      realm   The realm upon which the callback should operate
 *  @param      status  Phase the garbage collector is operating in
 * 
 *  @see        JS_SetGCCallback(), gpsee_addGCCallback()
 * 
 *  @returns    JS_TRUE on success, or JS_FALSE if we threw a JS exception   
 */
static JSBool WillFinalize_GCCallback(JSContext *cx, gpsee_realm_t *realm, JSGCStatus status)
{
  if (status != JSGC_MARK_END)
    return JS_TRUE;

  dprintf("%s\n", __func__);
 
  if (gpsee_ds_forEach(cx, pendingFinalizers, markObject, NULL) != JS_TRUE)
    return gpsee_throw(cx, CLASS_ID ".pendingFinalizers.iteration");

  return JS_TRUE;
}

static JSBool forceFinalize(JSContext *cx, const void *key, void *value, void *_private)
{
  dprintf("forceFinalize %p\n", key);
  WillFinalize_Finalize(cx, (JSObject *)key);
  JS_SetPrivate(cx,  (JSObject *)key, NULL);

  return JS_TRUE;
}

/** Called from FiniModule, FiniClass cleans up outstanding resources
 *  and returns JS_TRUE.  If there are pending finalizer CFunction
 *  closures, we return JS_FALSE which cascades back to the module
 *  loader, telling it not to unload this module.  The loader will
 *  try again on a subsequent GC.  During platform shutdown, a GC
 *  between global-object removal and context destruction should clear
 *  all finalizers.
 */ 
JSBool WillFinalize_FiniClass(JSContext *cx, gpsee_realm_t *realm, JSBool force)
{
  if (force)
    gpsee_ds_forEach(cx, pendingFinalizers, forceFinalize, NULL);

  if (gpsee_ds_hasData(cx, pendingFinalizers))
    return JS_FALSE;

  (void)gpsee_removeGCCallback(realm->grt, realm, WillFinalize_GCCallback);
  gpsee_ds_destroy(pendingFinalizers);
  
  return JS_TRUE;
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

  gpsee_realm_t *realm = gpsee_getRealm(cx);

  pendingFinalizers = gpsee_ds_create(realm->grt, 0, 16);
  if (!pendingFinalizers)
    return NULL;

  if (gpsee_addGCCallback(realm->grt, realm, WillFinalize_GCCallback) != JS_TRUE)
    return NULL;

  GPSEE_ASSERT(proto);
  return proto;
}



