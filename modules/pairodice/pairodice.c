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
 * ***** END LICENSE BLOCK ***** 
 */

/**
 *  @file	pairodice_module.c	A GPSEE module implementing a native class for simulating die-pair rolling.
 *					This is an exemplar module, provided to show how trivial native modules
 *					are implemented.
 *
 *  @author	Wes Garland
 *              PageMail, Inc.
 *		wes@page.ca
 *  @date	Jan 2008
 *  @version	$Id: pairodice.c,v 1.3 2011/04/04 18:17:52 wes Exp $
 *
 *  @note	To use this module, you must remove it from the IGNORE_MODULES list in the
 *		main GPSEE Makefile, then re-build and re-install.
 */

 /*
  * Sample Program:
  *
  * 	var PairODice = require("pairodice").PairODice;
  * 	var dice = new PairODice();
  *
  * 	print("First  roll: " + dice.roll());
  * 	print("Second roll: " + dice.roll());
  * 	print("Third  roll: " + dice.roll());
  */

static const char __attribute__((unused)) rcsid[]="$Id: pairodice.c,v 1.3 2011/04/04 18:17:52 wes Exp $";

#include "gpsee.h"

#define MODULE_ID GPSEE_GLOBAL_NAMESPACE_NAME ".pairodice"
#define CLASS_ID MODULE_ID ".PairODice"
static JSClass *pod_clasp;

/** Private C data used by instances of the PairODice class */
typedef struct
{
  unsigned int 	seed;
  int 		lastRoll;
} dice_private_t;

/** 
 *  Implements the PairODice Constructor.
 *
 *  @param	cx	JavaScript context
 *  @param	obj	Pre-allocated PairODice object
 *  @param	argc	Number of arguments passed to constructor
 *  @param	argv	Arguments passed to constructor
 *  @param	rval	The new object returned to JavaScript
 *
 *  @returns 	JS_TRUE on success
 */
static JSBool PairODice(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  dice_private_t	*private;

  if (JS_IsConstructing(cx) != JS_TRUE)
    return gpsee_throw(cx, MODULE_ID ".PairODice.constructor.notFunction: Cannot call constructor as a function!");

  /* JS passed us a pre-made object to initialize. 
   * Make that object the constructor's return value.
   */
  *rval = OBJECT_TO_JSVAL(obj);

  private = JS_malloc(cx, sizeof(*private));
  if (!private)
    return JS_FALSE;

  memset(private, 0, sizeof(*private));

  /* Use the private member to maintain separate random number seeds for each pair of dice.
   * We seed the random number generator with entroy derived from the system memory
   * allocator, clock, and process ID.
   */
  private->seed = (((size_t)private >> 3) + time(NULL) + getpid()) % RAND_MAX;
  (void)rand_r(&private->seed);
  JS_SetPrivate(cx, obj, private);

  return JS_TRUE;
}  

/** 
 *  PairODice Finalizer.
 *
 *  Called by the JS garbage collector, this routine release all resources
 *  used by an instance of the class when the object is collected.
 *
 *  @note  The finalizer will also be called on the class prototype
 *         during shutdown.
 *
 *  @param	cx	JavaScript context
 *  @param	obj	The object to finalize
 */
static void PairODice_Finalize(JSContext *cx, JSObject *obj)
{
  dice_private_t *private = JS_GetPrivate(cx, obj);

  if (private)
    free(private);

  return;
}

/** 
 *  Implements PairODice.prototype.roll(). 
 *
 *  @param	cx	JavaScript context
 *  @param	obj	The object on which the method is being called
 *  @param	argc	The number of arguments passed to the method
 *  @param	argv	The arguments passed to the method
 *  @param	rval	The return value from the method
 *
 *  @returns	JS_TRUE	On success
 */
static JSBool pod_roll(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  dice_private_t 	*private = JS_GetInstancePrivate(cx, obj, pod_clasp, NULL);
  int			dieOne, dieTwo;

  if (!private)
    return gpsee_throw(cx, CLASS_ID ".roll.instanceof: cannot apply method on this object");

  dieOne = (rand_r(&private->seed) % 6) + 1;
  dieTwo = (rand_r(&private->seed) % 6) + 1;

  private->lastRoll = dieOne + dieTwo;

  *rval = INT_TO_JSVAL(private->lastRoll);

  return JS_TRUE;
}

/**
 *  Implements the getter for PairOfDice instance
 *  property lastRoll.
 *
 *  @param	cx	JavaScript context
 *  @param	obj	The object on which the method is being called
 *  @param	id	TinyID for this property (unused)
 *  @param	vp	The value gotten from the getter
 *  
 *  @returns	JS_TRUE	On success
 */
static JSBool pod_lastRoll_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  dice_private_t 	*private = JS_GetInstancePrivate(cx, obj, pod_clasp, NULL);

  if (!private)
    return gpsee_throw(cx, CLASS_ID ".roll.instanceof: cannot apply method on this object");

  *vp = INT_TO_JSVAL(private->lastRoll);

  return JS_TRUE;
}
 
/**
 *  Initialize the PairODice class prototype.
 *
 *  @param	cx	Valid JS Context
 *  @param	obj	The Module's Exports Object
 *
 *  @returns	PairODice.prototype
 */
static JSObject *PairODice_InitClass(JSContext *cx, JSObject *obj)
{
  static JSClass pod_class = 
  {
    GPSEE_CLASS_NAME(PairODice),	/**< its name is PairODice */
    JSCLASS_HAS_PRIVATE,		/**< it uses JS_SetPrivate() */
    JS_PropertyStub,  
    JS_PropertyStub,  
    JS_PropertyStub,  
    JS_PropertyStub,
    JS_EnumerateStub, 
    JS_ResolveStub,   
    JS_ConvertStub,   
    PairODice_Finalize,			/**< it has a custom instance finalizer to clean up "private C data" */
    JSCLASS_NO_OPTIONAL_MEMBERS
  };

  /** Methods of PairODice.prototype */
  static JSFunctionSpec pod_methods[] = 
  {
    { "roll",			pod_roll,			0, 0, 0 },
    { NULL,			NULL,				0, 0, 0 }
  };

  /** Properties of PairODice.prototype */
  static JSPropertySpec pod_props[] = 
  {
    { "lastRoll", 0,   	JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY,	pod_lastRoll_getter,  	JS_PropertyStub },
    { NULL, 0, 0, NULL, NULL }
  };

  JSObject *proto = 
      JS_InitClass(cx, 			/* JS context from which to derive runtime information */
		   obj, 		/* Object to use for initializing class (constructor arg?) */
		   NULL, 		/* parent_proto - Prototype object for the class */
 		   &pod_class, 		/* clasp - Class struct to init. Defs class for use by other JSAPI funs, e.g. JS_GetClass() */
		   PairODice,		/* constructor function - Scope matches obj */
		   0,			/* nargs - Number of arguments for constructor (can be MAXARGS) */
		   pod_props,		/* ps - props struct for parent_proto */
		   pod_methods, 	/* fs - functions struct for parent_proto (normal "this" methods) */
		   NULL,		/* static_ps - static props struct for constructor */
		   NULL); 		/* static_fs - static funcs struct for constructor (methods like Math.Abs()) */

  pod_clasp = &pod_class;
  return proto;
}

/** Initialize the module, adding the PairODice export */
const char *pairodice_InitModule(JSContext *cx, JSObject *moduleObject)
{
  JSObject *prototype = PairODice_InitClass(cx, moduleObject);

  if (!prototype)
    return NULL;

  return MODULE_ID;
}

/**
 *  Finalize Module.  If we return JS_TRUE, that means this module has no (important) internal state, 
 *  which means that it is safe to unload the module if it was loaded as a dynamic shared object (DSO).
 */
JSBool pairodice_FiniModule(JSContext *cx, JSObject *moduleObject)
{
  return JS_TRUE;	/* Safe to unload this if it was running as DSO module */
}
