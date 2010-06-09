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
 * The Initial Developer of the Original Code is Mozilla.
 *
 * Portions created by PageMail, Inc. are 
 * Copyright (c) 2007-2010, PageMail, Inc. All Rights Reserved.
 *
 * Contributor(s): Wes Garland, wes@page.ca, PageMail, Inc.
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
#ifdef GPSEE_JSDB
# include "jsdebug.h"
# include "jsdb.h"
#endif
#undef offsetOf

static const char rcsid[]="$Id: gpsee-js.cpp,v 1.4 2010/04/28 12:45:52 wes Exp $";

#ifndef PRODUCT_SHORTNAME
# define PRODUCT_SHORTNAME	"gpsee-js"
#endif

#undef main
#define main(a,b,c) notMain(a,b,c)
#include "shell/js.cpp"
#undef main

#if defined(JS_THREADSAFE) && defined(jsworkers_h___) && !defined(GPSEE_NO_WORKERS)
# define WORKERS
#else
# if defined(GPSEE_NO_WORKERS)
#  undef WORKERS
# endif
#endif

#ifndef WORKERS
# warning Building without worker thread support
#endif

#ifdef GPSEE_JSDB
/*
 * This facilitates sending source to JSD (the debugger system) in the shell
 * where the source is loaded using the JSFILE hack in jsscan. The function
 * below is used as a callback for the jsdbgapi JS_SetSourceHandler hook.
 * A more normal embedding (e.g. mozilla) loads source itself and can send
 * source directly to JSD without using this hook scheme.
 */
static void
SendSourceToJSDebugger(const char *filename, uintN lineno,
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
#endif

int
main(int argc, char **argv, char **envp)
{
    int stackDummy;
    JSRuntime *rt;
    JSContext *cx;
    JSObject *glob, *it, *envobj;
    int result;
    gpsee_interpreter_t *jsi = gpsee_createInterpreter();
#ifdef GPSEE_JSDB
    JSDContext *jsdc;
    JSBool      jsdbc;
#endif

    global_class = *gpsee_getGlobalClass();
    CheckHelpMessages();
#ifdef HAVE_SETLOCALE
    setlocale(LC_ALL, "");
#endif

#ifdef JS_THREADSAFE
    if (PR_FAILURE == PR_NewThreadPrivateIndex(&gStackBaseThreadIndex, NULL) ||
        PR_FAILURE == PR_SetThreadPrivate(gStackBaseThreadIndex, &stackDummy)) {
        return 1;
    }
#else
    gStackBase = (jsuword) &stackDummy;
#endif

    gErrFile = stderr;
    gOutFile = stdout;

    argc--;
    argv++;

    if (!jsi)
      return 1;

    rt = jsi->rt;

    if (!InitWatchdog(rt))
        return 1;

#ifdef jsworkers_h___
    cx = NewContext(rt);
    if (!cx)
      return 1;
#else
    JS_SetContextCallback(rt, ContextCallback);
    cx = jsi->cx;
    ContextCallback(cx, JSCONTEXT_NEW);
#endif

    JS_SetGCParameterForThread(cx, JSGC_MAX_CODE_CACHE_BYTES, 16 * 1024 * 1024);

    JS_BeginRequest(cx);

    glob = jsi->globalObject;
    if (!glob)
        return 1;

    if (cx != jsi->cx)
      JS_SetGlobalObject(cx, glob);

#ifdef JS_HAS_CTYPES
    if (!JS_InitCTypesClass(cx, glob))
        return NULL;
#endif

    if (!JS_DefineFunctions(cx, glob, shell_functions))
        return 1;

    it = JS_DefineObject(cx, glob, "it", &its_class, NULL, 0);
    if (!it)
        return 1;
    if (!JS_DefineProperties(cx, it, its_props))
        return 1;
    if (!JS_DefineFunctions(cx, it, its_methods))
        return 1;

    if (!JS_DefineProperty(cx, glob, "custom", JSVAL_VOID, its_getter,
                           its_setter, 0))
        return 1;
    if (!JS_DefineProperty(cx, glob, "customRdOnly", JSVAL_VOID, its_getter,
                           its_setter, JSPROP_READONLY))
        return 1;

    envobj = JS_DefineObject(cx, glob, "environment", &env_class, NULL, 0);
    if (!envobj || !JS_SetPrivate(cx, envobj, envp))
        return 1;

#ifdef GPSEE_JSDB
    jsdc = JSD_DebuggerOnForUser(rt, jsi->realm, NULL, NULL);
    if (!jsdc)
        return 1;
    JSD_JSContextInUse(jsdc, cx);
    JS_SetSourceHandler(rt, SendSourceToJSDebugger, jsdc);
    jsdbc = JSDB_InitDebugger(rt, jsdc, 0);
#endif

#ifdef WORKERS
    class ShellWorkerHooks : public js::workers::WorkerHooks {
    public:
        JSObject *newGlobalObject(JSContext *cx) { return NewGlobalObject(cx); }
    };
    ShellWorkerHooks hooks;
    if (!JS_AddNamedRoot(cx, &gWorkers, "Workers") ||
        !js::workers::init(cx, &hooks, glob, &gWorkers)) {
        return 1;
    }
#endif

    result = ProcessArgs(cx, glob, argv, argc);

#ifdef WORKERS
    js::workers::finish(cx, gWorkers);
    JS_RemoveRoot(cx, &gWorkers);
    if (result == 0)
        result = gExitCode;
#endif

#ifdef GPSEE_JSDB
    if (jsdc) {
        if (jsdbc)
            JSDB_TermDebugger(jsdc);
        JSD_DebuggerOff(jsdc);
    }
#endif

    JS_EndRequest(cx);

#ifdef jsworkers_h___
    JS_CommenceRuntimeShutDown(rt);
    DestroyContext(cx, true);
#endif

    KillWatchdog();

    gpsee_destroyInterpreter(jsi);

    JS_ShutDown();
    return result;
}



