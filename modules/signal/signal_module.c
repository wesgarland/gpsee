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
 * ***** END LICENSE BLOCK ***** */

/**
 *  @file       signal_module.c		Module to expose POSIX signals as JavaScript events.
 *  @author     Wes Garland, wes@page.ca
 *  @date       Oct 2007
 *  @version    $Id: signal_module.c,v 1.4 2009/07/31 16:49:29 wes Exp $
 *
 *  @bug	Not compatible with gpsee_context_private.c: both want the context callback. Will
 *		need to write some multiplexing code.
 *  @bug	May result in deadlock on certain platforms due to implementation of PR_AtomicSet().
 *              As of Feb 25 2009 hg tip for nspr, here's what I think of the likelihood of deadlock
 *		possibilities:
 *		<pre>
 *		<b>Platform			Could Deadlock		Why</b>
 *		-----------------------------	---------------------	----------------------------------------------------------
 *		GCC 4				Probably Not		Depends on __sync_lock_test_and_set
 *		Darwin PPC/x86/x86_64		No			Non-branching asm
 *		Windows NT, 95			Maybe			Depends on InterlockedExchange()
 *		OSF1, Open VMS			Probably Not		Depends on _ATOMIC_EXCH_LONG
 *		AIX				Probably		Depends on fetch_and_add, c code, compare_and_swap
 *		BeOS				Yes			Uses PR_Lock / PR_Unlock of global monitor
 *		SunOS sparc/x86/x86_64		No			Non-branching asm
 *		OS/2				?			I think implementation is a recursive macro (won't build)
 *		Mac OS (pre Tiger?)		Probably Not		Depends on ENTER_CRITICAL_REGION guard
 *		HP/UX				No			Non-branching asm
 *		Linux ia64/x86/x86_64/PPC	No			Non-branching asm
 *		Linux alpha			Probably Not		Assembler implementation I didn't understand
 *		UnixWare -DUSE_SVR4_THREADS	Yes			Uses mutex_lock of global _unixware_atomic
 *		UnixWare, sco, ncr		Yes			Uses flockfile of global _uw_semf
 *
 *		<i>Note:</i> by "non-branching", I mean "doesn't branch outside of the subroutine".  Most (all?) of these loop.
 *		</pre>
 */
 
static __attribute__((unused)) const char rcsid[]="$Id: signal_module.c,v 1.4 2009/07/31 16:49:29 wes Exp $";
 
#include "gpsee.h"
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#define MODULE_ID GPSEE_GLOBAL_NAMESPACE_NAME	".module.ca.page.signal"

#define OS_MAX_SIGNAL 31  /* stick with POSIX */

struct signal_hnd
{
  JSContext		*cx;			/**<< Context in which to run the handler */
  jsval			pending;		/**<< JSVAL_TRUE - signal has been received, need to run handler */
  jsval			funv;			/**<< Function to call when signal is raised or JSVAL_VOID */
  struct sigaction	oact;			/**<< Signal action present when module was loaded */
};

/**
 * hnd->pending is used to indicate both that a signal is pending and to synchronize
 * write-access to the rest of the structure.
 *
 * hnd->pending can be checked unlocked. If it is JSVAL_TRUE, there is a pending signal.
 *
 * <pre>
 * <b>hnd->pending is set with CAS:</b>
 * 	Setting a pending signal:  		JSVAL_FALSE -> JSVAL_TRUE
 * 	Finished processing a pending signal: 	JSVAL_TRUE  -> JSVAL_FALSE
 * 	I am writing to the hnd structure:	JSVAL_FALSE -> JSVAL_VOID
 *      I am processing a pending signal:	JSVAL_TRUE  -> JSVAL_VOID
 * 	I am done processing the signal:	JSVAL_VOID  -> JSVAL_FALSE
 * </pre>
 */
typedef struct signal_hnd signal_hnd_t;

static signal_hnd_t signalEvents[OS_MAX_SIGNAL + 1];

/** Run JavaScript signal event handlers for pending signals 
 *  that this context has a registered event handler for.
 *  Transitions hnd->pending via CAS from JSVAL_TRUE->JSVAL_VOID->JSVAL_FALSE
 *
 *  @param	cx	JS context to use to find and execute JS event code
 *  @returns	JS_FALSE 
 */
static JSBool signal_runHandlers(JSContext *cx, void *ignored)
{
  JSBool 		b, ret = JS_TRUE;
  jsval			v;
  int			sig;
  jsval			argv[1];

  if (JS_IsExceptionPending(cx) == JS_TRUE)
    return JS_TRUE;	/* We don't run signal handlers when exceptions are pending. */

  for (sig = 1; sig <= OS_MAX_SIGNAL && ret == JS_TRUE; sig++)
  {
    b = jsval_CompareAndSwap(&signalEvents[sig].pending, JSVAL_TRUE, JSVAL_VOID);
    if (b != JS_TRUE)	/* Another thread is playing with this signal */
      continue;

    argv[0] = INT_TO_JSVAL(sig);
    ret = JS_CallFunctionValue(cx, JSVAL_TO_OBJECT(signalEvents[sig].funv), signalEvents[sig].funv, 1, argv, &v);

    b = jsval_CompareAndSwap(&signalEvents[sig].pending, JSVAL_VOID, JSVAL_FALSE);
    GPSEE_ASSERT(b == JS_TRUE);
  }

  return ret;
}

/** Process any signals pending for this context as it shuts down.
 */
static JSBool signal_contextCallback(JSContext *cx, uintN contextOp)
{
  int sig;

  if (contextOp == JSCONTEXT_DESTROY)
    signal_runHandlers(cx, NULL);

  for (sig = 1; sig <= OS_MAX_SIGNAL; sig++)
  {
    GPSEE_ASSERT(signalEvents[sig].cx != cx);
    if (signalEvents[sig].cx == cx)
      signalEvents[sig].cx = NULL;	
  }

  return JS_TRUE;
}

/** POSIX (OS-level) signal handler. 
 *  Mark the signal pending and ask the operation callback on the appropriate JS context to run ASAP.
 *
 *  @note	If there is already a pending signal, we do nothing.
 *  @note	Depends on POSIX 1003.1-2004 for sig argument.
 */
static void signal_js(int sig)
{
  if (jsval_CompareAndSwap(&signalEvents[sig].pending, JSVAL_FALSE, JSVAL_TRUE) == JS_TRUE)
  {
    if (signalEvents[sig].cx && signalEvents[sig].funv != JSVAL_VOID)
      JS_TriggerOperationCallback(signalEvents[sig].cx);
    else
      jsval_CompareAndSwap(&signalEvents[sig].pending, JSVAL_TRUE, JSVAL_FALSE);
  }

  return;
}

/**
 *  @param	cx			JS context calling this function
 *  @param	sighndCx		JS context in which the signal handler should run
 *  @param	sig			Signal handler is triggered by
 *  @param	funv			JS function to run soon after signal is received
 *  @param	contentionException	Value of exception to throw when we can't change the signal
 *					handler due to another thread CAS-locking signalEvents[sig]
 *  @param	pendingException	Value of exception to throw when we can't change the signal
 *					handler due to a pending signal.
 */
static const char *signal_changeSignalHandler(JSContext *cx, JSContext *sighndCx, int sig, jsval funv, 
					      const char *contentionException, const char *pendingException)
{
  JSBool b;

  if (jsval_CompareAndSwap(&signalEvents[sig].pending, JSVAL_FALSE, JSVAL_VOID) == JS_FALSE)
  {
    /* If we can't lock right away, try to clear any obstacles and try again */

    if (signalEvents[sig].cx != cx)
      JS_YieldRequest(cx);	/* Other thread might be stuck waiting for us so it can GC */
    else
      signal_runHandlers(cx, NULL);	/* Clear pending signal by handling it */

    if (signalEvents[sig].pending == JSVAL_TRUE)
      return pendingException;

    if (jsval_CompareAndSwap(&signalEvents[sig].pending, JSVAL_FALSE, JSVAL_VOID) == JS_FALSE)
      return contentionException;
  }
  
  signalEvents[sig].cx = sighndCx;
  signalEvents[sig].funv = funv;
    
  b = jsval_CompareAndSwap(&signalEvents[sig].pending, JSVAL_VOID, JSVAL_FALSE);
  GPSEE_ASSERT(b == JS_TRUE);

  return NULL;
}

/** Remove a JS signal handler. After this call, signal will be processed with OS-level
 *  handler that was active before the module was loaded, unless there was none.
 *
 *  @returns	Exception to throw or NULL
 */
static const char *signal_removeSignalHandler(JSContext *cx, int sig)
{
  const char *e = signal_changeSignalHandler(cx, NULL, sig, JSVAL_VOID, 
					     MODULE_ID ".remove.pending",
					     MODULE_ID ".remove.contention");

  if (e == NULL) 	/* no exceptions thrown in JS change, update OS to match */
  {
    if (signalEvents[sig].oact.sa_handler || signalEvents[sig].oact.sa_sigaction)
      sigaction(sig, &signalEvents[sig].oact, NULL);
    else
      signal(sig, SIG_DFL);
  }

  return e;
}

/**
 *  @returns	Exception to throw or NULL
 */
static const char *signal_setSignalHandler(JSContext *cx, int sig, jsval funv)
{
  const char * e = signal_changeSignalHandler(cx, cx, sig, funv,
					      MODULE_ID ".set.pending",
					      MODULE_ID ".set.contention");

  if (e == NULL) 	/* no exceptions thrown in change */
  {
    struct sigaction 	act;
    memset(&act, 0, sizeof(act));

    act.sa_handler = signal_js;
#if defined(SA_RESTART)
    act.sa_flags |= SA_RESTART;
#endif
    sigaction(sig, &act, &signalEvents[sig].oact);
  }

  return e;
}

/** signal setter - handles "onWINCH" and so forth. setting null requests
 *  removing the handler.
 */
static JSBool signal_setter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  int 			sig = JSVAL_TO_INT(id) * -1;
  const char		*e;

  if (sig > OS_MAX_SIGNAL)
    return gpsee_throw(cx, MODULE_ID ".eventHandler.setter.%i.unsupported", sig);

  switch (*vp)		/* Set to NULL means remove */
  {
    case JSVAL_NULL:
    case JSVAL_FALSE:
      if ((e = signal_removeSignalHandler(cx, sig)))
	return gpsee_throw(cx, e);
      return JS_TRUE;
  }

  if ((e = signal_setSignalHandler(cx, sig, *vp)))
    return gpsee_throw(cx, e);

  return JS_TRUE;
}

static JSBool signal_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  int sig = JSVAL_TO_INT(id) * -1;

  if (sig > OS_MAX_SIGNAL)
    return gpsee_throw(cx, MODULE_ID ".eventHandler.getter.%i.unsupported", sig);

  *vp = signalEvents[sig].funv;
  switch (*vp)
  {
    case JSVAL_NULL:
    case JSVAL_FALSE:
    case JSVAL_VOID:
      *vp = JSVAL_NULL;
  }

  return JS_TRUE;
}

static JSBool signal_kill(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  int32		sig;
  int32 	pid;
  int		i;
  jsval		NaN = JS_GetNaNValue(cx);

  static const struct 
  {
    int32	num;
    const char	*name;
  } sigmap[] =
  {
#define UseSignal(a)	{ SIG ## a, #a },
#include "signal_list.h"
#undef UseSignal
  };

  GPSEE_STATIC_ASSERT(sizeof(int32) == sizeof(pid_t));

  switch(argc)
  {
    case 1:
      pid = getpid();
      break;
    case 2:
      if (JS_ValueToECMAInt32(cx, argv[1], &pid) != JS_TRUE)
	pid = 0;

      if (pid == NaN)
	return gpsee_throw(cx, MODULE_ID ".kill.pid.isNaN");

      break;
    default:
      return gpsee_throw(cx, MODULE_ID ".kill.arguments.count");
  }

  if (pid == 0)
    return gpsee_throw(cx, MODULE_ID ".kill.pid.invalid");

  if (JS_ValueToECMAInt32(cx, argv[0], &sig) != JS_TRUE)
    return JS_FALSE;

  if (sig == 0)
    sig = NaN;	/* conversion function returns 0 when passed string */

  if (sig == NaN)
  {
    char *sigName = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));

    if (sigName)
    {
      if (sigName[0] == '0' && sigName[0] == '\0')
	sig = 0;
      else
      {
	for (i=0; i < sizeof(sigmap) / sizeof(sigmap[0]); i++)
	{
	  if (strcmp(sigmap[i].name, sigName) == 0)
	  {
	    sig = sigmap[i].num;
	    break;
	  }
	}
      }
    }
  }

  if (sig == NaN)
    return gpsee_throw(cx, MODULE_ID ".kill.signal.isNaN");

  if ((sig < 0) || (sig > OS_MAX_SIGNAL))
    return gpsee_throw(cx, MODULE_ID ".kill.signal.range");

  *rval = INT_TO_JSVAL(kill((pid_t)pid, sig));

  return JS_TRUE;
}

static JSBool signal_raise(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  if (argc != 1)
    return gpsee_throw(cx, MODULE_ID ".raise.arguments.count");

  return signal_kill(cx, obj, argc, argv, rval);  
}

static JSBool signal_send(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  if (argc != 2)
    return gpsee_throw(cx, MODULE_ID ".send.arguments.count");

  return signal_kill(cx, obj, argc, argv, rval);  
}

const char *signal_InitModule(JSContext *cx, JSObject *moduleObject)
{
  int 			prop;

  static JSFunctionSpec signal_methods[] = 
  {
    { "raise",			signal_raise,			0, 0, 0 },
    { "send",			signal_send,			0, 0, 0 },
    { "kill",			signal_kill,			0, 0, 0 },
    { NULL,			NULL,				0, 0, 0 },
  };

#if !defined(SIGCHLD)
# define SIGCHLD SIGCLD
#endif

#if !defined(SIGPWR)
# define SIGPWR SIGINFO
#endif

#if !defined(SIGPOLL)
# define SIGPOLL SIGIO
#endif

  /* tinyID is negative signal number */
  static JSPropertySpec signal_props[] = 
  {		
#define UseSignal(a)	{ "on" #a, -SIG ## a, JSPROP_ENUMERATE | JSPROP_PERMANENT, signal_getter, signal_setter },
#include "signal_list.h"
#undef UseSignal
    { NULL, 0, 0, NULL, NULL }
  };

  if (!JS_DefineFunctions(cx, moduleObject, signal_methods) || !JS_DefineProperties(cx, moduleObject, signal_props))
    return NULL;

  memset(signalEvents, 0, sizeof(signalEvents));
  
  /* Loading the signal module causes all catchable signals to be caught */
  for (prop = 0; prop < sizeof(signal_props) / sizeof(signal_props[0]); prop++)
  {
    int sig = signal_props[prop].tinyid * -1;

    GPSEE_ASSERT(sig <= OS_MAX_SIGNAL);
      
    signalEvents[sig].pending 	= JSVAL_FALSE;
    signalEvents[sig].funv	= JSVAL_VOID;
    sigaction(sig, NULL, &signalEvents[sig].oact);
  }

  /* Register a callback function which is asynchronous with respect to the running Javascript program, to periodically
   * interrupt the running Javascript program and run the signal handlers the Javascript program has registered. */
  gpsee_addAsyncCallback(cx, signal_runHandlers, NULL);

  /* Register a callback function to fire all pending callbacks when the JSContext is ready to finalize. */
  gpsee_setContextCallback(cx, signal_contextCallback);

  return MODULE_ID;
}

JSBool signal_FiniModule(JSContext *cx, JSObject *moduleObject)
{
  int sig;

  signal_runHandlers(cx, NULL);

  for (sig = 1; sig <= OS_MAX_SIGNAL; sig++)
  {
    signalEvents[sig].funv	= JSVAL_VOID;
    signalEvents[sig].cx  	= NULL;
    sigaction(sig, &signalEvents[sig].oact, NULL);
  }

  return JS_TRUE;
}

#warning "We need a GC callback here to mark funv as not collectable if Signal falls out of scope and GC runs"
