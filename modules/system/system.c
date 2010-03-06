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
 *  @version    $Id: system.c,v 1.1 2010/03/06 18:37:28 wes Exp $
 */
 
static __attribute__((unused)) const char rcsid[]="$Id: system.c,v 1.1 2010/03/06 18:37:28 wes Exp $";
 
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

static JSBool system_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  /* This implementation is a little funny in that it calls JS_EvaluateScript() on snprintf()d code.
   * It probably could be done another way, but this was the quickest, and I don't see any glaring
   * flaws with this course, other than a slightly bitter taste in my mouth. */
  static const char codeTemplate[] = "if(!this.hasOwnProperty('file'))file=require('fs-base');exports.%s=file.openDescriptor(%d,{'%s':true})";
  char code[sizeof(codeTemplate)+9];
  const char *propName;
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

  propName = stdioStreams[which].name;
  if (!JS_DeleteProperty(cx, obj, propName))
    return JS_FALSE;

  if (!JS_SetProperty(cx, obj, propName, vp))
    return JS_FALSE;

  return JS_TRUE;
}

static JSBool system_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
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

const char *system_InitModule(JSContext *cx, JSObject *module)
{
  gpsee_interpreter_t *jsi;
  static JSPropertySpec properties[] = {
    {"stdin",  0, JSPROP_ENUMERATE, system_getProperty, system_setProperty},
    {"stdout", 1, JSPROP_ENUMERATE, system_getProperty, system_setProperty},
    {"stderr", 2, JSPROP_ENUMERATE, system_getProperty, system_setProperty},
    {NULL, 0, 0, NULL, NULL}
  };

  if (!JS_DefineProperties(cx, module, properties))
    return NULL;

  jsi = (gpsee_interpreter_t*)JS_GetRuntimePrivate(JS_GetRuntime(cx));

  if (!system_InitEnv(cx, module))
    return NULL;

  if (!gpsee_createJSArray_fromVector(cx, module, "args", jsi->script_argv))
    return NULL;

  return MODULE_ID; 
}

JSBool system_FiniModule(JSContext *cx, JSObject *moduleObject)
{
  return JS_TRUE;
}
