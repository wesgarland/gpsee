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
 *  @file	cgi_module.c		A GPSEE module for exposing CGI-related services to JS.
 *  @author	Wes Garland
 *              PageMail, Inc.
 *		wes@page.ca
 *  @date	Jan 2008
 *  @version	$Id: cgi.c,v 1.3 2011/12/05 19:13:37 wes Exp $
 */

static const char __attribute__((unused)) rcsid[]="$Id: cgi.c,v 1.3 2011/12/05 19:13:37 wes Exp $";

#include "gpsee.h"
#include "cgi.h"

const char *cgi_InitModule(JSContext *cx, JSObject *moduleObject)
{
  JSObject *queryObject;

  if ((queryObject = query_InitObject(cx, moduleObject)) == NULL)
    return NULL;

  JS_DefineProperty(cx, moduleObject, "query", OBJECT_TO_JSVAL(queryObject), NULL, NULL, 
		    JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT);

#ifdef HAVE_APR
  if (PHPSession_InitClass(cx, moduleObject) == NULL)
    return NULL;
#endif

  return MODULE_ID;
}

JSBool cgi_FiniModule(JSContext *cx, gpsee_realm_t *realm, JSObject *moduleObject, JSBool force)
{
  JSBool b = JS_TRUE;

#ifdef HAVE_APR
  b &= PHPSession_FiniClass(cx, moduleObject);
#endif
  b &= query_FiniObject(cx, moduleObject);

  return b;
}

