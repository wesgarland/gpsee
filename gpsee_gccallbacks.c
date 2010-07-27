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
 */
/**
 *  @file       gpsee_gccallbacks.c     A multiplexing facility for GC Callbacks.
 *                                      Allows us to have realm-specific GC Callbacks,
 *                                      and any number of GC Callbacks without explicit
 *                                      cooperation between program/module/etc authors.
 *
 *  @note       The datastore, grt->gcCallbackList is an OTM (one-to-many) store which
 *              stores callback function pointers as keys with their attendant realms
 *              as values.
 *
 *  @ingroup    core
 *  @author     Wes Garland
 *  @date       June 2010
 *  @verison    $Id: gpsee_gccallbacks.c,v 1.1 2010/06/14 22:23:49 wes Exp $
 */

#include "gpsee.h"
#include "gpsee_private.h"

/**
 * Add a callback to the garbage collector.  Can add the same callback multiple times
 * with different realms. Realm pointer is never  de-referenced, only passed along to callback.
 *
 * @param       grt             GPSEE Runtime associated with the callback (GC is per-runtime)
 * @param       realm           Callback's invocation realm, or NULL for callbacks related to the GPSEE Runtime
 * @param       cb              Callback to add.
 * @returns     JS_TRUE on success, JS_FALSE on OOM
 *
 * @see JS_AddGCCallback()
 */
JSBool gpsee_addGCCallback(gpsee_runtime_t *grt, gpsee_realm_t *realm, gpsee_gcCallback_fn cb)
{
  return gpsee_ds_put(grt->gcCallbackList, cb, realm);
}

/**
 * Remove callback from the garbage collector. 
 *
 * @param       grt             GPSEE Runtime associated with the callback
 * @param       cb              Callback to remove
 * @param       realm           Realm associated with the callback to remove
 * @returns     JS_TRUE on success, JS_FALSE on if the callback was not found
 *
 * @see JS_AddGCCallback()
 */
JSBool gpsee_removeGCCallback(gpsee_runtime_t *grt, gpsee_realm_t *realm, gpsee_gcCallback_fn cb)
{
  return gpsee_ds_match_remove(grt->gcCallbackList, cb, realm);
}

static JSBool gcCallbackRemoveAllForRealm_cb(JSContext *cx, const void *key, void *value, void *private)
{
  gpsee_gcCallback_fn   fn              = key;
  gpsee_realm_t         *thisRealm      = value;
  gpsee_realm_t         *checkRealm     = private;

  if (thisRealm == checkRealm)
    if (gpsee_removeGCCallback(thisRealm->grt, thisRealm, fn) == JS_FALSE)
      return JS_FALSE;

  return JS_TRUE;
}

/**
 * Remove callbacks from the garbage collector. 
 *
 * @param       grt             GPSEE Runtime associated with the callback
 * @param       realm           Realm associated with the callbacks to remove
 * @returns     JS_TRUE on success, JS_FALSE on if the callback was not found
 *
 * @see JS_AddGCCallback()
 */
JSBool gpsee_removeAllGCCallbacks_forRealm(gpsee_runtime_t *grt, gpsee_realm_t *realm)
{
  return gpsee_ds_forEach(NULL, grt->gcCallbackList, gcCallbackRemoveAllForRealm_cb, (void *)realm);
}

/**
 *  Invoke a GC Callback.  
 *
 *  @param      cx      A context in the same runtime as the GC callback's invocation realm.
 *  @param      key     The GC callback to invoke
 *  @param      value   The GC callback's invocation realm
 *  @param      private A JSGCStatus indicating which phase the garbage collector is operating in.
 *  @returns    The callback's return value
 *
 *  @see gpsee_ds_forEach_fn.
 */
static JSBool gcCallback_cb(JSContext *cx, const void *key, void *value, void *private)
{
  gpsee_gcCallback_fn   fn      = key;
  gpsee_realm_t         *realm  = value;
  JSGCStatus            status  = (JSGCStatus)private;

  return fn(cx, realm, status);
}

/**
 *  @param      cx      Any context in the runtime
 *  @param      status  Which phase the garbage collector is operating in.
 *  @returns    JS_FALSE if any gpsee_gcCallback_fn callback returned JS_FALSE; otherwise, JS_TRUE
 *
 *  @note       This a hook, intended to be used by gpsee_createRuntime() et al
 */
JSBool gpsee_gcCallback(JSContext *cx, JSGCStatus status)
{
  gpsee_runtime_t 	*grt = JS_GetRuntimePrivate(JS_GetRuntime(cx));

  return gpsee_ds_forEach(cx, grt->gcCallbackList, gcCallback_cb, (void *)status);
}
