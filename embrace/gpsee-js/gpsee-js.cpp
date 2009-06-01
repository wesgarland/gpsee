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

#include <gpsee.h>

static const char rcsid[]="$Id: gpsee-js.cpp,v 1.1 2009/05/27 04:16:27 wes Exp $";
static int jsshell_contextPrivate_id = 1234;	/* we use the address, not the number */

#undef JS_GetContextPrivate
#undef JS_SetContextPrivate
#undef JS_GetGlobalObject
#undef main

#if 0
#define JS_GetContextPrivate(cx)	gpsee_getContextPrivate(cx, &jsshell_contextPrivate_id, sizeof(void *), NULL)
#define JS_SetContextPrivate(cx, data)	((*(void **)gpsee_getContextPrivate(cx, &jsshell_contextPrivate_id, sizeof(void *), NULL) = data), JS_TRUE)
#define JS_SetContextCallback(rt, cb)	gpsee_getContextPrivate(cx ? cx : ((gpsee_interpreter_t *)JS_GetRuntimePrivate(rt))->cx, NULL, 0, cb)
#endif 

#undef JS_NewRuntime
#undef JS_DestroyRuntime

void gpseejs_DestroyRuntime(JSRuntime *rt)
{
  JS_SetContextCallback(rt, NULL);
  gpsee_destroyInterpreter((gpsee_interpreter_t *)JS_GetRuntimePrivate(rt));
}

#define JS_DestroyRuntime(rt) gpseejs_DestroyRuntime(rt)

#define JS_InitStandardClasses(cx, obj) gpsee_initGlobalObject(cx, obj, NULL, NULL)
#define JS_SetGlobalObject(cx, obj) gpsee_initGlobalObject(cx, obj, NULL, NULL);

#define PRODUCT_SHORTNAME	"gpsee-js"

static void __attribute__((noreturn)) fatal(const char *message)
{
  if (!message)
    message = "UNDEFINED MESSAGE - OUT OF MEMORY?";

  if (isatty(STDERR_FILENO))
  {
    fprintf(stderr, "\007Fatal Error in " PRODUCT_SHORTNAME ": %s\n", message);
  }
  else
    gpsee_log(SLOG_EMERG, "Fatal Error: %s", message);

  exit(1);
}

/** GPSEE uses panic() to panic, expects embedder to provide */
void __attribute__((noreturn)) panic(const char *message)
{
  fatal(message);
}

#define JS_NewRuntime(n) gpseejs_NewRuntime(n)
JSRuntime *gpseejs_NewRuntime(size_t n)
{
  extern char * const *environ;

  gpsee_interpreter_t *jsi = gpsee_createInterpreter(NULL, environ);

  return jsi->rt;
}

JSObject *gpseejs_NewObject(JSContext *cx, JSClass *clasp, JSObject *proto, JSObject *parent)
{
  extern JSClass global_class;

  if (clasp == &global_class)
    clasp = gpsee_getGlobalClass();

  return JS_NewObject(cx, clasp, proto, parent);
}
#define JS_NewObject(cx, clasp, proto, parent)	gpseejs_NewObject(cx, clasp, proto, parent)

#include "shell/js.cpp"

