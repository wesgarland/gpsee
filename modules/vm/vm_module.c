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
 *  @version    $Id: vm_module.c,v 1.3 2010/01/26 22:37:30 wes Exp $
 *
 *  $Log: vm_module.c,v $
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
 
static __attribute__((unused)) const char rcsid[]="$Id: vm_module.c,v 1.3 2010/01/26 22:37:30 wes Exp $";
 
#include "gpsee.h"

#define MODULE_ID GPSEE_GLOBAL_NAMESPACE_NAME	".module.ca.page.vm"

/*
JS_GC
JS_GetImplementationVersion
JS_DumpNamedRoots
JS_SetThreadStackLimit
JS_SetScriptStackQuota
JS_SealObject
JS_SetBranchCallback
JS_MakeStructImmutabl
JS_EncodeString
JS_DecodeBytes
JS_SetLocaleCallbacks
*/

/** version getter */
static JSBool vm_version_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  const char *s = JS_GetImplementationVersion();

  *vp = STRING_TO_JSVAL(JS_InternString(cx, s));

  return JS_TRUE;
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
  int		i;

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
      printf("%s%s", i ? " " : "", buf);
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
    { NULL,			NULL,				0, 0, 0 },
  };

  static JSPropertySpec vm_static_props[] = 
  {
    { "version",	0, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY, 	vm_version_getter, 	JS_PropertyStub },
    { "jit",		0, JSPROP_ENUMERATE | JSPROP_PERMANENT, 			vm_jit_getter,		vm_jit_setter },
    { NULL, 0, 0, NULL, NULL }
  };

  if (JS_DefineProperties(cx, moduleObject, vm_static_props) != JS_TRUE)
    return NULL;

  if (JS_DefineFunctions(cx, moduleObject, vm_static_methods) != JS_TRUE)
    return NULL;

  return MODULE_ID;
}

JSBool vm_FiniModule(JSContext *cx, JSObject *moduleObject)
{
  return JS_TRUE;
}
