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
 *  @file       environment.c   An object representing the environment variables
 *                              the program was run with, and also representing
 *                              what environment variables sub-processes will be
 *                              spawned with (although that features is not part
 *                              of the CommonJS system module spec.)
 *  @author     Donny Viszneki
 *  @date       Feb 2010
 *  @version    $Id:$
 */

#include "gpsee.h"
#include "system_module.h"

extern char **environ;

static JSBool Environment_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  JSString *    jstrId;
  const char *  cstrId;
  const char *  cstrVal;
  char *        cstrVal2;
  JSString *    jstrVal;

  jstrId = JS_ValueToString(cx, id);
  if (!jstrId)
    return JS_FALSE;

  cstrId = JS_GetStringBytes(jstrId);
  cstrVal = getenv(cstrId);
  if (!cstrVal)
  {
    *vp = JSVAL_VOID;
    return JS_TRUE;
  }

  cstrVal2 = JS_strdup(cx, cstrVal);
  if (!cstrVal2)
    return JS_FALSE;

  jstrVal = JS_NewString(cx, cstrVal2, strlen(cstrVal2));
  if (!jstrVal)
    return JS_FALSE;

  *vp = STRING_TO_JSVAL(jstrVal);
  return JS_TRUE;
}

static JSBool Environment_addProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  JSString *    jstrId;
  const char *  cstrId;
  const char *  cstrVal;
  JSString *    jstrVal;

  jstrId = JS_ValueToString(cx, id);
  if (!jstrId)
    return JS_FALSE;

  jstrVal = JS_ValueToString(cx, *vp);
  if (!jstrVal)
    return JS_FALSE;

  cstrId = JS_GetStringBytes(jstrId);
  cstrVal = JS_GetStringBytes(jstrVal);

  return JS_TRUE;
}

static JSBool Environment_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  JSString *    jstrId;
  const char *  cstrId;
  const char *  cstrVal;
  JSString *    jstrVal;

  jstrId = JS_ValueToString(cx, id);
  if (!jstrId)
    return JS_FALSE;

  jstrVal = JS_ValueToString(cx, *vp);
  if (!jstrVal)
    return JS_FALSE;

  cstrId = JS_GetStringBytes(jstrId);
  cstrVal = JS_GetStringBytes(jstrVal);

  if (setenv(cstrId, cstrVal, 1))
  {
    if (errno == EINVAL)
      return gpsee_throw(cx, MODULE_ID ".env.*.set: invalid environment variable name \"%s\"", cstrId);
    else
      return gpsee_throw(cx, MODULE_ID ".env.*.set: unknown error assigning \"%s\" to environment variable \"%s\" (%m)", cstrVal, cstrId);
  }

  return JS_TRUE;
}

static JSBool Environment_deleteProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  return JS_TRUE;
}

static JSBool Environment_resolveProperty(JSContext *cx, JSObject *obj, jsval id, uintN flags, JSObject **objp)
{
  JSString *  jstrId;
  char *      cstrId;

  jstrId = JS_ValueToString(cx, id);
  if (!jstrId)
    return JS_FALSE;
  cstrId = JS_GetStringBytes(jstrId);
  if (getenv(cstrId))
  {
    if (!JS_DefineProperty(cx, obj, cstrId, JSVAL_VOID, NULL, NULL, JSPROP_SHARED|JSPROP_ENUMERATE))
      return JS_FALSE;

    *objp = obj;
  }
  else
    *objp = obj;

  return JS_TRUE;
}

static JSBool Environment_enumerate(JSContext *cx, JSObject *obj, JSIterateOp enum_op, jsval *statep, jsid *idp)
{
  char *cstr;
  char *cstr2;
  char *eqchar;
  JSString *jstr;

  switch (enum_op)
  {
    case JSENUMERATE_INIT:
      /* This will represent our index in the environment array */
      *statep = JSVAL_ZERO;
      if (idp)
        /* We don't know how many we got */
        if (!JS_ValueToId(cx, JSVAL_ZERO, idp))
          return JS_FALSE;
      return JS_TRUE;

    case JSENUMERATE_NEXT:
      cstr = environ[JSVAL_TO_INT(*statep)];
      if (!cstr)
      {
        *statep = JSVAL_NULL;
        //JS_ValueToId(cx, JSVAL_VOID, idp);
        return JS_TRUE;
      }
      eqchar = strchr(cstr, '=');
      GPSEE_ASSERT(eqchar);
      cstr2 = JS_malloc(cx, eqchar - cstr + 1);
      if (!cstr2)
        return JS_FALSE;
      strncpy(cstr2, cstr, eqchar - cstr);
      cstr2[eqchar - cstr] = '\0';
      jstr = JS_NewString(cx, cstr2, strlen(cstr2));
      if (!jstr)
        return JS_FALSE;
      if (!JS_ValueToId(cx, STRING_TO_JSVAL(jstr), idp))
        return JS_FALSE;
      *statep = INT_TO_JSVAL(JSVAL_TO_INT(*statep) + 1);
      return JS_TRUE;
        
        
    case JSENUMERATE_DESTROY:
      return JS_TRUE;

    /* Quiet compiler warnings ;) */
    default:
      GPSEE_ASSERT(enum_op == JSENUMERATE_INIT || enum_op == JSENUMERATE_NEXT || enum_op == JSENUMERATE_DESTROY);
      return JS_TRUE;
  }
}

/** Initializes system.env 
 *  @param cx
 *  @param dest   Object upon which to define an "env" property for accessing environ(7)
 *  @returns      The value of the newly-created "env" property, or NULL on error.
 */
JSObject *system_InitEnv(JSContext *cx, JSObject *dest)
{
  JSObject *systemDotEnv;

  /** Our JSClass gives us custom property setters and getters */
  static JSClass env_class =
  {
    GPSEE_CLASS_NAME(Environment),            /**< the name of our JSClass */
    JSCLASS_NEW_ENUMERATE|                    /**< we use the new enumerator API, JSNewEnumerateOp */
    JSCLASS_NEW_RESOLVE,                      /**< we use the new property resolution API, JSNewResolveOp */
    Environment_addProperty,                  /**< default property creator */
    Environment_deleteProperty,               /**< our property deleter */
    Environment_getProperty,                  /**< our property getter */
    Environment_setProperty,                  /**< our property setter */
    (JSEnumerateOp)Environment_enumerate,     /**< our property enumerator */
    (JSResolveOp)Environment_resolveProperty, /**< default property resolver */
    JS_ConvertStub,                           /**< default property conversion */
    JS_FinalizeStub,                          /**< default finalizer */
    JSCLASS_NO_OPTIONAL_MEMBERS
  };

  systemDotEnv = JS_NewObject(cx, &env_class, NULL, dest);
  if (!systemDotEnv)
    return NULL;
  if (!JS_DefineProperty(cx, dest, "env", OBJECT_TO_JSVAL(systemDotEnv), NULL, NULL,
                         JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT))
    return NULL;

  return systemDotEnv;
}
