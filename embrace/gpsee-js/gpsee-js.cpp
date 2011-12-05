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

/*
 * @note        This file is extremely sensitive in changes to JSAPI due to
 *              its promicuous use of both shell and JSAPI internals. It is
 *              intended to be used ONLY with the recommended JSAPI release;
 *              GPSEE cannot provide sufficient API insulation.
 */
#include <gpsee.h>
#ifdef GPSEE_JSDB
# include "jsdebug.h"
# include "jsdb.h"
#endif
#undef offsetOf

static const char rcsid[]="$Id: gpsee-js.cpp,v 1.6 2010/12/02 21:59:42 wes Exp $";

#ifndef PRODUCT_SHORTNAME
# define PRODUCT_SHORTNAME	"gpsee-js"
#endif

#undef main
#define main(a,b,c) notMain(a,b,c)

gpsee_interpreter_t *jsi;
#define JS_NewContext(rt, dummy) gpsee_createContext(jsi->realm)
#define JS_DestroyContext(cx) gpsee_destroyContext(cx)

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
    JSContext *cx;
    int result;
#ifdef GPSEE_JSDB
    JSDContext *jsdc;
    JSBool jsdbc;
#endif

#if defined(__SURELYNX__)
    static apr_pool_t *permanent_pool = apr_initRuntime();
#endif

    jsi = gpsee_createInterpreter();
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

#ifdef XP_OS2
   /* these streams are normally line buffered on OS/2 and need a \n, *
    * so we need to unbuffer then to get a reasonable prompt          */
    setbuf(stdout,0);
    setbuf(stderr,0);
#endif

    gErrFile = stderr;
    gOutFile = stdout;

    argc--;
    argv++;

#ifdef XP_WIN
    // Set the timer calibration delay count to 0 so we get high
    // resolution right away, which we need for precise benchmarking.
    extern int CALIBRATION_DELAY_COUNT;
    CALIBRATION_DELAY_COUNT = 0;
#endif

    if (!InitWatchdog(jsi->rt))
        return 1;

    cx = NewContext(jsi->rt);
    if (!cx)  
        return 1;

    JS_SetGCParameterForThread(cx, JSGC_MAX_CODE_CACHE_BYTES, 16 * 1024 * 1024);

#ifdef GPSEE_JSDB
    jsdc = JSD_DebuggerOnForUser(rt, jsi->realm, NULL, NULL);
    if (!jsdc)
        return 1;
    JSD_JSContextInUse(jsdc, cx);
    JS_SetSourceHandler(rt, SendSourceToJSDebugger, jsdc);
    jsdbc = JSDB_InitDebugger(rt, jsdc, 0, DEBUGGER_JS);
#endif

    result = shell(cx, argc, argv, envp);

#ifdef GPSEE_JSDB
    if (jsdc) {
        if (jsdbc)
            JSDB_TermDebugger(jsdc);
        JSD_DebuggerOff(jsdc);
    }
#endif

    JS_CommenceRuntimeShutDown(jsi->rt);

    DestroyContext(cx, true);

    KillWatchdog();

    gpsee_destroyInterpreter(jsi);

    JS_ShutDown();
    return result;
}
