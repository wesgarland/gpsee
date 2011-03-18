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
 * Copyright (c) 2009-2010, Nick Galbreath. All Rights Reserved.
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
 * ***** END LICENSE **
 *  @file   curl_module.c       GPSEE wrapper around the easycurl interface of the
 *                              libcurl networking library http://curl.haxx.se/libcurl/c/
 *  @author Nick Galbreath
 *          client9 LLC
 *          nickg@client9.com
 *  @date   Jan 2010
 *  @version    $Id: curl.c,v 1.1 2010/06/23 15:50:10 hdon Exp $
 */

// THIRDPARTY
#include <curl/curl.h>

// LOCAL
#include "gpsee.h"

#include "libcurl_constants.h"
#include "libcurl_curlinfo.h"

#define MODULE_ID GPSEE_GLOBAL_NAMESPACE_NAME ".module.com.client9.curl"
static const char __attribute__((unused)) rcsid[]="$Id: curl.c,v 1.1 2010/06/23 15:50:10 hdon Exp $";

// Forward declarations
static JSClass* easycurl_slist_class;
static JSObject* easycurl_slist_proto;

static JSClass* easycurl_class;
static JSObject* easycurl_proto;

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

/**
 * returns a *copy* of slist as an array for debugging only
 */
static JSBool easycurl_slist_toArray(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{

  // slist is a bit weird since it can have a NULL value for private data
  // so can't use GetInstancePrivate.  Have to check instance and then get
  // private data
  if (!JS_InstanceOf(cx, obj, easycurl_slist_class, NULL))
    return JS_FALSE;

  struct curl_slist *slist = (struct curl_slist *)(JS_GetPrivate(cx, obj));

  JSObject* ary = JS_NewArrayObject(cx, 0, NULL);
  *rval = OBJECT_TO_JSVAL(ary);
  int count = 0;
  while (slist != NULL) {
    JSString* s = JS_NewStringCopyN(cx, slist->data, strlen(slist->data));
    if (!s)
      return JS_FALSE;
    jsval v = STRING_TO_JSVAL(s);
    JS_SetElement(cx, ary, count, &v);
    slist = slist->next;
    ++count;
  }
  return JS_TRUE;
}

static JSBool easycurl_slist_append(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
  struct curl_slist *slist = NULL;

  // slist is a bit weird since it can have a NULL value for private data
  // so can't use GetInstancePrivate.  Have to check instance and then get
  // private data
  if (!JS_InstanceOf(cx, obj, easycurl_slist_class, NULL))
    return JS_FALSE;

  slist = (struct curl_slist *)(JS_GetPrivate(cx, obj));

  JSString* str = JS_ValueToString(cx, argv[0]);
  if (!str)
    return JS_FALSE;


  // root the new str
  argv[0] = STRING_TO_JSVAL(str);

  const char* s = (const char*) JS_GetStringBytes(str);
  slist = curl_slist_append(slist, s);
  if (!slist)
  {
    JS_ReportOutOfMemory(cx);
    return JS_FALSE;
  }

  JS_SetPrivate(cx, obj, (void*) slist);

  return JS_TRUE;
}

// just makes an empty slist
static JSBool easycurl_slist_ctor(JSContext* cx, JSObject* obj, uintN argc, jsval* vp, jsval* rval)
{
  JSObject* newobj = JS_NewObject(cx, easycurl_slist_class, easycurl_slist_proto, NULL);
  *rval = OBJECT_TO_JSVAL(newobj);
  JS_SetPrivate(cx, newobj, (void*)0);
  return JS_TRUE;
}

static void easycurl_slist_finalize(JSContext* cx, JSObject* obj)
{
  struct curl_slist* list = (struct curl_slist*)(JS_GetPrivate(cx, obj));
  // SHOULD ASSERT HERE
  if (list)
    curl_slist_free_all(list);
}

/**
 * CALLBACKS
 *   C callbacks are defined which then call curl object javascript methods
 *
 */
static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{
  jsval rval;
  jsval argv[1];
  size_t len = size * nmemb;

  // passback from the read/write/header operations
  struct callback_data* cb = (struct callback_data*)(stream);

  // get private data from handle for context
  JSContext* cx = cb->cx;
  JSObject* obj = cb->obj;

  // TODO: Donny; JS_ResumeRequest
  JSObject* bthing = gpsee_newByteThing(cx, ptr, len, JS_TRUE);
  argv[0] = OBJECT_TO_JSVAL(bthing);

  // TODO: check function return value to see if we want to abort or not?
  if (!JS_CallFunctionName(cx, obj, "write", 1, argv, &rval))
    return -1;

  // TODO: Donny: JS_SuspendRequest
  return len;
}

static size_t header_callback( void *ptr, size_t size, size_t nmemb, void *stream)
{
  JSString* s;
  jsval rval;
  jsval argv[1];
  size_t len = size * nmemb;

  // passback from the read/write/header operations
  struct callback_data* cb = (struct callback_data*)(stream);

  // get private data from handle for context
  JSContext* cx = cb->cx;
  JSObject* obj = cb->obj;

  // TODO: Donny: JS_ResumeRequest(cx, XXXX);
  // TODO: http headers are always ASCII?  Need to read spec, handle gracefully
  s = JS_NewStringCopyN(cx, (const char*) ptr, len);
  if (!s)
    return -1;

  // root new string
  argv[0] = STRING_TO_JSVAL(s);

  // TODO -- needed or this is auto-rooted in call below?
  //JS_AddRoot(cx, &(argv[0]));
  if (!JS_CallFunctionName(cx, obj, "header", 1, argv, &rval))
    return -1;

  // TODO: Donny: JS_SuspendRequest(cx);
  return len;
}

static int debug_callback(CURL *c, curl_infotype itype, char * buf, size_t len, void *userdata)
{
  jsval rval;
  jsval argv[2];

  // passback from the read/write/header operations
  struct callback_data* cb = (struct callback_data*)(userdata);

  // get private data from handle for context
  JSContext* cx = cb->cx;
  JSObject* obj = cb->obj;

  // TODO: Donny; JS_ResumeRequest
  argv[0] = INT_TO_JSVAL(itype);
  JSObject* bthing = gpsee_newByteThing(cx, buf, len, JS_TRUE);

  argv[1] = OBJECT_TO_JSVAL(bthing);

  // TODO: check function return value to see if we want to abort or not?
  if (!JS_CallFunctionName(cx, obj, "debug", 2, argv, &rval)) {
    // TBD?
  }

  // required
  return 0;
}

/*******************************************************/
/**
 * Helper function to determine what type of value curl_getinfo returns
 *
 */
static int info_expected_type(int opt)
{
    // opt & 0xf00000
    opt = (opt & CURLINFO_TYPEMASK);

    // opt is 0x100000 to 0x400000
    //  5*4 = 20;
    opt = opt >> 20;
    if (opt < 1 || opt > 4)
        opt = 0;
    return opt;
}

static JSBool jscurl_getinfo(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
  struct callback_data* cb = (struct callback_data*)(JS_GetInstancePrivate(cx, obj, easycurl_class, rval));
  if (!cb)
    return JS_FALSE;
  CURL* handle = cb->handle;
  int opt = 0;
  CURLcode c = 0;

  jsval arg = argv[0];
  if (!JS_ValueToInt32(cx, arg, &opt))
    return JS_FALSE;

  // TODO: DONNY NOTES: can curl_easy_getinfo() fail for other reasons?
  //  does it have a way of reporting what went wrong?
  switch (info_expected_type(opt))
  {
    // STRINGS
  case 1:
  {
    char* val = NULL;
    JSString* s = NULL;

    c = curl_easy_getinfo(handle, opt, &val);
    if (c != CURLE_OK) {
      JS_ReportError(cx, "Unable to get string value");
      return JS_FALSE;
    }
    s = JS_NewStringCopyN(cx, val, strlen(val));
    if (!s)
      return JS_FALSE;
    *rval = STRING_TO_JSVAL(s);
    return JS_TRUE;
  }
  // LONGS
  case 2:
  {
    long val = 0;
    c = curl_easy_getinfo(handle, opt, &val);
    if (c != CURLE_OK)
    {
      JS_ReportError(cx, "Unable to get long value");
      return JS_FALSE;
    }
    return (JS_NewNumberValue(cx, val, rval)) ? JS_TRUE : JS_FALSE;
  }
  // DOUBLE
  case 3:
  {
    double dval = 0;
    c = curl_easy_getinfo(handle, opt, &dval);
    if (c != CURLE_OK)
    {
      JS_ReportError(cx, "Unable to get double value");
      return JS_FALSE;
    }
    return (JS_NewNumberValue(cx, dval, rval)) ? JS_TRUE : JS_FALSE;
  }

  // SLIST OR OBJECTS
  case 4:
  {
      if (opt == CURLINFO_PRIVATE) {
          JS_ReportError(cx, "Sorry, that option isn't supported.");
          return JS_FALSE;
      }

    struct curl_slist *list = NULL;
    JSObject* newobj = NULL;

    c = curl_easy_getinfo(handle, opt, &list);
    if (c != CURLE_OK)
    {
      JS_ReportError(cx, "Unable to get slist value");
      return JS_FALSE;
    }

    newobj = JS_NewObject(cx, easycurl_slist_class, NULL, NULL);
    // TODO: Donny  OOM here?
    *rval = OBJECT_TO_JSVAL(newobj);
    JS_SetPrivate(cx, newobj, (void*) list);
    return JS_TRUE;
  }
  default:
    JS_ReportError(cx, "Unknown curl_easy_getinfo option");
    return JS_FALSE;
  }
}

/**
 * Helper function to determine what type of value a curl_setopt and
 * takes
 *
 * There are defined in decimal so we use "<" not bit ops
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
  struct callback_data* cb = (struct callback_data*)(JS_GetInstancePrivate(cx, obj, easycurl_class, NULL));
  if (!cb)
    return JS_FALSE;

  CURL* handle = cb->handle;
  int opt = 0;
  CURLcode c = 0;
  jsval arg = argv[0];
  if (!JS_ValueToInt32(cx, arg, &opt))
    return JS_FALSE;

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
    if (!JS_ValueToInt32(cx, argv[1], &val))
      return JS_FALSE;
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
      if (JSVAL_IS_NULL(argv[1])) {
        // null is ok, used to reset to default behavior
        c = curl_easy_setopt(handle, opt, NULL);
      }
      else
      {
        struct curl_slist* slist = NULL;
        JSObject* slistobj = JSVAL_TO_OBJECT(argv[1]);

        if (!JS_InstanceOf(cx, slistobj, easycurl_slist_class, NULL))
          return JS_FALSE;
        slist = (struct curl_slist*) (JS_GetPrivate(cx, slistobj));
        // null is ok!
        c = curl_easy_setopt(handle, opt, slist);
      }
      break;
    }
    default:
    {
      // DEFAULT CASE IS NORMAL STRING
      char* val = NULL;
      // TODO: why null and void?
      if (!JSVAL_IS_NULL(argv[1]) &&  !JSVAL_IS_VOID(argv[1])) {
        JSString* str = JS_ValueToString(cx, argv[1]);
        if (!str)
          return JS_FALSE;
        // root new string
        argv[1] = STRING_TO_JSVAL(str);
        // TODO CHECK that arg is a function.. if not, it will abort anyways when curl calls it.
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
  struct callback_data* cb = (struct callback_data*)(JS_GetInstancePrivate(cx, obj, easycurl_class, rval));
  if (!cb)
    return JS_FALSE;

  // TODO: Donny: set the cx here and not in construction
  // TODO: Donny: JS_SuspendRequest
  CURLcode c = curl_easy_perform(cb->handle);
  if (c == 0)
    return JS_TRUE;

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

  // TODO why are read and write the same thing.  Doh!
  //     which is which?
  c = curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
  c = curl_easy_setopt(handle, CURLOPT_WRITEDATA, cb);

  c = curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, header_callback);
  c = curl_easy_setopt(handle, CURLOPT_HEADERDATA, cb);

  c = curl_easy_setopt(handle, CURLOPT_READFUNCTION, write_callback);
  c = curl_easy_setopt(handle, CURLOPT_READDATA, cb);

  c = curl_easy_setopt(handle, CURLOPT_DEBUGFUNCTION, debug_callback);
  c = curl_easy_setopt(handle, CURLOPT_DEBUGDATA, cb);

  return JS_TRUE;
}

static JSBool jscurl_version(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
  char* v = curl_version();
  JSString* s = JS_NewStringCopyN(cx, v, strlen(v));
  if (!s)
    return JS_FALSE;
  *rval = STRING_TO_JSVAL(s);
  return JS_TRUE;
}

static JSBool jscurl_getdate(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
  JSString* str = JS_ValueToString(cx, argv[0]);
  if (!str)
    return JS_FALSE;

  // root string
  argv[0] = STRING_TO_JSVAL(str);
  const char* val = JS_GetStringBytes(str);
  double d = curl_getdate(val, NULL);

  return JS_NewNumberValue(cx, d, rval) ? JS_TRUE : JS_FALSE;
}

static void easycurl_finalize(JSContext* cx, JSObject* obj)
{
  struct callback_data* cb = (struct callback_data*)(JS_GetPrivate(cx, obj));
  if (cb)
  {
    /* On death, curl sends a "closing connection" debug message
     * If we are in GC, we don't want to send a request back to JS
     * (can occur only if verbose is 1)
     */
    curl_easy_setopt(cb->handle, CURLOPT_VERBOSE, 0);
    curl_easy_cleanup(cb->handle);
    JS_free(cx, cb);
  }
}

static JSBool easycurl_ctor(JSContext* cx, JSObject* obj, uintN argc, jsval* vp, jsval* rval)
{
  struct callback_data* cb = NULL;
  JSObject* newobj = NULL;

  CURL* handle = curl_easy_init();
  if (!handle)
  {
    JS_ReportError(cx, "Unable to initialize libcurl!");
    return JS_FALSE;
  }
  newobj = JS_NewObject(cx, easycurl_class, easycurl_proto, NULL);
  if (!newobj)
    return JS_FALSE;
  *rval = OBJECT_TO_JSVAL(newobj);

  cb = (struct callback_data*) JS_malloc(cx, sizeof(struct callback_data));
  if (!cb)
  {
    JS_ReportOutOfMemory(cx);
    return JS_FALSE;
  }

  cb->handle = handle;
  cb->cx = cx;
  cb->obj = newobj;

  if (!jscurl_setupcallbacks(cb))
  {
    JS_free(cx, cb);
    // TODO: Donny: should this really be OOM?
    JS_ReportError(cx, "failed to set libcurl callbacks");
    return JS_FALSE;
  }

  JS_SetPrivate(cx, newobj, (void*) cb);
  return JS_TRUE;
}

/***********************************************************/

const char *curl_InitModule(JSContext *cx, JSObject *obj)
{

  static JSFunctionSpec easycurl_slist_fn[] =
    {
      JS_FS("append",  easycurl_slist_append,  1,0,0),
      JS_FS("toArray", easycurl_slist_toArray, 0,0,0),
      JS_FS_END
    };

  static JSClass slist_class =
    {
      GPSEE_CLASS_NAME(easycurl_slist),
      JSCLASS_HAS_PRIVATE, // | JSCLASS_CONSTRUCT_PROTOTYPE,
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

  static JSFunctionSpec easycurl_fn[] =
    {
      JS_FS("setopt",  jscurl_setopt,  2,0,0),
      JS_FS("getinfo", jscurl_getinfo, 1,0,0),
      JS_FS("perform", jscurl_perform, 0,0,0),
      JS_FS("getdate", jscurl_getdate, 1,0,0),
      JS_FS("version", jscurl_version, 0,0,0),
      JS_FS_END
    };

  static JSClass easy_class =
    {
      GPSEE_CLASS_NAME(easycurl),
      JSCLASS_HAS_PRIVATE, // | JSCLASS_CONSTRUCT_PROTOTYPE,
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

  /* ----------------------------------- */

  easycurl_class = &easy_class;
  easycurl_proto =
    JS_InitClass(cx, obj,
                 NULL,             /* prototype   */
                 easycurl_class,  /* class       */
                 easycurl_ctor, 0, /* ctor, args  */
                 NULL,             /* properties  */
                 easycurl_fn,      /* functions   */
                 NULL,             /* static_ps   */
                 NULL              /* static_fs   */
      );

  if (!easycurl_proto)
    return NULL;

  if (!JS_DefineConstDoubles(cx, easycurl_proto, easycurl_options))
    return NULL;

  if (!JS_DefineConstDoubles(cx, easycurl_proto, easycurl_info))
    return NULL;

  easycurl_slist_class = &slist_class;
  easycurl_slist_proto =
    JS_InitClass(cx, obj,
                 NULL,                   /* prototype   */
                 easycurl_slist_class,   /* class       */
                 easycurl_slist_ctor, 0, /* ctor, args  */
                 NULL,                   /* properties  */
                 easycurl_slist_fn,
                 NULL,                   /* static_ps   */
                 NULL                    /* static_fs   */
      );

  if (!easycurl_slist_proto)
    return NULL;

  return MODULE_ID;
}

JSBool curl_FiniModule(JSContext *cx, JSObject *moduleObject)
{
  return JS_TRUE;       /* Safe to unload this if it was running as DSO module */
}
