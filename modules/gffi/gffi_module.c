
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
 *  @file	gffi_module.c		GPSEE Module for implementing a Foreign Function Interface.
 *
 *  @author	Wes Garland
 *              PageMail, Inc.
 *		wes@page.ca
 *  @date	May 2009
 *  @version	$Id: gffi_module.c,v 1.2 2009/07/27 21:10:47 wes Exp $
 */

static const char __attribute__((unused)) rcsid[]="$Id: gffi_module.c,v 1.2 2009/07/27 21:10:47 wes Exp $";

#include "gpsee.h"
#include "gffi_module.h"

/** Initialize the module */
const char *gffi_InitModule(JSContext *cx, JSObject *moduleObject)
{
   JSObject *proto;

  proto = MutableStruct_InitClass(cx, moduleObject, NULL);
  if (proto == NULL)
    return NULL;

  proto = CFunction_InitClass(cx, moduleObject, NULL);
  if (proto == NULL)
    return NULL;

  proto = Memory_InitClass(cx, moduleObject, NULL);
  if (proto == NULL)
    return NULL;

#define jsv(val) if (JS_DefineProperty(cx, moduleObject, #val, jsve_ ## val, NULL, NULL, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY) == JS_FALSE) return NULL;
#include "jsv_constants.decl"
#undef jsv

  if (((int)-1) < 0)
  {
    if (JS_DefineProperty(cx, moduleObject, "int", jsve_sint, 
			  NULL, NULL, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY) == JS_FALSE) 
      return NULL;
  }
  else
  {
    if (JS_DefineProperty(cx, moduleObject, "int", jsve_uint, 
			  NULL, NULL, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY) == JS_FALSE) 
      return NULL;
  }

  if (((char)-1) < 0)
  {
    if (JS_DefineProperty(cx, moduleObject, "char", jsve_schar, 
			  NULL, NULL, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY) == JS_FALSE) 
      return NULL;
  }
  else
  {
    if (JS_DefineProperty(cx, moduleObject, "char", jsve_uchar,
			  NULL, NULL, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY) == JS_FALSE) 
      return NULL;
  }

  if (defines_InitObjects(cx, moduleObject) != JS_TRUE)
    return NULL;

  return MODULE_ID;
}

JSBool gffi_FiniModule(JSContext *cx, JSObject *moduleObject)
{
  return JS_TRUE;
}

