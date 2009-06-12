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
 * Copyright (c) 2007, PageMail, Inc. All Rights Reserved.
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
 *  @file	thread_module.c	A gpsee class for creating threads from JS.
 *  @author	Wes Garland
 *              PageMail, Inc.
 *		wes@page.ca
 *  @date	Oct 2007
 *  @version	$Id: thread_module.c,v 1.2 2009/06/12 17:01:21 wes Exp $
 *
 *  Basic strategy: JS code instanciates Thread object and calls JS start() method. 
 *                  C Back end makes a thread with NSPR, runs the run() method. If 
 *		    new thread (child) changes 'this', the thread object in
 *		    the parent has the same properties updated.
 *  
 *  Trickiness:     Cannot free resources until the thread has joined AND the 
 *                  JS Thread object is out of scope.
 *
 *  Naming Convention: 	thread_ prefix:	mechanics of performing an operation, does not 
 *					throw exceptions, but may return exception text 
 *					for caller to throw.
 *			th_ 	prefix:	method, getter, or setter of a JS Thread object
 *					or descendant.
 *			Thread_ prefix:	Code meant to be called by the embedder.
 * 
 *  @warning	By making the Thread class visible to JS, you're allowing JS code
 *		to create JSContexts. Make sure you clean those up before you
 *		call JS_DestroyRuntime.  To clean them up, call thread_FiniModule().
 */

static const char __attribute__((unused)) rcsid[]="$Id: thread_module.c,v 1.2 2009/06/12 17:01:21 wes Exp $";

#define DEBUG 1	/* moz */
#include <nspr.h>
#include "gpsee.h"
#if defined(SOLARIS)
# include <sys/processor.h>
#endif

#define MODULE_ID GPSEE_GLOBAL_NAMESPACE_NAME	".module.ca.page.thread"

#define THREAD_THREADID_MAX_SIZE	(sizeof(void *) * 2) + 1	/* ThreadID is a hex string of ptr addr */
#define THREAD_JOINALL_MAX_SLEEP_TICKS 	PR_TicksPerSecond() * 5
#define THREAD_INTERLEAVE_SWEEPERS	0

/* Immutable values, used to cheaply compare thread states. Assigned during thread_InitModule() */
static jsval thState_new = JSVAL_VOID;
static jsval thState_runnable = JSVAL_VOID;
static jsval thState_joining = JSVAL_VOID;
static jsval thState_dead = JSVAL_VOID;;

/** Possible termination states for a given thread. */
typedef enum
{
  th_term_not		= 1,		/**< JS Thread has not yet terminated */
  th_term_normal,			/**< JS Thread terminated normally */
  th_term_abnormal			/**< JS Thread terminated abnormally */
} thread_termination_t;

/** Possible data types we might find in JS_GetPrivate() for this class */
typedef enum
{
  tod_protected = 1,			/**< Data is "protected" - belongs to class proto object [normally one per runtime] */
  tod_private				/**< Data is "private" - belongs to a class instance (object) */
} thread_object_data_type_t;

typedef struct thread_protected thread_protected_t;	/* forward decl */

/** A thread handle, used to maintain the state of a given thread.
 *  Private data attached to an instance of Thread.
 */
typedef struct
{
  thread_object_data_type_t	type;			/**< MUST be first element in the struct */

  jsval				state;			/**< new | runnable | joining | dead */
  jsval				rval;			/**< JS return value from run() */
  thread_termination_t		termination;		/**< Thread's termination type (not | normal | abnormal) */

  thread_protected_t		*protected;		/**< Protected data (private data in prototype) */
  PRThread			*thread;		/**< NSPR thread handle */
  const char			*threadID;		/**< Our threadID, used as a prop of Thread.threadList */
} thread_private_t;

/** Reasonable Defaults for initalizing a thread handle */
static const thread_private_t thread_privateInitialzer = 
{
  tod_private,
  JSVAL_VOID,	JSVAL_VOID,	th_term_not,
  NULL, NULL, NULL
};

/** Java-style "Protected" storage for the Thread class - this private data
 *  is actually attached to Thread.prototype
 */
struct thread_protected
{
  thread_object_data_type_t	type;			/**< MUST be first element in the struct */
  JSObject 			*threadList;		/**< Pointer to the (book-keeping) list of Thread objects */
#if !defined(THREAD_INTERLEAVE_SWEEPERS) || (THREAD_INTERLEAVE_SWEEPERS == 0)
  jsval				sweepLock;		/**< Set to JS_TRUE when we're sweeping the thread list for dead threads */
#endif
};

/** @warning	Union relies on two structs and enum all having the same address
 *		as addr, and the enum being first in the structs. Is there a compiler 
 *		out there that could actually screw that up?
 */
typedef union
{
  thread_private_t		*private;		/**< Private data: Belongs to a thread object instance */
  thread_protected_t		*protected;		/**< Protected data: Belongs to the thread class protyped */
  thread_object_data_type_t	*type;			/**< Way to dereference type without casing to private/protected struct ptr */
  void				*addr;			/**< Syntactic sugar. */
} thread_object_type_t;

/** Macro to verify that the object's private member is in a usable state */
#define CHECK_OBJECT_PRIVATE(funcLabel) \
  do { \
   if (!hnd) return gpsee_throw(cx, MODULE_ID "." #funcLabel ".invalidObject: Invalid Thread object!"); \
    if (hnd->type != tod_private) return gpsee_throw(cx, MODULE_ID "." #funcLabel ".prototype: Expected instance, got prototype!"); \
  } while(0)

/** Macro to verify that the thread handle is in a usable state */
#define CHECK_THREAD(funcLabel) \
  do { \
    if (!hnd->thread) return gpsee_throw(cx, MODULE_ID "." #funcLabel ".invalidHandle: Invalid Thread object handle!"); \
  } while(0)

/** 
 * Add a thread to Thread.threadList.
 *
 * Thread.threadList is NOT just a "pretty interface" for the JavaScript programmer. Its
 * primary purpose is to serves as a secondary root for the newly-constructed Thread 
 * object.  The only way to remove a thread from the list is for the thread to finish 
 * running. This means that the garbage collector will call not Thread_Finalize() until
 * the Thread instance is out-of-scope AND the thread is no longer running.
 *
 *  - Thread instance object is inserted into Thread.threadList when marked runnable
 *  - Can't put Thread instance objects in Thread.threadList when new, stillborns would
 *    never get collected until the engine shut down.
 *
 * @param	cx	JS Context
 * @param	obj	Thread instance object we're adding to the list.
 * @param	hnd	Private data structure belonging to thread object
 * @returns 	NULL on success, or a message to throw on failure.
 */
static const char *thread_addToList(JSContext *cx, JSObject *obj, thread_private_t *hnd)
{
  if (JS_DefineProperty(cx, hnd->protected->threadList, hnd->threadID, OBJECT_TO_JSVAL(obj), JS_PropertyStub, JS_PropertyStub, 
			JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT) != JS_TRUE)
    return MODULE_ID ".addToList.defineProperty";

  return NULL;
}

/** 
 *  Remove a thread from Thread.threadList. 
 *  - Thread instance object is removed from Thread.threadList when marked dead
 * @param	hnd	Private data structure belonging to thread object
 * @returns 	NULL on success, or a message to throw on failure.
 */
static const char *thread_removeFromList(JSContext *cx, thread_private_t *hnd)
{
  JSBool found = JS_FALSE; 	/* quell warning */

  if (JS_SetPropertyAttributes(cx, hnd->protected->threadList, hnd->threadID, JSPROP_READONLY, &found) != JS_TRUE)
    return MODULE_ID ".removeFromList.setPropertyAttributes";

  GPSEE_ASSERT(found == JS_TRUE);

  if (JS_DeleteProperty(cx, hnd->protected->threadList, hnd->threadID))
    return MODULE_ID ".removeFromList.deleteProperty";

  return NULL;
}

/** Actual mechanics for joining a thread.
 *  This routine blocks until the thread is joined or cannot be joined.
 *
 *  There is a potentially interesting situation here.
 *  Say, we have a thread which is trying to join hnd->thread, but the
 *  interpreter is trying to shutdown, and the thread in hnd->thread never
 *  terminates. Since the embedder is shutting down the interpreter, he
 *  will call thread_FiniModule(), which in turn calls Thread_joinAll().
 *
 *  I think that the right behaviour is for this to block until hnd->thread
 *  terminates, even if that's never.  Just like a for(;;) program in 
 *  single threaded JavaScript, it won't terminate until the user sends SIGINT.
 *
 *  @param	cx 	JavaScript Context of the caller (not ncessarily the thread's context)
 *  @param	hnd	Thread handle
 *  @param	rval	[out] JS_TRUE if the thread was joined, JS_FALSE if it was not. 
 *			rval is not modified if we don't enter thState_joining. If rval is
 *			not needed, pass NULL.
  *
 *  @returns	NULL on success, or an error message to throw. Throw message MAY contain a %i,
 *		followed by a %s, for NSPR error number and text string.
 */
static const char *thread_join(JSContext *cx, thread_private_t *hnd, jsval *rval)
{
  jsrefcount	depth;
  PRThread	*theThread;

  if (rval)
    *rval = JS_FALSE;

  if (!hnd->thread)
    return NULL;

  if (jsval_CompareAndSwap(&hnd->state, thState_runnable, thState_joining) != JS_TRUE)
    return MODULE_ID ".join.state: Cannot join thread; not in state 'runnable'";

  depth = JS_SuspendRequest(cx);

  theThread = hnd->thread;
  hnd->thread = NULL;

  while ((PR_GetThreadState(theThread) == PR_JOINABLE_THREAD) && (PR_JoinThread(theThread) != PR_SUCCESS))
  {
    if (PR_GetError() != PR_PENDING_INTERRUPT_ERROR)
    {
      JS_ResumeRequest(cx, depth);
      return MODULE_ID ".join.error.%i: Unable to join thread (%s)";
    }
  }

  if (rval)
    *rval = JS_TRUE;

  JS_ResumeRequest(cx, depth);

  if (jsval_CompareAndSwap(&hnd->state, thState_joining, thState_dead) == JS_TRUE)
    thread_removeFromList(cx, hnd);
  else
    GPSEE_NOT_REACHED("impossible");

  return NULL;
}

/** Actual mechanics for executing the JavaScript code of a new thread.
 *  Caller has supplied JS context but it is our job to clean it up.
 */
static void thread_run(void *vthread_argv)
{
  /* Now in a new native thread, let's run some JavaScript */
  void			**thread_argv = vthread_argv;
  thread_private_t	*hnd = thread_argv[0];
  JSContext		*cx  = thread_argv[1];
  JSObject		*obj = thread_argv[2];

  JS_SetContextThread(cx);
  JS_BeginRequest(cx);		/* Context for thread, not calling context which is already in a request */

  JS_free(cx, thread_argv);
  if (JS_CallFunctionName(cx, obj, "run", 0, NULL, &hnd->rval) == JS_TRUE)
    hnd->termination = th_term_normal;
  else
  {
    jsval val;	/* Exception Value */

    hnd->termination = th_term_abnormal;

    if (JS_GetPendingException(cx, &val) == JS_TRUE)
    {
      JS_DefineProperty(cx, obj, "exception", val, JS_PropertyStub, JS_PropertyStub, JSPROP_ENUMERATE);
      JS_ClearPendingException(cx);
    }
  }

/* xxx JS_EndRequest(cx); cx cb needs */
  JS_DestroyContext(cx);

  return; /* The NSPR (OS) thread is now dead */
}

/** Cause the current thread to yield.
 *
 *  @note 	Later versions of spidermonkey will have JS_YieldRequest()
 *              which should be used in here instead. 
 */
static JSBool thread_yield(JSContext *cx)
{
  jsrefcount saveDepth = JS_SuspendRequest(cx);
  PR_Sleep(PR_INTERVAL_NO_WAIT);
  JS_ResumeRequest(cx, saveDepth);

  return JS_TRUE;
}

/** Join all threads which were started by this class.
 * 
 *  If JS code is still running, it is possible that more threads will be
 *  started while this routine is running, and this routine will not join
 *  them. It is important, then, for the embedding programmer to call this
 *  routine AFTER running his script, but BEFORE shutting the runtime. 
 *
 *  If this routine is NOT called, Thread_Finalize() will be call on threads
 *  that are still active.  When this happens, the finalizer will block,
 *  stalling the GC. This is very bad.
 *
 *  This routine will not attempt to join threads which are in "joining" state.
 *  These are considered to be unjoinable. But if we find some, we'll yield
 *  and try again, until they eventually go away. We use an exponential
 *  back off algorithm to determine yield time.
 * 
 *  @note This routine is called automatically (from thread_FiniModule()) when
 * 	  used with the  gpsee.c startup/shutdown sequence.
 *
 *  @param	cx		A JavaScript context owned by the calling thread.
 *  @param	protected	The class prototype's JS_GetPrivate() storage
 *
 *  @returns	NULL		on success, or an error message to throw otherwise.
 */
static const char *thread_joinAll(JSContext *cx, JSObject *proto)
{
  JSIdArray		*ida;
  thread_private_t	*hnd;
  PRIntervalTime	ticks = 1;
  const char		*e;
  int			foundJoiningState;
  int			i;
  JSClass		*clasp = JS_GetClass(cx, proto);
  thread_protected_t	*protected = JS_GetPrivate(cx, proto);

  GPSEE_ASSERT(protected != NULL);
  GPSEE_ASSERT(clasp != NULL);

  do
  {
    JS_ResumeRequest(cx, JS_SuspendRequest(cx)); /* encourage the GC to interrupt us */

    ida = JS_Enumerate(cx, protected->threadList);

    foundJoiningState = 0;

    for (i = 0; i < ida->length; i++)
    {
      jsval 		v;
      JSObject		*threadObj;
      JSObject		*owner;
      
      JS_GetMethodById(cx, protected->threadList, ida->vector[i], &owner, &v);
      if ((owner != protected->threadList) || !JSVAL_IS_OBJECT(v))
	continue;

      threadObj = JSVAL_TO_OBJECT(v);

      if (!(hnd = JS_GetInstancePrivate(cx, threadObj, clasp, NULL)))
	continue;

      if ((e = thread_join(cx, hnd, NULL)))
      {
	jsrefcount depth;

	if (hnd->state == thState_joining)
	  foundJoiningState = 1;

	depth = JS_SuspendRequest(cx);

	/* XXX Not really happy with this. Maybe some kind of
	 * conditional would be a little kinder. -wg
	 */

	PR_Sleep(ticks);
	ticks *= 2;
	if (ticks > THREAD_JOINALL_MAX_SLEEP_TICKS)
	  ticks = THREAD_JOINALL_MAX_SLEEP_TICKS;

	JS_ResumeRequest(cx, ticks);
      }
    }

    JS_DestroyIdArray(cx, ida);
  } while(foundJoiningState);
  
  return NULL;
}

/**
 *  Sweep through the thread list, looking for threads which 
 *  have terminated but not joined.
 *
 *  @note This routine is safe to interleave and must be tested
 *        in interleaved mode to help find races. However, 
 *	  running it in interleaved mode has an adverse affect
 *	  on application performance, except possibly in the
 *	  most esoteric cases (tight loops that spawn short-lived
 *	  threads ad nauseum)
 */
JSBool Thread_Sweep(JSContext *cx, JSObject *proto)
{
  JSClass		*clasp = JS_GetClass(cx, proto);
  thread_private_t	*hnd;
  thread_protected_t	*protected = JS_GetPrivate(cx, proto);
  jsid			id;
  JSBool		retval;
  JSObject		*iter = NULL;

  JSObject		*threadObj;
  JSObject		*owner;

#if !defined(THREAD_INTERLEAVE_SWEEPERS) || (THREAD_INTERLEAVE_SWEEPERS == 0)
  if (jsval_CompareAndSwap(&protected->sweepLock, JSVAL_FALSE, JSVAL_TRUE) != JS_TRUE)
    return JS_TRUE; /* Another lwp doing this already */
#endif

  threadObj = NULL;

  JS_AddNamedRoot(cx, &threadObj, "ThreadSweep_threadObj");

  iter = JS_NewPropertyIterator(cx , protected->threadList);
  if (!iter)
  {
    retval = JS_FALSE;
    goto out;
  }

  JS_AddNamedRoot(cx, &iter, "ThreadSweep_iter");

  while (((retval = JS_NextProperty(cx, iter, &id)) == JS_TRUE) && (id != JSVAL_VOID))
  {
    jsval v;

    JS_GetMethodById(cx, protected->threadList, id, &owner, &v);
    if ((owner != protected->threadList) || !JSVAL_IS_OBJECT(v))
      continue;

    threadObj = JSVAL_TO_OBJECT(v);

    if (!(hnd = JS_GetInstancePrivate(cx, threadObj, clasp, NULL)))
      continue;

    if (hnd->state != thState_runnable)
      continue;

    if (hnd->termination == th_term_not)
      continue;

    (void)thread_join(cx, hnd, NULL);	/* will suspend/resume the request */
  }

  JS_RemoveRoot(cx, &iter);

  out:
  JS_RemoveRoot(cx, &threadObj);

#if !defined(THREAD_INTERLEAVE_SWEEPERS) || (THREAD_INTERLEAVE_SWEEPERS == 0)
  if (jsval_CompareAndSwap(&protected->sweepLock, JSVAL_TRUE, JSVAL_FALSE) != JS_TRUE)
    GPSEE_NOT_REACHED("impossible");
#endif
  
  return retval;
}

JSBool Thread_SweepBCB(JSContext *cx, JSScript *script, void *private)
{
  JSObject	*Thread_prototype = (JSObject *)private;
  
  return Thread_Sweep(cx, Thread_prototype);
}

JSBool Thread_YieldBCB(JSContext *cx, JSScript *script, void *unused)
{
  return thread_yield(cx);
}

/** Release all resources allocated by the Thread class. 
 *  This routine MUST be called by the embedding before the
 *  main context or runtime are destroyed. This routine will
 *  block until all the threads are done running.
 *
 *  @see thread_joinAll()
 */
JSBool thread_FiniModule(JSContext *cx, JSObject *moduleObject)
{
  JSObject		*proto = JS_GetPrivate(cx, moduleObject);
  thread_protected_t	*protected = JS_GetPrivate(cx, proto);

  (void)thread_joinAll(cx, proto);

  JS_GC(cx);

  JS_free(cx, protected);
  JS_SetPrivate(cx, proto, NULL);

  return JS_TRUE;
}

/**
 *  Finalize a Thread instance or Thread.prototype. Called from the garbage collector,
 *  when obj is no longer reachable. This means that thread handle is out of JS scope,
 *  AND has been removed from Thread.threadList.
 *
 *  This gets called by SpiderMonkey for tod_protected as well, but we'll let FiniModule free that up.
 *
 *  @param	cx	Context GC is running in 
 *  @param	obj	Unrooted object which needs cleaning up
 */
static void Thread_Finalize(JSContext *cx, JSObject *obj)     /**< Called when obj instance or class proto is GCed */
{
  thread_object_type_t	uhnd;

  uhnd.addr = JS_GetPrivate(cx, obj);

  if (!uhnd.addr)
    return;	/* Possible during JS_DestroyContext...JS_ForceGC after thread_FiniModule? */

  if (*uhnd.type == tod_private)
  {
    thread_private_t	*hnd = uhnd.private;

    if (jsval_CompareAndSwap(&hnd->state, thState_new, thState_dead) == JS_TRUE)
    {
      GPSEE_ASSERT(hnd->thread == NULL);  /* stillborn: never called start(), not in threadList */
    }
    else
    {
      /*  There's a few ways we can get here:
       *  - Quick race with the thState_dead comparison above (no harm, no foul)
       *  - Interpreter is shutting down while threads are still running. This
       *    means the embedding programmer screwed up and didn't call thread_FiniModule().
       *  - The thread is no longer running, but needs to be joined to free up the
       *    OS resources
       * 
       *  Cases 1 & 3 will not block. Case 2 is basically GIGO.
       */ 
      if (hnd->thread)
	(void)thread_join(cx, hnd, NULL);
    }
    
    JS_free(cx, (void *)hnd->threadID);
    JS_free(cx, hnd);
  }
  /* tod_protected is cleaned up in FiniModule */

  return;
}

/** 
 *  Implements the Thread Constructor, takes a function as its argument. 
 *  Function argument is what is run when the thread is started.
 */
static JSBool Thread(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  thread_private_t	*hnd;
  JSObject		*proto;

  /* Thread() called as function. */   
  if (JS_IsConstructing(cx) != JS_TRUE)
    return gpsee_throw(cx, MODULE_ID ".constructor.notFunction: Cannot call constructor as a function!");

  *rval = OBJECT_TO_JSVAL(obj);

  hnd = JS_malloc(cx, sizeof(*hnd));
  if (hnd)
  {
    *hnd = thread_privateInitialzer;
    hnd->threadID = JS_malloc(cx, THREAD_THREADID_MAX_SIZE);
  }

  if (!hnd || !hnd->threadID)
  {    
    JS_ReportOutOfMemory(cx);
    return JS_FALSE;
  }

  sprintf((char *)hnd->threadID, GPSEE_PTR_FMT, hnd);
  hnd->state = thState_new; 
  
  if (argc != 1)
    return gpsee_throw(cx, MODULE_ID ".constructor.arguments.count");

  if (JSVAL_IS_OBJECT(argv[0]) && 
    (JS_ObjectIsFunction(cx, JSVAL_TO_OBJECT(argv[0])) == JS_TRUE))
  {
    /* Constructor argument is a function: the run method */
    JS_DefineProperty(cx, obj, "run", argv[0], JS_PropertyStub, JS_PropertyStub, JSPROP_ENUMERATE | JSPROP_PERMANENT);
  }
  else
    return gpsee_throw(cx, MODULE_ID ".constructor.arguments.0.typeof");

  proto = JS_GetPrototype(cx, obj);
  hnd->protected = JS_GetPrivate(cx, proto);
  GPSEE_ASSERT(proto && hnd->protected);

  JS_SetPrivate(cx, obj, hnd);

  return JS_TRUE;
}  

/** 
 *  Implements Thread.prototype.start(). 
 *  Launches an NSPR thread for which a Thread object has been constructed,
 *  and then calls thread_run() to execute JS code in this thread. 
 */
static JSBool th_start(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  jsrefcount		depth;
  thread_private_t	*hnd = JS_GetPrivate(cx, obj);
  extern rc_list	rc;
  JSContext		*new_cx;
  void			**thread_argv;
  const char		*e;

  /* These may become properties of Thread.prototype some day */
  PRThreadType		threadType 	= PR_SYSTEM_THREAD;		/**< user | system */
  PRThreadScope		threadScope	= PR_GLOBAL_THREAD;		/**< glocal | local | global_bound */
  PRThreadPriority	threadPriority	= PR_PRIORITY_NORMAL;		/**< low | normal | high | urgent */
  
  CHECK_OBJECT_PRIVATE(start);

  if (argc)
    return gpsee_throw(cx, MODULE_ID ".start.arguments.count: Too many arguments (%i) specified", argc);

  if (jsval_CompareAndSwap(&hnd->state, thState_new, thState_runnable) != JS_TRUE)
    return gpsee_throw(cx, MODULE_ID ".start.state: Cannot start thread - invalid state!");

  if ((e = thread_addToList(cx, obj, hnd)))
    return gpsee_throw(cx, e);

  /* Create a new context for the new thread to use. Context will never 
   * be used by any other thread, and thread itself will clean it up.
   */
  new_cx = JS_NewContext(JS_GetRuntime(cx),atoi(
      rc_default_value(rc, "gpsee_thread_stack_chunk_size", 
		       rc_default_value(rc, "gpsee_stack_chunk_size", "8192"))));
  if (!new_cx)
    return gpsee_throw(cx, MODULE_ID ".start.context: Cannot create thread context!");

  JS_SetOptions(new_cx, JS_GetOptions(cx));
#if 0
  JS_SetBranchCallback(new_cx, gpsee_branchCallback);
#else
#warning FIX ME BCB
#endif

  depth = JS_SuspendRequest(cx);

  JS_ClearContextThread(new_cx);

  thread_argv = JS_malloc(cx, sizeof(*thread_argv) * 3);
  thread_argv[0] = (void *)hnd;
  thread_argv[1] = (void *)new_cx;
  thread_argv[2] = (void *)obj;

  hnd->thread = PR_CreateThread(threadType, thread_run, (void *)thread_argv, 
				threadPriority, threadScope, PR_JOINABLE_THREAD, 0);

  if (!hnd->thread)	/* NSPR thread did not start */
  {
    /* joining -> dead is not possible here because thread_join() 
     * will not attempt to join when !hnd->thread.
     */
    if (jsval_CompareAndSwap(&hnd->state, thState_runnable, thState_dead) == JS_TRUE)
    {
      hnd->termination = th_term_abnormal;
      JS_free(cx, thread_argv);
      JS_DestroyContext(new_cx);
      JS_ResumeRequest(cx, depth);

      if ((e = thread_removeFromList(cx, hnd)))
	return gpsee_throw(cx, e);
    }
    else
    {
      GPSEE_NOT_REACHED("impossible");
    }
  }

  JS_ResumeRequest(cx, depth);
  return JS_TRUE;
}

/** 
 *  Implements Thread.prototype.join(). 
 *  Blocks until either this thread (obj) has finished running,
 *  or it is discovered that this thread is not joinable (i.e. already joined,
 *  another thread trying to join, etc.)
 *
 *  Return value of Thread.prototype.join() true when the thread was joined,
 *  and false when it could not be joined because it was in state running:
 *   - the thread was never started
 *   - another thread is trying to join it
 *   - the thread is no longer running
 *
 *  Thread.prototype.join() throws an exception when
 *   - Tried to join a thread with itself
 */
static JSBool th_join(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  thread_private_t	*hnd = JS_GetPrivate(cx, obj);
  const char		*e;

  CHECK_OBJECT_PRIVATE(join);
/*
  CHECK_THREAD(join);
  */
  if (hnd->thread == PR_GetCurrentThread())
    return gpsee_throw(cx, MODULE_ID ".join.this: Cannot join a thread with itself!");

  e = thread_join(cx, hnd, rval);
  if (e)
  {
    char errText[PR_GetErrorTextLength() + 1];
    return gpsee_throw(cx, e, PR_GetError(), PR_GetErrorText(errText));
  }

  return JS_TRUE;
}

#if defined(SOLARIS)
static JSBool th_getcpuid(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  *rval = INT_TO_JSVAL(getcpuid());
  return JS_TRUE;
}
#endif

/** 
 *  Implements Thread.sleep().
 *  Blocks until argv[0] seconds have passed. argv[0] is parsed like a float.
 *
 *  This is a static method of Thread, so call it like Thread.Sleep(2.0) to put the
 *  current thread to sleep. There is no way to order another thread to become sleepy.
 * 
 *  Resolution depends on size of platform clock tick and precision of jsdouble. It's fine.
 *  See NSPR PR_Sleep() and PR_TickPerSecond() docs for more info.
 */
static JSBool th_sleep(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  jsdouble 		d, NaN;
  PRIntervalTime	sleepTime;
  jsrefcount		saveDepth;

  if (argc != 1)
    return gpsee_throw(cx, MODULE_ID ".sleep.arguments");

  if (JSVAL_IS_DOUBLE(argv[0]))
    d = *JSVAL_TO_DOUBLE(argv[0]);
  else
  {
    if (JS_ValueToNumber(cx, argv[0], &d) != JS_TRUE)
      return gpsee_throw(cx, MODULE_ID ".sleep.arguments.0.NaN: Could not convert argument to jsdouble!");
  }

  NaN = JS_GetNaNValue(cx);
    
  if (d == NaN)
    return gpsee_throw(cx, MODULE_ID ".sleep.arguments.0.NaN");

  sleepTime = (PR_TicksPerSecond() * d);

  if (sleepTime == 0)
  {
    sleepTime = PR_INTERVAL_NO_WAIT;		/* Probably zero, but let's not count on NSPR's internals */
  }
  else
  {
    if (sleepTime == PR_INTERVAL_NO_TIMEOUT) 	/* What are the odds of that?? */
      sleepTime -= 1;				/* Nobody'll notice */
  }

  saveDepth = JS_SuspendRequest(cx);
  PR_Sleep(sleepTime);
  JS_ResumeRequest(cx, saveDepth);

  return JS_TRUE; 
}

/** 
 *  Implements Thread.yield().
 *  
 *  Surrenders the processor to ready threads of the same priority. 
 */
static JSBool th_yield(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return thread_yield(cx);
}

/** Implements Thread.exit() - Terminate the current thread.
 *
 *  Treats exiting the primordial thread the same as System.exit().
 *  Note that exiting the main thread will not cause the program
 *  to terminate immediately, it will still block waiting for all 
 *  the other threads to join and THEN exit.
 */
static JSBool th_exit(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  gpsee_interpreter_t	*jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  PRThread 		*callingThread = PR_GetCurrentThread();

  if (callingThread == jsi->primordialThread)
  {
    if (argc)
      jsi->exitCode = JSVAL_TO_INT(argv[0]);
    else
      jsi->exitCode = 0;

    jsi->exitType = et_requested;

    return JS_FALSE;
  }
  else
  {
    GPSEE_ASSERT(callingThread != ((thread_private_t *)JS_GetPrivate(cx, obj))->thread);

    if (argc)
      JS_DefineProperty(cx, obj, "exitCode", argv[0], JS_PropertyStub, JS_PropertyStub, 
			JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY);
    else
      JS_DefineProperty(cx, obj, "exitCode", JSVAL_VOID, JS_PropertyStub, JS_PropertyStub, 
			JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY);
  }

  return JS_FALSE;
}

/**
 *  Implements Thread.prototype.state getter. Returns JS Strings  "new", "runnable", "joining", or "dead".
 */
static JSBool th_state_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  thread_private_t	*hnd = JS_GetPrivate(cx, obj);

  CHECK_OBJECT_PRIVATE(state_getter);

  *vp = hnd->state;

  return JS_TRUE;
}

/** 
 * Get a JS Thread ID. Resprentation subject to change over time.
 * @note ThreadIDs may be reused from time to time!
 */
static JSBool th_threadID_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  thread_private_t	*hnd = JS_GetPrivate(cx, obj);
 
  *vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, hnd->threadID));
	
  return JS_TRUE;
}

/** Dummy setter for Thread.threadList. 
 *  This object should be read-only to the JS user, but if 
 *  we flag it as such, JSAPI won't let us change it with
 *  JS_SetProperty()  (or -- if it does, it will throw warnings
 *  in strict mode).
 */
static JSBool th_threadList_setter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  return JS_ReportWarning(cx, MODULE_ID ".threadList.setter: This is a read-only property!");
}

static JSBool th_threadList_length_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  JSIdArray 	*ida;

  ida = JS_Enumerate(cx, obj);
  *vp = INT_TO_JSVAL(ida->length);
  JS_DestroyIdArray(cx, ida);

  return JS_TRUE;
}

/**
 *  Initialize the Thread class prototype.
 *
 *  When threads are made with this package, there becomes a one-to-one relationship
 *  between a thread_private_t, a JSContext and PRThread while the thread is running.
 *
 *  @note We do a couple of odd things here:
 *
 *  - Allocate an NSPR thread private index. This is used to find the thread_private_t
 *    for the current thread without an object to look at. That has to happen if we
 *    want to be able to chain error reporters, have the error reporter chaining
 *    work based on what the context's error reporter was at the time the object
 *    was instanciated, and not "use up" the JS ContextPrivate storage. The NSPR TPI
 *    is stored in a static global. I can't think of a better place for it, and it
 *    shouldn't cause us any MT difficulties since it is only written to once.
 *
 *  - Allocate JS Private storage in the prototype. This is used to simulate
 *    class-private (java: protected) storage for the class.  This storage holds
 *    a thread_protected_t pointer, which is used to cache the global object that
 *    Thread_InitModule was called with, along with a list of threads to be joined,
 *    etc.  The reason its stored in here instead of somewhere else is so that we 
 *    have cross-thread storage which still allows Thread_InitModule to be MT-safe,
 *    i.e. in case the class embedder decides to fire up two different runtimes
 *    or something.
 *
 *  @param	cx	Valid JS Context
 *  @param	obj	The Global Object
 *
 *  @warning	By making the Thread class visible to JS, you're allowing JS code
 *		to create JSContexts. Make sure you clean those up before you
 *		call JS_DestroyRuntime, or you'll be sorry. Call thread_FiniModule()
 *		to clean up.
 */

const char *thread_InitModule(JSContext *cx, JSObject *moduleObject)
{
  /** Description of this class: */
  static JSClass thread_class = 
  {
    MODULE_ID ".Thread",	/**< its name is Thread */
    JSCLASS_HAS_PRIVATE,	/**< it uses JS_SetPrivate() */
    JS_PropertyStub,  
    JS_PropertyStub,  
    JS_PropertyStub,  
    JS_PropertyStub,
    JS_EnumerateStub, 
    JS_ResolveStub,   
    JS_ConvertStub,   
    Thread_Finalize,		/**< it has a custom finalizer */
    JSCLASS_NO_OPTIONAL_MEMBERS
  };

  /** Methods of Thread.prototype */
  static JSFunctionSpec thread_methods[] = 
  {
    { "join",			th_join,			0, 0, 0 },
    { "start",			th_start,			0, 0, 0 },
    { NULL,			NULL,				0, 0, 0 }
  };

  /** Methods of Thread */
  static JSFunctionSpec thread_static_methods[] = 
  {
    { "sleep",			th_sleep,			0, 0, 0 },
#if defined(SOLARIS)
    { "getcpuid",               th_getcpuid,                    0, 0, 0 },
#endif
    { "yield",			th_yield,			0, 0, 0 },
    { "exit",			th_exit,			0, 0, 0 },
    { NULL,			NULL,				0, 0, 0 }
  };

  /** Properties of Thread.prototype */
  static JSPropertySpec thread_props[] = 
  {
    { "state", 0,    	JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY,	th_state_getter,  	JS_PropertyStub },
    { "threadID", 0, 	JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY,	th_threadID_getter,	JS_PropertyStub },
    { NULL, 0, 0, NULL, NULL }
  };

  JSObject		*ctor;
  JSObject		*lengthObject;
  JSObject		*proto;
  thread_protected_t	*protected;

  proto =
      JS_InitClass(cx, 			/* JS context from which to derive runtime information */
		   moduleObject,	/* Object to use for initializing class (constructor arg?) */
		   NULL, 		/* parent_proto - Prototype object for the class */
 		   &thread_class, 	/* clasp - Class struct to init. Defs class for use by other API funs */
		   Thread,		/* constructor function - Scope matches obj */
		   0,			/* nargs - Number of arguments for constructor (can be MAXARGS) */
		   thread_props,	/* ps - props struct for parent_proto */
		   thread_methods, 	/* fs - functions struct for parent_proto (normal "this" methods) */
		   NULL,		/* static_ps - props struct for constructor */
		   thread_static_methods); /* static_fs - funcs struct for constructor (methods like Math.Abs()) */

  GPSEE_ASSERT(proto);

  /* Hide proto in module object so we can retrieve it during Fini.
   * Since module object holds the class, it also holds a root for
   * the prototype.
   */
  JS_SetPrivate(cx, moduleObject, proto); 

  ctor = JS_GetConstructor(cx, proto);
  GPSEE_ASSERT(ctor);

  protected = JS_malloc(cx, sizeof(*protected));
  if (!protected)
  {
    JS_ReportOutOfMemory(cx);
    return NULL;
  }

  memset(protected, 0, sizeof(*protected));
  protected->type	= tod_protected;
  protected->threadList	= JS_NewObject(cx, NULL, NULL, NULL);
#if !defined(THREAD_INTERLEAVE_SWEEPERS) || (THREAD_INTERLEAVE_SWEEPERS == 0)
  protected->sweepLock	= JSVAL_FALSE;
#endif

  /* Defining threadList on ctor serves two purposes:
   *  - provide a GC root for protected->threadList
   *  - expose static property Thread.threadList to JS code
   */
  if (JS_DefineProperty(cx, ctor, "threadList", OBJECT_TO_JSVAL(protected->threadList),
			JS_PropertyStub, th_threadList_setter, JSPROP_ENUMERATE | JSPROP_PERMANENT) != JS_TRUE)
    goto errout;

  /* 
   * Define a length property on threadList that uses a C getter.
   * We set the parent property, so that the C getter can look
   * it up an enumerate it.
   */
  lengthObject = JS_NewObject(cx, NULL, NULL, NULL);
  if (JS_SetParent(cx, lengthObject, protected->threadList) != JS_TRUE)
    goto errout;

  if (JS_DefineProperty(cx, protected->threadList, "length", OBJECT_TO_JSVAL(lengthObject), 
			th_threadList_length_getter,  JS_PropertyStub, JSPROP_PERMANENT | JSPROP_READONLY) != JS_TRUE)
    goto errout;

  /* These jsvals are used directly for jsval_CompareAndSwap and may NOT be changed! */

  if (thState_new == JSVAL_VOID)	/* Already computed thState_ values? */
  {
    thState_new		= STRING_TO_JSVAL(JS_InternString(cx, "new"));
    thState_runnable	= STRING_TO_JSVAL(JS_InternString(cx, "runnable"));
    thState_joining	= STRING_TO_JSVAL(JS_InternString(cx, "joining"));
    thState_dead	= STRING_TO_JSVAL(JS_InternString(cx, "dead"));
  }

  /* These should be tunables */
#if 0
  gpsee_addBranchCallback(cx, Thread_SweepBCB, proto, 0x7fff);  /* lower: fewer dead os threads lying around, higher: faster */
  gpsee_addBranchCallback(cx, Thread_YieldBCB, NULL, 0x0fff);	/* lower: fewer GC spin locks, higher: threads interrupted less */
#else
#warning FIX ME BCB
#endif
  PR_SetConcurrency(4);

  JS_SetPrivate(cx, proto, protected);
  
  return MODULE_ID;

  errout: 
  if (protected)
    free(protected);

  return NULL;
}

/**** Thread State Transition Diagram
 *    State transition is enforced with jsval_CompareAndSwap()
 *
 * <pre>
 * void          new              runnable                joining        dead       Why
 * =======================================================================================================================
 * |---Thread----->                                                                 Constructor
 *                |--th_start-------->                                              About to start NSPR thread
 *                                   |-------------th_start---------------->        NSPR thread failed to start
 *                                   |--------thread_join--->                       Waiting on PR_JoinThread
 *                                                          |--thread_join->        Thread stopped running
 *                |----------------------Thread_Finalize------------------->        Still born (never ran)
 *
 * </pre>
 * - Thread instance object is removed from Thread.threadList when marked dead
 * - Thread instance object is inserted into Thread.threadList when marked runnable
 * - Can't put Thread instance objects in Thread.threadList when new, stillborns would
 *   never get collected until the engine shut down.
 * - thread_join will not try to transition states until hnd->thread is non-NULL
 */

