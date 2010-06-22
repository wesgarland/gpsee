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
 * Copyright (c) 2007-2010, PageMail, Inc. All Rights Reserved.
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
 * @file	gpsee_debugger.c
 * @brief       Support for adding debugger functionality to GPSEE embeddings.
 * @ingroup     debugger
 * @author	Wes Garland
 * @date	June 2010
 * @version	$Id: gpsee_debugger.c,v 1.1 2010/06/14 22:23:49 wes Exp $
 */
 
static __attribute__((unused)) const char rcsid[]="$Id: gpsee_debugger.c,v 1.1 2010/06/14 22:23:49 wes Exp $";

#include <prinit.h>
#include "gpsee.h"
#include "jsdebug.h"
#include "jsdb.h"

JS_PUBLIC_API(JSBool) JS_SetFunctionNative(JSContext *cx, JSFunction *fun, JSNative funp);
JS_PUBLIC_API(JSBool) JS_SetFunctionFastNative(JSContext *cx, JSFunction *fun, JSFastNative funp);

static void SendSourceToJSDebugger(const char *filename, uintN lineno,
                                   jschar *str, size_t length,
                                   void **listenerTSData, void *closure)
{
  JSDSourceText *jsdsrc = (JSDSourceText *) *listenerTSData;
  JSDContext *jsdc  = (JSDContext *)closure;

  if (!jsdsrc) {
    if (!filename)
      filename = "typein";
    if (1 == lineno) {
      jsdsrc = JSD_NewSourceText(jsdc, filename);
    } else {
      jsdsrc = JSD_FindSourceForURL(jsdc, filename);
      if (jsdsrc && JSD_SOURCE_PARTIAL !=
          JSD_GetSourceStatus(jsdc, jsdsrc)) {
        jsdsrc = NULL;
      }
    }
  }
  if (jsdsrc) {
    jsdsrc = JSD_AppendUCSourceText(jsdc,jsdsrc, str, length,
                                    JSD_SOURCE_PARTIAL);
  }
  *listenerTSData = jsdsrc;
}     

/** 
 *  Entry point for native-debugger debugging. Set this function as a break point
 *  in your native-language debugger debugging gsrdb, and you can set breakpoints
 *  on JSNative (and JSFastNative) functions from the gsrdb user interface.
 */
void __attribute__((noinline)) gpsee_breakpoint(void) 
{
  printf("\n");
  return;
}

JSBool gpsee_fastNative_breakpoint(JSContext *cx, uintN argc, jsval *vp)
{
  gpsee_realm_t *realm = gpsee_getRealm(cx);
  JSFunction    *fun   = JS_ValueToFunction(cx, JS_CALLEE(cx, vp));
  JSFastNative  jsfn   = gpsee_ds_get(realm->moduleData, fun);

  gpsee_breakpoint();

  if (!jsfn)
    panic("Corrupted fast native breakpoint");

  return jsfn(cx, argc, vp);
}

JSBool gpsee_native_breakpoint(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  gpsee_realm_t *realm = gpsee_getRealm(cx);
  JSFunction    *fun   = JS_ValueToFunction(cx, argv[-2]);
  JSNative      jsn    = gpsee_ds_get(realm->moduleData, fun);
  gpsee_breakpoint();

  if (!jsn)
    panic("Corrupted fast native breakpoint");

  return jsn(cx, obj, argc, argv, rval);
}

JSBool gpsee_native_break(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  JSFunction    *fun;
  JSNative      jsn;
  gpsee_realm_t *realm = gpsee_getRealm(cx);
  jsdouble      d;

  if (argc != 1)
    return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".nativeBreak.arguments.count");

  fun = JS_ValueToFunction(cx, argv[0]);
  if (!fun)
    return JS_FALSE;

  jsn = JS_GetFunctionNative(cx, fun);
  if (jsn)
  {
    JS_SetFunctionNative(cx, fun, gpsee_native_breakpoint);
  }
  else
  {
    jsn = (JSNative)JS_GetFunctionFastNative(cx, fun);
    if (!jsn)
      return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".nativeBreak.type: Cannot set a native breakpoint on an interpreted function");

    JS_SetFunctionFastNative(cx, fun, gpsee_fastNative_breakpoint);
  }

  gpsee_ds_put(realm->moduleData, fun, jsn);

  d = (jsdouble)(size_t)jsn;
  return JS_NewNumberValue(cx, d, rval);
}

/** Initialize the debugger for this embedding. Not sure if this is per-cx or per-rt.
 *  Must be called before the embedding processes JavaScript to be debugged. This is
 *  'debuggee' context, as opposed to 'debugger'
 *
 *  @param      cx              The current JavaScript context
 *  @param      realm           The current GPSEE realm
 *  @param      debugger        The fully-qualified path to the debugger.js we will invoke
 *  @returns    A new JSD Context
 */
JSDContext *gpsee_initDebugger(JSContext *cx, gpsee_realm_t *realm, const char *debugger)
{
  JSDContext    *jsdc;
  JSFunction    *fn;

  jsdc = JSD_DebuggerOnForUser(realm->grt->rt, realm, NULL, NULL);
  if (!jsdc)
    panic("Could not start JSD debugger layer");
  JSD_JSContextInUse(jsdc, cx);
  JS_SetSourceHandler(realm->grt->rt, SendSourceToJSDebugger, jsdc);

  if (JSDB_InitDebugger(realm->grt->rt, jsdc, 0, debugger) != JS_TRUE)
    panic("Could not start JSDB debugger layer");

  realm->grt->useCompilerCache = 0;       /* Interacts poorly with JSD source-code transmitter */

  /* Native break has to be defined in terms of the debuggee, or we will have to 
   * contort ourselves badly to modify the JSFunction cross-runtime. JSClass for Function
   * will be wrong, for starters.
   */
  fn = JS_DefineFunction(cx, realm->globalObject, "native_break", gpsee_native_break, 0, 0);
  if (!fn)
    panic("Could not start JSDB debugger layer");

  return jsdc;
}

/** Turn off the debugger.
 *  @see gpsee_initDebugger()
 *
 *  @param      jsdc    The JSD Context to finalize
 */
void gpsee_finiDebugger(JSDContext *jsdc)
{
  JSDB_TermDebugger(jsdc);       
  JSD_DebuggerOff(jsdc);
}

