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
 *  @file	util.c		General support code for GPSEE's gffi module
 *
 *  @author	Wes Garland
 *              PageMail, Inc.
 *		wes@page.ca
 *  @date	Sep 2009
 *  @version	$Id: util.c,v 1.1 2009/09/15 19:38:16 wes Exp $
 */

#include <gpsee.h>
#include "gffi.h"

/** Return a generic pointer back to JS for use in comparisons.
 *  Pointers in JavaScript are just hexadecimal numbers encoded with String.  
 *  Can't do numbers / valueof because there are 2^64 pointers in 64-bit 
 *  land and only 2^53 integer doubles.
 *
 *  @param	cx	JavaScript context to use
 *  @param 	vp	Returned pointer for JS consumption
 *  @param	pointer	Pointer to encode
 *  @returns	JS_FALSE if an exception has been thrown
 */
JSBool pointer_toString(JSContext *cx, void *pointer, jsval *vp)
{
  JSString		*str;
  char			ptrbuf[1 + 2 + (sizeof(void *) * 2) + 1];	/* @ 0x hex_number NUL */

  snprintf(ptrbuf, sizeof(ptrbuf), "@" GPSEE_PTR_FMT, pointer);

  str = JS_NewStringCopyZ(cx, ptrbuf);
  if (!str)
    return JS_FALSE;

  *vp = STRING_TO_JSVAL(str);

  return JS_TRUE;
}

/** Decode a JS pointer string back to a generic pointer. Intended for
 *  use in comparison operations and nothing more.
 *
 *  @param	cx		JavaScript context to use
 *  @param 	v		Encoded pointer, must come from pointer_toString()
 *  @param	pointer_p	Memory in which to return the decoded pointer
 *  @returns	JS_FALSE if an exception has been thrown
 */
JSBool pointer_fromString(JSContext *cx, jsval v, void **pointer_p, const char *throwLabel)
{
  char			*str;

  str = JS_GetStringBytes(JS_ValueToString(cx, v));
  if ((sscanf(str, "@" GPSEE_PTR_FMT, pointer_p) != 1))
    return gpsee_throw(cx, "%s.pointer.invalid", throwLabel);

  return JS_TRUE;
}

