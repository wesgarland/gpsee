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
 *              in a realm were stored in the gpsee_interpreter_t structure.
 *              That structure is now reserved for per-runtime storage.
 *
 *  A pointer to the current gpsee realm is stored in the following places:
 *   - The private slot of the super-global, if its class pointer matches
 *     gpsee_getGlobalClass();
 *   - A list of gpsee realms in the jsi->realmsByContext data store, indexed
 *     by context (context creator is responsible for adding; 
 *     gpsee_newContext() knows about it.
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
 *  @version    $Id:$
 */

#include "gpsee.h"
#include "gpsee_private.h"

static gpsee_realm_t *getRealm(JSContext *cx)
{
  JSObject              *global = JS_GetGlobalObject(cx);
  gpsee_interpreter_t   *jsi;
  gpsee_realm_t         *realm = NULL;

  if (global && JS_GET_CLASS(cx, global) == gpsee_getGlobalClass())
  {
    realm = JS_GetPrivate(cx, global);
    GPSEE_ASSERT(realm);
    if (realm)
      return realm;
  }

  jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  gpsee_enterAutoMonitor(cx, &jsi->monitors.realms);
  if (jsi && jsi->realmsByContext)
    realm = gpsee_ds_get(cx, jsi->realmsByContext, cx);
  gpsee_leaveAutoMonitor(jsi->monitors.realms);

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

/** 
 *  Set the GPSEE realm for the indicated context.
 *
 *  Migrating a context from one realm to another is forbidden.
 *  If the context has a GPSEE super-global, it must not already belong to a different realm. 
 *  
 *  @param      cx              The JavaScript context used to call into JSAPI, and to assign to the
 *                              specified GPSEE Realm.
 *  @param      realm           The GPSEE Realm in which to add ccx
 *
 *  @returns    JS_TRUE on success; JS_FALSE if we threw an exception.
 */
JSBool gpsee_setRealm(JSContext *cx, gpsee_realm_t *realm)
{
  gpsee_interpreter_t   *jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  JSObject *global = JS_GetGlobalObject(cx);

  if (global && JS_GET_CLASS(cx, global) == gpsee_getGlobalClass())
  {
    /* Found a super-global */
    gpsee_realm_t       *other_realm = JS_GetPrivate(cx, global);
    
    if (other_realm && other_realm != realm)
      return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".setRealm: Passed context already has a GPSEE Realm!");
  }

  if (global)
    JS_SetPrivate(cx, global, realm);

  return gpsee_ds_put(cx, jsi->realmsByContext, cx, realm);
}

/**
 *  Create a new GPSEE Realm. New realm will be initialized to have all members NULL, except
 *   - the context and name provided
 *   - an initialized module system
 *   - an initialized data store
 *   - an initialized global object (if no global set in cx)
 *
 *  @param      cx      A JavaScript context to be associated with this realm
 *  @param      name    A symbolic name, for use in debugging, to describe this realm. Does not need to be unique.
 *
 *  @returns    The new realm, or NULL if we threw an exception.
 */
gpsee_realm_t *gpsee_createRealm(JSContext *cx, const char *name)
{
  gpsee_interpreter_t   *jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  gpsee_realm_t         *realm = NULL;

  if (getRealm(cx))
  {
    (void)gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".createRealm.create: Context already associated with a realm");
    goto err_out;
  }

  realm = JS_malloc(cx, sizeof(*realm));
  if (!realm)
    goto err_out;

  memset(realm, 0, sizeof(*realm));
  realm->cx = cx;
#ifdef GPSEE_DEBUG_BUILD
  realm->name = JS_strdup(cx, name);
  if (!realm->name)
    goto err_out; 
#endif

  realm->globalObject = JS_GetGlobalObject(cx);
  if (!realm->globalObject)
  {
    realm->globalObject = JS_NewObject(cx, gpsee_getGlobalClass(), NULL, NULL);
    if (!realm->globalObject)
      goto err_out;
  }

  JS_AddNamedRoot(cx, &realm->globalObject, "super-global");

  gpsee_enterAutoMonitor(cx, &jsi->monitors.realms);
  if (gpsee_ds_put(cx, jsi->realms, realm, NULL) == JS_FALSE)
    goto err_out;

  if (gpsee_initializeModuleSystem(realm, cx) == JS_FALSE)
    panic("Unable to initialize module system");

  if (gpsee_initGlobalObject(cx, realm->globalObject) == JS_FALSE)
    goto err_out;

  if (gpsee_setRealm(cx, realm) == JS_FALSE)
    goto err_out;

  goto out;

  err_out:
  if (realm)
  {
    JS_free(cx, realm);
    if (realm->name)
      JS_free(cx, (char *)realm->name);

    realm = NULL;
  }

  out:
  gpsee_leaveAutoMonitor(jsi->monitors.realms);
  return realm;
}

static JSBool destroyRealmContext_cb(JSContext *cx, const void *key, void *value, void *private)
{
  JSContext     *context = (void *)key;
  gpsee_realm_t *realm = value;
  gpsee_realm_t *dying_realm = private;

  GPSEE_ASSERT(realm == gpsee_getRealm(context));

  if ((cx != context) && realm == dying_realm)
    JS_DestroyContext((JSContext *)context);

  return JS_TRUE;
}

/**
 *   Destroy a GPSEE realm.  Destroys all resources in the realm. The primordial
 *   context and the super-global's root will be the among first resources destroyed,
 *   possibly destroying other contexts in their wake (such as might occur during
 *   finalization of objects in a thread class).
 *
 *   @param     cx      A context in the same runtime as the realm
 *   @param     realm   The realm to destroy
 *
 *   @returns JS_TRUE on success
 */
JSBool gpsee_destroyRealm(JSContext *cx, gpsee_realm_t *realm)
{
  gpsee_interpreter_t   *jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));

  gpsee_shutdownModuleSystem(realm, cx);

  JS_BeginRequest(cx);
  JS_RemoveRoot(cx, &realm->globalObject);
  gpsee_enterAutoMonitor(cx, &realm->monitors.programModuleDir);
  realm->mutable.programModuleDir = NULL;
  gpsee_leaveAutoMonitor(realm->monitors.programModuleDir);
  JS_EndRequest(cx);

  if (realm->cx != cx)
    JS_DestroyContext(realm->cx);

  if (gpsee_ds_forEach(cx, jsi->realmsByContext, destroyRealmContext_cb, realm))
    return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".destroyRealmContext");

  gpsee_moduleSystemCleanup(realm);
  return JS_TRUE;
}

/**
 *  Create a new JS Context, initialized as a member of the passed realm.
 *
 *  @param      realm           The realm to which the new context belongs.
 *  @returns    A pointer to a new JSContext, or NULL if we threw an exception.
 */
JSContext *gpsee_newContext(gpsee_realm_t *realm)
{
  JSContext             *cx;
  gpsee_interpreter_t   *jsi = JS_GetRuntimePrivate(JS_GetRuntime(realm->cx));

  cx = JS_NewContext(jsi->rt, 8192);
  if (gpsee_setRealm(cx, realm) == JS_FALSE)
  {
    JS_DestroyContext(cx);
    return NULL;
  }

  JS_SetThreadStackLimit(cx, jsi->threadStackLimit);
  return cx;
}


