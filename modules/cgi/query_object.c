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
 *  @file	query_object.c	A Spidermonkey object for processing CGI requests. Uses eekim's ancient cgilib.
 *  @author	Wes Garland
 *  @date	Dec 2007
 *  @version	$Id: query_object.c,v 1.3 2009/05/27 04:51:45 wes Exp $
 */

static const char __attribute__((unused)) rcsid[]="$Id: query_object.c,v 1.3 2009/05/27 04:51:45 wes Exp $";

#include "gpsee.h"
#include <cgi-lib.h>
#include "cgi_module.h"

#define OBJECT_ID MODULE_ID ".query"

typedef struct
{
  llist	*query;
  const char *uploadDir;
} query_private_t;

void query_Finalize(JSContext *cx, JSObject *obj)
{
  query_private_t	*hnd = JS_GetPrivate(cx, obj);

  if (!hnd)
    return;

#if defined(fixed_libcgihtml)
  if (hnd->query)
    magic_free_function(hnd->query);
#else
#warning minor core leak	/* Once per script interpreter. EEK does not provide a freer */
#endif

  if (hnd->uploadDir)
  {
    rmdir(hnd->uploadDir);	/* this should be recursive */
    JS_free(cx, (void *)hnd->uploadDir);
  }

  JS_free(cx, hnd);
}

/** Needs nargs to be at least 2 for GC roots.
 *  
 *  JS method returns true if values were read.
 */
static JSBool query_readQuery(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  extern rc_list	rc;
  query_private_t		*hnd = JS_GetPrivate(cx, obj);
  node			*n;
  JSString		*str;
  int			retval;
  jsrefcount		depth;
  jsval			val;

  if (hnd->query)
    return gpsee_throw(cx, OBJECT_ID ".readQuery.alreadyRead");
  else
    hnd->query = JS_malloc(cx, sizeof(*hnd->query));

  if (argc > 0) 
  { 
    str = JS_ValueToString(cx, argv[0]);
    hnd->uploadDir = JS_strdup(cx, JS_GetStringBytes(str));
  }
  else
    hnd->uploadDir = rc_value(rc, OBJECT_ID ".default_upload_dir");

  /* fix this later by making upload files == /dev/null in libcgihtml.so when upload dir is null */
  if (!hnd->uploadDir)
    gpsee_log(SLOG_NOTICE, "Unspecified upload dir is a security problem! Specify rc." OBJECT_ID ".default_upload_dir!");

  depth = JS_SuspendRequest(cx);
  retval = read_cgi_input(hnd->query, hnd->uploadDir);
  JS_ResumeRequest(cx, depth);

  *rval = retval ? JSVAL_TRUE : JSVAL_FALSE;

  for (n = hnd->query->head; n; n = n->next)
  {
    /* temporary fix? we don't want to overwrite our module readQuery() member */
    if (!strcmp(n->entry.name,"readQuery"))
      continue;

    if (n->entry.value == NULL)
      n->entry.value = strdup("");	/* Match eekim's horrible semantics */

    /* later optimization note: almost all of the cases where we'd need to
     * create arrays instead of strings are also cases where the CGI
     * variables of the same name are adjacent in the query
     */

    if ((JS_GetProperty(cx, obj, n->entry.name, &val) == JS_TRUE) && (!JSVAL_IS_VOID(val)))
    {
      /* Already seen a CGI variable with this name */

      if (JSVAL_IS_STRING(val))	/* Only seen one, create a new array */
      {
	argv[0] = val;
	argv[1] = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, n->entry.value));

	argv[0] = OBJECT_TO_JSVAL(JS_NewArrayObject(cx, 2, argv));

	if (JS_SetProperty(cx, obj, n->entry.name, argv + 0) == JS_FALSE)
	  return gpsee_throw(cx, OBJECT_ID ".readQuery.property.recreate.%s", n->entry.name);
      }
      else
      {
	jsuint length;

	/* seen many: append to the array */
	if (JS_IsArrayObject(cx, JSVAL_TO_OBJECT(val)) != JS_TRUE)
	  return gpsee_throw(cx, OBJECT_ID ".readQuery.property.type.%s", n->entry.name);

	if (JS_GetArrayLength(cx, JSVAL_TO_OBJECT(val), &length) != JS_TRUE)
	  return gpsee_throw(cx, OBJECT_ID ".readQuery.property.%s.length.get", n->entry.name);

	if (JS_DefineElement(cx, JSVAL_TO_OBJECT(val), length, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, n->entry.value)),
			     JS_PropertyStub, JS_PropertyStub, JSPROP_ENUMERATE) != JS_TRUE)
	  return gpsee_throw(cx, OBJECT_ID ".readQuery.property.%s.element.%u", n->entry.name, length);

	if (JS_SetArrayLength(cx, JSVAL_TO_OBJECT(val), length) != JS_TRUE)
	  return gpsee_throw(cx, OBJECT_ID ".readQuery.property.%s.length.set", n->entry.name);
      }
    }
    else
    {
      /* First time for this property, insert it as a string */
      argv[0] = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, n->entry.value));

      if (JS_SetProperty(cx, obj, n->entry.name, argv + 0) == JS_FALSE)
	return gpsee_throw(cx, OBJECT_ID ".readQuery.property.create.%s", n->entry.name);
    }
  }
  
  return JS_TRUE;
}

JSBool query_FiniObject(JSContext *cx, JSObject *moduleObject)
{
  return JS_TRUE;
}

JSObject *query_InitObject(JSContext *cx, JSObject *moduleObject)
{
  JSObject 		*queryObject;
  query_private_t	*hnd;

  static JSClass query_class = 	/* not exposed to JS, but finalizer needed when GC */
  {	
    GPSEE_CLASS_NAME(Query),
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  
    JS_PropertyStub,  
    JS_PropertyStub,  
    JS_PropertyStub,
    JS_EnumerateStub, 
    JS_ResolveStub,   
    JS_ConvertStub,   
    query_Finalize,
    JSCLASS_NO_OPTIONAL_MEMBERS
  };

  static JSFunctionSpec query_methods[] = 
  {
    { "readQuery",		query_readQuery,		2, 0, 0 },
    { NULL,			NULL,				0, 0, 0 }
  };

  static JSPropertySpec query_props[] = 
  {
    { NULL, 0, 0, NULL, NULL }
  };

  queryObject = JS_NewObject(cx, &query_class, NULL, moduleObject);
  if (!queryObject)
    return NULL;

  if (!JS_DefineFunctions(cx, queryObject, query_methods) || !JS_DefineProperties(cx, queryObject, query_props))
    return NULL;

  hnd = JS_malloc(cx, sizeof(*hnd));
  if (!hnd)
  {
    JS_ReportOutOfMemory(cx);
    return NULL;
  }

  memset(hnd, 0, sizeof(*hnd));
  JS_SetPrivate(cx, queryObject, hnd);

  return queryObject;
}


