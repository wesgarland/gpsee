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
 *  @version	$Id: gffi_module.c,v 1.7 2009/10/29 18:35:05 wes Exp $
 */

static const char __attribute__((unused)) rcsid[]="$Id: gffi_module.c,v 1.7 2009/10/29 18:35:05 wes Exp $";

#include "gpsee.h"
#include "gffi_module.h"


static JSBool gffi_errno_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  *vp = INT_TO_JSVAL(errno);
  return JS_TRUE;
}

static JSBool gffi_errno_setter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  if (JSVAL_IS_INT(*vp))
    errno = JSVAL_TO_INT(vp);
  else
  {
    jsdouble d;

    if (JS_ValueToNumber(cx, *vp, &d) == JS_FALSE)
      return JS_FALSE;

    errno = d;
    if (errno != d)
      return gpsee_throw(cx, MODULE_ID ".errno.setter.overflow");
  }
  return JS_TRUE;
}

JSObject *CFunction_proto = NULL;
/** Initialize the module */
const char *gffi_InitModule(JSContext *cx, JSObject *moduleObject)
{
  JSObject *proto;

  static JSPropertySpec gffi_props[] =
  {
    { "errno",		0, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, gffi_errno_getter, gffi_errno_setter },
    { NULL, 0, 0, NULL, NULL }
  };

  static JSFunctionSpec gffi_methods[] = 
  {
    JS_FS_END
  };

  proto = MutableStruct_InitClass(cx, moduleObject, NULL);
  if (proto == NULL)
    return NULL;

  CFunction_proto = CFunction_InitClass(cx, moduleObject, NULL);
  if (proto == NULL)
    return NULL;

  proto = Memory_InitClass(cx, moduleObject, NULL);
  if (proto == NULL)
    return NULL;

  if (!Library_InitClass(cx, moduleObject, NULL))
    return NULL;

  if (!WillFinalize_InitClass(cx, moduleObject, NULL))
    return NULL;

  if (JS_DefineProperty(cx, moduleObject, "pointerSize", INT_TO_JSVAL(sizeof(void *)), NULL, NULL, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY) == JS_FALSE) 
    return NULL;  

#define jsv(val) if (JS_DefineProperty(cx, moduleObject, #val, jsve_ ## val, NULL, NULL, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY) == JS_FALSE) return NULL;
#include "jsv_constants.decl"
#undef jsv

#include "aux_types.incl"

  if (defines_InitObjects(cx, moduleObject) != JS_TRUE)
    return NULL;

  if (JS_DefineFunctions(cx, moduleObject, gffi_methods) != JS_TRUE)
    return NULL;

  if (JS_DefineProperties(cx, moduleObject, gffi_props) != JS_TRUE)
    return NULL;

  return MODULE_ID;
}

JSBool gffi_FiniModule(JSContext *cx, JSObject *moduleObject)
{
  return JS_TRUE;
}

