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
 *  @file	binary.c		GPSEE implementation of the ServerJS binary module. This
 *					module provides ByteString and ByteArray, which are both
 *					instances of Binary.
 *  @author	Wes Garland
 *              PageMail, Inc.
 *		wes@page.ca
 *  @date	Jan 2008
 *  @version	$Id: binary.c,v 1.4 2011/12/05 19:13:37 wes Exp $
 */

static const char __attribute__((unused)) rcsid[]="$Id: binary.c,v 1.4 2011/12/05 19:13:37 wes Exp $";

#include "gpsee.h"
#define _BINARY_MODULE_C
#include "binary.h"
#include "base64.h"

JSClass *byteString_clasp;
JSClass *byteArray_clasp;

/** Static method of binary module which can convert a bytething into
 *  a JS String, encoding in base 64 along the way.
 */
static JSBool binary_toBase64(JSContext *cx, int argc, jsval *vp)
{
  jsval                 *argv = JS_ARGV(cx, vp);
  JSObject              *obj;
  byteThing_handle_t    *hnd;
  char                  *s;
  JSString              *str;
  
  if (argc != 1)
    return gpsee_throw(cx, MODULE_ID ".toBase64.arguments.count");
  if (!JSVAL_IS_OBJECT(argv[0]))
    return gpsee_throw(cx, MODULE_ID ".toBase64.arguments.0: not an object");
  obj = JSVAL_TO_OBJECT(argv[0]);
  if (!gpsee_isByteThing(cx, obj))
    return gpsee_throw(cx, MODULE_ID ".toBase64.arguments.0: not a ByteThing");
  hnd = JS_GetPrivate(cx, obj);
  if (!hnd)
    return gpsee_throw(cx, MODULE_ID ".toBase64.arguments.0: ByteThing handle missing!");

  if (hnd->length)
  {
    s = JS_malloc(cx, ((((hnd->length) + 2) / 3) * 4) + 1);
    if (!s)
      return JS_FALSE;
    (void)binary_to_b64(hnd->buffer, hnd->length, s);

    str = JS_NewStringCopyZ(cx, s);
    JS_free(cx, s);
  }
  else
  {
    str = JS_NewStringCopyN(cx, "", 0);
  }

  if (!str)
    return JS_FALSE;

  JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(str));
  return JS_TRUE;
}

/** Static method of binary module which can convert a JS string
 *  into a bytething, decoding base 64 along the way
 */
static JSBool binary_fromBase64(JSContext *cx, int argc, jsval *vp)
{
  jsval                 *argv = JS_ARGV(cx, vp);
  JSString              *str;
  const char            *s;
  unsigned char         *buf;
  JSObject              *obj;
  byteThing_handle_t    *hnd;

  if (argc != 1)
    return gpsee_throw(cx, MODULE_ID ".fromBase64.arguments.count");

  if (JSVAL_IS_STRING(argv[0]))
    str = JSVAL_TO_STRING(argv[0]);
  else
  {
    str = JS_ValueToString(cx, argv[0]);
    if (!str)
      return JS_FALSE;
  }

  s = JS_GetStringBytesZ(cx, str);
  if (!s)
    return JS_FALSE;
  
  obj = gpsee_newByteThing(cx, NULL, ((strlen(s) / 4) * 3) + 2, JS_TRUE);
  hnd = JS_GetPrivate(cx, obj);
  buf = b64_to_binary(cx, s, hnd->buffer, &hnd->length);
  if (!buf)
    return JS_FALSE;

  JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(obj));
  return JS_TRUE;
}  
  

/** Initialize the module */
const char *binary_InitModule(JSContext *cx, JSObject *moduleObject)
{
  JSObject *proto;

  static JSFunctionSpec binary_static_methods[] = 
  {
    JS_FN("toBase64",               binary_toBase64,                0, 0),
    JS_FN("fromBase64",             binary_fromBase64,              0, 0),
/** @todo
    JS_FN("toQuotedPrintable",      binary_toQuotedPrintable,       0, 0),
    JS_FN("fromQuotedPrintable",    binary_fromQuotePrintable,      0, 0),
*/
    { NULL, NULL, 0, 0, 0 },
  };

  if (JS_DefineFunctions(cx, moduleObject, binary_static_methods) != JS_TRUE)
    return NULL;

  proto = Binary_InitClass(cx, moduleObject);
  if (proto == NULL)
    return NULL;

  if (ByteString_InitClass(cx, moduleObject, proto) == NULL)
    return NULL;

  if (ByteArray_InitClass(cx, moduleObject, proto) == NULL)
    return NULL;

  
  return MODULE_ID;
}

JSBool binary_FiniModule(JSContext *cx, gpsee_realm_t *realm, JSObject *moduleObject, JSBool force)
{
  return JS_TRUE;
}

