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
 *  @file       system.c	GPSEE implementation of the CommonJS "system"
 *                              module with lazily loaded module dependencies
 *                              for the stdio features.
 *  @author     Donny Viszneki
 *  @date       Feb 2010
 *  @version    $Id: system.c,v 1.4 2010/09/01 18:12:36 wes Exp $
 */
 
static __attribute__((unused)) const char rcsid[]="$Id: system.c,v 1.4 2010/09/01 18:12:36 wes Exp $";
 
#include "gpsee.h"
#include "system.h"

extern char **environ;
static struct {
  const char const *name;
  const char const *mode;
} stdioStreams[] = {
  {"stdin",  "read"},
  {"stdout", "write"},
  {"stderr", "write"}
};

/** Getter system.stdin, .stdout, .stderr; id is actually the file descriptor */
static JSBool system_stdioGetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  /* This implementation is a little funny in that it calls JS_EvaluateScript() on snprintf()d code.
   * It probably could be done another way, but this was the quickest, and I don't see any glaring
   * flaws with this course, other than a slightly bitter taste in my mouth. */
  static const char codeTemplate[] = "if(!this.hasOwnProperty('file'))this.file=require('fs-base');exports.%s=this.file.openDescriptor(%d,{'%s':true})";
  char code[sizeof(codeTemplate)+9];
  int which;
  JSObject *moduleScope;

  GPSEE_ASSERT(JSVAL_IS_INT(id));
  which = JSVAL_TO_INT(id);
  GPSEE_ASSERT(which >= 0 && which <= 2);

  /* Generate Javascript code to run */
  snprintf(code, sizeof(code), codeTemplate, stdioStreams[which].name, which, stdioStreams[which].mode);
  //printf("running: %s\n", code);
  /* We guarantee that "obj" is only going to be our "exports" property, therefore
   * its parent object is the system module scope.
   */
  moduleScope = JS_GetParent(cx, obj);
  if (!JS_EvaluateScript(cx, moduleScope, code, strlen(code), __FILE__, __LINE__, vp))
    return JS_FALSE;
  return JS_TRUE;
}

/** Setter system.stdin, .stdout, .stderr; id is actually the file descriptor */
static JSBool system_stdioSetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  int which;
  const char *propName;

  GPSEE_ASSERT(JSVAL_IS_INT(id));
  which = JSVAL_TO_INT(id);
  GPSEE_ASSERT(which >= 0 && which <= 2);

  propName = stdioStreams[which].name;
  if (!JS_DeleteProperty(cx, obj, propName))
    return JS_FALSE;

  if (!JS_SetProperty(cx, obj, propName, vp))
    return JS_FALSE;

  return JS_TRUE;
}

static JSBool system_platform_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  *vp = STRING_TO_JSVAL(JS_InternString(cx, GPSEE_GLOBAL_NAMESPACE_NAME "XXX" GPSEE_CURRENT_VERSION_STRING));

  return JS_TRUE;
}

static JSBool system_global_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  gpsee_realm_t *realm = gpsee_getRealm(cx);
  
  if (!realm)
    return JS_FALSE;

  *vp = OBJECT_TO_JSVAL(realm->globalObject);

  return JS_TRUE;
}

static JSBool system_print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  static const char 	js[]="require('system').stdout.writeln";
  static const char 	jr[]="require('system').stdout";
  jsval			v, w;

  if (JS_EvaluateScript(cx, obj, js, sizeof(jr) - 1, __FILE__, __LINE__, &w) == JS_FALSE)
    return JS_FALSE;

  if (!JSVAL_IS_OBJECT(w))
    return JS_FALSE;

  if (JS_EvaluateScript(cx, obj, js, sizeof(js) - 1, __FILE__, __LINE__, &v) == JS_FALSE)
    return JS_FALSE;

  return JS_CallFunctionValue(cx, JSVAL_TO_OBJECT(w), v, argc, argv, rval);
}

const char *system_InitModule(JSContext *cx, JSObject *module)
{
  gpsee_realm_t *realm;

  static JSPropertySpec properties[] = 
  {
    { "stdin",  	0, JSPROP_ENUMERATE, system_stdioGetProperty, 	system_stdioSetProperty },
    { "stdout", 	1, JSPROP_ENUMERATE, system_stdioGetProperty, 	system_stdioSetProperty },
    { "stderr", 	2, JSPROP_ENUMERATE, system_stdioGetProperty, 	system_stdioSetProperty },
    { "platform",	0, JSPROP_ENUMERATE, system_platform_getter,  	JS_PropertyStub },
    { "global",		0, JSPROP_ENUMERATE, system_global_getter,	JS_PropertyStub },
    { NULL, 0, 0, NULL, NULL }
  };

  static JSFunctionSpec	methods[] = 
  {
    { "print",			system_print,			0, 0, 0 },
    { NULL,			NULL,				0, 0, 0 },
  };

  if (!JS_DefineProperties(cx, module, properties))
    return NULL;

  if (JS_DefineFunctions(cx, module, methods) != JS_TRUE)
    return NULL;

  realm = gpsee_getRealm(cx);
  if (!realm)
    return NULL;

  if (!system_InitEnv(cx, module))
    return NULL;

  gpsee_enterAutoMonitor(cx, &realm->monitors.script_argv);
  if (!gpsee_createJSArray_fromVector(cx, module, "args", realm->monitored.script_argv))
  {
    gpsee_leaveAutoMonitor(realm->monitors.script_argv);
    return NULL;
  }
  gpsee_leaveAutoMonitor(realm->monitors.script_argv);

  return MODULE_ID; 
}

JSBool system_FiniModule(JSContext *cx, JSObject *moduleObject)
{
  return JS_TRUE;
}

