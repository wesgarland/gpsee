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
 * ***** END LICENSE BLOCK ***** 
 */

/**
 *  @file	gpsee.c 	Core GPSEE.
 *  @author	Wes Garland
 *  @date	Aug 2007
 *  @version	$Id: gpsee.c,v 1.31 2010/06/23 15:13:58 wes Exp $
 *
 *  Routines for running JavaScript programs, reporting errors via standard SureLynx
 *  mechanisms, throwing exceptions portably, etc. 
 *
 *  Can be used to embed Spidermonkey in other applications (e.g. MTA) or in the
 *  standalone SureLynx JS shell. 
 */

static __attribute__((unused)) const char gpsee_rcsid[]="$Id: gpsee.c,v 1.31 2010/06/23 15:13:58 wes Exp $";

#define _GPSEE_INTERNALS
#include "gpsee.h"
#include "gpsee_private.h"

#define	GPSEE_BRANCH_CALLBACK_MASK_GRANULARITY	0xfff

extern rc_list rc;

#if defined(GPSEE_DEBUG_BUILD)
# define dprintf(a...) do { if (gpsee_verbosity(0) > 2) gpsee_printf(cx, "gpsee\t> "), gpsee_printf(cx, a); } while(0)
#else
# define dprintf(a...) while(0) gpsee_printf(cx, a)
#endif

/** Increase, Decrease, or change application verbosity.
 *
 *  @param	changeBy	Amount to increase verbosity
 *  @returns	current verbosity (after change applied)
 */
signed int gpsee_verbosity(signed int changeBy)
{
  static signed int verbosity = 0;

  verbosity += changeBy;
  
  GPSEE_ASSERT(verbosity >= 0);
  if (verbosity < 0)
    verbosity = 0;

  return verbosity;
}

void gpsee_setVerbosity(signed int newValue)
{
  gpsee_verbosity(-1 * gpsee_verbosity(0));
  gpsee_verbosity(newValue);
}

/** @see JS_Assert() in jsutil.c */
void gpsee_assert(const char *s, const char *file, JSIntn ln)
{
  fprintf(stderr, "Assertion failure: %s, at %s:%d\n", s, file, ln);
  gpsee_log(NULL, SLOG_ERR, "Assertion failure: %s, at %s:%d\n", s, file, ln);
  abort();
}   

/** Handler for fatal GPSEE errors.
 *
 *  @param      message         Arbitrary text describing the
 *  @note       Exits with status 1
 */
void __attribute__((weak)) __attribute__((noreturn)) panic(const char *message)
{
  fprintf(stderr, "GPSEE Fatal Error: %s\n", message);
  gpsee_log(NULL, SLOG_NOTTY_NOTICE, "GPSEE Fatal Error: %s", message);
  exit(1);
}

static void output_message(JSContext *cx, gpsee_runtime_t *grt, const char *er_pfx, const char *log_message, JSErrorReport *report, int printOnTTY)
{
  if (grt->errorLogger)
    grt->errorLogger(cx, er_pfx, log_message);
  else
  {
    if (JSREPORT_IS_WARNING(report->flags))
      gpsee_log(cx, SLOG_NOTTY_INFO, "%s %s", er_pfx, log_message);
    else
      gpsee_log(cx, SLOG_NOTTY_NOTICE, "%s %s", er_pfx, log_message);
  }

  if (printOnTTY)
    gpsee_fprintf(cx, stderr, "%s %s\n", er_pfx, log_message);

  return;
}

/** Error Reporter for Spidermonkey. Used to report warnings and
 *  uncaught exceptions.
 */
void gpsee_errorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
  const char 		*ctmp;
  char			er_filename[64];
  char			er_exception[16];
  char			er_warning[16];
  char			er_number[16];
  char			er_lineno[16];
  char			er_charno[16];
  char			er_pfx[64];
  gpsee_runtime_t 	*grt = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  int                   printOnTTY = 0;

  if (grt->errorReport == er_none)
    return;

  if (!report)
  {
    gpsee_log(cx, SLOG_NOTICE, "JS error from unknown source: %s\n", message);
    return;
  }

  /* Conditionally ignore reported warnings. */
  if (JSREPORT_IS_WARNING(report->flags))
  {
    if (grt->errorReport & er_noWarnings)
      return;

    if (rc_bool_value(rc, "gpsee_report_warnings") == rc_false)
      return;

    if (gpsee_verbosity(0) >= GPSEE_WARNING_OUTPUT_VERBOSITY)
      printOnTTY = 1 && isatty(STDERR_FILENO);
  }
  else
    if (gpsee_verbosity(0) >= GPSEE_ERROR_OUTPUT_VERBOSITY)
      printOnTTY = 1 && isatty(STDERR_FILENO);

  if (report->filename)
  {
    const char *bn = strrchr(report->filename, '/');

    if (bn)
      bn += 1;
    else
      bn = report->filename;

    snprintf(er_filename, sizeof(er_filename), "in %s ", bn);
  }
  else
    er_filename[0] = (char)0;

  if (report->lineno)
    snprintf(er_lineno, sizeof(er_lineno), "line %u ", report->lineno);
  else
    er_lineno[0] = (char)0;

  if (report->tokenptr && report->linebuf)
    snprintf(er_charno, sizeof(er_charno), "ch %ld ", (long int)(report->tokenptr - report->linebuf));
  else
    er_charno[0] = (char)0;

  er_warning[0] = (char)0;

  if (JSREPORT_IS_EXCEPTION(report->flags))
  {
    snprintf(er_exception, sizeof(er_exception), "exception ");
  }
  else
  {
    er_exception[0] = (char)0;

    if (JSREPORT_IS_WARNING(report->flags))
      snprintf(er_warning, sizeof(er_warning), "%swarning ", (JSREPORT_IS_STRICT(report->flags) ? "strict " : ""));
  }

  if (report->errorNumber)     
    snprintf(er_number, sizeof(er_number), "#%i ", report->errorNumber);
  else
    er_number[0] = (char)0;

  snprintf(er_pfx, sizeof(er_pfx), "JS %s%s%s%s" "%s" "%s%s%s" "-",
	   er_warning, er_exception, ((er_exception[0] || er_warning[0]) ? "" : "error "), er_number,	/* error 69 */
	   er_filename,											/* in myprog.js */
	   ((er_lineno[0] || er_charno[0]) ? "at " :""), er_lineno, er_charno				/* at line 12, ch 34 */
	   );

  /* embedded newlines -- log each separately */
  while ((ctmp = strchr(message, '\n')) != 0)
  {
    char log_message[strlen(message)];

    ctmp++;
    strncpy(log_message, message, ctmp-message);
    log_message[ctmp - message] = (char)0;
    output_message(cx, grt, er_pfx, log_message, report, printOnTTY);
    message = ctmp;

    memset(er_pfx, ' ', strlen(er_pfx));
    er_pfx[0] = '|';   
  }
  
  output_message(cx, grt, er_pfx, message, report, printOnTTY);
}

/** API-Compatible with Print() in js.c. Uses more RAM, but much less likely
 *  to intermingle output newlines when called from two different threads.
 */
JSBool gpsee_global_print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  JSString 	*str;
  jsrefcount 	depth;
  int		i;

  /* Iterate over all arguments, converting them to strings as necessary, replacing
   * the contents of argv with the results. */
  for (i = 0; i < argc; i++)
  {
    /* Not already a string? */
    if (!JSVAL_IS_STRING(argv[i]))
    {
      /* Convert to string ; may call toString() and the like */
      str = JS_ValueToString(cx, argv[i]);
      if (!str)
	return JS_FALSE;	/* threw */
      /* Root the string in our argument vector, we don't need the old argument now, anyway */
      argv[i] = STRING_TO_JSVAL(str);
    }
    else
      str = JSVAL_TO_STRING(argv[i]);
  }

  /* Now we'll print each member of argv, which at this point contains the results of the previous
   * loop we iterated over argv in, so they are all guaranteed to be JSVAL_STRING type. */
  for (i = 0; i < argc; i++)
  {
    str = JS_ValueToString(cx, argv[i]);
    GPSEE_ASSERT(str);
    /* Suspend request, in case this write operation blocks TODO make optional? */
    depth = JS_SuspendRequest(cx);
    /* Print the argument, taking care for putting a space between arguments, and a newline at the end */
    gpsee_printf(cx, "%s%s%s", i ? " " : "", JS_GetStringBytes(str), i+1==argc?"\n":"");
    JS_ResumeRequest(cx, depth);
  }

  *rval = JSVAL_VOID;
  return JS_TRUE;
}


/** Throw an exception from our C code to be caught by the 
 *  running JavaScript program.
 *
 *  To use this function, in most cases, do:
 *  <pre>
 *  if (badness)
 *    return gpsee_throw(cx, "bad things just happened");
 *  </pre>
 *
 *  @param	cx	Context from which to device runtime information
 *  @param	fmt	printf-style format for the text to throw as an exception
 *  @param	...	printf-style arguments to fmt
 *
 *  @returns	JS_FALSE
 *
 *  @warning	This function will panic if the runtime cannot allocate 
 *		enough memory to prepare the exception-throwing message.
 *
 *  @note	This function uses the APR SureLynx gpsee_makeLogFormat() function,
 *		so %m will work, and so will addPercentM_Handler() with surelynx.
*/
JSBool gpsee_throw(JSContext *cx, const char *fmt, ...)
{
  char 		*message;
  va_list	ap;
  size_t	length;
  char		fmtNew[GPSEE_MAX_LOG_MESSAGE_SIZE];

  message = JS_malloc(cx, GPSEE_MAX_THROW_MESSAGE_SIZE);
  if (!message)
    panic(GPSEE_GLOBAL_NAMESPACE_NAME ": out of memory in gpsee_throw!");

  va_start(ap, fmt);
  length = vsnprintf(message, GPSEE_MAX_THROW_MESSAGE_SIZE, gpsee_makeLogFormat(fmt, fmtNew), ap);
  va_end(ap);

  if (length <= 0)
  {
    gpsee_cpystrn(message, "JS Engine unnamed exception", GPSEE_MAX_THROW_MESSAGE_SIZE);
    length = strlen(message);
  }

  if (JS_IsExceptionPending(cx) == JS_TRUE)
    gpsee_log(cx, SLOG_ERR, GPSEE_GLOBAL_NAMESPACE_NAME ": Already throwing an exception; not throwing '%s'!", message);
  else
    JS_ReportError(cx, "%s", message);

  return JS_FALSE;
}

/** Create a JS Array from a C-style argument vector.
 *
 *  @param	cx	  JS Context
 *  @param 	obj	  JS Object to which to attach the array
 *  @param      arrayName Property name wrt 'obj'
 *  @param	argv	  A NULL-terminated array of char *
 *
 *  @returns	JS_TRUE on success
 */ 
JSBool gpsee_createJSArray_fromVector(JSContext *cx, JSObject *obj, const char *arrayName, char * const argv[])
{
  char * const	*argp;
  JSObject	*argsObj;

  if ((argsObj = JS_NewArrayObject(cx, 0, NULL)) == NULL) 
    return JS_FALSE;

  if (JS_DefineProperty(cx, obj, arrayName, OBJECT_TO_JSVAL(argsObj), NULL, NULL, 0) != JS_TRUE)
    return JS_FALSE;

  if (argv)
  {
    for (argp = argv; *argp; argp++)
    {
      if (JS_DefineElement(cx, argsObj, argp-argv, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, *argp)), 
			   NULL, NULL, JSPROP_ENUMERATE) != JS_TRUE)
	return JS_FALSE;
    }
  }

  return JS_TRUE;
}

/** Lazy resolver for global classes.
 *  Originally from js.c; cruft removed.
 */
static JSBool global_newresolve(JSContext *cx, JSObject *obj, jsval id, uintN flags, JSObject **objp)
{
  if ((flags & JSRESOLVE_ASSIGNING) == 0) 
  {
    JSBool resolved;

    if (!JS_ResolveStandardClass(cx, obj, id, &resolved))
      return JS_FALSE;

    if (resolved)
    {
      *objp = obj;
    }
  }

  return JS_TRUE;
}

#if !defined(GPSEE_NO_ASYNC_CALLBACKS)
/******************************************************************************************** Asynchronous Callbacks */

JSBool gpsee_removeAsyncCallbackContext(JSContext *cx, uintN contextOp);
/** Thread for triggering closures registered with gpsee_addAsyncCallback() */
static void gpsee_asyncCallbackTriggerThreadFunc(void *grt_vp)
{
  gpsee_runtime_t *grt = (gpsee_runtime_t *) grt_vp;
  GPSEEAsyncCallback *cb;

  /* Run this loop as long as there are async callbacks registered */
  do {
    JSContext *cx;

    /* Acquire mutex protecting grt->asyncCallbacks */
    PR_Lock(grt->asyncCallbacks_lock);

    /* Grab the head of the list */
    cb = grt->asyncCallbacks;
    if (cb)
    {
      /* Grab the JSContext from the head */
      cx = cb->cx;

      /* Trigger operation callbacks on the first context */
      JS_TriggerOperationCallback(cx);

      /* Iterate over each operation callback */
      while ((cb = cb->next))
      {
        /* Trigger operation callback on each new context found */
        if (cx != cb->cx)
        {
          cx = cb->cx;
          JS_TriggerOperationCallback(cx);
        }
      }
    }

    /* Relinquish mutex */
    PR_Unlock(grt->asyncCallbacks_lock);

    /* Sleep for a bit; interrupted by PR_Interrupt() */
    PR_Sleep(PR_INTERVAL_MIN); // TODO this should be configurable!!
  }
  while (grt->asyncCallbacks);
}
/** Our "Operation Callback" multiplexes this Spidermonkey facility. It is called automatically by Spidermonkey, and is
 *  triggered on a regular interval by gpsee_asyncCallbackTriggerThreadFunc() */
JSBool gpsee_operationCallback(JSContext *cx)
{
  gpsee_runtime_t *grt = (gpsee_runtime_t *) JS_GetRuntimePrivate(JS_GetRuntime(cx));
  GPSEEAsyncCallback *cb;

  /* The callbacks registered with GPSEE may want to invoke JSAPI functionality, which might toss us back out
   * to another invocation of gpsee_operationCallback(). The JSAPI docs for "operation callbacks" [1] suggest
   * removing the operation callback before calling JSAPI functionality from within an operation callback,
   * then resetting it when we're done making JSAPI calls. Since it's rather inexpensive, we'll just do it here
   * and then consumers of gpsee_addAsyncCallback() needn't worry about it (we don't want them touching that
   * callback slot anyway! 
   *
   * [1] https://developer.mozilla.org/en/JS_SetOperationCallback
   *
   * Another side note: we do it before if(cb) because if gpsee_asyncCallbacks is empty, we want to uninstall our
   * operation callback altogether. */
  JS_SetOperationCallback(cx, NULL);

  cb = grt->asyncCallbacks;
  if (cb)
  {
    GPSEEAsyncCallback *next;
    do
    {
      /* Save the 'next' link in case the callback deletes itself */
      next = cb->next;
      /* Invoke callback */
      if (!((*(cb->callback))(cb->cx, cb->userdata, cb)))
        /* Propagate exceptions */
        return JS_FALSE;
    }
    while ((cb = next));

    /* Reinstall our operation callback */
    JS_SetOperationCallback(cx, gpsee_operationCallback);

    return JS_TRUE;
  }

  return JS_TRUE;
}
/** Registers a closure of the form callback(cx, userdata) to be called by Spidermonkey's Operation Callback API.
 *  You must *NEVER* call this function from *within* a callback function which has been registered with this facility!
 *  The punishment might just be deadlock! Don't call this function from a different thread/JSContext than the one that
 *  that you're associating the callback with.
 *
 *  This call may traverse the entire linked list of registrations. Don't add and remove callbacks a lot!
 *
 *  @returns      A pointer that can be used to delete the callback registration at a later time, or NULL on error.
 */
GPSEEAsyncCallback *gpsee_addAsyncCallback(JSContext *cx, GPSEEAsyncCallbackFunction callback, void *userdata)
{
  gpsee_runtime_t *grt = (gpsee_runtime_t *) JS_GetRuntimePrivate(JS_GetRuntime(cx));
  GPSEEAsyncCallback *newcb, **pp;

  /* Allocate the new callback entry struct */
  newcb = JS_malloc(cx, sizeof(GPSEEAsyncCallback));
  if (!newcb)
  {
    JS_ReportOutOfMemory(cx);
    return NULL;
  }

  /* Initialize the new callback entry struct (except 'next' member, which gets set while we have a lock on the list) */
  newcb->callback = callback;
  newcb->userdata = userdata;
  newcb->cx = cx;

  /* Acquire mutex protecting grt->asyncCallbacks */
  PR_Lock(grt->asyncCallbacks_lock);
  /* Insert the new callback into the list */
  /* Locate a sorted insertion point into the linked list; sort by 'cx' member */
  for (pp = &grt->asyncCallbacks; *pp && (*pp)->cx > cx; pp = &(*pp)->next);
  /* Insert! */
  newcb->next = *pp;
  *pp = newcb;
  /* Relinquish mutex */
  PR_Unlock(grt->asyncCallbacks_lock);
  /* If this is the first time this context has had a callback registered, we must register a context callback to clean
   * up all callbacks associated with this context. Note that we don't want to do this for the primordial context, but
   * it's a moot point because gpsee_maybeGC() is registered soon after context instantiation and should never be
   * removed until just before context finalization, anyway. */
  if (!newcb->next || newcb->next->cx != cx)
    gpsee_getContextPrivate(cx, &grt->asyncCallbacks, 0, gpsee_removeAsyncCallbackContext);

  /* Return a pointer to the new callback entry struct */
  return newcb;
}
/** Deletes a single gpsee_addAsyncCallback() registration. You may call this function from within the callback closure
 *  you are deleting, but not from within a different one. You must not call this function if you are not in the
 *  JSContext associated with the callback you are removing.
 *
 *  This call may traverse the entire linked list of registrations. Don't add and remove callbacks a lot. 
 *
 *  @param      cx      Current context; does not need to be a the context the callback was registered with.
 *  @param      cbHnd   Handle for the callback we are deleting
 */
void gpsee_removeAsyncCallback(JSContext *cx, GPSEEAsyncCallback *cbHnd)
{
  gpsee_runtime_t *grt = (gpsee_runtime_t *) JS_GetRuntimePrivate(JS_GetRuntime(cx));
  GPSEEAsyncCallback *cb;

  /* Acquire mutex protecting grt->asyncCallbacks */
  PR_Lock(grt->asyncCallbacks_lock);
  /* Locate the entry we want */
  for (cb = grt->asyncCallbacks; cb && cb->next != cbHnd; cb = cb->next);
  /* Remove the entry from the linked list */
  cb->next = cb->next->next;
  /* Relinquish mutex */
  PR_Unlock(grt->asyncCallbacks_lock);
  /* Free the memory */
  JS_free(cx, cbHnd);
}
/** Deletes all async callbacks associated with the current context. Suitable for use as as JSContextCallback.
 *  This is NOT SAFE to call from within an async callback.
 *  You must not call this function if you are not in the JSContext associated with the
 *  callback you are removing. This function is intended for being called during the finalization of a JSContext (ie.
 *  during the context callback, gpsee_contextCallback().)
 *
 *  @note This call may traverse the entire linked list of registrations. Don't add and remove callbacks a lot. 
 *
 *  @param      cx              The state of the JS context if used as a JSContextCallback. If calling directly, pass JSCONTEXT_DESTROY.
 *  @param      contextOp       
 *  @returns    JS_TRUE
 *
 *  @todo Investigate using gpsee_removeAsyncCallbackContext() to clean up async callbacks on context shutdown.
 */
JSBool gpsee_removeAsyncCallbackContext(JSContext *cx, uintN contextOp)
{
  gpsee_runtime_t *grt = (gpsee_runtime_t *) JS_GetRuntimePrivate(JS_GetRuntime(cx));
  GPSEEAsyncCallback **cb, **cc, *freeme = NULL;

#ifdef GPSEE_DEBUG_BUILD
  /* Assert that cx is on current thread */
  JS_BeginRequest(cx);
  JS_EndRequest(cx);
#endif

  if (contextOp != JSCONTEXT_DESTROY)
    return JS_TRUE;
  if (!grt->asyncCallbacks)
    return JS_TRUE;

  /* Acquire mutex protecting grt->asyncCallbacks */
  PR_Lock(grt->asyncCallbacks_lock);
  /* Locate the first entry we want to remove */
  for (cb = &grt->asyncCallbacks; *cb && (*cb)->cx != cx; cb = &(*cb)->next);
  if (*cb)
  {
    freeme = *cb;
    /* Locate the final entry we want remove */
    for (cc = cb; *cc && (*cc)->cx == cx; cc = &(*cc)->next);
    /* Remove all the entries we grabbed */
    *cb = *cc;
  }
  /* Relinquish mutex */
  PR_Unlock(grt->asyncCallbacks_lock);

  /* Free the memory */
  while (freeme)
  {
    GPSEEAsyncCallback *next = freeme->next;
    JS_free(cx, freeme);
    /* Break at end of removed segment */
    if (&freeme->next == cc)
      break;
    freeme = next;
  }
  return JS_TRUE;
}

/** Deletes all gpsee_addAsyncCallback() registrations. This routine
 *  runs unlocked and is
 *  intend as a shutdown-helper function and not an API entry point.
 *
 *  @param      cx      Context in the same runtime as grt
 *  @param      grt     The runtime from which to remove all callbacks
 */
static void removeAllAsyncCallbacks_unlocked(JSContext *cx, gpsee_runtime_t *grt)
{
  GPSEEAsyncCallback *cb, *d;

  /* Delete everything! */
  cb = grt->asyncCallbacks;
  while (cb)
  {
    d = cb->next;
    JS_free(cx, cb);
    cb = d;
  }
  grt->asyncCallbacks = NULL;
}
#endif

static JSBool gpsee_maybeGC(JSContext *cx, void *ignored, GPSEEAsyncCallback *cb)
{
  JS_MaybeGC(cx);
  return JS_TRUE;
}

static JSBool destroyRealm_cb(JSContext *cx, const void *key, void *value, void *private)
{
  gpsee_realm_t *realm = (void *)key;  
  return gpsee_destroyRealm(cx, realm);
}

/**
 *  @note	If this is the LAST runtime in the application,
 *		the API user should call JS_Shutdown() to avoid
 *		memory leaks.
 */
int gpsee_destroyRuntime(gpsee_runtime_t *grt)
{
  JSContext *cx = grt->coreCx;

#if !defined(GPSEE_NO_ASYNC_CALLBACKS)
  GPSEEAsyncCallback * cb;
  
  /* Clean up "operation callback" stuff */
  /* Cut off the head of the linked list to ensure that the "operation callback" trigger thread doesn't begin a new
   * run over its contents */
  /* Acquire mutex protecting grt->asyncCallbacks */
  PR_Lock(grt->asyncCallbacks_lock);
  cb = grt->asyncCallbacks;
  grt->asyncCallbacks = NULL;
  /* Relinquish mutex */
  PR_Unlock(grt->asyncCallbacks_lock);
  /* Interrupt the trigger thread in case it is in */
  if (PR_Interrupt(grt->asyncCallbackTriggerThread) != PR_SUCCESS)
    gpsee_log(cx, SLOG_WARNING, "PR_Interrupt(grt->asyncCallbackTriggerThread) failed!\n");
  /* Wait for the trigger thread to see this */
  if (PR_JoinThread(grt->asyncCallbackTriggerThread) != PR_SUCCESS)
    gpsee_log(cx, SLOG_WARNING, "PR_JoinThread(grt->asyncCallbackTriggerThread) failed!\n");
  grt->asyncCallbackTriggerThread = NULL;
  /* Destroy mutex */
  PR_DestroyLock(grt->asyncCallbacks_lock);
  /* Now we can free the contents of the list */
  grt->asyncCallbacks = cb;
  removeAllAsyncCallbacks_unlocked(cx, grt);
#endif

  gpsee_resetIOHooks(cx, grt);
  JS_SetGCCallback(cx, NULL);

  if (gpsee_ds_forEach(cx, grt->realms, destroyRealm_cb, NULL) == JS_FALSE)
    panic(GPSEE_GLOBAL_NAMESPACE_NAME ".destroyRuntime: Error destroying realm");

  gpsee_ds_destroy(grt->realms);
  gpsee_ds_destroy(grt->realmsByContext);
  gpsee_ds_destroy(grt->gcCallbackList);
  
  gpsee_shutdownMonitorSystem(grt);
  JS_DestroyContext(cx);
  JS_DestroyRuntime(grt->rt);

  free(grt);
  return 0;
}

JSClass *gpsee_getGlobalClass(void)
{
  /** Global object's class definition */
  static JSClass global_class = 
  {
    "Global", 					/* name */
    JSCLASS_NEW_RESOLVE | JSCLASS_HAS_PRIVATE | JSCLASS_GLOBAL_FLAGS,	/* flags */
    JS_PropertyStub,  				/* add property */
    JS_PropertyStub,				/* del property */
    JS_PropertyStub,				/* get property */
    JS_PropertyStub,				/* set property */
    JS_EnumerateStandardClasses,		/* enumerate */
    (JSResolveOp)global_newresolve,		/* resolve */
    JS_ConvertStub,   				/* convert */
    JS_FinalizeStub,				/* finalize */
    JSCLASS_NO_OPTIONAL_MEMBERS
  };

  return &global_class;
}

/** Initialize a global (or global-like) object. This task is normally performed by gpsee_createRuntime. 
 *  This function attaches gpsee-specific prototypes to the object, after initializing it with 
 *  JS_InitStandardClasses(). It does NOT call JS_SetGlobalObject(), or provide global object GC roots.
 *
 *  @param	cx		JavaScript context
 *  @param      realm           The realm the object belongs to
 *  @param	obj		The object to modify
 *
 *  @returns	JS_TRUE on success.  Failure may leave obj partially initialized.
 */
JSBool gpsee_initGlobalObject(JSContext *cx, gpsee_realm_t *realm, JSObject *obj)
{
  if (JS_InitStandardClasses(cx, obj) != JS_TRUE)
    return JS_FALSE;

#ifdef JS_HAS_CTYPES
    if (JS_InitCTypesClass(cx, obj) != JS_TRUE)
      return JS_FALSE;
#endif

  if (JS_DefineProperty(cx, obj, "gpseeNamespace", 
		    STRING_TO_JSVAL(JS_NewStringCopyZ(cx, GPSEE_GLOBAL_NAMESPACE_NAME)), NULL, NULL, 0) != JS_TRUE)
    return JS_FALSE;

  if (JS_DefineFunction(cx, obj, "print", gpsee_global_print, 0, 0) == NULL)
    return JS_FALSE;  

  return gpsee_modulizeGlobal(cx, realm, obj, __func__, 0);
}

/**
 *  Set the maximum (or minimum, depending on arch) address which JSAPI is allowed to use on the C stack.
 *  Any attempted use beyond this address will cause an exception to be thrown, rather than risking a
 *  segfault.  The value used will be noted in gpsee_runtime_t::threadStackLimit, so it can be
 *  reused by code which creates new contexts (including gpsee_createContext()).
 *
 *  If rc.gpsee_thread_stack_limit is zero this check is disabled.
 *
 *  @param	cx		JS Context to set - must be set before any JS code runs
 *  @param	stackBase	An address near the top (bottom) of the stack
 *
 *  @see gpsee_createContext();
 */
void gpsee_setThreadStackLimit(JSContext *cx, void *stackBase, jsuword maxStackSize)
{
  gpsee_runtime_t   *grt = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  jsuword 	        stackLimit;

  if (maxStackSize == 0)     /* Disable checking for stack overflow if limit is zero. */
  {
    stackLimit = 0;
  } 
  else 
  {
    GPSEE_ASSERT(stackBase != NULL);
#if JS_STACK_GROWTH_DIRECTION > 0
    stackLimit = (jsuword)stackBase + maxStackSize;
#else
    stackLimit = (jsuword)stackBase - maxStackSize;
#endif
  }

  JS_SetThreadStackLimit(cx, stackLimit);
  grt->threadStackLimit = stackLimit;
  return;
}

/** Instanciate a GPSEE Runtime 
 *
 *  @returns	A handle to the runtime, ready for use.
 */
gpsee_runtime_t *gpsee_createRuntime(void)
{
  const char		*jsVersion;
  JSRuntime		*rt;
  JSContext 		*cx;
  gpsee_runtime_t	*grt;
  static jsval		setUTF8 = JSVAL_FALSE;

  if (!getenv("GPSEE_NO_UTF8_C_STRINGS"))
  {
    if (jsval_CompareAndSwap(&setUTF8, JSVAL_FALSE, JSVAL_TRUE) == JS_TRUE)
      JS_SetCStringsAreUTF8();
  }

  grt = calloc(sizeof(*grt), 1);

  /* You need a runtime and one or more contexts to do anything with JS. */
  if (!(rt = JS_NewRuntime(strtol(rc_default_value(rc, "gpsee_heap_maxbytes", "0x4000"), NULL, 0))))
    panic(GPSEE_GLOBAL_NAMESPACE_NAME ": unable to create JavaScript runtime!");

  JS_SetRuntimePrivate(rt, grt);

  /* Control the maximum amount of memory the JS engine will allocate and default to infinite */
  JS_SetGCParameter(rt, JSGC_MAX_BYTES, strtol(rc_default_value(rc, "gpsee_gc_maxbytes", "0"), NULL, 0) ?: (size_t)-1);
  
  /* Create the core context, used only by GPSEE internals */
  if (!(cx = JS_NewContext(rt, atoi(rc_default_value(rc, "gpsee_stack_chunk_size", "8192")))))
    panic(GPSEE_GLOBAL_NAMESPACE_NAME ": unable to create JavaScript context!");

  if (gpsee_initializeMonitorSystem(cx, grt) == JS_FALSE)
    panic(__FILE__ ": Unable to intialize monitor subsystem");

  grt->rt               = rt;
  grt->coreCx           = cx;
  grt->realms           = gpsee_ds_create(grt, 0, 1);
  grt->realmsByContext  = gpsee_ds_create(grt, 0, 1);
  grt->gcCallbackList   = gpsee_ds_create(grt, GPSEE_DS_OTM_KEYS, 1);

  grt->useCompilerCache = rc_bool_value(rc, "gpsee_cache_compiled_modules") != rc_false ? 1 : 0;

  /* Set the JavaScript version for compatibility reasons if required. */
  if ((jsVersion = rc_value(rc, "gpsee_javascript_version")))
  {
    JSVersion version = atof(jsVersion) * 100; /* see: jspubtd.h -wg */
    JS_SetVersion(cx, version);
  }
  else
  {
    JS_SetVersion(cx, JSVERSION_LATEST);
  }

  JS_BeginRequest(cx);	/* Request stays alive as long as the grt does */
  JS_SetOptions(cx, JS_GetOptions(cx) | JSOPTION_ANONFUNFIX);
  if (gpsee_initIOHooks(cx, grt) == JS_FALSE)
    panic(__FILE__ ": Unable to initialized hookable I/O subsystem");
  JS_SetErrorReporter(cx, gpsee_errorReporter);

#if !defined(GPSEE_NO_ASYNC_CALLBACKS)
  /* Initialize async callback subsystem */
  grt->asyncCallbacks = NULL;
  /* Create mutex to protect access to 'asyncCallbacks' */
  grt->asyncCallbacks_lock = PR_NewLock();
  /* Start the "operation callback" trigger thread */
  grt->asyncCallbackTriggerThread = PR_CreateThread(PR_SYSTEM_THREAD, gpsee_asyncCallbackTriggerThreadFunc,
        grt, PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 0);
  if (!grt->asyncCallbackTriggerThread)
    panic(__FILE__ ": PR_CreateThread() failed!");
  /* Add a callback to spin the garbage collector occasionally */
  gpsee_addAsyncCallback(cx, gpsee_maybeGC, NULL);
  /* Add a context callback to remove any async callbacks associated with the context */
#endif

  JS_SetGCCallback(cx, gpsee_gcCallback);       
  return grt;
}

/** Used instead of JS_InitClass in module code. 
 *
 *  Asserting in thie function means your class does conform to GPSEE naming
 *  conventions: <code>MODULE_ID ".ClassName"</code>.
 *
 */
JSObject *gpsee_InitClass (JSContext *cx, JSObject *obj, JSObject *parent_proto,
			   JSClass *clasp, JSNative constructor, uintN nargs,
			   JSPropertySpec *ps, JSFunctionSpec *fs,
			   JSPropertySpec *static_ps, JSFunctionSpec *static_fs,
			   const char *moduleID)
{
  JSObject 	*ret;
  size_t	moduleID_len = strlen(moduleID);
  const char	*fullName = clasp->name;

  GPSEE_ASSERT(strncmp(moduleID, clasp->name, moduleID_len) == 0);	
  GPSEE_ASSERT(clasp->name[moduleID_len] == '.');

  if ((strncmp(moduleID, clasp->name, moduleID_len) != 0) || (clasp->name[moduleID_len] != '.'))
  {
    gpsee_log(cx, SLOG_NOTICE, "Initializing incorrectly-named class %s in module %s",
	      clasp->name, moduleID);
  }
  else
  {
    clasp->name += moduleID_len + 1;
  }

  ret = JS_InitClass(cx, obj, parent_proto, clasp, constructor, nargs,
		     ps, fs, static_ps, static_fs);

  clasp->name = fullName;
  return ret;
}

/** A variadic version of JS_GetInstancePrivate(), which can check multiple class 
 *  pointers in one call. If the class is any of the pointers' types, the private
 *  handle will be returned. The macro gpsee_getInstancePrivate() is exactly the
 *  same, except the trailing NULL is automatically inserted. That is the preferred
 *  interface to this functionality.
 *
 *  @see gpsee_getInstancePrivate()
 *
 *  @param	cx	JavaScript context
 *  @param	obj	The object to check
 *  @param	...	Zero or more additional class pointers to check, followed by a NULL
 *  @returns	The object's private slot
 */
void *gpsee_getInstancePrivateNTN(JSContext *cx, JSObject *obj, ...)
{
  va_list 	ap;
  JSClass	*clasp, *clasp2;
  void		*prvslot = NULL;

  if (obj == JSVAL_TO_OBJECT(JSVAL_NULL))
    return NULL;

  /* Retreive JSClass* of 'obj' argument */
  clasp = JS_GET_CLASS(cx, obj);

  /* Iterate through var-args list, stopping when we find a match, or  */
  va_start(ap, obj);
  while((clasp2 = va_arg(ap, JSClass *)))
    if (clasp2 == clasp) {
      prvslot = JS_GetPrivate(cx, obj);
      break;
    }
  va_end(ap);

  return prvslot;
}

/** Instanciate a JavaScript interpreter -- i.e. a runtime,
 *  a context, a global object.
 *  @returns	A handle to the interpreter, ready for use.
 */
gpsee_interpreter_t *gpsee_createInterpreter(void)
{
  gpsee_interpreter_t *jsi;

  jsi = calloc(sizeof(*jsi), 1);
  if (!jsi)
    return NULL;

  jsi->grt = gpsee_createRuntime();
  if (!jsi->grt)
    goto out;
  
  jsi->realm = gpsee_createRealm(jsi->grt, __func__);
  if (!jsi->grt)
    goto out;
  
  jsi->cx = gpsee_createContext(jsi->realm);
  if (!jsi->cx)
    goto out;

  jsi->globalObject     = jsi->realm->globalObject;
  jsi->rt               = jsi->grt->rt;

  return jsi;

  out:
  if (jsi->grt)
    gpsee_destroyRuntime(jsi->grt);

  free(jsi);
  return NULL;
}

void gpsee_destroyInterpreter(gpsee_interpreter_t *jsi)
{
  gpsee_destroyRuntime(jsi->grt);
}

/** Return the GPSEE runtime associated with the current JS Context.
 *  @param      cx      The current JS context
 *  @returns    A pointer to the GPSEE runtime
 */
gpsee_runtime_t *gpsee_getRuntime(JSContext *cx)
{
  return (gpsee_runtime_t *)JS_GetRuntimePrivate(JS_GetRuntime(cx)); 
}
