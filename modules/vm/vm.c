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
 * ***** END LICENSE BLOCK ***** */

/**
 *  @file       vm_module.c	Object to hold properties/methods which are tightly
 *				bound to the JavaScript interpreter itself.
 *
 *  @author     Wes Garland
 *  @date       Jan 2008
 *  @version    $Id: vm.c,v 1.10 2011/12/05 19:13:38 wes Exp $
 *
 *  $Log: vm.c,v $
 *  Revision 1.10  2011/12/05 19:13:38  wes
 *  Synchronize with hg revision 344218a2a12a tip
 *
 *  Revision 1.9  2010/12/17 21:37:02  wes
 *  vm module: added object/string/jsvalPtrValue methods
 *
 *  Revision 1.8  2010/12/02 21:47:28  wes
 *  vm module: Added halt() method
 *
 *  Revision 1.7  2010/06/14 22:12:01  wes
 *  Sync'd with mercurial
 *
 *  Revision 1.6  2010/04/14 00:38:52  wes
 *  Synchronize with Mercurial
 *
 *  Revision 1.5  2010/03/06 18:17:14  wes
 *  Synchronize Mercurial and CVS Repositories
 *
 *  Revision 1.4  2010/02/17 15:57:18  wes
 *  vm module: Added jsval, dumpHeap, dumpValue, dumpObject
 *
 *  Revision 1.3  2010/01/26 22:37:30  wes
 *  Test for JITable program modules
 *
 *  Revision 1.2  2009/12/01 21:30:11  wes
 *  Changed vm.gc semantic so that no-args now means force-gc instead of maybe-gc
 *
 *  Revision 1.1  2009/03/30 23:55:45  wes
 *  Initial Revision for GPSEE. Merges now-defunct JSEng and Open JSEng projects.
 *
 */
 
static __attribute__((unused)) const char rcsid[]="$Id: vm.c,v 1.10 2011/12/05 19:13:38 wes Exp $";
 
#include "gpsee.h"

#define MODULE_ID GPSEE_GLOBAL_NAMESPACE_NAME	".module.ca.page.vm"

/** version getter */
static JSBool vm_version_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  const char *s = JS_GetImplementationVersion();

  *vp = STRING_TO_JSVAL(JS_InternString(cx, s));

  return JS_TRUE;
}

/** Generic Getter */
static JSBool vm_generic_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  void          *ptr;
  gpsee_realm_t *realm;

  if (JSVAL_TO_INT(id) != -1)
  {
    realm = gpsee_getRealm(cx);
    if (!realm)
      return gpsee_throw(cx, MODULE_ID ": Unable to determine GPSEE Realm for current context");
  }

  switch(JSVAL_TO_INT(id))
  {
    case -1:
      ptr = cx;
      break;
    case -2:
      ptr = realm->grt->rt;
      break;
    case -3:
      ptr = realm->globalObject;
      break;
    case -4:
      ptr = realm;
      break;
    default:
      return gpsee_throw(cx, MODULE_ID ": invalid tinyID!");
  }

  obj = gpsee_newByteThing(cx, ptr, 0, JS_FALSE);
  if (!obj)
    return JS_FALSE;

  *vp = OBJECT_TO_JSVAL(obj);

  return JS_TRUE;
}

static JSBool vm_jsval(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  char buf[32];

  snprintf(buf, sizeof(buf), GPSEE_PTR_FMT, (void *)argv[0]);

  *rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, buf));
  return JS_TRUE;
}

static JSBool vm_objectPtrValue(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  byteThing_handle_t    *hnd;

  if (argc != 1)
    return gpsee_throw(cx, MODULE_ID ".objectPtrValue.arguments.count");

  if (!JSVAL_IS_OBJECT(argv[0]))
    return gpsee_throw(cx, MODULE_ID ".objectPtrValue.arguments.0.type");

  obj = JSVAL_TO_OBJECT(argv[0]);
  if (!gpsee_isByteThing(cx, obj))
    return gpsee_throw(cx, MODULE_ID ".objectPtrValue.notByteThing");

  hnd = JS_GetPrivate(cx, obj);
  if (hnd->length && hnd->length != sizeof(JSObject *))
    return gpsee_throw(cx, MODULE_ID ".objectPtrValue.storageSize");

  *rval = OBJECT_TO_JSVAL((JSObject *)hnd->buffer);

  return JS_TRUE;
}

static JSBool vm_stringPtrValue(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  byteThing_handle_t    *hnd;

  if (argc != 1)
    return gpsee_throw(cx, MODULE_ID ".stringPtrValue.arguments.count");

  if (!JSVAL_IS_OBJECT(argv[0]))
    return gpsee_throw(cx, MODULE_ID ".stringPtrValue.arguments.0.type");

  obj = JSVAL_TO_OBJECT(argv[0]);
  if (!gpsee_isByteThing(cx, obj))
    return gpsee_throw(cx, MODULE_ID ".stringPtrValue.notByteThing");

  hnd = JS_GetPrivate(cx, obj);
  if (hnd->length && hnd->length != sizeof(JSString *))
    return gpsee_throw(cx, MODULE_ID ".stringPtrValue.storageSize");

  *rval = STRING_TO_JSVAL((JSString *)hnd->buffer);

  return JS_TRUE;
}

static JSBool vm_jsvalPtrValue(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  byteThing_handle_t    *hnd;

  if (argc != 1)
    return gpsee_throw(cx, MODULE_ID ".jsvalPtrValue.arguments.count");

  if (!JSVAL_IS_OBJECT(argv[0]))
    return gpsee_throw(cx, MODULE_ID ".jsvalPtrValue.arguments.0.type");

  obj = JSVAL_TO_OBJECT(argv[0]);
  if (!gpsee_isByteThing(cx, obj))
    return gpsee_throw(cx, MODULE_ID ".jsvalPtrValue.notByteThing");

  hnd = JS_GetPrivate(cx, obj);
  if (hnd->length && hnd->length != sizeof(JSObject *))
    return gpsee_throw(cx, MODULE_ID ".jsvalPtrValue.storageSize");

  *rval = *(jsval *)hnd->buffer; /* cast-align ok */

  return JS_TRUE;
}
	
static JSBool vm_jschars(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  JSString      *str = NULL;
  jschar        *ptr;

  if (argc != 1)
    return gpsee_throw(cx, MODULE_ID ".jschars.arguments.count");
    
  if (!JSVAL_IS_STRING(argv[0]))
  {
    str = JS_ValueToString(cx, argv[0]);
    if (!str)
      return JS_FALSE; /* OOM */
    argv[0] = STRING_TO_JSVAL(str);                             /* Provide a temporary GC root */
    ptr = JS_GetStringChars(str);
  }
  else
  {
    ptr = JS_GetStringChars(JSVAL_TO_STRING(argv[0]));
  }

  obj = gpsee_newByteThing(cx, ptr, 0, JS_FALSE);
  if (!obj)
    return JS_FALSE;

  *rval = OBJECT_TO_JSVAL(obj);

  if (str)
    JS_SetReservedSlot(cx, obj, 0, STRING_TO_JSVAL(str));        /* Provide a proper lifetimed GC root */
    
  return JS_TRUE;
}

static JSBool vm_dumpValue(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
#if defined(DEBUG)
  JS_FRIEND_API(void) js_DumpValue(jsval val);

  if (argc != 1)
    return gpsee_throw(cx, MODULE_ID ".dumpValue.arguments.count");

 js_DumpValue(argv[0]);
 return JS_TRUE;
#else
  return gpsee_throw(cx, MODULE_ID ".dumpValue.undefined: requires a debug JSAPI build");
#endif  
}

static JSBool vm_dumpObject(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
#if defined(DEBUG)
  JS_FRIEND_API(void) js_DumpObject(jsval val);

  if (argc != 1)
    return gpsee_throw(cx, MODULE_ID ".dumpObject.arguments.count");

  if (!JSVAL_IS_OBJECT(argv[0]))
    return gpsee_throw(cx, MODULE_ID ".dumpObject.arguments.0.type: must be an object");

 js_DumpObject(argv[0]);
 return JS_TRUE;
#else
  return gpsee_throw(cx, MODULE_ID ".dumpObject.undefined: requires a debug JSAPI build");
#endif  
}

static JSBool vm_halt(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}

/**
 *  Dump the JS heap.
 *
 *  arguments:	start, find, maxDepth, ignore, filename
 *  default:	all,  null, 1000,	null, /dev/stdout
 */
static JSBool vm_dumpHeap(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
#if defined(DEBUG)
  JSObject 	*start = NULL, *find = NULL, *ignore = NULL;
  int32		maxDepth = 1000;
  char		*filename = (char *)"/dev/stdout";
  FILE		*file;
  JSBool	b;

  if (JS_ConvertArguments(cx, argc, argv, "ooios", &start, &find, &maxDepth, &ignore, &filename) == JS_FALSE)
    return JS_FALSE;

  file = fopen(filename, "r");
  if (!file)
    return gpsee_throw(cx, MODULE_ID ".dumpHeap.fopen: %m");

  b = JS_DumpHeap(cx, file, start, 0, find, maxDepth, ignore);
  fclose(file);
  return b;
#else
  return gpsee_throw(cx, MODULE_ID ".dumpHeap.undefined: requires a debug JSAPI build");
#endif
}

/** Garbage Collector method for Javascript. 
 *  One optional argument, boolean, whether or not to force the garbage collection.
 */
static JSBool vm_gc(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  JSBool b;

  if (argc == 0)
  {
    JS_GC(cx);
  }
  else
  {
    if (JS_ValueToBoolean(cx, argv[0], &b) == JS_FALSE)
      return JS_FALSE;

    if (b)
      JS_GC(cx);
    else
      JS_MaybeGC(cx);
  }

  return JS_TRUE;
}

#ifdef JS_GCMETER
static JSBool vm_dumpGCStats(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  const char 	*filename = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
  FILE		*file;
  
  depth = JS_SuspendRequest(cx);
  file = fopen(filename, "w");
  flock(file, LOCK_EX);
  JS_ResumeRequest(cx);

  js_DumpGCStats();

  fclose(file);
}
#endif

static JSBool vm_isCompilableUnit(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  JSString 	*str;
  jsrefcount 	depth;
  size_t	len, maxLen = 0;
  char		*buf;
  size_t        i;

  /** Coerce all args .toString() and calculate maximal buffer size */
  for (i = 0; i < argc; i++)
  {
    if (!JSVAL_IS_STRING(argv[i]))
    {
      str = JS_ValueToString(cx, argv[i]);
      argv[i] = STRING_TO_JSVAL(str);
    }
    else
      str = JSVAL_TO_STRING(argv[i]);

    len = JS_GetStringLength(JSVAL_TO_STRING(argv[i]));
    if (len > maxLen)
      maxLen = len;
  }

  buf = JS_malloc(cx, maxLen + 1);
  if (!buf)
    return JS_FALSE;

  if (argc > 1)
  {
    for (i = 0; i < (argc - 1); i++)
    {
      str = JS_ValueToString(cx, argv[i]);
      if (!str)
      {
	JS_free(cx, buf);
	return JS_FALSE;
      }

      strcpy(buf, JS_GetStringBytes(str));
      depth = JS_SuspendRequest(cx);
      gpsee_printf(cx, "%s%s", i ? " " : "", buf);
      JS_ResumeRequest(cx, depth);
    }
  }
  else
    i = 0;

  if (argc)
  {
    str = JS_ValueToString(cx, argv[i]);
    if (!str)
    {
      JS_free(cx, buf);
      return JS_FALSE;
    }

    strcpy(buf, JS_GetStringBytes(str));
  }
  else
    *buf = (char)0;

  *rval = JS_BufferIsCompilableUnit(cx, obj, buf, strlen(buf)) == JS_TRUE ? JSVAL_TRUE : JSVAL_FALSE;

  JS_free(cx, buf);
  return JS_TRUE;
}

static JSBool vm_jit_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  *vp = (JS_GetOptions(cx) & JSOPTION_JIT) ? JSVAL_TRUE : JSVAL_FALSE;
  return JS_TRUE;
}


static JSBool vm_jit_setter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  if (gpsee_isFalsy(cx, *vp))
    JS_SetOptions(cx, JS_GetOptions(cx) & ~JSOPTION_JIT);
  else
    JS_SetOptions(cx, JS_GetOptions(cx) | JSOPTION_JIT);
  return JS_TRUE;
}

const char *vm_InitModule(JSContext *cx, JSObject *moduleObject)
{
  static JSFunctionSpec vm_static_methods[] = 
  {
#ifdef JS_GCMETER
    { "dumpGCstats",		vm_dumpGCstats,			0, 0, 0 },	/* char: filename */
#endif
    { "GC",			vm_gc,				0, 0, 0 },	
    { "isCompilableUnit",	vm_isCompilableUnit,		0, 0, 0 },
    { "jsval",			vm_jsval,			0, 0, 0 },
    { "objectPtrValue",		vm_objectPtrValue,		0, 0, 0 },
    { "stringPtrValue",		vm_stringPtrValue,		0, 0, 0 },
    { "jsvalPtrValue",		vm_jsvalPtrValue,		0, 0, 0 },
    { "jschars",                vm_jschars,                     0, 0, 0 },
    { "dumpHeap",		vm_dumpHeap,			0, 0, 0 },
    { "dumpValue",		vm_dumpValue,			0, 0, 0 },
    { "dumpObject",		vm_dumpObject,			0, 0, 0 },
    { "halt",			vm_halt,			0, 0, 0 },
    { NULL,			NULL,				0, 0, 0 },
  };

  static JSPropertySpec vm_static_props[] = 
  {
    { "version",	 0, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY, 	vm_version_getter, 	JS_PropertyStub },
    { "jit",		 0, JSPROP_ENUMERATE | JSPROP_PERMANENT, 			vm_jit_getter,		vm_jit_setter },
    { "cx",             -1, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY, 	vm_generic_getter, 	JS_PropertyStub },
    { "rt",             -2, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY, 	vm_generic_getter, 	JS_PropertyStub },
    { "globalObject",   -3, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY, 	vm_generic_getter, 	JS_PropertyStub },
    { "realm",          -4, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY, 	vm_generic_getter, 	JS_PropertyStub },
    { NULL, 0, 0, NULL, NULL }
  };

  if (JS_DefineProperties(cx, moduleObject, vm_static_props) != JS_TRUE)
    return NULL;

  if (JS_DefineFunctions(cx, moduleObject, vm_static_methods) != JS_TRUE)
    return NULL;

  return MODULE_ID;
}

JSBool vm_FiniModule(JSContext *cx, gpsee_realm_t *realm, JSObject *moduleObject, JSBool force)
{
  return JS_TRUE;
}
