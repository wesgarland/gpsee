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
 * ***** END LICENSE BLOCK ***** 
 */

/**
 *  @file	gpsee.h
 *  @author	Wes Garland, wes@page.ca
 *  @version	$Id: gpsee.h,v 1.32 2010/05/12 01:12:50 wes Exp $
 *
 *  @defgroup   core            GPSEE Core
 *  @{
 *  @defgroup   api             GPSEE API
 *  @defgroup   realms          GPSEE Realms
 *  @defgroup   module_system   GPSEE Module System - implementation of Modules
 *  @defgroup   async           GPSEE Async Callbacks
 *  @defgroup   monitors        GPSEE Monitors
 *  @defgroup   datastores      GPSEE Data Stores
 *  @defgroup   bytethings      GPSEE ByteThings
 *  @}
 *  @defgroup   internal        GPSEE Internals
 *  @defgroup   modules         GPSEE Modules - Modules which ship with GPSEE
 */

#ifndef GPSEE_H
#define GPSEE_H

#include "gpsee_config.h" /* MUST BE INCLUDED FIRST */
#include <prthread.h>
#include <prlock.h>

#if defined(GPSEE_SURELYNX_STREAM)
# define GPSEE_MAX_LOG_MESSAGE_SIZE	ASL_MAX_LOG_MESSAGE_SIZE
# define gpsee_makeLogFormat(a,b)	makeLogFormat_r(a,b)
#else
# define GPSEE_MAX_LOG_MESSAGE_SIZE	1024
const char *gpsee_makeLogFormat(const char *fmt, char *fmtNew);
#endif

#if !defined(NO_GPSEE_SYSTEM_INCLUDES)
# if defined(GPSEE_SURELYNX_STREAM)
#  include <gpsee_surelynx.h>
#  include <apr_surelynx.h>
#  include <flock.h>
#  undef __FUNCTION__
# elif defined(GPSEE_UNIX_STREAM)
#  include <unistd.h>
#  include <stdio.h>
#  include <stdlib.h>
#  include <string.h>
#  include <syslog.h>
#  include <sys/file.h>
#  include <errno.h>
#  include <limits.h>
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <fcntl.h>
#  include <sys/wait.h>
#  include <stdarg.h>
# endif
#endif

#if !defined(PATH_MAX)
# define PATH_MAX	pathconf((char *)"/", _PC_MAX_CANON)
#endif

#include <jsapi.h>
#if !defined(INT_TO_JSVAL) && defined(JSVAL_ZERO) /* Bug workaround for tracemonkey, introduced Aug 18 2009, patched here Sep 25 2009 - xxx wg  */
#undef JSVAL_ZERO
#undef JSVAL_ONE
#define JSVAL_ZERO	(INT_TO_JSVAL_CONSTEXPR(0))
#define JSVAL_ONE	(INT_TO_JSVAL_CONSTEXPR(1))
#endif

#if defined(INT_TO_JSVAL) /* Support for spidermonkey/tracemonkey older than Aug 18 2009 */
# define INT_TO_JSVAL_CONSTEXPR(x) INT_TO_JSVAL(x)
#endif

#if !defined(_GPSEE_INTERNALS)
/** Intercept calls to JS_InitClass to insure proper class naming */
# define JS_InitClass(a,b,c,d,e,f,g,h,i,j)	gpsee_InitClass(a,b,c,d,e,f,g,h,i,j, MODULE_ID)
/** Wrapper macro for class names (no quotes!) for JSClass when used with GPSEE */
# define GPSEE_CLASS_NAME(a)	MODULE_ID "." #a
#endif

#if defined(JS_THREADSAFE)
# include <prinit.h>
#endif

#if !defined(JS_DLL_CALLBACK)
# define JS_DLL_CALLBACK /* */
#endif

#define GPSEE_MAX_THROW_MESSAGE_SIZE		256
#define GPSEE_GLOBAL_NAMESPACE_NAME 		"gpsee"
#define GPSEE_CURRENT_VERSION_STRING		"0.2-pre2"
#define GPSEE_MAJOR_VERSION_NUMBER	        0
#define GPSEE_MINOR_VERSION_NUMBER		2
#define GPSEE_MICRO_VERSION_NUMBER		0
#include <gpsee_formats.h>

#if !defined(fieldSize)
# define fieldSize(st, field)    sizeof(((st *)NULL)->field)
#endif
 
#if !defined(offsetOf)
# define offsetOf(st, field)     ((size_t)((void *)&(((st *)NULL)->field)))
#endif

#if defined(__cplusplus)
extern "C" {
#endif
typedef enum
{
  er_all	= 0,
  er_noWarnings	= 1 << 1,
  er_none	= er_noWarnings | 1 << 31
} errorReport_t;

/* exitType_t describes the lifecycle phase of a gpsee_runtime_t */
typedef enum
{
  et_unknown	= 0,

  /* This should only be set by gpsee_runProgramModule, and indicates that the Javascript program is finished.
   * (@todo refine?) */
  et_finished           = 1 << 0,

  /* Anywhere that a function can throw an exception, you can throw an uncatchable exception (ie. return JS_FALSE,
   * or NULL) after setting gpsee_runtime_t.exitType to et_requested. The meaning of this behavior is to unwind
   * the stack and exit immediately. Javascript code cannot catch this type of exit, however for that exact reason,
   * it is not the recommended way to exit your program, as a proper SystemExit exception (TODO unimplemented!) will
   * get the job done as long as you don't catch it (and you only should if you have a valid reason for it) but also
   * then a person can include your module and choose to belay the exit, making SystemExit more powerful and friendly.
   * This is used in System.exit() and Thread.exit().
   */
  et_requested          = 1 << 1,

  et_compileFailure	= 1 << 16,			
  et_execFailure	= 1 << 17,
  et_exception		= 1 << 18,

  et_errorMask 		= et_exception,
  et_successMask	= et_finished | et_requested
} exitType_t;

typedef struct dataStore *      gpsee_dataStore_t;      /**< Handle describing a GPSEE data store */
typedef struct moduleHandle     moduleHandle_t; 	/**< Handle describing a loaded module */
typedef struct moduleMemo       moduleMemo_t; 		/**< Handle to module system's realm-wide memo */
typedef struct modulePathEntry *modulePathEntry_t; 	/**< Pointer to a module path linked list element */
typedef void *                  gpsee_monitor_t;        /**< Synchronization primitive */
typedef void *                  gpsee_autoMonitor_t; /**< Synchronization primitive */

/** Signature for callback functions that can be registered by gpsee_addAsyncCallback() 
 *  @ingroup async
 */
typedef JSBool (*GPSEEAsyncCallbackFunction)(JSContext*, void*);

/** Data struct for entries in the asynchronous callback multiplexor.
 *  @ingroup async
 */
typedef struct GPSEEAsyncCallback
{
  GPSEEAsyncCallbackFunction  callback;   /**< Callback function */
  void                        *userdata;  /**< Opaque data pointer passed to the callback */
  JSContext                   *cx;        /**< Pointer to JSContext for the callback */
  struct GPSEEAsyncCallback   *next;      /**< Linked-list link! */
} GPSEEAsyncCallback;

/** Handle describing a gpsee runtime important details and state
 *  @ingroup core
 */
typedef struct
{
  JSRuntime 		*rt;			/**< Handle for this GPSEE runtime's JavaScript runtime */
  JSContext             *coreCx;                /**< JSContext for use by gpsee core */
  PRThread	        *primordialThread;      /**< Primordial thread for this runtime */
  gpsee_dataStore_t	realms;	                /**< Key-value index; key = realm, value = NULL */
  gpsee_dataStore_t	realmsByContext;	/**< Key-value index; key = context, value = realm */
  gpsee_dataStore_t     monitorList_unlocked;   /**< Key-value index; key = monitor, value = NULL. Must hold grt->monitors.monitor to use. */
  size_t		stackChunkSize;		/**< For calls to JS_NewContext() */
  jsuint                threadStackLimit;       /**< Upper bound on C stack bounds per context */
  int 			exitCode;		/**< Exit Code from System.exit() etc */
  exitType_t		exitType;		/**< Why the script stopped running */
  errorReport_t		errorReport;		/**< What errors to report? 0=all unless RC file overrides */
  void			(*errorLogger)(JSContext *cx, const char *pfx, const char *msg); /**< Alternate logging function for error reporter */

#if defined(JS_THREADSAFE)
  PRThread	*requireLockThread;		/**< Matches NULL or PR_GetCurrentThread if we are allowed to require; change must be atomic */
  size_t	requireLockDepth;
#endif
  GPSEEAsyncCallback    *asyncCallbacks;        /**< Pointer to linked list of OPCB entries */
  PRLock                *asyncCallbacks_lock;
  PRThread              *asyncCallbackTriggerThread;
  unsigned int          useCompilerCache:1;     /**< Option: Do we use the compiler cache? */
  const char            *pendingErrorMessage;   /**< This provides a way to provide an extra message for gpsee_reportErrorSourceCode() */

  /* Hookable IO vtable */
  int    (*user_io_printf)      (JSContext *, const char *, ...);
  int    (*user_io_fprintf)     (JSContext *, FILE *, const char *, ...);
  int    (*user_io_vfprintf)    (JSContext *, FILE *, const char *, va_list);
  size_t (*user_io_fwrite)      (void *, size_t, size_t, FILE *, JSContext *);
  size_t (*user_io_fread)       (void *, size_t, size_t, FILE *, JSContext *);
  char * (*user_io_fgets)       (char *, int, FILE *, JSContext *);
  int    (*user_io_fputs)       (const char *, FILE *, JSContext *);
  int    (*user_io_fputc)       (int, FILE *, JSContext *);
  int    (*user_io_puts)        (const char *, JSContext *);

  /* Per-fd hooks */
  size_t                                user_io_hooks_len;
  struct { jsval input; jsval output; } *user_io_hooks;

#ifdef JS_THREADSAFE
  struct
  {
    gpsee_monitor_t     monitor;        /** Monitor for use by the monitor subsystem */
    gpsee_autoMonitor_t realms;         /** Held during realm creation once the realm is visible but before it's completely initialized, 
                                            and whenever grt->realms or grt->realmsByContext pointers need synchronization. */
  } monitors;
#endif
} gpsee_runtime_t;

/** Handle describing a GPSEE Realm.
 *  @ingroup realms
 */
typedef struct
{
#ifdef GPSEE_DEBUG_BUILD
  const char            *name;                  /**< Name of the realm */
#endif
  gpsee_runtime_t       *grt;                   /**< GPSEE Runtime which created this realm */
  JSObject		*globalObject;		/**< Global object ("super-global") */
  JSContext             *cachedCx;              /**< A partially initialied context left over from realm creation, or NULL */
  moduleMemo_t 		*modules;		/**< List of loaded modules and their shutdown requirements etc */
  moduleHandle_t 	*unreachableModule_llist;/**< List of nearly-finalized modules waiting only for final free & dlclose */
  const char 		*moduleJail;		/**< Top-most UNIX directory allowed to contain modules, excluding libexec dir */
  modulePathEntry_t	modulePath;		/**< GPSEE module path */
  JSObject		*userModulePath;	/**< Module path augumented by user, e.g. require.paths */
  gpsee_dataStore_t     moduleData;             /**< Scratch-pad for modules; keys are unique pointers */
  struct
  {
    moduleHandle_t      *programModule;
    const char          *programModuleDir;        
    char * const *      script_argv;            /**< argv from main() */
  } mutable;                                    /**< In order to read/write a member of volatile, you must enter the similarly-named monitor */
#ifdef JS_THREADSAFE
  struct
  {
    gpsee_autoMonitor_t     programModule;
    gpsee_autoMonitor_t     programModuleDir;
    gpsee_autoMonitor_t     script_argv;
  } monitors;                                   /**< Monitors for mutable members */
#endif
} gpsee_realm_t;

/** Convenience structure for gpsee_createInterpreter() / gpsee_destroyInterpreter().
 *  @ingroup core
 */
typedef struct
{
  gpsee_runtime_t       *grt;           /**< A new GPSEE Runtime */
  gpsee_realm_t         *realm;         /**< A GPSEE Realm in the grt runtime */
  JSContext             *cx;            /**< A JS Context for the realm */
  JSObject              *globalObject;  /**< The global object for the realm */
} gpsee_interpreter_t;

/** @addtogroup realms
 *  @{
 */
GPSEEAsyncCallback*gpsee_addAsyncCallback(JSContext *cx, GPSEEAsyncCallbackFunction callback, void *userdata);
void   gpsee_removeAsyncCallback(JSContext *cx, GPSEEAsyncCallback *c);
JSBool gpsee_operationCallback(JSContext *cx);
/** @} */

/* core routines */
JS_EXTERN_API(void)                 gpsee_destroyInterpreter(gpsee_interpreter_t *jsi);
JS_EXTERN_API(gpsee_interpreter_t *)gpsee_createInterpreter() __attribute__((malloc));
JS_EXTERN_API(gpsee_runtime_t *)    gpsee_createRuntime() __attribute__((malloc));
JS_EXTERN_API(int)                  gpsee_destroyRuntime(gpsee_runtime_t *grt);
JS_EXTERN_API(int)                  gpsee_getExceptionExitCode(JSContext *cx);
JS_EXTERN_API(JSBool)               gpsee_reportUncaughtException(JSContext *cx, jsval exval, int dumpStack);
JS_EXTERN_API(void) 		    gpsee_setThreadStackLimit(JSContext *cx, void *stackBase, jsuword maxStackSize);
JS_EXTERN_API(JSBool)               gpsee_throw(JSContext *cx, const char *fmt, ...) __attribute__((format(printf,2,3)));
JS_EXTERN_API(void)                 gpsee_errorReporter(JSContext *cx, const char *message, JSErrorReport *report);
JS_EXTERN_API(void*)                gpsee_getContextPrivate(JSContext *cx, const void *id, size_t size, JSContextCallback cb);
JS_EXTERN_API(JSContextCallback)    gpsee_setContextCallback(JSContext *cx, JSContextCallback cb);
JS_EXTERN_API(JSBool)               gpsee_compileScript(JSContext *cx, const char *scriptFilename, FILE *scriptFile, 
							const char *scriptCode, JSScript **script, JSObject *scope, JSObject **scriptObject);
JS_EXTERN_API(JSBool)               gpsee_loadModule(JSContext *cx, JSObject *parentObject, uintN argc, jsval *argv, jsval *rval);
JS_EXTERN_API(JSObject*)            gpsee_InitClass(JSContext *cx, JSObject *obj, JSObject *parent_proto,
                                                    JSClass *clasp, JSNative constructor, uintN nargs,
                                                    JSPropertySpec *ps, JSFunctionSpec *fs,
                                                    JSPropertySpec *static_ps, JSFunctionSpec *static_fs,
                                                    const char *moduleID);
JS_EXTERN_API(const char *)         gpsee_programRelativeFilename(JSContext *cx, const char *long_filename);
JS_EXTERN_API(JSObject*)            gpsee_findModuleVarObject_byID(JSContext *cx, const char *moduleID);
JS_EXTERN_API(JSBool)               gpsee_runProgramModule(JSContext *cx, const char *scriptFilename, const char *scriptCode, FILE *scriptFile, char * const argv[], char * const script_environ[]);
JS_EXTERN_API(JSBool)               gpsee_modulizeGlobal(JSContext *cx, gpsee_realm_t *realm, JSObject *glob, const char *label, size_t id);
JS_EXTERN_API(const char *)	    gpsee_getModuleCName(moduleHandle_t *module);
JS_EXTERN_API(JSBool)               gpsee_getModuleDataStore(JSContext *cx, gpsee_dataStore_t *dataStore_p);
JS_EXTERN_API(JSBool)               gpsee_getModuleData(JSContext *cx, const void *key, void **data_p, const char *throwPrefix);
JS_EXTERN_API(JSBool)               gpsee_setModuleData(JSContext *cx, const void *key, void *data);
JS_EXTERN_API(JSBool)               gpsee_initGlobalObject(JSContext *cx, gpsee_realm_t *realm, JSObject *obj);
JS_EXTERN_API(JSClass*)             gpsee_getGlobalClass(void) __attribute__((const));
/** @addtogroup realms
 *  @{
 */
gpsee_realm_t *      gpsee_createRealm(gpsee_runtime_t *grt, const char *name) __attribute__((malloc));
gpsee_realm_t *      gpsee_getRealm(JSContext *cx);
JSBool               gpsee_destroyRealm(JSContext *cx, gpsee_realm_t *realm);
JSContext *          gpsee_newContext(gpsee_realm_t *realm);
void                 gpsee_destroyContext(JSContext *cx);
/** @} */

/* GPSEE data stores */
/** Iterator function for gpsee_ds_forEach().
 * @ingroup datastores
 */
typedef JSBool (* gpsee_ds_forEach_fn)(JSContext *cx, const void *key, void * value, void *private);
gpsee_dataStore_t       gpsee_ds_create                 (gpsee_runtime_t *grt, size_t initialSizeHint) __attribute__((malloc));
void *                  gpsee_ds_get                    (gpsee_dataStore_t store, const void *key);
JSBool                  gpsee_ds_put                    (gpsee_dataStore_t store, const void *key, void *value);
void *                  gpsee_ds_remove                 (gpsee_dataStore_t store, const void *key);
JSBool                  gpsee_ds_forEach                (JSContext *cx, gpsee_dataStore_t store, gpsee_ds_forEach_fn fn, void *private);
void                    gpsee_ds_destroy                (gpsee_dataStore_t store);
gpsee_dataStore_t       gpsee_ds_create_unlocked        (size_t initialSizeHint) __attribute__((malloc));

/* GPSEE monitors */
void                    gpsee_enterAutoMonitor          (JSContext *cx, gpsee_autoMonitor_t *monitor_p);
void                    gpsee_leaveAutoMonitor          (gpsee_autoMonitor_t monitor);
gpsee_monitor_t         gpsee_getNilMonitor             (void) __attribute__((const));
gpsee_monitor_t         gpsee_createMonitor             (gpsee_runtime_t *grt) __attribute__((malloc));
void                    gpsee_enterMonitor              (gpsee_monitor_t monitor);
void                    gpsee_leaveMonitor              (gpsee_monitor_t monitor);
void                    gpsee_destroyMonitor            (gpsee_runtime_t *grt, gpsee_monitor_t monitor);

/* support routines */
JS_EXTERN_API(signed int)           gpsee_verbosity(signed int changeBy);
JS_EXTERN_API(void)                 gpsee_setVerbosity(signed int newValue);
JS_EXTERN_API(void)                 gpsee_assert(const char *s, const char *file, JSIntn ln);
JS_EXTERN_API(JSBool)               gpsee_global_print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JS_EXTERN_API(char*)                gpsee_cpystrn(char *dst, const char *src, size_t dst_size);
JS_EXTERN_API(size_t)               gpsee_catstrn(char *dst, const char *src, size_t dst_size);
JS_EXTERN_API(const char*)          gpsee_basename(const char *filename);
JS_EXTERN_API(const char*)          gpsee_dirname(const char *filename, char *buf, size_t bufLen);
JS_EXTERN_API(int)                  gpsee_resolvepath(const char *path, char *buf, size_t bufsiz);
JS_EXTERN_API(JSBool)               gpsee_createJSArray_fromVector(JSContext *cx, JSObject *obj, const char *arrayName, char * const argv[]);
JS_EXTERN_API(void)                 gpsee_printTable(JSContext *cx, FILE *out, char *s, int ncols, const char **pfix, int shrnk, size_t maxshrnk);

/* Hookable I/O routines */
JS_EXTERN_API(void)                 gpsee_initIOHooks(JSContext *cx, gpsee_runtime_t *grt);

/* GPSEE JSAPI idiom extensions */
JS_EXTERN_API(void*)                gpsee_getInstancePrivateNTN(JSContext *cx, JSObject *obj, ...); 
#define                             gpsee_getInstancePrivate(cx, obj, ...) gpsee_getInstancePrivateNTN(cx, obj, __VA_ARGS__, NULL)
JS_EXTERN_API(void)                 gpsee_byteThingTracer(JSTracer *trc, JSObject *obj);

static inline int	gpsee_isByteThingClass(JSContext *cx, const JSClass *clasp)
{
  return clasp ? (clasp->mark == (JSMarkOp)gpsee_byteThingTracer) : 0;
}
static inline int	gpsee_isByteThing(JSContext *cx, JSObject *obj)
{
  return gpsee_isByteThingClass(cx, JS_GET_CLASS(cx, obj));
}
static inline int	gpsee_isFalsy(JSContext *cx, jsval v)
{
  if (JSVAL_IS_STRING(v))
    return (v == JS_GetEmptyStringValue (cx));

  switch(v)
  {
    case JSVAL_NULL:
    case JSVAL_VOID:
    case JSVAL_FALSE:
    case JSVAL_ZERO:
      return 1;
    default:
      return 0;
  }
}

void __attribute__((noreturn)) panic(const char *message);

/* management routines */

#if defined(GPSEE_DEBUG_BUILD)
# define GPSEE_ASSERT(_expr) \
    ((_expr)?((void)0):gpsee_assert(# _expr,__FILE__,__LINE__))
# define GPSEE_NOT_REACHED(_reasonStr) \
    gpsee_assert(_reasonStr,__FILE__,__LINE__)
#else
# define GPSEE_ASSERT(_expr) ((void)0)
# define GPSEE_NOT_REACHED(_reasonStr) ((void)0)
#endif
	
/**
 * Compile-time assert. "condition" must be a constant expression.
 * The macro can be used only in places where an "extern" declaration is
 * allowed. Originally from js/src/jsutil.h
 */
#define GPSEE_STATIC_ASSERT(condition) extern void gpsee_static_assert(int arg[(condition) ? 1 : -1])

#if !defined(JS_THREADSAFE) && !defined(MAKEDEPEND)
/* Can probably support single-threaded builds, just not tested */
# error "Building without JS_THREADSAFE!"
#endif /* !defined(JS_THREADSAFE) */

#if defined(HAVE_ATOMICH_CAS)
# include <atomic.h>
/** Compare-And-Swap jsvals using fast, unportable routines in Solaris 10's atomic.h
 *  which take advantage of hardware optimizations on supported platforms.
 *
 *  @param	vp	jsval to change
 *  @param	oldv	Only change vp if it matches oldv
 *  @param	newv	Value for new vp to become
 *  @returns	JS_TRUE when values are swapped
 *
 *  @example	if (jsval_CompareAndSwap(&v, wasThisVal, becomeThisVal) == JS_TRUE) { do_something(); };
 */
static inline JSBool jsval_CompareAndSwap(volatile jsval *vp, const jsval oldv, const jsval newv) 
{ 
  GPSEE_STATIC_ASSERT(sizeof(jsval) == sizeof(void *));
  /* jslock.c code: return js_CompareAndSwap(vp, oldv, newv) ? JS_TRUE : JS_FALSE; */
  return (atomic_cas_ptr((void  *)vp, oldv, newv) == (void *)oldv) ? JS_TRUE : JS_FALSE;
}
#else
/** Compare-And-Swap jsvals using code lifted from jslock.c ca. Spidermonkey 1.8.
 *
 *  @param      vp      jsval to change
 *  @param      oldv    Only change vp if it matches oldv
 *  @param      newv    Value for new vp to become
 *  @returns    JS_TRUE when values are swapped
 *
 *  @example    if (jsval_CompareAndSwap(&v, wasThisVal, becomeThisVal) == JS_TRUE) { do_something(); };
 */
#include <gpsee_lock.c>
static inline JSBool jsval_CompareAndSwap(jsval *vp, const jsval oldv, const jsval newv)
{
  return js_CompareAndSwap(vp, oldv, newv) ? JS_TRUE : JS_FALSE;
}
#endif

#if defined(GPSEE_UNIX_STREAM)
# include <gpsee_unix.h>
#endif
	
#include <gpsee_flock.h>

#if defined(__cplusplus)
}
#endif

#if defined(GPSEE_DEBUG_BUILD) && !defined(DOXYGEN)
/* Convenience enums, for use in debugger only */
typedef enum
{
  jsval_true = JSVAL_TRUE,
  jsval_false = JSVAL_FALSE,
  jsval_null = JSVAL_NULL,
  jsval_void = JSVAL_VOID
} jsval_t;

typedef enum
{
  js_true = JS_TRUE,
  js_false = JS_FALSE
} jsbool_t;

static jsbool_t  __attribute__((unused)) __jsbool = js_false;
static jsval_t  __attribute__((unused)) __jsval = jsval_void;
#endif

/** Flags describing implementation characteristics of a particular byte thing.
 *  @ingroup bytethings
 */
typedef enum 
{ 
  bt_immutable	= 1 << 0 	/**< byteThing is immutable -- means we can count on hnd->buffer etc never changing */
} byteThing_flags_e;

/** Generic structure for representing pointer-like-things which we store in 
 *  private slots for various modules, used to facilitate casts.  All byteThing
 *  private handles must start like this one.
 *
 *  Keys to understanding byte things -- all byte things must
 *  - have hnd->buffer which is where meaningful data is stored
 *  - have hnd->length which is where amount of meaningful data is stored (0 == don't know)
 *  - have hnd->memoryOwner which is a JSObject * which is the only object whose finalizer may free hnd->buffer
 *  - have a JSTraceOp in the class definition which uses the gpsee_byteThingTracer()
 *  - that trace op keeps hnd->memoryOwner GC-entrained when it is not "us"
 *  - any ByteThing may freely use another byteThing's hnd->buffer so long as it marks the other as the memoryOwner
 *
 *  @ingroup bytethings
 */
typedef struct
{
  size_t                length;                 /**< Number of characters in buf */
  unsigned char         *buffer;                /**< Backing store */
  JSObject		*memoryOwner;		/**< What JS Object owns the memory? NULL means "nobody" */
  byteThing_flags_e	btFlags;		/**< Flags describing this ByteThing variant */
} byteThing_handle_t;

/** Macro to insure a proper byteThing is being set up. Requirements;
 *  - called before JS_InitClass, but after JSClass is defined
 *  - Type X has x_handle_t private data type
 *  - Type X has x_class JSClass
 *  @ingroup bytethings
 */
#define GPSEE_DECLARE_BYTETHING_CLASS(cls)	 								\
GPSEE_STATIC_ASSERT(offsetOf(cls ## _handle_t, btFlags) == offsetOf(byteThing_handle_t, btFlags)); 		\
GPSEE_STATIC_ASSERT(offsetOf(cls ## _handle_t, length) == offsetOf(byteThing_handle_t, length)); 		\
GPSEE_STATIC_ASSERT(offsetOf(cls ## _handle_t, buffer) == offsetOf(byteThing_handle_t, buffer)); 		\
GPSEE_STATIC_ASSERT(offsetOf(cls ## _handle_t, memoryOwner) == offsetOf(byteThing_handle_t, memoryOwner)); 	\
GPSEE_ASSERT(cls ## _class.finalize != JS_FinalizeStub);							\
cls ## _class.mark = (JSMarkOp)gpsee_byteThingTracer;								\
cls ## _class.flags |= JSCLASS_HAS_PRIVATE | JSCLASS_MARK_IS_TRACE;


#define GPSEE_DECLARE_BYTETHING_EXTCLASS(ecls) { JSClass ecls ## _class = ecls ##_eclass.base; GPSEE_DECLARE_BYTETHING_CLASS(ecls); ecls ## _eclass.base = ecls ##_class; }

#define GPSEE_ERROR_OUTPUT_VERBOSITY   0
#define GPSEE_WARNING_OUTPUT_VERBOSITY 1
#define GPSEE_ERROR_POINTER_VERBOSITY  1
#define GPSEE_MODULE_DEBUG_VERBOSITY   3
#define GPSEE_XDR_DEBUG_VERBOSITY      3
#define GSR_MIN_TTY_VERBOSITY          1
#define GSR_FORCE_STACK_DUMP_VERBOSITY 2
#define GSR_PREPROGRAM_TTY_VERBOSITY   2
#define GSR_PREPROGRAM_NOTTY_VERBOSITY 0
#endif /* GPSEE_H */
