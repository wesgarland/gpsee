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
 *  @file	Binary.c	Implementation of the Binary stub class.
 *  @author	Wes Garland
 *              PageMail, Inc.
 *		wes@page.ca
 *  @date	Jan 2008
 *  @version	$Id: BinaryStub.c,v 1.2 2010/03/06 18:17:13 wes Exp $
 *
 *  Based on https://wiki.mozilla.org/ServerJS/Binary/B
 */

static const char __attribute__((unused)) rcsid[]="$Id: BinaryStub.c,v 1.2 2010/03/06 18:17:13 wes Exp $";
#include "gpsee.h"
#include "binary.h"

#define CLASS_ID  MODULE_ID ".Binary"

/**
 *  Binary constructor
 *
 *  @param	cx	JavaScript context
 *  @param	obj	Pre-allocated Binary object
 *  @param	argc	Number of arguments passed to constructor
 *  @param	argv	Arguments passed to constructor
 *  @param	rval	The new object returned to JavaScript
 *
 *  @returns 	JS_TRUE on success
 */
static JSBool Binary(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  /* Binary() called as function. */   
  if (JS_IsConstructing(cx) != JS_TRUE)
    return gpsee_throw(cx, CLASS_ID ".constructor.notFunction: Cannot call constructor as a function!");

  /* Binary() called as constructor */
  return gpsee_throw(cx, CLASS_ID ".constructor: this class may not be instanciated!");
}  

/**
 *  Initialize the Window class prototype.
 *
 *  @param	cx	Valid JS Context
 *  @param	obj	The Global Object
 *
 *  @returns	Window.prototype
 */
JSObject *Binary_InitClass(JSContext *cx, JSObject *obj)
{
  /** Description of this class: */
  static JSClass binary_class = 
  {
    GPSEE_CLASS_NAME(Binary),		/**< its name is Binary */
    0,					/**< no special flags */
    JS_PropertyStub,  			/**< addProperty stub */
    JS_PropertyStub,  			/**< deleteProperty stub */
    JS_PropertyStub,			/**< custom getProperty */
    JS_PropertyStub, 			/**< custom setProperty */
    JS_EnumerateStub, 			/**< enumerateProperty stub */
    JS_ResolveStub,   			/**< resolveProperty stub */
    JS_ConvertStub,   			/**< convertProperty stub */
    JS_FinalizeStub,			/**< it has a custom finalizer */

    JSCLASS_NO_OPTIONAL_MEMBERS
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
 		   &binary_class,	/* clasp - Class struct to init. Defs class for use by other API funs */
		   Binary,		/* constructor function - Scope matches obj */
		   0,			/* nargs - Number of arguments for constructor (can be MAXARGS) */
		   instance_props,	/* ps - props struct for parent_proto */
		   instance_methods, 	/* fs - functions struct for parent_proto (normal "this" methods) */
		   NULL,		/* static_ps - props struct for constructor */
		   NULL); 		/* static_fs - funcs struct for constructor (methods like Math.Abs()) */

  GPSEE_ASSERT(proto);

  return proto;
}
