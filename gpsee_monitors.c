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

/** Initialize the monitor subsystem associated with the passed jsi.
 *  This routine runs unlocked: it is the job of the caller to 
 *  insure that no other threads are running.
 *
 *  @see gpsee_createInterpreter()
 *
 *  @param      cx      The current JS context (used for throwing exceptions and gpsee_ds_create())
 *  @param      jsi     The interpreter whose monitor subsystem is to be initialized 
 *  @returns    JS_TRUE on success, JS_FALSE if we threw
 */
JSBool gpsee_initializeMonitorSystem(JSContext *cx, gpsee_interpreter_t *jsi)
{
#ifdef JS_THREADSAFE
  jsi->monitors.monitor = PR_NewMonitor();

  if (!jsi->monitors.monitor)
    return gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".monitors.initialize: Could not initialize monitor subsystem");

  jsi->monitorList = gpsee_ds_create_unlocked(cx, 5);
#endif

  return JS_TRUE;
}

/**
 *  Create a monitor. Infallible.
 *
 *  @param      cx              A context belonging to the current interpreter. Used to locate the interpreter's
 *                              gpsee_interpreter_t and subsequently the interpreter's monitor monitor. Not used
 *                              to execute JSAPI code.
 *  @param      monitor_p       A pointer to the monitor. This address must stay valid for the lifetime of the interpreter.
 *
 *  @returns    The new monitor, or NULL if this was not a JS_THREADSAFE build.
 */
gpsee_monitor_t gpsee_createMonitor(JSContext *cx)
{
#ifdef JS_THREADSAFE
  gpsee_interpreter_t 	*jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  gpsee_monitor_t       monitor;

  GPSEE_ASSERT(jsi->monitors.monitor);

  PR_EnterMonitor(jsi->monitors.monitor);
  monitor = PR_NewMonitor();
  gpsee_ds_put(cx, jsi->monitorList, monitor, NULL);
  PR_ExitMonitor(jsi->monitors.monitor);

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
 *  @param      cx              A context belonging to the current interpreter. Only used if we need to create
 *                              the monitor.
 *  @param      monitor_p       A pointer to the monitor. This address must stay valid for the lifetime of the interpreter.
 *
 *  @see gpsee_createMonitor()
 */
void gpsee_enterAutoMonitor(JSContext *cx, gpsee_autoMonitor_t *monitor_p)
{
#ifdef JS_THREADSAFE
  GPSEE_ASSERT(((gpsee_interpreter_t *)JS_GetRuntimePrivate(JS_GetRuntime(cx)))->monitors.monitor);

  if (!*monitor_p)
    *monitor_p = gpsee_createMonitor(cx);

  if (*monitor_p == nilMonitor)
    return;
  
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
 *  Destroy a monitor that was created by gpsee_createMonitor().
 */
void gpsee_destroyMonitor(gpsee_monitor_t *monitor_p)
{
  gpsee_enterMonitor(monitor_p);
  gpsee_leaveMonitor(*monitor_p);
  PR_DestroyMonitor(*monitor_p);
}

#ifdef JS_THREADSAFE
static JSBool destroyMonitor(JSContext *nullcx, const void *key, void *value, void *private)
{
  gpsee_monitor_t monitor = (void *)key;

  GPSEE_ASSERT(monitor && monitor != nilMonitor);

  gpsee_enterMonitor(monitor);
  gpsee_leaveMonitor(monitor);
  PR_DestroyMonitor(monitor);

  return JS_TRUE;
}
#endif

/**
 *  Shut down the monitor system.
 *  Resets monitors to NULL as they are destroyed.
 */
void gpsee_shutdownMonitorSystem(gpsee_interpreter_t *jsi)
{
#ifdef JS_THREADSAFE
  PRMonitor     *monitorMonitor;

  if (jsi->monitors.monitor)
  {
#warning Monitor system shutdown is far from optimal
    JSContext *cx = JS_NewContext(jsi->rt, 0x4000);
    JS_BeginRequest(cx);

    PR_EnterMonitor(jsi->monitors.monitor);
    monitorMonitor = jsi->monitors.monitor;
    jsi->monitors.monitor = NULL;
    gpsee_ds_forEach(NULL, jsi->monitorList, destroyMonitor, NULL);
    gpsee_ds_destroy(cx, jsi->monitors.monitor);
    PR_ExitMonitor(monitorMonitor);

    JS_EndRequest(cx);
    JS_DestroyContext(cx);
  }
#endif

  return;
}

