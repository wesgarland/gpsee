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
 * Copyright (c) 2010, PageMail, Inc. All Rights Reserved.
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
 *
 *  @file       gpsee_realms.c          Code to create and use GPSEE Realms.
 *
 *  The GPSEE Realm is a data structure which describes everything locally
 *  unique to the running script.  The super-global, module memos, private 
 *  storage used by native code, etc, are stored in the current realm. Once
 *  a context is assigned to a particular realm, it may not migrate.
 *
 *  Realms may be shared among native threads, and may encompass more than
 *  one JavaScript context.  Each runtime, worker thread, sandbox, etc -- 
 *  wherever we don't want covert (or overt) communication channels -- gets
 *  its own realm.  JSObjects <b>can</b> be shared across runtimes, but 
 *  <b>should</b> only be shared across extents. The natural exception to
 *  this rule exists when setting up an overt communication channel, such
 *  as the messaging-passing API between parent and worker threads.
 *
 *  @note       Prior to the arrival of gpsee realms, many items now stored
 *              in a realm were stored in the gpsee_runtime_t structure.
 *              That structure is now reserved for per-runtime storage.
 *
 *  A pointer to the current gpsee realm is stored in the following places:
 *   - The private slot of the super-global, if its class pointer matches
 *     gpsee_getGlobalClass();
 *   - A list of gpsee realms in the grt->realmsByContext data store, indexed
 *     by context (context creator is responsible for adding; 
 *     gpsee_createContext() knows about it.
 *   - Potentially elsewhere
 *
 *  All pointers stored in the gpsee_realm_t are valid for the lifetime of
 *  the realm, and are safe to read unlocked. Facilities described by each
 *  pointer may be synchronized by facility-specific means.
 *
 *  Writing pointers into the gpsee_realm_t should be done one of two ways:
 *   - Unlocked, during realm creation/initialization
 *   - Through a compare-and-swap operation. The initial state of unused
 *     pointers in this structure is guaranteed to be NULL.
 *
 *  The address of the gpsee realm is guaranteed to never change, and
 *  remain valid until all contexts associated with the realm are 
 *  destroyed.
 *
 *  @author     Wes Garland, wes@page.ca
 *  @date       May 2010
 *  @version    $Id: gpsee_realms.c,v 1.2 2010/09/01 18:12:35 wes Exp $
 */

#include "gpsee.h"
#include "gpsee_private.h"

static gpsee_realm_t *getRealm(JSContext *cx)
{
  JSObject              *global = JS_GetGlobalObject(cx);
  gpsee_runtime_t       *grt;
  gpsee_realm_t         *realm = NULL;

  if ((realm = gpsee_getModuleScopeRealm(cx, NULL)))
    return realm;

  grt = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  gpsee_enterAutoMonitor(cx, &grt->monitors.realms);
  if (grt && grt->realmsByContext)
    realm = gpsee_ds_get(grt->realmsByContext, cx);
  gpsee_leaveAutoMonitor(grt->monitors.realms);

  if (global && JS_GET_CLASS(cx, global) == gpsee_getGlobalClass())
    GPSEE_ASSERT(realm);

  return realm;
}

/**
 *  Retrieve the GPSEE Realm data structure for the supplied context.
 *
 *  @param      cx      A context in the GPSEE Realm
 *  @returns    The realm, or NULL if we've thrown an exception.
 *
 *  @note       This function is more pedantic in DEBUG builds than
 *              RELEASE builds. In DEBUG we find what we <i>should</i>,
 *              in RELEASE we find what we <i>can</i>.
 */
gpsee_realm_t *gpsee_getRealm(JSContext *cx)
{
  gpsee_realm_t *realm = getRealm(cx);

  GPSEE_ASSERT(realm);

  if (!realm)
  {
    (void)gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".getRealm: Could not identify GPSEE Realm.");
    return NULL;
  }

  return realm;
}       

static JSBool ModuleObject(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}

/**
 *  Create a new GPSEE Realm. New realm will be initialized to have all members NULL, except
 *   - the context and name provided (name only present in debug build)
 *   - an initialized module system (and hence module data store)
 *   - an initialized I/O hooks data store
 *   - an initialized global object
 *   - a prototype for the module object
 *
 *  @param      grt     The GPSEE runtime to which the new realm will belong
 *  @param      name    A symbolic name, for use in debugging, to describe this realm. Does not need to be unique.
 *
 *  @returns    The new realm, or NULL if we threw an exception.
 */
gpsee_realm_t *gpsee_createRealm(gpsee_runtime_t *grt, const char *name)
{
  gpsee_realm_t         *realm = NULL;
  JSContext             *cx;

  cx = JS_NewContext(grt->rt, 8192);
  if (!cx)
    return NULL;

  JS_BeginRequest(cx);
  JS_SetOptions(cx, JS_GetOptions(grt->coreCx));
  gpsee_enterAutoMonitor(cx, &grt->monitors.realms);

  realm = JS_malloc(cx, sizeof(*realm));
  if (!realm)
    goto err_out;

  memset(realm, 0, sizeof(*realm));
  realm->grt = grt;

#ifdef GPSEE_DEBUG_BUILD
  realm->name = JS_strdup(cx, name);
  if (!realm->name)
    goto err_out; 
#endif

  realm->user_io_pendingWrites = gpsee_ds_create(grt, GPSEE_DS_OTM_KEYS, 0);
  if (!realm->user_io_pendingWrites)
    return JS_FALSE;

#if defined(JSRESERVED_GLOBAL_COMPARTMENT)
  realm->globalObject = JS_NewCompartmentAndGlobalObject(cx, gpsee_getGlobalClass(), NULL);
#else
  realm->globalObject = JS_NewGlobalObject(cx, gpsee_getGlobalClass());
#endif
  if (!realm->globalObject)
    goto err_out;

  {
    static JSClass moduleObjectClass = 
    {
      GPSEE_GLOBAL_NAMESPACE_NAME ".Module", 0,
      JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
      JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
    };

#undef JS_InitClass    
    moduleObjectClass.name += sizeof(GPSEE_GLOBAL_NAMESPACE_NAME);
    realm->moduleObjectProto = JS_InitClass(cx, realm->globalObject, NULL, &moduleObjectClass, 
					    ModuleObject, 0, NULL, NULL, NULL, NULL);
    moduleObjectClass.name -= sizeof(GPSEE_GLOBAL_NAMESPACE_NAME);
    realm->moduleObjectClass = &moduleObjectClass;
#define JS_InitClass poison
  }

  if (!realm->moduleObjectProto)
    goto err_out;

  JS_AddNamedObjectRoot(cx, &realm->globalObject, "super-global");

  if (gpsee_ds_put(grt->realms, realm, NULL) == JS_FALSE)
    goto err_out;

  if (gpsee_initializeModuleSystem(cx, realm) == JS_FALSE)
    panic("Unable to initialize module system");

  if (gpsee_initGlobalObject(cx, realm, realm->globalObject) == JS_FALSE)
    goto err_out;

  realm->cachedCx = cx;

  goto out;

  err_out:
  if (realm)
  {
    JS_free(cx, realm);
#ifdef GPSEE_DEBUG_BUILD
    if (realm->name)
      JS_free(cx, (char *)realm->name);
#endif
    realm = NULL;
  }

  if (cx) {
    JS_DestroyContext(cx);
    cx = NULL;
  }

  out:
  gpsee_leaveAutoMonitor(grt->monitors.realms);
  if (cx)
    JS_EndRequest(cx);
  return realm;
}

/** Callback which destroys context for a particular realm */
static JSBool destroyRealmContext_cb(JSContext *cx, const void *key, void *value, void *private)
{
  JSContext     *context = (void *)key;
  gpsee_realm_t *realm = value;
  gpsee_realm_t *dying_realm = private;

  if (realm == dying_realm)
    gpsee_destroyContext((JSContext *)context);

  return JS_TRUE;
}

/**
 *   Destroy a GPSEE realm.  Destroys all resources in the realm. 
 *   It is the caller's responsibility to insure no other thread is trying to use this realm.
 *
 *   @param     realm   The realm to destroy
 *   @param     cx      A context which is in the realm's runtime but not in the realm. The
 *                      context must be in a request.
 *
 *   @returns JS_TRUE on success
 */
JSBool gpsee_destroyRealm(JSContext *cx, gpsee_realm_t *realm)
{
  size_t                fd;
  gpsee_runtime_t       *grt = realm->grt;

  gpsee_uio_dumpPendingWrites(cx, realm);

  /** Clean up any user I/O hooks belonging to the current realm */
  for (fd = 0; fd < grt->user_io.hooks_len; fd++)
  {
    gpsee_enterAutoMonitor(cx, &grt->monitors.user_io);
    memset(&grt->user_io.hooks[fd], 0, sizeof(grt->user_io.hooks[0]));
    gpsee_leaveAutoMonitor(grt->monitors.user_io);
  }

  gpsee_removeAllGCCallbacks_forRealm(realm->grt, realm);
  JS_RemoveObjectRoot(cx, &realm->globalObject);
  JS_SetGlobalObject(cx, NULL);

  JS_EndRequest(cx);
  JS_GC(cx);
  JS_BeginRequest(cx);
  gpsee_shutdownModuleSystem(cx, realm);

  gpsee_enterAutoMonitor(cx, &realm->monitors.programModuleDir);
  realm->monitored.programModuleDir = NULL;
  gpsee_leaveAutoMonitor(realm->monitors.programModuleDir);

  if (gpsee_ds_forEach(cx, grt->realmsByContext, destroyRealmContext_cb, realm) == JS_FALSE)
    return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".destroyRealmContext");

  gpsee_moduleSystemCleanup(cx, realm);

  if (realm->cachedCx)
    JS_DestroyContext(realm->cachedCx);
  
  gpsee_ds_destroy(realm->user_io_pendingWrites);

#ifdef GPSEE_DEBUG_BUILD
  memset(realm, 0xde, sizeof(*realm));
#endif
  JS_free(cx, realm);

  return JS_TRUE;
}

/**
 *  Create a new JS Context, initialized as a member of the passed realm, with an active JS request
 *  on the current thread. The following context initializations will be performed:
 *  - JS Request will be entered
 *  - JS Options inherited from grt->coreCx
 *  - JS version inherited from grt->coreCx
 *  - Thread stack limit will be initialized
 *  - Global variable will be set to realm's global
 *  - Error reporter will be set to gpsee_errorReporter
 *  - Operation callback will be initialized to use the muxed async facility
 *
 *  @param      realm           The realm to which the new context belongs.
 *  @returns    A pointer to a new JSContext, or NULL if we threw an exception or realm was NULL.
 *
 *  @note       If NULL is passed for realm, we return NULL without throwing a
 *              new exception. This is to allow chaining with gpsee_createRealm().
 *
 *  @note       The grt->monitors.cx monitor is used to insure that we don't churn the cx pointer
 *              in/out of the grt->realmsByContext list if we happen to call create/destroy at 
 *              nearly the exact same time on two threads.
 */
JSContext *gpsee_createContext(gpsee_realm_t *realm)
{
  JSContext             *cx;

  if (!realm)
    return NULL;
  
  gpsee_enterAutoMonitorRT(realm->grt, &realm->grt->monitors.cx);

  cx = realm->cachedCx;
  if ((cx == NULL) || (jsval_CompareAndSwap((jsval *)&realm->cachedCx, (jsval)cx, (jsval)NULL) == JS_FALSE))
    cx = JS_NewContext(realm->grt->rt, 8192);

  JS_SetThreadStackLimit(cx, realm->grt->threadStackLimit);

  if (gpsee_ds_put(realm->grt->realmsByContext, cx, realm) == JS_FALSE)
  {
    JS_DestroyContext(cx);
    gpsee_leaveAutoMonitor(realm->grt->monitors.cx);
    return NULL;
  }

  gpsee_leaveAutoMonitor(realm->grt->monitors.cx);

  JS_BeginRequest(cx);
  JS_SetOptions(cx, JS_GetOptions(realm->grt->coreCx));
  JS_SetGlobalObject(cx, realm->globalObject);
  JS_SetErrorReporter(cx, gpsee_errorReporter);
  JS_SetOperationCallback(cx, gpsee_operationCallback);
  JS_SetVersion(cx, JS_GetVersion(realm->grt->coreCx));

  return cx;
}

/** Destroy the passed JS context, closing the request opened during gpsee_createContext(), 
 *  and removing the context from the relevant runtime/realm book-keeping memos.
 *
 *  @param      cx      The context to destroy
 *
 */
void gpsee_destroyContext(JSContext *cx)
{       
  gpsee_runtime_t       *grt = gpsee_getRuntime(cx);
  gpsee_realm_t         *realm;

  JS_EndRequest(cx);
  gpsee_enterAutoMonitor(cx, &grt->monitors.cx);
  JS_DestroyContext(cx);

  realm = gpsee_ds_remove(grt->realmsByContext, cx);
  GPSEE_ASSERT(realm);
  gpsee_leaveMonitor(grt->monitors.cx);
}
