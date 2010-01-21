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
 * The Initial Developer of the Original Code is Nick Galbreath
 *
 * Portions created by the Initial Developer are
 * Copyright (c) 2009, Nick Galbreath. All Rights Reserved.
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
 * ***** END LICENSE /**
 *  @file   curl_module.c       GPSEE wrapper around the easycurl interface of the
 *                              libcurl networking library http://curl.haxx.se/libcurl/c/
 *  @author Nick Galbreath
 *          client9 LLC
 *          nickg@client9.com
 *  @date   Jan 2010
 *  @version    $Id: $
 */

// THIRDPARTY
#include <curl/curl.h>

// LOCAL
#include "Memory.h"

#include "gpsee.h"

#include "libcurl_constants.h"
#include "libcurl_curlinfo.h"

// TBD on namespace
#define MODULE_ID GPSEE_GLOBAL_NAMESPACE_NAME ".module.ca.page.curl"
static const char __attribute__((unused)) rcsid[]="$Id: $";

struct callback_data {
  CURL* handle;

  // used in callbacks
  JSContext* cx;
  JSObject* obj;
};

/**
 * easycurl "slist" -- just a linked list of data
 *   only operations are "append".. and implied
 *   ctors/dtors.
 *
 */
static JSClass easycurl_slist_class;


static JSBool easycurl_slist_valueOf(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
  struct curl_slist *slist=NULL;
  slist = (struct curl_slist *)(JS_GetPrivate(cx, obj));
  JSObject* ary = JS_NewArrayObject(cx, 0, NULL);
  *rval = OBJECT_TO_JSVAL(ary);
  int count = 0;
  while (slist != NULL) {
    JSString* s = JS_NewStringCopyN(cx, slist->data, strlen(slist->data));
    jsval v = STRING_TO_JSVAL(s);
    JS_SetElement(cx, ary, count, &v);
    slist = slist->next;
    count++;
  }
  return JS_TRUE;
}

static JSBool easycurl_slist_append(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
  struct curl_slist *slist=NULL;
  slist = (struct curl_slist *)(JS_GetPrivate(cx, obj));

  JSString* str = JS_ValueToString(cx, argv[0]);
  if (!str)
  {
    JS_ReportError(cx, "arg1 is not a string\n");
    return JS_FALSE;
  }
  argv[0] = STRING_TO_JSVAL(str);

  const char* s = (const char*) JS_GetStringBytes(str);
  slist = curl_slist_append(slist, s);
  if (!slist)
  {
    JS_ReportError(cx, "slist append failed");
    return JS_FALSE;
  }

  JS_SetPrivate(cx, obj, (void*) slist);

  return JS_TRUE;
}

// just makes an empty slist
static JSBool easycurl_slist_ctor(JSContext* cx, JSObject* obj, uintN argc, jsval* vp, jsval* rval)
{
  JSObject* newobj = JS_NewObject(cx, &easycurl_slist_class, NULL, NULL);
  *rval = OBJECT_TO_JSVAL(newobj);
  JS_SetPrivate(cx, newobj, (void*)0);
  return JS_TRUE;
}

static void easycurl_slist_finalize(JSContext* cx, JSObject* obj)
{
  struct curl_slist* list = (struct curl_slist*)(JS_GetPrivate(cx, obj));
  // SHOULD ASSERT HERE
  if (list)
  {
    curl_slist_free_all(list);
    JS_SetPrivate(cx, obj, 0);
  }
}

static JSFunctionSpec easycurl_slist_fn[] =
{
  JS_FS("append", easycurl_slist_append, 1, 0, 0),
  JS_FS("valueOf", easycurl_slist_valueOf, 0, 0, 0),
  JS_FS_END
};

static JSClass easycurl_slist_class =
{
  "easycurl_slist", JSCLASS_HAS_PRIVATE, // | JSCLASS_CONSTRUCT_PROTOTYPE,
  JS_PropertyStub,
  JS_PropertyStub,
  JS_PropertyStub,
  JS_PropertyStub,
  JS_EnumerateStub,
  JS_ResolveStub,
  JS_ConvertStub,
  easycurl_slist_finalize,  /* FINALIZE */
  JSCLASS_NO_OPTIONAL_MEMBERS
};

/**
 * CALLBACKS
 *   C callbacks are defined which then call curl object javascript methods
 *
 */
static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{
  size_t len = size * nmemb;

  // passback from the read/write/header operations
  struct callback_data* cb = (struct callback_data*)(stream);

  // get private data from handle for context
  JSContext* cx = cb->cx;
  JSObject* obj = cb->obj;

  JSObject* bthing = byteThing_fromCArray(cx, (const unsigned char *) ptr, len);
  jsval argv[1];
  argv[0] = OBJECT_TO_JSVAL(bthing);

  jsval rval;
  if (!JS_CallFunctionName(cx, obj, "write", 1, argv, &rval))
    return -1;

  return len;
}

static size_t header_callback( void *ptr, size_t size, size_t nmemb, void *stream)
{

  size_t len = size * nmemb;

  // passback from the read/write/header operations
  struct callback_data* cb = (struct callback_data*)(stream);

  // get private data from handle for context
  JSContext* cx = cb->cx;
  JSObject* obj = cb->obj;

  // TBD: http headers are always ASCII?  Need to read spec
  JSString* s = JS_NewStringCopyN(cx, (const char*) ptr, len);
  if (!s)
    return -1;

  jsval rval;

  jsval argv[1];
  argv[0] = STRING_TO_JSVAL(s);
  // TBD -- needed or this is auto-rooted in call below?
  //JS_AddRoot(cx, &(argv[0]));
  if (!JS_CallFunctionName(cx, obj, "header", 1, argv, &rval))
    return -1;

  return len;
}


/*******************************************************/


static JSBool jscurl_getinfo(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
  struct callback_data* cb = (struct callback_data*)(JS_GetPrivate(cx, obj));
  CURL* handle = cb->handle;

  int opt = 0;
  CURLcode c = 0;

  jsval arg = argv[0];
  if (!JS_ValueToInt32(cx, arg, &opt))
  {
    JS_ReportError(cx, "first arg not an int or is an unknown option");
    return JS_FALSE;
  }

  switch (opt)
  {
    // STRINGS
  case CURLINFO_EFFECTIVE_URL:
#ifdef CURLINFO_REDIRECT_URL:
  case CURLINFO_REDIRECT_URL:
#endif
  case CURLINFO_CONTENT_TYPE:
    //case CURLINFO_PRIMARY_IP:
  case CURLINFO_FTP_ENTRY_PATH:
  {
    char* val = NULL;
    c = curl_easy_getinfo(handle, opt, &val);
    if (c != CURLE_OK) {
      JS_ReportError(cx, "Unable to get string value");
      return JS_FALSE;
    }
    JSString* s = JS_NewStringCopyN(cx, val, strlen(val));
    *rval = STRING_TO_JSVAL(s);
    return JS_TRUE;
  }
  // LONGS
  case CURLINFO_RESPONSE_CODE:
  case CURLINFO_HTTP_CONNECTCODE:
  case CURLINFO_FILETIME:
  case CURLINFO_REDIRECT_COUNT:
  case CURLINFO_HEADER_SIZE:
  case CURLINFO_REQUEST_SIZE:
  case CURLINFO_SSL_VERIFYRESULT:
  case CURLINFO_HTTPAUTH_AVAIL:
  case CURLINFO_PROXYAUTH_AVAIL:
  case CURLINFO_OS_ERRNO:
  case CURLINFO_NUM_CONNECTS:
  case CURLINFO_LASTSOCKET:
  {
    long val = 0;
    c = curl_easy_getinfo(handle, opt, &val);
    if (c != CURLE_OK)
    {
      JS_ReportError(cx, "Unable to get long value");
      return JS_FALSE;
    }
    JSBool ok = JS_NewNumberValue(cx, val, rval);
    if (!ok)
    {
      JS_ReportError(cx, "Unable to covert number to JS!");
      return JS_FALSE;
    }
    return JS_TRUE;
  }
  // DOUBLES
  case CURLINFO_TOTAL_TIME:
  case CURLINFO_CONNECT_TIME:
    //  case CURLINFO_APPCONNECT_TIME:
  case CURLINFO_PRETRANSFER_TIME:
  case CURLINFO_STARTTRANSFER_TIME:
  case CURLINFO_REDIRECT_TIME:
  case CURLINFO_SIZE_UPLOAD:
  case CURLINFO_SIZE_DOWNLOAD:
  case CURLINFO_SPEED_DOWNLOAD:
  case CURLINFO_CONTENT_LENGTH_DOWNLOAD:
  case CURLINFO_CONTENT_LENGTH_UPLOAD:
  {
    double dval = 0;
    c = curl_easy_getinfo(handle, opt, &dval);
    if (c != CURLE_OK)
    {
      JS_ReportError(cx, "Unable to get double value");
      return JS_FALSE;
    }
    JSBool ok = JS_NewNumberValue(cx, dval, rval);
    if (!ok)
    {
      JS_ReportError(cx, "Unable to covert number to JS!");
      return JS_FALSE;
    }
    return JS_TRUE;
  }
  // SLISTS
  case CURLINFO_SSL_ENGINES:
  case CURLINFO_COOKIELIST:
  {
    struct curl_slist *list = NULL;

    c = curl_easy_getinfo(handle, opt, &list);
    if (c != CURLE_OK)
    {
      JS_ReportError(cx, "Unable to get slist value");
      return JS_FALSE;
    }

    JSObject* newobj = JS_NewObject(cx, &easycurl_slist_class, NULL, NULL);
    *rval = OBJECT_TO_JSVAL(newobj);
    JS_SetPrivate(cx, newobj, (void*) list);
    return JS_TRUE;
  }
  // Not SUpported
  case CURLINFO_PRIVATE:
    //case CURLINFO_CERTINFO:
    JS_ReportError(cx, "Sorry, that option isn't supported yet");
    return JS_FALSE;
    break;
  default:
    JS_ReportError(cx, "Unknown curl_easy_getinfo option");
    return JS_FALSE;
  }
}

/**
 * Helper function to determine what type of value a curl_setopt takes
 *
 * @return 0 if invalid, 1,2,3 if valid
 */
static int option_expected_type(int opt)
{
  if (opt < CURLOPTTYPE_LONG)
    return 0;  // invalid
  if (opt < CURLOPTTYPE_OBJECTPOINT)
    return 1;  // long
  if (opt < CURLOPTTYPE_FUNCTIONPOINT)
    return 2; // string or slist
  if (opt < CURLOPTTYPE_OFF_T)
    return 3; // callback functions
  return 0;   // invalid
}

static JSBool jscurl_setopt(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
  struct callback_data* cb = (struct callback_data*)(JS_GetPrivate(cx, obj));
  CURL* handle = cb->handle;
  int opt = 0;
  CURLcode c = 0;
  jsval arg = argv[0];
  if (!JS_ValueToInt32(cx, arg, &opt))
  {
    JS_ReportError(cx, "first arg not an int");
    return JS_FALSE;
  }

  arg = argv[1];
  int optype = option_expected_type(opt);
  if (!optype)
  {
    JS_ReportError(cx, "Unknown or invalid curl_setopt option value");
    return JS_FALSE;
  }
  if (optype == 1)
  {
    int val;
    if (!JS_ValueToInt32(cx, argv[1], &val)) {
      JS_ReportError(cx, "second arg not an int");
      return JS_FALSE;
    }
    c = curl_easy_setopt(handle, opt, val);
  }
  else if (optype == 2)
  {
    switch (opt)
    {
    case CURLOPT_HTTPHEADER:
    case CURLOPT_HTTP200ALIASES:
    case CURLOPT_QUOTE:
    case CURLOPT_POSTQUOTE:
    case CURLOPT_PREQUOTE:
    case CURLOPT_TELNETOPTIONS: {
      // take linked list
      if (JSVAL_IS_NULL(argv[1]) || JSVAL_IS_VOID(argv[1])) {
        // null is ok, used to reset to default behavior
        c = curl_easy_setopt(handle, opt, NULL);
      }
      else
      {
        // TBD OBJECT TYPE CHECK
        JSObject* slistobj = JSVAL_TO_OBJECT(argv[1]);
        struct curl_slist* slist = (struct curl_slist*) (JS_GetPrivate(cx, slistobj));
        c = curl_easy_setopt(handle, opt, slist);
      }
      break;
    }
    default:
    {
      // DEFAULT CASE IS NORMAL STRING
      char* val = NULL;
      if (!JSVAL_IS_NULL(argv[1]) &&  !JSVAL_IS_VOID(argv[1])) {
        JSString* str = JS_ValueToString(cx, argv[1]);
        if (!str) {
          JS_ReportError(cx, "arg1 is not a string\n");
          return JS_FALSE;
        }
        argv[1] = STRING_TO_JSVAL(str);
        // TODO CHECK INSTANCE OF EASYCURL_CALLBACK
        val = JS_GetStringBytes(str);
      }
      c = curl_easy_setopt(handle, opt, val);
    }
    }
  }

  if (c == 0)
    return JS_TRUE;

  // something bad happened
  JS_ReportError(cx, "Failed with %d: %s", c, curl_easy_strerror(c));
  return JS_FALSE;
}

static JSBool jscurl_perform(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
  struct callback_data* cb = (struct callback_data*)(JS_GetPrivate(cx, obj));
  CURLcode c = curl_easy_perform(cb->handle);
  if (c == 0)
  {
    return JS_TRUE;
  }

  // something bad happened
  JS_ReportError(cx, "Failed with %d: %s", c, curl_easy_strerror(c));
  return JS_FALSE;
}


/*
 * Internal Helper function
 */
static JSBool jscurl_setupcallbacks(struct callback_data* cb)
{
  CURL* handle = cb->handle;

  CURLcode c = 0;
  // TBD error check

  // TBD why are read and write the same thing.  Doh!
  //     which is which?
  c = curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
  c = curl_easy_setopt(handle, CURLOPT_WRITEDATA, cb);

  c = curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, header_callback);
  c = curl_easy_setopt(handle, CURLOPT_HEADERDATA, cb);

  c = curl_easy_setopt(handle, CURLOPT_READFUNCTION, write_callback);
  c = curl_easy_setopt(handle, CURLOPT_READDATA, cb);

  return JS_TRUE;
}

static JSBool jscurl_version(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
  char* v = curl_version();
  JSString* s = JS_NewStringCopyN(cx, v, strlen(v));
  *rval = STRING_TO_JSVAL(s);
  return JS_TRUE;
}

static JSBool jscurl_getdate(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
  JSString* str = JS_ValueToString(cx, argv[0]);
  if (!str)
    return JS_FALSE;
  argv[0] = STRING_TO_JSVAL(str);
  const char* val = JS_GetStringBytes(str);
  double d = curl_getdate(val, NULL);

  JSBool ok = JS_NewNumberValue(cx, d, rval);
  if (!ok)
  {
    JS_ReportError(cx, "Unable to covert to JS!");
    return JS_FALSE;
  }
  return JS_TRUE;
}

static void easycurl_finalize(JSContext* cx, JSObject* obj)
{
  struct callback_data* cb = (struct callback_data*)(JS_GetPrivate(cx, obj));
  if (cb)
  {
    curl_easy_cleanup(cb->handle);
    free(cb);
    JS_SetPrivate(cx, obj, 0);
  }
}

static JSFunctionSpec easycurl_fn[] =
{
  JS_FS("setopt", jscurl_setopt, 2, 0, 0),
  JS_FS("getinfo", jscurl_getinfo, 1,0,0),
  JS_FS("perform", jscurl_perform, 0, 0, 0),
  JS_FS("getdate", jscurl_getdate, 1,0,0),
  JS_FS("version", jscurl_version, 0,0,0),
  JS_FS_END
};

static JSClass easycurl_class =
{
  "easycurl", JSCLASS_HAS_PRIVATE, // | JSCLASS_CONSTRUCT_PROTOTYPE,
  JS_PropertyStub,
  JS_PropertyStub,
  JS_PropertyStub,
  JS_PropertyStub,
  JS_EnumerateStub,
  JS_ResolveStub,
  JS_ConvertStub,
  easycurl_finalize,  /* FINALIZE */
  JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool easycurl_ctor(JSContext* cx, JSObject* obj, uintN argc, jsval* vp, jsval* rval)
{
  CURL* handle = curl_easy_init();
  if (!handle)
  {
    JS_ReportError(cx, "Unable to initialize libcurl!");
    return JS_FALSE;
  }
  JSObject* newobj = JS_NewObject(cx, &easycurl_class, NULL, NULL);
  *rval = OBJECT_TO_JSVAL(newobj);

  struct callback_data* cb = (struct callback_data*) malloc(sizeof(struct callback_data));
  if (!cb)
  {
    return JS_FALSE;
  }

  cb->handle = handle;
  cb->cx = cx;
  cb->obj = newobj;

  if (!jscurl_setupcallbacks(cb))
  {
    free(cb);
    return JS_FALSE;
  }

  JS_SetPrivate(cx, newobj, (void*) cb);
  return JS_TRUE;
}

/***********************************************************/

const char *curl_InitModule(JSContext *cx, JSObject *obj)
{

  JSObject* curlmem = Memory_InitClass(cx, obj);
  if (!curlmem)
  {
    // TBD REPORTING
    return NULL;
  }

  JSObject* easycurl_proto =
    JS_InitClass(cx, obj,
                 NULL,             /* prototype   */
                 &easycurl_class,  /* class       */
                 easycurl_ctor, 0, /* ctor, args  */
                 NULL,             /* properties  */
                 easycurl_fn,      /* functions   */
                 NULL,             /* static_ps   */
                 NULL              /* static_fs   */
      );

  if (!easycurl_proto)
  {
    // TBD ERROR REPORTING
    return NULL;
  }

  if (!JS_DefineConstDoubles(cx, easycurl_proto, easycurl_options))
  {
    // TBD ERROR REPORTIG
    fprintf(stderr, "whoopsies -- bad easycurl options spec");
    return NULL;
  }

  if (!JS_DefineConstDoubles(cx, easycurl_proto, easycurl_info))
  {
    // TBD ERROR REPORTING
    fprintf(stderr, "whoopsies -- bad info spec");
    return NULL;
  }

  JSObject* easycurl_slist_proto =
    JS_InitClass(cx, obj,
                 NULL,                   /* prototype   */
                 &easycurl_slist_class,  /* class       */
                 easycurl_slist_ctor, 0, /* ctor, args  */
                 NULL,                   /* properties  */
                 easycurl_slist_fn,
                 NULL,                   /* static_ps   */
                 NULL                    /* static_fs   */
      );

  if (!easycurl_slist_proto)
  {
    // TBD: error?
    return NULL;
  }

  return MODULE_ID;
}

JSBool curl_FiniModule(JSContext *cx, JSObject *moduleObject)
{
  return JS_TRUE;       /* Safe to unload this if it was running as DSO module */
}
