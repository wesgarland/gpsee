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
 *  @file	binary_module.c		GPSEE implementation of the ServerJS binary module. This
 *					module provides ByteString and ByteArray, which are both
 *					instances of Binary.
 *  @author	Wes Garland
 *              PageMail, Inc.
 *		wes@page.ca
 *  @date	Jan 2008
 *  @version	$Id: binary_module.c,v 1.1 2009/05/27 04:51:45 wes Exp $
 */

static const char __attribute__((unused)) rcsid[]="$Id: binary_module.c,v 1.1 2009/05/27 04:51:45 wes Exp $";

#include "gpsee.h"
#include "binary_module.h"

JSClass *byteString_clasp;
JSClass *byteArray_clasp;

JSBool testMe(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
  printf("class name: %s for %p\n", JS_GET_CLASS(cx, obj)->name, obj);
  return JS_TRUE;
}

/** Initialize the module */
const char *binary_InitModule(JSContext *cx, JSObject *moduleObject)
{
  JSObject *proto;

  proto = Binary_InitClass(cx, moduleObject);
  if (proto == NULL)
    return NULL;

  if (ByteString_InitClass(cx, moduleObject, proto) == NULL)
    return NULL;

  if (ByteArray_InitClass(cx, moduleObject, proto) == NULL)
    return NULL;

  if (JS_DefineFunction(cx, moduleObject, "testMe", testMe, 0, 0) == NULL)
    abort();

  return MODULE_ID;
}

JSBool binary_FiniModule(JSContext *cx, JSObject *moduleObject)
{
  return JS_TRUE;
}

