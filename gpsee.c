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
 *  @version	$Id: gpsee.c,v 1.10 2009/07/23 19:00:40 wes Exp $
 *
 *  Routines for running JavaScript programs, reporting errors via standard SureLynx
 *  mechanisms, throwing exceptions portably, etc. 
 *
 *  Can be used to embed Spidermonkey in other applications (e.g. MTA) or in the
 *  standalone SureLynx JS shell. 
 *
 *  $Log: gpsee.c,v $
 *  Revision 1.10  2009/07/23 19:00:40  wes
 *  Merged with upstream
 *
 *  Revision 1.9  2009/07/23 18:34:37  wes
 *  Added *gpsee_getInstancePrivateNTN, always set JS version
 *
 *  Revision 1.8  2009/06/12 17:01:20  wes
 *  Merged with upstream
 *
 *  Revision 1.7  2009/06/11 17:17:10  wes
 *  C strings are now UTF8
 *
 *  Revision 1.6  2009/05/27 04:29:18  wes
 *  Refactored interpreter creation to allow better GPSEE/JSAPI mapping
 *
 *  Revision 1.5  2009/05/19 21:27:28  wes
 *  Made C Strings UTF-8
 *
 *  Revision 1.4  2009/04/01 22:30:55  wes
 *  Bugfixes for getopt, linux build, and module-case in tests
 *
 *  Revision 1.3  2009/04/01 20:05:49  wes
 *  Fixed double-uncaught-log
 *
 *  Revision 1.2  2009/03/31 15:13:57  wes
 *  Updated to reflect correct module name cases and build unix stream
 *
 *  Revision 1.1  2009/03/30 23:55:43  wes
 *  Initial Revision for GPSEE. Merges now-defunct JSEng and Open JSEng projects.
 *
 *  Revision 1.3  2007/12/21 19:48:09  wes
 *   - changed log() calls to apr_surelynx_log() to remove problems
 *     linking with libm.so
 *   - Improved "print" so that it will usually output a line of
 *     text and its linefeed in the same buffer, so multi-threading
 *     print statements looks nicer.
 *   - Remove APR pool scaffolding (unnecessary complication)
 *   - Now jots down primordial threadID in interpreter handle,
 *     for use by Thread.exit()
 *
 *  Revision 1.2  2007/11/07 19:55:21  wes
 *   - Support for _FiniClass to allow us to cleanly tear down
 *     class prototype C private data before shutting down the
 *     JS engine
 *   - Correct JS Context semantics for MT embedding
 *   - Support for rc.gpsee_javascript_version
 *   - Change global object GC root pointer from temp stack to heap
 *
 */

static __attribute__((unused)) const char gpsee_rcsid[]="$Id: gpsee.c,v 1.10 2009/07/23 19:00:40 wes Exp $";

#define _GPSEE_INTERNALS
#include "gpsee.h"
#include "gpsee_private.h"

#define	GPSEE_BRANCH_CALLBACK_MASK_GRANULARITY	0xfff

#ifdef GPSEE_DEBUG_BUILD
int *gpsee_stackBase;		/**< Hook for debug module, populated by main-intercept macro in gpsee.h */
#endif

extern rc_list rc;

/** Increase, Decrease, or change application verbosity.
 *
 *  @param	changeBy	Amount to increase verbosity
 *  @returns	current verbosity (after change applied)
 */
signed int gpsee_verbosity(signed int changeBy)
{
  static signed int verbosity = 0;

  verbosity += changeBy;
  return verbosity;
}

#if defined(GPSEE_DEBUG_BUILD)
/** @see JS_Assert() in jsutil.c */
void gpsee_assert(const char *s, const char *file, JSIntn ln)
{
  gpsee_printf("Assertion failure: %s, at %s:%d\n", s, file, ln);
  gpsee_log(SLOG_ERR, "Assertion failure: %s, at %s:%d\n", s, file, ln);
  abort();
}   
#endif

/** Error Reporter for Spidermonkey. Used to report warnings and
 *  uncaught exceptions.
 */
void gpsee_errorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
  const char 		*ctmp;
  int			loglevel = SLOG_NOTICE;

  char			er_filename[64];
  char			er_exception[16];
  char			er_warning[16];
  char			er_number[16];
  char			er_lineno[16];
  char			er_charno[16];
  char			er_pfx[64];
  gpsee_interpreter_t 	*jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));

  if (jsi->errorReport == er_none)
    return;

  if (!report)
  {
    gpsee_log(loglevel, "JS error from unknown source: %s\n", message);
    return;
  }

  /* Conditionally ignore reported warnings. */
  if (JSREPORT_IS_WARNING(report->flags))
  {
    if (jsi->errorReport & er_noWarnings)
      return;

    if (rc_bool_value(rc, "gpsee_report_warnings") == rc_false)
      return;
  }

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
    snprintf(er_lineno, sizeof(er_lineno), "line %u ", report->lineno + (jsi ? jsi->linenoOffset : 0));
  else
    er_lineno[0] = (char)0;

  if (report->tokenptr && report->linebuf)
    snprintf(er_charno, sizeof(er_charno), "ch %ld ", (long int)(report->tokenptr - report->linebuf));
  else
    er_charno[0] = (char)0;

  er_warning[0] = (char)0;

  if (JSREPORT_IS_EXCEPTION(report->flags))
  {
    loglevel = SLOG_INFO;
    snprintf(er_exception, sizeof(er_exception), "exception ");
  }
  else
  {
    er_exception[0] = (char)0;

    if (JSREPORT_IS_WARNING(report->flags))
    {
      loglevel = SLOG_DEBUG;
      snprintf(er_warning, sizeof(er_warning), "%swarning ", (JSREPORT_IS_STRICT(report->flags) ? "strict " : ""));
    }
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
    if (jsi->errorLogger)
      jsi->errorLogger(cx, er_pfx, log_message);
    else
      gpsee_log(loglevel, "%s %s", er_pfx, log_message);
    message = ctmp;
  }

  if (jsi->errorLogger)
    jsi->errorLogger(cx, er_pfx, message);
  else
    gpsee_log(loglevel, "%s %s", er_pfx, message);
}

/** API-Compatible with Print() in js.c. Uses more RAM, but much less likely
 *  to intermingle output newlines when called from two different threads.
 */
JSBool gpsee_global_print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  JSString 	*str;
  jsrefcount 	depth;
  size_t	len, maxLen = 0;
  char		*buf;
  int		i;

  /** Coerce all args .toString() and calculate maximal buffer size */
  for (i = 0; i < argc; i++)
  {
    if (!JSVAL_IS_STRING(argv[i]))
    {
      str = JS_ValueToString(cx, argv[i]);
      if (!str)
	return JS_FALSE;	/* threw */
      argv[i] = STRING_TO_JSVAL(str);
    }
    else
      str = JSVAL_TO_STRING(argv[i]);

    len = JS_GetStringLength(JSVAL_TO_STRING(argv[i]));
    if (len > maxLen)
      maxLen = len;
  }

  buf = JS_malloc(cx, maxLen + 1);
  if (!buf)
    return JS_FALSE;

  if (argc > 1)
  {
    for (i = 0; i < (argc - 1); i++)
    {
      str = JS_ValueToString(cx, argv[i]);
      if (!str)
	return JS_FALSE;

      strcpy(buf, JS_GetStringBytes(str));
      depth = JS_SuspendRequest(cx);
      gpsee_printf("%s%s", i ? " " : "", buf);
      JS_ResumeRequest(cx, depth);
    }
  }
  else
    i = 0;

  if (argc)
  {
    str = JS_ValueToString(cx, argv[i]);
    if (!str)
      return JS_FALSE;

    strcpy(buf, JS_GetStringBytes(str));
  }
  else
    *buf = (char)0;

  depth = JS_SuspendRequest(cx);
  gpsee_printf("%s%s\n", i ? " " : "", buf);
  JS_ResumeRequest(cx, depth);

  JS_free(cx, buf);

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
 *  @warning	This function will panic if the interpreter cannot allocate 
 *		enough memory to prepare the exception-throwing message.
 *
 *  @note	This function uses the APR SureLynx gpsee_makeLogFormat() function,
 *		so %m will work, and so will addPercentM_Handler() with surelynx.
*/
JSBool gpsee_throw(JSContext *cx, const char *fmt, ...)
{
  char 		*message;
  JSString	*messageStr;
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

  messageStr = JS_NewStringCopyN(cx, message, length);
  if (!messageStr)
  {
    gpsee_log(SLOG_ERR, GPSEE_GLOBAL_NAMESPACE_NAME ": Unthrown Exception: %s\n", message);
    panic(GPSEE_GLOBAL_NAMESPACE_NAME ": unable to throw exception - out of memory?");
  }

  if (JS_IsExceptionPending(cx) == JS_TRUE)
    gpsee_log(SLOG_ERR, GPSEE_GLOBAL_NAMESPACE_NAME ": Already throwing an exception; not throwing '%s'!", message);
  else
    JS_SetPendingException(cx, STRING_TO_JSVAL(messageStr));

  return JS_FALSE;
}

/** Create a JS Array from a C-style argument vector.
 *
 *  @param	cx	JS Context
 *  @param 	obj	JS Object to which to attach the array
 *  @param	argv	A NULL-terminated array of char *
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
      *objp = obj;
  }

  return JS_TRUE;
}

/******************************************************************************************** Asynchronous Callbacks */

JSBool gpsee_removeAsyncCallbackContext(JSContext *cx, uintN contextOp);
/** Thread for triggering closures registered with gpsee_addAsyncCallback() */
static void gpsee_asyncCallbackTriggerThreadFunc(void *jsi_vp)
{
  gpsee_interpreter_t *jsi = (gpsee_interpreter_t *) jsi_vp;
  GPSEEAsyncCallback *cb;

  /* Run this loop as long as there are async callbacks registered */
  do {
    JSContext *cx;

    /* Acquire mutex protecting jsi->asyncCallbacks */
    PR_Lock(jsi->asyncCallbacks_lock);

    /* Grab the head of the list */
    cb = jsi->asyncCallbacks;
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
    PR_Unlock(jsi->asyncCallbacks_lock);

    /* Sleep for a bit */
    sleep(1); // TODO this should be configurable!!
  }
  while (jsi->asyncCallbacks);
}
/** Our "Operation Callback" multiplexes this Spidermonkey facility. It is called automatically by Spidermonkey, and is
 *  triggered on a regular interval by gpsee_asyncCallbackTriggerThreadFunc() */
static JSBool gpsee_operationCallback(JSContext *cx)
{
  gpsee_interpreter_t *jsi = (gpsee_interpreter_t *) JS_GetRuntimePrivate(JS_GetRuntime(cx));
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

  cb = jsi->asyncCallbacks;
  if (cb)
  {
    GPSEEAsyncCallback *next;
    do
    {
      /* Save the 'next' link in case the callback deletes itself */
      next = cb->next;
      /* Invoke callback */
      if (!((*(cb->callback))(cb->cx, cb->userdata)))
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
  gpsee_interpreter_t *jsi = (gpsee_interpreter_t *) JS_GetRuntimePrivate(JS_GetRuntime(cx));
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

  /* Acquire mutex protecting jsi->asyncCallbacks */
  PR_Lock(jsi->asyncCallbacks_lock);
  /* Insert the new callback into the list */
  /* Locate a sorted insertion point into the linked list; sort by 'cx' member */
  for (pp = &jsi->asyncCallbacks; *pp && (*pp)->cx > cx; pp = &(*pp)->next);
  /* Insert! */
  newcb->next = *pp;
  *pp = newcb;
  /* Relinquish mutex */
  PR_Unlock(jsi->asyncCallbacks_lock);
  /* If this is the first time this context has had a callback registered, we must register a context callback to clean
   * up all callbacks associated with this context. Note that we don't want to do this for the primordial context, but
   * it's a moot point because gpsee_maybeGC() is registered soon after context instantiation and should never be
   * removed until just before context finalization, anyway. */
  if (!newcb->next || newcb->next->cx != cx)
    gpsee_getContextPrivate(cx, &jsi->asyncCallbacks, 0, gpsee_removeAsyncCallbackContext);

  /* Return a pointer to the new callback entry struct */
  return newcb;
}
/** Deletes a single gpsee_addAsyncCallback() registration. You may call this function from within the callback closure
 *  you are deleting, but not from within a different one. You must not call this function if you are not in the
 *  JSContext associated with the callback you are removing.
 *
 *  This call may traverse the entire linked list of registrations. Don't add and remove callbacks a lot. */
void gpsee_removeAsyncCallback(JSContext *cx, GPSEEAsyncCallback *d)
{
  gpsee_interpreter_t *jsi = (gpsee_interpreter_t *) JS_GetRuntimePrivate(JS_GetRuntime(cx));
  GPSEEAsyncCallback *cb;

  /* Acquire mutex protecting jsi->asyncCallbacks */
  PR_Lock(jsi->asyncCallbacks_lock);
  /* Locate the entry we want */
  for (cb = jsi->asyncCallbacks; cb && cb->next != d; cb = cb->next);
  /* Remove the entry from the linked list */
  cb->next = cb->next->next;
  /* Relinquish mutex */
  PR_Unlock(jsi->asyncCallbacks_lock);
  /* Free the memory */
  JS_free(cx, d);
}
/** Deletes any number of gpsee_addAsyncCallback() registrations for a given JSContext. This is NOT SAFE to call from
 *  within such a callback. You must not call this function if you are not in the JSContext associated with the
 *  callback you are removing. This function is intended for being called during the finalization of a JSContext (ie.
 *  during the context callback, gpsee_contextCallback().)
 *
 *  This call may traverse the entire linked list of registrations. Don't add and remove callbacks a lot. */
JSBool gpsee_removeAsyncCallbackContext(JSContext *cx, uintN contextOp)
{
  gpsee_interpreter_t *jsi = (gpsee_interpreter_t *) JS_GetRuntimePrivate(JS_GetRuntime(cx));
  GPSEEAsyncCallback **cb, **cc, *freeme = NULL;

  if (contextOp != JSCONTEXT_DESTROY)
    return JS_TRUE;

  /* Acquire mutex protecting jsi->asyncCallbacks */
  PR_Lock(jsi->asyncCallbacks_lock);
  /* Locate the first entry we want to remove */
  for (cb = &jsi->asyncCallbacks; *cb && (*cb)->cx != cx; cb = &(*cb)->next);
  if (*cb)
  {
    freeme = *cb;
    /* Locate the final entry we want remove */
    for (cc = cb; *cc && (*cc)->cx == cx; cc = &(*cc)->next);
    /* Remove all the entries we grabbed */
    *cb = *cc;
  }
  /* Relinquish mutex */
  PR_Unlock(jsi->asyncCallbacks_lock);

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
/** Deletes all gpsee_addAsyncCallback() registrations */
void gpsee_removeAsyncCallbacks(gpsee_interpreter_t *jsi)
{
  GPSEEAsyncCallback *cb, *d;
  JSContext *cx = jsi->cx;
  /* Delete everything! */
  cb = jsi->asyncCallbacks;
  while (cb)
  {
    d = cb->next;
    JS_free(cx, cb);
    cb = d;
  }
  jsi->asyncCallbacks = NULL;
}
static JSBool gpsee_maybeGC(JSContext *cx, void *ignored)
{
  JS_MaybeGC(cx);
  return JS_TRUE;
}

/**
 *  @note	If this is the LAST interpreter in the application,
 *		the API user should call JS_Shutdown() to avoid
 *		memory leaks.
 */
int gpsee_destroyInterpreter(gpsee_interpreter_t *interpreter)
{
  GPSEEAsyncCallback * cb;

  /* Clean up "operation callback" stuff */
  /* Cut off the head of the linked list to ensure that the "operation callback" trigger thread doesn't begin a new
   * run over its contents */
  /* Acquire mutex protecting jsi->asyncCallbacks */
  PR_Lock(interpreter->asyncCallbacks_lock);
  cb = interpreter->asyncCallbacks;
  interpreter->asyncCallbacks = NULL;
  /* Wait for the trigger thread to see this */
  PR_JoinThread(interpreter->asyncCallbackTriggerThread);
  interpreter->asyncCallbackTriggerThread = NULL;
  /* Now we can free the contents of the list */
  interpreter->asyncCallbacks = cb;
  gpsee_removeAsyncCallbacks(interpreter);
  /* Relinquish mutex */
  PR_Unlock(interpreter->asyncCallbacks_lock);
  /* Destroy mutex */
  PR_DestroyLock(interpreter->asyncCallbacks_lock);

  gpsee_shutdownModuleSystem(interpreter->cx);

  JS_RemoveRoot(interpreter->cx, &interpreter->globalObj);
  JS_EndRequest(interpreter->cx);
  JS_DestroyContext(interpreter->cx);
  JS_DestroyRuntime(interpreter->rt);
  
  if (interpreter->modules)
    free(interpreter->modules);
  free(interpreter);
  return 0;
}

JSClass *gpsee_getGlobalClass()
{
  /** Global object's class definition */
  static JSClass global_class = 
  {
    "Global", 					/* name */
    JSCLASS_NEW_RESOLVE,			/* flags */
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

/** Initialize a global (or global-like) object. This task is normally performed by gpsee_createInterpreter. 
 *  This function attaches gpsee-specific prototypes to the object, after initializing it with 
 *  JS_InitStandardClasses(). It does NOT call JS_SetGlobalObject(), or provide global object GC roots.
 *
 *  @param	cx		JavaScript context
 *  @param	obj		The object to modify
 *  @param	script_argv	Argument vector for the script (not the interpreter)
 *  @param	script_env	Environment variables for the script (often, but not always, 
 *  				the same as the interpreter)
 *
 *  @returns	JS_TRUE on success.  Failure may leave obj partially initialized.
 */
JSBool gpsee_initGlobalObject(JSContext *cx, JSObject *obj, char * const script_argv[], char * const script_environ[])
{
  if (JS_InitStandardClasses(cx, obj) != JS_TRUE)
    return JS_FALSE;

  if (JS_DefineProperty(cx, obj, "gpseeNamespace", 
		    STRING_TO_JSVAL(JS_NewStringCopyZ(cx, GPSEE_GLOBAL_NAMESPACE_NAME)), NULL, NULL, 0) != JS_TRUE)
    return JS_FALSE;

  if (JS_DefineFunction(cx, obj, "print", gpsee_global_print, 0, 0) == NULL)
    return JS_FALSE;

  if (script_argv)
    gpsee_createJSArray_fromVector(cx, obj, "arguments", script_argv);

  if (script_environ)
    gpsee_createJSArray_fromVector(cx, obj, "environ", script_environ);

  if (JS_DefineFunction(cx, obj, "loadModule", gpsee_loadModule, 0, 0) == NULL)
    return JS_FALSE;

  if (JS_DefineFunction(cx, obj, "require", gpsee_loadModule, 0, 0) == NULL)
    return JS_FALSE;

  return JS_TRUE;
}

/** Instanciate a JavaScript interpreter -- i.e. a runtime,
 *  a context, a global object.
 *
 *  @returns	A handle to the interpreter, ready for use.
 */
gpsee_interpreter_t *gpsee_createInterpreter(char * const script_argv[], char * const script_environ[])
{
  JSClass		*global_class = gpsee_getGlobalClass();
  const char		*jsVersion;
  JSRuntime		*rt;
  JSContext 		*cx;
  gpsee_interpreter_t	*interpreter;

  JS_SetCStringsAreUTF8();

  interpreter = calloc(sizeof(*interpreter), 1);

  /* You need a runtime and one or more contexts to do anything with JS. */
  if (!(rt = JS_NewRuntime(strtol(rc_default_value(rc, "gpsee_heap_maxbytes", "0x40000"), NULL, 0))))
    panic(GPSEE_GLOBAL_NAMESPACE_NAME ": unable to create JavaScript runtime!");

  JS_SetRuntimePrivate(rt, interpreter);

  /* Control the maximum amount of memory the JS engine will allocate and default to infinite */
  JS_SetGCParameter(rt, JSGC_MAX_BYTES, strtol(rc_default_value(rc, "gpsee_gc_maxbytes", "0"), NULL, 0) ?: (size_t)-1);
  
  /* Create the default context, used for the primordial thread */
  if (!(cx = JS_NewContext(rt, atoi(rc_default_value(rc, "gpsee_stack_chunk_size", "8192")))))
    panic(GPSEE_GLOBAL_NAMESPACE_NAME ": unable to create JavaScript context!");

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

  JS_BeginRequest(cx);	/* Request stays alive as long as the interpreter does */
  JS_SetErrorReporter(cx, gpsee_errorReporter);

  interpreter->globalObj = JS_NewObject(cx, global_class, NULL, NULL);
  if (!interpreter->globalObj)
    panic(GPSEE_GLOBAL_NAMESPACE_NAME ": unable to create global object!");

  JS_AddNamedRoot(cx, &interpreter->globalObj, "globalObj");	/* Technically, probably unnecessary but WTH */

  if (gpsee_initGlobalObject(cx, interpreter->globalObj, script_argv, script_environ) == JS_FALSE)
    panic(GPSEE_GLOBAL_NAMESPACE_NAME ": unable to initialize global object!");

#if !defined(MAKEDEPEND) && !defined(DOXYGEN)
# if defined(JS_THREADSAFE)
  interpreter->primordialThread	= PR_GetCurrentThread();
# else
#  error "We need threads"
# endif
  interpreter->cx	 	= cx;
  interpreter->rt 	 	= rt;
#endif
  
  gpsee_initializeModuleSystem(cx);

  interpreter->useCompilerCache = rc_bool_value(rc, "gpsee_cache_compiled_modules") != rc_false ? 1 : 0;

  /* Initialize async callback subsystem */
  interpreter->asyncCallbacks = NULL;
  /* Create mutex to protect access to 'asyncCallbacks' */
  interpreter->asyncCallbacks_lock = PR_NewLock();
  /* Start the "operation callback" trigger thread */
  JS_SetOperationCallback(cx, gpsee_operationCallback);
  interpreter->asyncCallbackTriggerThread = PR_CreateThread(PR_SYSTEM_THREAD, gpsee_asyncCallbackTriggerThreadFunc,
        interpreter, PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_UNJOINABLE_THREAD, 0);
  if (!interpreter->asyncCallbackTriggerThread)
    panic(__FILE__ ": TERRIBL ERROR: PR_CreateThread() failed!");
  /* Add a callback to spin the garbage collector occasionally */
  gpsee_addAsyncCallback(cx, gpsee_maybeGC, NULL);
  /* Add a context callback to remove any async callbacks associated with the context */

  return interpreter;
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
    gpsee_log(SLOG_NOTICE, "Initializing incorrectly-named class %s in module %s",
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
 */
void *gpsee_getInstancePrivateNTN(JSContext *cx, JSObject *obj, ...)
{
  va_list 	ap;
  JSClass	*clasp;
  void		*prvslot;

  va_start(ap, obj);
  while((clasp = va_arg(ap, JSClass *)))
  {
    prvslot = JS_GetInstancePrivate(cx, obj, clasp, NULL);
    if (prvslot)
      return prvslot;
  }
  va_end(ap);
  return NULL;
}
