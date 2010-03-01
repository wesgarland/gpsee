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
 * Copyright (c) 2007-2009, PageMail, Inc. All Rights Reserved.
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
 *  @file	PHPSession_class.c	A Spidermonkey class for reading PHP session files with libphpsess.so
 *  @author	Wes Garland
 *  @date	Dec 2007
 *  @version	$Id: PHPSession_class.c,v 1.2 2009/03/31 15:12:13 wes Exp $
 */
static __attribute__((unused)) const char rcsid[]="$Id: PHPSession_class.c,v 1.2 2009/03/31 15:12:13 wes Exp $";

#include "gpsee.h"
#include "cgi.h"

#if defined(HAVE_APR)
#include <phpsess.h>

#define CLASS_ID MODULE_ID ".PHPSession"

void PHPSession_Finalize(JSContext *cx, JSObject *obj)
{
  apr_pool_t *pool = JS_GetPrivate(cx, obj);

  if (pool)
    apr_pool_destroy(pool);
}

/** Can call JS constructor with either 0 or 1 args. 0 args pulls session PHPSESSID from HTTP_COOKIE, 1 arg specifies session id */
static JSBool PHPSession(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  jsrefcount		depth;
  apr_status_t		status;

  const char		*PHPSessionID;
  apr_pool_t		*pool;
  apr_hash_t		*phpSession;

  apr_hash_index_t      *hashLoc;
  apr_ssize_t           klen;
  const char            *key;

  if (JS_IsConstructing(cx) != JS_TRUE)
    return gpsee_throw(cx, CLASS_ID ".constructor.notFunction: Cannot call constructor as a function!");

  *rval = OBJECT_TO_JSVAL(obj);

  depth = JS_SuspendRequest(cx);
  status = apr_pool_create(&pool, NULL);
  JS_ResumeRequest(cx, depth);

  if (status != APR_SUCCESS)
  {
    JS_ReportOutOfMemory(cx);
    return FALSE;
  }

  JS_SetPrivate(cx, obj, pool);

  if (argc == 0) 
  { 
    const char	*cookies = getenv("HTTP_COOKIE");
    char	*s, *sPHPSessionID;

    if (!cookies)
      return gpsee_throw(cx, CLASS_ID ".constructor.cookies.none");

    s = strstr(apr_pstrdup(pool, cookies), "PHPSESSID=");
    if (!s)
      return gpsee_throw(cx, CLASS_ID ".constructor.cookies.noSessionID");

    /* Determine PHP Session ID from HTTP Cookie */
    sPHPSessionID = s + (sizeof("PHPSESSID=") - 1);
    s = sPHPSessionID + strspn(sPHPSessionID, "0123456789abcdef");
    *s = (char)0;
    PHPSessionID = sPHPSessionID;
  }
  else
  {
    PHPSessionID = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
  }

  depth = JS_SuspendRequest(cx);
  status = phpSession_load(pool, pool, PHPSessionID, &phpSession);
  JS_ResumeRequest(cx, depth);

  if (status != APR_SUCCESS)
  {
    char errbuf[128];

    switch(status)
    {
      case APR_ENOENT:
        return gpsee_throw(cx, CLASS_ID ".constructor.gone: %s", apr_strerror(status, errbuf, sizeof(errbuf)));

      default:
	return gpsee_throw(cx, CLASS_ID ".constructor.%i: %s", status, apr_strerror(status, errbuf, sizeof(errbuf)));
    }
  }
  
  if (!phpSession)
    return gpsee_throw(cx, CLASS_ID ".constructor.session.empty");

  for (status = APR_SUCCESS, hashLoc = apr_hash_first(pool, phpSession);
       hashLoc && (status == (APR_SUCCESS));
       hashLoc = apr_hash_next(hashLoc))
  {
    php_value_t *value;

    apr_hash_this(hashLoc, (const void **)&key, &klen, (void **)&value);

    if (!key || !klen)
      continue;

    if (key[klen])	/** 99.9999% sure this is impossible, but libphpsess.so might change? */
    {
      char *newkey = strncpy(apr_palloc(pool, klen + 1), key, klen);
      newkey[klen] = (char)0;
      key = newkey;
    }

    switch(value->type)
    {
      case php_string:
	argv[0] = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, value->data.string));
	if (JS_SetProperty(cx, obj, key, argv + 0) == JS_FALSE)
	  return gpsee_throw(cx, CLASS_ID ".constructor.property.create.string.%s", key);
	break;

      case php_integer:
	argv[0] = INT_TO_JSVAL(value->data.integer);
	if (JS_SetProperty(cx, obj, key, argv + 0) == JS_FALSE)
	  return gpsee_throw(cx, CLASS_ID ".constructor.property.create.integer.%s", key);
	break;

      case php_date:
	gpsee_throw(cx, CLASS_ID ".constructor.property.date.unsupported.%s", key);	/* not in libphpsess.so either */
	break;

      case php_null:
	if (JS_SetProperty(cx, obj, JSVAL_NULL, argv + 0) == JS_FALSE)
	  return gpsee_throw(cx, CLASS_ID ".constructor.property.create.null.%s", key);
	break;
    }
  }

  JS_SetPrivate(cx, obj, NULL);

  depth = JS_SuspendRequest(cx);
  apr_pool_destroy(pool);
  JS_ResumeRequest(cx, depth);

  return JS_TRUE;
}

JSBool PHPSession_FiniClass(JSContext *cx, JSObject *proto)
{
  return JS_TRUE;
}

JSObject *PHPSession_InitClass(JSContext *cx, JSObject *obj)
{
  static JSClass phpsess_class = 
  {
    GPSEE_CLASS_NAME(PHPSession),
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  
    JS_PropertyStub,  
    JS_PropertyStub,  
    JS_PropertyStub,
    JS_EnumerateStub, 
    JS_ResolveStub,   
    JS_ConvertStub,   
    PHPSession_Finalize,
    JSCLASS_NO_OPTIONAL_MEMBERS
  };

  static JSFunctionSpec phpsess_methods[] = 
  {
    { NULL,			NULL,				0, 0, 0 }
  };

  JSObject *proto = 
      JS_InitClass(cx, 			/* JS context from which to derive runtime information */
		   obj, 		/* Object to use for initializing class (constructor arg?) */
		   NULL, 		/* parent_proto - Prototype object for the class */
		   &phpsess_class, 	/* clasp - Class struct to init. Defs class for use by other API funs */
		   PHPSession,		/* constructor function - Scope matches obj */
		   1,			/* nargs - Number of arguments for constructor */
		   NULL,		/* ps - props struct for parent_proto */
		   phpsess_methods, 	/* fs - functions struct for parent_proto (normal "this" methods) */
		   NULL, 		/* static_ps - props struct for constructor */
		   NULL);		/* static_fs - funcs struct for constructor (methods like Math.Abs()) */

  return proto;
}
#endif /* HAVE_APR */
