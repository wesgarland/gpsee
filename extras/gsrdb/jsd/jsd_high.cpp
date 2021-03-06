/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/*
 * @file jsd_high.cpp
 * @brief JavaScript Debugging support - 'High Level' functions
 * @ingroup jsd
 * @author Netscape Communications Corporation
 */

#include "jsd.h"
#ifdef GPSEE
#include <gpsee.h>
#endif

/***************************************************************************/

/* XXX not 'static' because of old Mac CodeWarrior bug */ 
JSCList _jsd_context_list = JS_INIT_STATIC_CLIST(&_jsd_context_list);

/* these are used to connect JSD_SetUserCallbacks() with JSD_DebuggerOn() */
static JSD_UserCallbacks _callbacks;
static void*             _user = NULL; 
static JSRuntime*        _jsrt = NULL;

#ifdef JSD_HAS_DANGEROUS_THREAD
static void* _dangerousThread = NULL;
#endif

#ifdef JSD_THREADSAFE
void* _jsd_global_lock = NULL;
#endif

#ifdef DEBUG
void JSD_ASSERT_VALID_CONTEXT(JSDContext* jsdc)
{
    JS_ASSERT(jsdc->inited);
    JS_ASSERT(jsdc->jsrt);
    JS_ASSERT(jsdc->dumbContext);
    JS_ASSERT(jsdc->glob);
}
#endif

#ifndef GPSEE
static JSClass global_class = {
    "JSDGlobal", 
    JSCLASS_GLOBAL_FLAGS,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};
#endif

static JSBool
_validateUserCallbacks(JSD_UserCallbacks* callbacks)
{
    return !callbacks ||
           (callbacks->size && callbacks->size <= sizeof(JSD_UserCallbacks));
}

void
jsd_BeginRequest(JSDContext *jsdc)
{
    JSD_LOCK();
    if (!jsdc->dumbLevel++)
        JS_SetContextThread(jsdc->dumbContext);

    JS_BeginRequest(jsdc->dumbContext);
    JSD_UNLOCK();
}

void
jsd_EndRequest(JSDContext *jsdc)
{
    JSD_LOCK();
    JS_EndRequest(jsdc->dumbContext);

    if (!--jsdc->dumbLevel)
        JS_ClearContextThread(jsdc->dumbContext);
    JSD_UNLOCK();
}

static JSDContext*
_newJSDContext(JSRuntime*         jsrt,
               JSContext*         jscx,
#ifdef GPSEE
               gpsee_realm_t*     realm,
#endif
               JSD_UserCallbacks* callbacks, 
               void*              user)
{
    JSDContext* jsdc = NULL;

    if( ! jsrt )
        return NULL;

    if( ! _validateUserCallbacks(callbacks) )
        return NULL;

    jsdc = (JSDContext*) calloc(1, sizeof(JSDContext));
    if( ! jsdc )
        goto label_newJSDContext_failure;

    if( ! JSD_INIT_LOCKS(jsdc) )
        goto label_newJSDContext_failure;

    JS_INIT_CLIST(&jsdc->links);

    jsdc->jsrt = jsrt;
    jsdc->jscx = jscx;
    jsdc->oldHooks = NULL;

    if( callbacks )
        memcpy(&jsdc->userCallbacks, callbacks, callbacks->size);
    
    jsdc->user = user;

#ifdef JSD_HAS_DANGEROUS_THREAD
    jsdc->dangerousThread = _dangerousThread;
#endif

    JS_INIT_CLIST(&jsdc->threadsStates);
    JS_INIT_CLIST(&jsdc->sources);
    JS_INIT_CLIST(&jsdc->removedSources);

    jsdc->sourceAlterCount = 1;

    if( ! jsd_CreateAtomTable(jsdc) )
        goto label_newJSDContext_failure;

    if( ! jsd_InitObjectManager(jsdc) )
        goto label_newJSDContext_failure;

    if( ! jsd_InitScriptManager(jsdc) )
        goto label_newJSDContext_failure;

#ifdef GPSEE
    jsdc->dumbContext = gpsee_createContext(realm);
    if (jsdc->dumbContext)
      JS_EndRequest(jsdc->dumbContext);
#else
    jsdc->dumbContext = JS_NewContext(jsdc->jsrt, 256);
#endif
    if( ! jsdc->dumbContext )
        goto label_newJSDContext_failure;

    jsd_BeginRequest(jsdc);

#ifdef GPSEE
    jsdc->glob = realm->globalObject;
#else
    jsdc->glob = JS_NewGlobalObject(jsdc->dumbContext, &global_class);
    if( ! jsdc->glob )
        goto label_newJSDContext_failure;

    if( ! JS_InitStandardClasses(jsdc->dumbContext, jsdc->glob) )
        goto label_newJSDContext_failure;
#endif

    jsd_EndRequest(jsdc);

    jsdc->data = NULL;
    jsdc->inited = JS_TRUE;

    JSD_LOCK();
    JS_INSERT_LINK(&jsdc->links, &_jsd_context_list);
    JSD_UNLOCK();

    return jsdc;

label_newJSDContext_failure:
    if( jsdc ) {
        jsd_DestroyObjectManager(jsdc);
        jsd_DestroyAtomTable(jsdc);     
        if (jsdc->dumbContext)
          jsd_EndRequest(jsdc);
        free(jsdc);
    }
    return NULL;
}

static void
_destroyJSDContext(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);

    JSD_LOCK();
    JS_REMOVE_LINK(&jsdc->links);
    JSD_UNLOCK();

    jsd_DestroyObjectManager(jsdc);
    jsd_DestroyAtomTable(jsdc);

    jsdc->inited = JS_FALSE;

    /*
    * We should free jsdc here, but we let it leak in case there are any 
    * asynchronous hooks calling into the system using it as a handle
    *
    * XXX we also leak the locks
    */
    JS_SetContextThread(jsdc->dumbContext);
#ifdef GPSEE
    JS_BeginRequest(jsdc->dumbContext);
    gpsee_destroyContext(jsdc->dumbContext);
#else
    JS_DestroyContext(jsdc->dumbContext);
#endif
    jsdc->dumbContext = NULL;
}

/***************************************************************************/

JSDContext*
jsd_DebuggerOnForContext(JSContext*         jscx, 
#ifdef GPSEE
                         gpsee_realm_t*     realm,
#endif
                         JSD_UserCallbacks* callbacks, 
                         void*              user)
{
    JSDContext* jsdc;
    JSRuntime* jsrt = JS_GetRuntime(jscx);
    JSDebugHooks* hooks = (JSDebugHooks*)malloc(sizeof (JSDebugHooks));
    if (!hooks)
        return NULL;
    
    jsdc = _newJSDContext(jsrt, jscx, realm, callbacks, user);
    if (!jsdc) {
        free(hooks);
        return NULL;
    }

    JSDebugHooks hookTemplate = {
        jsd_DebuggerHandler, 
        jsdc,
        jsd_NewScriptHookProc,
        jsdc,
        jsd_DestroyScriptHookProc, 
        jsdc,
        jsd_DebuggerHandler, 
        jsdc,
        NULL,
        jsdc,
        jsd_TopLevelCallHook,
        jsdc,
        jsd_FunctionCallHook,
        jsdc,
        jsd_ObjectHook,
        jsdc,
        jsd_ThrowHandler,
        jsdc,
        jsd_DebugErrorHook, 
        jsdc
    };

    jsdc->hooks = hooks;
    /* set hooks here */
    *hooks = hookTemplate;
    JSDebugHooks *oldHooks = JS_SetContextDebugHooks(jscx, hooks);
    const JSDebugHooks *globalHooks = JS_GetGlobalDebugHooks(jsrt);
    if (oldHooks != globalHooks)
        jsdc->oldHooks = oldHooks;

    if( jsdc->userCallbacks.setContext )
        jsdc->userCallbacks.setContext(jsdc, jsdc->user);
    return jsdc;
}

JSDContext*
jsd_DebuggerOnForUser(JSRuntime*         jsrt, 
#ifdef GPSEE
                      gpsee_realm_t*     realm,
#endif
                      JSD_UserCallbacks* callbacks, 
                      void*              user)
{
    JSDContext* jsdc;

    jsdc = _newJSDContext(jsrt, NULL, realm, callbacks, user);
    if( ! jsdc )
        return NULL;

    jsdc->hooks = NULL;
    /* set hooks here */
    JS_SetNewScriptHookProc(jsdc->jsrt, jsd_NewScriptHookProc, jsdc);
    JS_SetDestroyScriptHookProc(jsdc->jsrt, jsd_DestroyScriptHookProc, jsdc);
    JS_SetDebuggerHandler(jsdc->jsrt, jsd_DebuggerHandler, jsdc);
    JS_SetExecuteHook(jsdc->jsrt, jsd_TopLevelCallHook, jsdc);
    JS_SetCallHook(jsdc->jsrt, jsd_FunctionCallHook, jsdc);
    JS_SetObjectHook(jsdc->jsrt, jsd_ObjectHook, jsdc);
    JS_SetThrowHook(jsdc->jsrt, jsd_ThrowHandler, jsdc);
    JS_SetDebugErrorHook(jsdc->jsrt, jsd_DebugErrorHook, jsdc);
#ifdef LIVEWIRE
    LWDBG_SetNewScriptHookProc(jsd_NewScriptHookProc, jsdc);
#endif
    if( jsdc->userCallbacks.setContext )
        jsdc->userCallbacks.setContext(jsdc, jsdc->user);
    return jsdc;
}

JSDContext*
jsd_DebuggerOn(gpsee_realm_t *realm)
{
    JS_ASSERT(_jsrt);
    JS_ASSERT(_validateUserCallbacks(&_callbacks));
    return jsd_DebuggerOnForUser(_jsrt, realm, &_callbacks, _user);
}

void
jsd_DebuggerOff(JSDContext* jsdc)
{
    /* clear hooks here */
    if (jsdc->jscx) {
        JS_SetContextDebugHooks(jsdc->jscx,
                                jsdc->oldHooks
                                ? jsdc->oldHooks
                                : JS_GetGlobalDebugHooks(jsdc->jsrt));
        free(jsdc->hooks);
    } else {
        JS_SetNewScriptHookProc(jsdc->jsrt, NULL, NULL);
        JS_SetDestroyScriptHookProc(jsdc->jsrt, NULL, NULL);
        JS_SetDebuggerHandler(jsdc->jsrt, NULL, NULL);
        JS_SetExecuteHook(jsdc->jsrt, NULL, NULL);
        JS_SetCallHook(jsdc->jsrt, NULL, NULL);
        JS_SetObjectHook(jsdc->jsrt, NULL, NULL);
        JS_SetThrowHook(jsdc->jsrt, NULL, NULL);
        JS_SetDebugErrorHook(jsdc->jsrt, NULL, NULL);
#ifdef LIVEWIRE
        LWDBG_SetNewScriptHookProc(NULL,NULL);
#endif
    }

    /* clean up */
    JSD_LockScriptSubsystem(jsdc);
    jsd_DestroyScriptManager(jsdc);
    JSD_UnlockScriptSubsystem(jsdc);
    jsd_DestroyAllSources(jsdc);
    
    JSD_SetContextProc setContext = NULL;
    void* user;
    if (jsdc->userCallbacks.setContext) {
        setContext = jsdc->userCallbacks.setContext;
        user = jsdc->user;
    }
    _destroyJSDContext(jsdc);

    if (setContext)
        setContext(NULL, user);
}

void
jsd_SetUserCallbacks(JSRuntime* jsrt, JSD_UserCallbacks* callbacks, void* user)
{
    _jsrt = jsrt;
    _user = user;

#ifdef JSD_HAS_DANGEROUS_THREAD
    _dangerousThread = JSD_CURRENT_THREAD();
#endif

    if( callbacks )
        memcpy(&_callbacks, callbacks, sizeof(JSD_UserCallbacks));
    else
        memset(&_callbacks, 0 , sizeof(JSD_UserCallbacks));
}

void*
jsd_SetContextPrivate(JSDContext* jsdc, void *data)
{
    void *rval = jsdc->data;
    jsdc->data = data;
    return rval;
}

void*
jsd_GetContextPrivate(JSDContext* jsdc)
{
    return jsdc->data;
}

void
jsd_ClearAllProfileData(JSDContext* jsdc)
{
    JSDScript *current;
    
    JSD_LOCK_SCRIPTS(jsdc);
    current = (JSDScript *)jsdc->scripts.next;
    while (current != (JSDScript *)&jsdc->scripts)
    {
        jsd_ClearScriptProfileData(jsdc, current);
        current = (JSDScript *)current->links.next;
    }

    JSD_UNLOCK_SCRIPTS(jsdc);
}

JSDContext*
jsd_JSDContextForJSContext(JSContext* context)
{
    JSDContext* iter;
    JSDContext* jsdc = NULL;
    JSRuntime*  runtime = JS_GetRuntime(context);

    JSD_LOCK();
    for( iter = (JSDContext*)_jsd_context_list.next;
         iter != (JSDContext*)&_jsd_context_list;
         iter = (JSDContext*)iter->links.next )
    {
        if( runtime == iter->jsrt )
        {
            jsdc = iter;
            break;
        }
    }
    JSD_UNLOCK();
    return jsdc;
}    

JSBool
jsd_DebugErrorHook(JSContext *cx, const char *message,
                   JSErrorReport *report, void *closure)
{
    JSDContext* jsdc = (JSDContext*) closure;
    JSD_ErrorReporter errorReporter;
    void*             errorReporterData;
    
    if( ! jsdc )
    {
        JS_ASSERT(0);
        return JS_TRUE;
    }
    if( JSD_IS_DANGEROUS_THREAD(jsdc) )
        return JS_TRUE;

    /* local in case hook gets cleared on another thread */
    JSD_LOCK();
    errorReporter     = jsdc->errorReporter;
    errorReporterData = jsdc->errorReporterData;
    JSD_UNLOCK();

    if(!errorReporter)
        return JS_TRUE;

    switch(errorReporter(jsdc, cx, message, report, errorReporterData))
    {
        case JSD_ERROR_REPORTER_PASS_ALONG:
            return JS_TRUE;
        case JSD_ERROR_REPORTER_RETURN:
            return JS_FALSE;
        case JSD_ERROR_REPORTER_DEBUG:
        {
            jsval rval;
            JSD_ExecutionHookProc   hook;
            void*                   hookData;

            /* local in case hook gets cleared on another thread */
            JSD_LOCK();
            hook = jsdc->debugBreakHook;
            hookData = jsdc->debugBreakHookData;
            JSD_UNLOCK();

            jsd_CallExecutionHook(jsdc, cx, JSD_HOOK_DEBUG_REQUESTED,
                                  hook, hookData, &rval);
            /* XXX Should make this dependent on ExecutionHook retval */
            return JS_TRUE;
        }
        case JSD_ERROR_REPORTER_CLEAR_RETURN:
            if(report && JSREPORT_IS_EXCEPTION(report->flags))
                JS_ClearPendingException(cx);
            return JS_FALSE;
        default:
            JS_ASSERT(0);
            break;
    }
    return JS_TRUE;
}

JSBool
jsd_SetErrorReporter(JSDContext*       jsdc, 
                     JSD_ErrorReporter reporter, 
                     void*             callerdata)
{
    JSD_LOCK();
    jsdc->errorReporter = reporter;
    jsdc->errorReporterData = callerdata;
    JSD_UNLOCK();
    return JS_TRUE;
}

JSBool
jsd_GetErrorReporter(JSDContext*        jsdc, 
                     JSD_ErrorReporter* reporter, 
                     void**             callerdata)
{
    JSD_LOCK();
    if( reporter )
        *reporter = jsdc->errorReporter;
    if( callerdata )
        *callerdata = jsdc->errorReporterData;
    JSD_UNLOCK();
    return JS_TRUE;
}
