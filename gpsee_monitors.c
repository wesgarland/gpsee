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
 * @file        gpsee_monitors.c        Monitor-pattern concurrency for GPSEE.
 *
 *                                      Auto monitors are monitors which are
 *                                      created on demand (by noticing that
 *                                      their containing pointer is NULL) and
 *                                      cleaned up automatically when the 
 *                                      monitor system is shut down.
 * @date        May 2010
 * @author      Wes Garland
 * @version     $Id:$ 
 *
 */

#include "gpsee.h"
#include <prmon.h>

gpsee_monitor_t         nilMonitor = (gpsee_monitor_t)"NIL Monitor - using this monitor is like running unlocked";

/** Initialize the monitor subsystem associated with the passed grt.
 *  This routine runs unlocked: it is the job of the caller to 
 *  insure that no other threads are running.
 *
 *  @see gpsee_createRuntime()
 *
 *  @param      cx      The current JS context (used for throwing exceptions and gpsee_ds_create())
 *  @param      grt     The runtime whose monitor subsystem is to be initialized 
 *  @returns    JS_TRUE on success, JS_FALSE if we threw
 */
JSBool gpsee_initializeMonitorSystem(JSContext *cx, gpsee_runtime_t *grt)
{
#ifdef JS_THREADSAFE
  grt->monitors.monitor = PR_NewMonitor();

  if (!grt->monitors.monitor)
    return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".monitors.initialize: Could not initialize monitor subsystem");

  /* Access to monitorList_unlocked is guarded by grt->monitors.monitor henceforth */
  grt->monitorList_unlocked = gpsee_ds_create_unlocked(5);
#endif

  return JS_TRUE;
}

/**
 *  Create a monitor. Infallible.
 *
 *  @param      grt             The GPSEE runtime that owns the monitor
 *  @param      monitor_p       A pointer to the monitor. This address must stay valid for the lifetime of the runtime.
 *
 *  @returns    The new monitor, or NULL if this was not a JS_THREADSAFE build.
 */
gpsee_monitor_t gpsee_createMonitor(gpsee_runtime_t *grt)
{
#ifdef JS_THREADSAFE
  gpsee_monitor_t       monitor;

  GPSEE_ASSERT(grt->monitors.monitor);

  PR_EnterMonitor(grt->monitors.monitor);
  monitor = PR_NewMonitor();
  gpsee_ds_put(grt->monitorList_unlocked, monitor, NULL);
  PR_ExitMonitor(grt->monitors.monitor);

  return monitor;
#else
  return gpsee_getNilMonitor();
#endif
}

/**
 *  Get the nil monitor.  The nil monitor is a special monitor which can be
 *  passed to gpsee_enterMonitor() and gpsee_leaveMonitor(), allowing them
 *  to run unlocked but without reporting an error:
 *     NULL:   Uninitialized monitor
 *     nil:    Complete absence of a monitor
 */
gpsee_monitor_t gpsee_getNilMonitor()
{
  return nilMonitor;
}

/**
 *  Enter an auto-monitor (RAII). Infallible. Creates the monitor as-needed. 
 *
 *  @param      cx              A context belonging to the current runtime. Only used if we need to create
 *                              the monitor, to find the GPSEE runtime pointer.
 *  @param      monitor_p       A pointer to the monitor. This address must stay valid for the lifetime of the runtime.
 *
 *  @see gpsee_createMonitor()
 */
void gpsee_enterAutoMonitor(JSContext *cx, gpsee_autoMonitor_t *monitor_p)
{
#ifdef JS_THREADSAFE
  gpsee_runtime_t   *grt = JS_GetRuntimePrivate(JS_GetRuntime(cx));

  GPSEE_ASSERT(grt->monitors.monitor);

  if (!*monitor_p)
    *monitor_p = gpsee_createMonitor(grt);

  gpsee_enterMonitor(*monitor_p);
#endif
}

/**
 *  Enter a monitor. Infallible
 *
 *  @param      monitor         The monitor to enter.
 */
void gpsee_enterMonitor(gpsee_monitor_t monitor)
{
  if (monitor == nilMonitor)
    return;

  PR_EnterMonitor(monitor);
}

/**
 *  Leave a monitor. Infallible.
 *
 *  @param      monitor         The monitor to leave
 */
void gpsee_leaveMonitor(gpsee_monitor_t monitor)
{
#ifdef JS_THREADSAFE
  PRStatus      status;
  
  GPSEE_ASSERT(monitor);

  if (monitor == nilMonitor)
    return;

  status = PR_ExitMonitor(monitor);

  GPSEE_ASSERT(status == PR_SUCCESS);   /* An assertion failure here means we were not in the monitor */
#endif
}

/**
 *  Leave a monitor. Infallible.
 *
 *  @param      monitor         The monitor to leave
 */
void gpsee_leaveAutoMonitor(gpsee_autoMonitor_t monitor)
{
  return gpsee_leaveMonitor(monitor);
}

/**
 *  Destroy a monitor that was created by gpsee_createMonitor(). It is the caller's
 *  responsibility to insure that no other threads may want to use the monitor while
 *  this routine is running.
 *
 *  @param      grt             The GPSEE runtime which owns the monitor
 *  @param      monitor         The monitor to destroy
 */
void gpsee_destroyMonitor(gpsee_runtime_t *grt, gpsee_monitor_t monitor)
{
  if (monitor == nilMonitor)
    return;

  PR_EnterMonitor(grt->monitors.monitor);
  gpsee_ds_remove(grt->monitorList_unlocked, monitor);
  PR_ExitMonitor(grt->monitors.monitor);
  PR_DestroyMonitor(monitor);
}

#ifdef JS_THREADSAFE
static JSBool destroyMonitor_cb(JSContext *nullcx, const void *key, void *value, void *private)
{
  gpsee_monitor_t monitor = (void *)key;

  GPSEE_ASSERT(monitor && monitor != nilMonitor);

  PR_DestroyMonitor(monitor);

  return JS_TRUE;
}
#endif

/**
 *  Shut down the monitor system.  It is the caller's responsibility to insure
 *  that no monitors are held when this routine is called.
 */
void gpsee_shutdownMonitorSystem(gpsee_runtime_t *grt)
{
#ifdef JS_THREADSAFE
  gpsee_ds_forEach(NULL, grt->monitorList_unlocked, destroyMonitor_cb, NULL);
  gpsee_ds_destroy(grt->monitorList_unlocked);
  PR_DestroyMonitor(grt->monitors.monitor);
#endif

  return;
}

