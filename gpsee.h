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
 *  @version	$Id: gpsee.h,v 1.38 2010/12/20 18:34:56 wes Exp $
 *
 *  @defgroup   core            GPSEE Core API
 *  @{
 *  @defgroup   realms          GPSEE Realms
 *  @defgroup   module_system   GPSEE Module System - implementation of Modules
 *  @defgroup   async           GPSEE Async Callbacks
 *  @defgroup   monitors        GPSEE Monitors
 *  @defgroup   datastores      GPSEE Data Stores
 *  @defgroup   bytethings      GPSEE ByteThings
 *  @}
 *  @defgroup   internal        GPSEE Internals
 *  @defgroup   modules         GPSEE Modules - Modules which ship with GPSEE
 *  @defgroup   utility         Utility / Portability functions: system-level functions provided for/by GPSEE 
 *  @defgroup   debugger        GPSEE Debugger Facilities
 */

#ifndef GPSEE_H
#define GPSEE_H

/* Forward declarations & primitive typedefs */
typedef struct gpsee_realm 	gpsee_realm_t;		/**< @ingroup realms */
typedef struct dataStore *      gpsee_dataStore_t;      /**< Handle describing a GPSEE data store */
typedef struct moduleHandle     moduleHandle_t; 	/**< Handle describing a loaded module */
typedef struct moduleMemo       moduleMemo_t; 		/**< Handle to module system's realm-wide memo */
typedef struct modulePathEntry *modulePathEntry_t; 	/**< Pointer to a module path linked list element */
typedef void *                  gpsee_monitor_t;        /**< Synchronization primitive */
typedef void *                  gpsee_autoMonitor_t;    /**< Synchronization primitive */
typedef struct GPSEEAsyncCallback GPSEEAsyncCallback;   /**< @ingroup async */

#include "gpsee_config.h" /* MUST BE INCLUDED FIRST */
#include <prthread.h>
#include <prlock.h>

#if defined(GPSEE_SURELYNX_STREAM)
# define GPSEE_MAX_LOG_MESSAGE_SIZE	ASL_MAX_LOG_MESSAGE_SIZE
# define gpsee_makeLogFormat(a,b)	makeLogFormat_r(a,b)
#else
# define GPSEE_MAX_LOG_MESSAGE_SIZE	1024
const char *gpsee_makeLogFormat(const char *fmt, char *fmtNew); /**< @ingroup utility */
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
#ifdef GPSEE_DEBUGGER
# include "jsdebug.h"
# include "jsdb.h"
#endif

/* Compatibility macros to maintain backward compatbility with ~JS 1.8.1 */
#if !defined(JS_TYPED_ROOTING_API)
# define JS_AddValueRoot(cx, vp)                JS_AddRoot(cx, vp);
# define JS_AddStringRoot(cx, rp)               JS_AddRoot(cx, rp);
# define JS_AddObjectRoot(cx, rp)               JS_AddRoot(cx, rp);
# define JS_AddDoubleRoot(cx, rp)               JS_AddRoot(cx, rp);
# define JS_AddGCThingRoot(cx, rp)              JS_AddRoot(cx, rp);

# define JS_AddNamedValueRoot(cx, vp, name)     JS_AddNamedRoot(cx, vp, name);
# define JS_AddNamedStringRoot(cx, rp, name)    JS_AddNamedRoot(cx, rp, name);
# define JS_AddNamedObjectRoot(cx, rp, name)    JS_AddNamedRoot(cx, rp, name);
# define JS_AddNamedDoubleRoot(cx, rp, name)    JS_AddNamedRoot(cx, rp, name);
# define JS_AddNamedGCThingRoot(cx, rp, name)   JS_AddNamedRoot(cx, rp, name);

# define JS_RemoveValueRoot(cx, vp)             JS_RemoveRoot(cx, vp);
# define JS_RemoveStringRoot(cx, rp)            JS_RemoveRoot(cx, rp);
# define JS_RemoveObjectRoot(cx, rp)            JS_RemoveRoot(cx, rp);
# define JS_RemoveDoubleRoot(cx, rp)            JS_RemoveRoot(cx, rp);
# define JS_RemoveGCThingRoot(cx, rp)           JS_RemoveRoot(cx, rp);
#endif

#if !defined(JS_HAS_NEW_GLOBAL_OBJECT)
# define JS_NewGlobalObject(cx, clasp)          JS_NewObject(cx, clasp, NULL, NULL);
#endif

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
#include <gpsee_version.h>
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

  et_compileFailure	= 1 << 16,			/* Unable to compile some code */
  et_execFailure	= 1 << 17,			/* Exec failed, but not with exception */
  et_exception		= 1 << 18,			/* Uncaught exception */
  et_dummy		= 1 << 19,			/* Dummy, so that et_errorMask has a unique value */
  et_successMask	= et_finished | et_requested,
  et_errorMask 		= ~et_successMask | et_dummy
} exitType_t;

#ifndef GPSEE_NO_ASYNC_CALLBACKS
/** addtogroup async
 *  @{
 */
/** Signature for callback functions that can be registered by gpsee_addAsyncCallback()  */
typedef JSBool (*GPSEEAsyncCallbackFunction)(JSContext*, void*, GPSEEAsyncCallback *);

/** Data struct for entries in the asynchronous callback multiplexor. */
struct GPSEEAsyncCallback
{
  GPSEEAsyncCallbackFunction  callback;   /**< Callback function */
  void                        *userdata;  /**< Opaque data pointer passed to the callback */
  JSContext                   *cx;        /**< Pointer to JSContext for the callback */
  struct GPSEEAsyncCallback   *next;      /**< Linked-list link */
}; 
/** @} */
#endif

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
  gpsee_dataStore_t     gcCallbackList;         /**< List of GC callback functions and their invocation realms */
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
#ifndef GPSEE_NO_ASYNC_CALLBACKS
  GPSEEAsyncCallback    *asyncCallbacks;        /**< Pointer to linked list of OPCB entries */
  PRLock                *asyncCallbacks_lock;
  PRThread              *asyncCallbackTriggerThread;
#endif
  unsigned int          useCompilerCache:1;     /**< Option: Do we use the compiler cache? */
  const char            *pendingErrorMessage;   /**< This provides a way to provide an extra message for gpsee_reportErrorSourceCode() */

#ifdef JS_THREADSAFE
  struct
  {
    gpsee_monitor_t     monitor;        /** Monitor for use by the monitor subsystem */
    gpsee_autoMonitor_t realms;         /** Held during realm creation once the realm is visible but before it's completely initialized, 
                                            and whenever grt->realms or grt->realmsByContext pointers need synchronization. */
    gpsee_autoMonitor_t user_io;        /**< Monitor which must be held to read/write user_io */
    gpsee_autoMonitor_t cx;             /**< Monitor which is held during gpsee_createContext() and gpsee_destroyContext() */
  } monitors;
#endif

  struct
  {
    int    (*printf)      (JSContext *, const char *, ...);                   /**< Hookable I/O vtable entry for printf */
    int    (*fprintf)     (JSContext *, FILE *, const char *, ...);           /**< Hookable I/O vtable entry for fprintf */
    int    (*vfprintf)    (JSContext *, FILE *, const char *, va_list);       /**< Hookable I/O vtable entry for vfprintf */
    size_t (*fwrite)      (void *, size_t, size_t, FILE *, JSContext *);      /**< Hookable I/O vtable entry for fwrite */
    size_t (*fread)       (void *, size_t, size_t, FILE *, JSContext *);      /**< Hookable I/O vtable entry for fread */
    char * (*fgets)       (char *, int, FILE *, JSContext *);                 /**< Hookable I/O vtable entry for fgets */
    int    (*fputs)       (const char *, FILE *, JSContext *);                /**< Hookable I/O vtable entry for fputs */
    int    (*fputc)       (int, FILE *, JSContext *);                         /**< Hookable I/O vtable entry for fputc */
    int    (*fgetc)       (FILE *, JSContext *);                              /**< Hookable I/O vtable entry for fgetc */
    int    (*puts)        (const char *, JSContext *);                        /**< Hookable I/O vtable entry for puts */

    struct 
    { 
      gpsee_realm_t           *realm;                 /**< GPSEE Realm the I/O hooks belong to */
      jsval                   input;                  /**< JavaScript function to generate output */
      jsval                   output;                 /**< JavaScript function to collect input */
      JSContext               *hookCx;                /**< A JS Context which can be used by any thread holding the hookMonitor */
      gpsee_autoMonitor_t     monitor;                /**< Monitor which must be held to use hookCx, or to call any vtable function */
    } *hooks;                                         /**< Javascript I/O hooks array; per-fd hooks; indexed by file descriptor  */
    size_t                    hooks_len;              /**< Number of entries in hooks array (maxfd+1) */
  } user_io;                                          /**< Hookable I/O vtable and JavaScript I/O hooks */
} gpsee_runtime_t;

/** Handle describing a GPSEE Realm.
 *  @ingroup realms
 */
struct gpsee_realm
{
  gpsee_runtime_t       *grt;                   /**< GPSEE Runtime which created this realm */
  JSObject		*globalObject;		/**< Global object ("super-global") */
  JSContext             *cachedCx;              /**< A partially initialied context left over from realm creation, or NULL */
  moduleMemo_t 		*modules;		/**< List of loaded modules and their shutdown requirements etc */
  moduleHandle_t 	*unreachableModule_llist;/**< List of nearly-finalized modules waiting only for final free & dlclose */
  const char 		*moduleJail;		/**< Top-most UNIX directory allowed to contain modules, excluding libexec dir */
  modulePathEntry_t	modulePath;		/**< GPSEE module path */
  JSObject		*moduleObjectProto;	/**< Prototype for all module objects */
  JSClass		*moduleObjectClass;	/**< Class for all module objects */
  JSObject		*userModulePath;	/**< Module path augumented by user, e.g. require.paths */
  JSObject		*requireDotMain;	/**< Pointer to the program module's "module free var" */
  gpsee_dataStore_t     moduleData;             /**< Scratch-pad for modules; keys are unique pointers */
  gpsee_dataStore_t     user_io_pendingWrites;  /**< Pending data which will be written on the next async callback */

  struct
  {
    moduleHandle_t      *programModule;
    const char          *programModuleDir;        
    char * const *      script_argv;            /**< argv from main() */
  } monitored;                                    

/**< In order to read/write a member of monitored, you must enter the similarly-named monitor */
#ifdef JS_THREADSAFE
  struct
  {
    gpsee_autoMonitor_t     programModule;
    gpsee_autoMonitor_t     programModuleDir;
    gpsee_autoMonitor_t     script_argv;
  } monitors;                                   /**< Monitors for monitored members */
#endif

#ifdef GPSEE_DEBUG_BUILD
  const char            *name;                  /**< Name of the realm */
#else
  const char		*unused;
#endif
};

/** Convenience structure for gpsee_createInterpreter() / gpsee_destroyInterpreter().
 *  @ingroup core
 */
typedef struct
{
  gpsee_runtime_t       *grt;           /**< A new GPSEE Runtime */
  gpsee_realm_t         *realm;         /**< A GPSEE Realm in the grt runtime */
  JSRuntime             *rt;            /**< The JS runtime associated with the GPSEE Runtime */
  JSContext             *cx;            /**< A JS Context for the realm */
  JSObject              *globalObject;  /**< The global object for the realm */
} gpsee_interpreter_t;

/** @addtogroup core
 *  @{
 */
typedef JSBool (* gpsee_gcCallback_fn)(JSContext *cx, gpsee_realm_t *, JSGCStatus); /**< GPSEE GC Callback function type */
JSBool                  gpsee_addGCCallback             (gpsee_runtime_t *grt, gpsee_realm_t *realm, gpsee_gcCallback_fn cb);
JSBool                  gpsee_removeGCCallback          (gpsee_runtime_t *grt, gpsee_realm_t *realm, gpsee_gcCallback_fn cb);
JSBool                  gpsee_removeAllGCCallbacks_forRealm(gpsee_runtime_t *grt, gpsee_realm_t *realm);
/** @} */
/** @addtogroup async
 *  @{
 */
#ifndef GPSEE_NO_ASYNC_CALLBACKS
GPSEEAsyncCallback *    gpsee_addAsyncCallback          (JSContext *cx, GPSEEAsyncCallbackFunction callback, void *userdata);
void                    gpsee_removeAsyncCallback       (JSContext *cx, GPSEEAsyncCallback *c);
#endif
/** @} */

/* core routines */
JS_EXTERN_API(void)                 gpsee_destroyInterpreter(gpsee_interpreter_t *jsi);
JS_EXTERN_API(gpsee_interpreter_t *)gpsee_createInterpreter(void) __attribute__((malloc));
JS_EXTERN_API(gpsee_runtime_t *)    gpsee_createRuntime(void) __attribute__((malloc));
gpsee_runtime_t *                   gpsee_getRuntime(JSContext *cx);
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
/** @addtogroup modules
 *  @{
 */
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
/** @} */
JS_EXTERN_API(JSBool)               gpsee_initGlobalObject(JSContext *cx, gpsee_realm_t *realm, JSObject *obj);
JS_EXTERN_API(JSClass*)             gpsee_getGlobalClass(void) __attribute__((const));
/** @addtogroup realms
 *  @{
 */
gpsee_realm_t *      gpsee_createRealm(gpsee_runtime_t *grt, const char *name) __attribute__((malloc));
gpsee_realm_t *      gpsee_getRealm(JSContext *cx);
JSBool               gpsee_destroyRealm(JSContext *cx, gpsee_realm_t *realm);
JSContext *          gpsee_createContext(gpsee_realm_t *realm);
void                 gpsee_destroyContext(JSContext *cx);
/** @} */

/** @addtogroup datastores
 *  @{
 */
typedef JSBool (* gpsee_ds_forEach_fn)(JSContext *cx, const void *key, void * value, void *_private); /**< Iterator function for gpsee_ds_forEach() */
gpsee_dataStore_t       gpsee_ds_create                 (gpsee_runtime_t *grt, uint32 flags, size_t initialSizeHint) __attribute__((malloc));
void *                  gpsee_ds_get                    (gpsee_dataStore_t store, const void *key);
JSBool                  gpsee_ds_put                    (gpsee_dataStore_t store, const void *key, void *value);
void *                  gpsee_ds_remove                 (gpsee_dataStore_t store, const void *key);
JSBool                  gpsee_ds_match_remove           (gpsee_dataStore_t store, const void *key, const void *value);
void                    gpsee_ds_empty                  (gpsee_dataStore_t store);
JSBool                  gpsee_ds_forEach                (JSContext *cx, gpsee_dataStore_t store, gpsee_ds_forEach_fn fn, void *_private);
void                    gpsee_ds_destroy                (gpsee_dataStore_t store);
gpsee_dataStore_t       gpsee_ds_create_unlocked        (size_t initialSizeHint) __attribute__((malloc));
JSBool 			gpsee_ds_hasData		(JSContext *cx, gpsee_dataStore_t store);

/**
 *  Flag to create an unlocked GPSEE Data Store.  An unlocked data store 
 *  requires external synchronization for read/write access. Provided so that
 *  we can use data stores to build synchronization primitives. If you are 
 *  not sure that you need this flag, then you don't.
 */
#define GPSEE_DS_UNLOCKED       (1 << 0)
/** 
 *  Flag to create a GPSEE Data Store where the value pointer is considered part of the key.
 */
#define GPSEE_DS_OTM_KEYS    (1 << 1)
/** @} */

/* GPSEE monitors */
/** @addtogroup monitors
 *  @{
 */
void                    gpsee_enterAutoMonitorRT        (gpsee_runtime_t *grt, gpsee_autoMonitor_t *monitor_p);
void                    gpsee_enterAutoMonitor          (JSContext *cx, gpsee_autoMonitor_t *monitor_p);
void                    gpsee_leaveAutoMonitor          (gpsee_autoMonitor_t monitor);
gpsee_monitor_t         gpsee_getNilMonitor             (void) __attribute__((const));
gpsee_monitor_t         gpsee_createMonitor             (gpsee_runtime_t *grt) __attribute__((malloc));
void                    gpsee_enterMonitor              (gpsee_monitor_t monitor);
void                    gpsee_leaveMonitor              (gpsee_monitor_t monitor);
void                    gpsee_destroyMonitor            (gpsee_runtime_t *grt, gpsee_monitor_t monitor);

/** @addtogroup debugger
 *  @{
 */
#if defined(GPSEE_DEBUGGER) || defined(DOXYGEN)
JSDContext *            gpsee_initDebugger(JSContext *cx, gpsee_realm_t *realm, const char *debugger);
void                    gpsee_finiDebugger(JSDContext *jsdc);
JSBool                  gpsee_native_break(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
#endif
/** @} */

/* support routines */
int			gpsee_isatty(int fd);
JS_EXTERN_API(signed int)           gpsee_verbosity(signed int changeBy);
JS_EXTERN_API(void)                 gpsee_setVerbosity(signed int newValue);
JS_EXTERN_API(void)                 gpsee_assert(const char *s, const char *file, JSIntn ln) __attribute__((noreturn));
JS_EXTERN_API(JSBool)               gpsee_global_print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JS_EXTERN_API(void)                 gpsee_printTable(JSContext *cx, FILE *out, char *s, int ncols, const char **pfix, int shrnk, size_t maxshrnk);

/** @addtogroup utils
 *  @{
 */
JS_EXTERN_API(char*)                gpsee_cpystrn(char *dst, const char *src, size_t dst_size);
JS_EXTERN_API(size_t)               gpsee_catstrn(char *dst, const char *src, size_t dst_size);
JS_EXTERN_API(const char*)          gpsee_basename(const char *filename);
JS_EXTERN_API(const char*)          gpsee_dirname(const char *filename, char *buf, size_t bufLen);
JS_EXTERN_API(int)                  gpsee_resolvepath(const char *path, char *buf, size_t bufsiz);
JS_EXTERN_API(JSBool)               gpsee_createJSArray_fromVector(JSContext *cx, JSObject *obj, const char *arrayName, char * const argv[]);
/** @} */

/* Hookable I/O routines */
JS_EXTERN_API(void)                 gpsee_resetIOHooks(JSContext *cx, gpsee_runtime_t *grt);
JSBool gpsee_initIOHooks(JSContext *cx, gpsee_runtime_t *grt);
void gpsee_uio_dumpPendingWrites(JSContext *cx, gpsee_realm_t *realm);

/* GPSEE JSAPI idiom extensions */
JS_EXTERN_API(void*)                gpsee_getInstancePrivateNTN(JSContext *cx, JSObject *obj, ...); 
#define                             gpsee_getInstancePrivate(cx, obj, ...) gpsee_getInstancePrivateNTN(cx, obj, __VA_ARGS__, NULL)
JS_EXTERN_API(void)                 gpsee_byteThingTracer(JSTracer *trc, JSObject *obj); /**< @ingroup bytethings */
JSObject *gpsee_newByteThing(JSContext *cx, void *buffer, size_t length, JSBool copy);

/** Determine if JSClass instaciates bytethings or not.
 *  @ingroup    bytethings
 *  @param      cx      The current JS context
 *  @param      clasp   The class pointer to test
 *  @returns    0 if it is not
 */
#ifndef DOXYGEN
static inline 
#endif
int	gpsee_isByteThingClass(JSContext *cx, const JSClass *clasp)
{
  return clasp ? (clasp->mark == (JSMarkOp)gpsee_byteThingTracer) : 0;
}

/** Determine if an object is a bytething or not.
 *  @ingroup    bytethings
 *  @param      cx      The current JS context
 *  @param      obj     The object to test
 *  @returns    0 if it is not
 */
#ifndef DOXYGEN
static inline 
#endif
int	gpsee_isByteThing(JSContext *cx, JSObject *obj)
{
  return gpsee_isByteThingClass(cx, JS_GET_CLASS(cx, obj));
}

/** Determine if a jsval is falsey (null, void, false, zero).
 *  @ingroup    utils
 *  @param      cx      The current JS context
 *  @param      v       The value to test
 *  @returns    1 if the value is falsey, 0 otherwise
 */
#ifndef DOXYGEN
static inline 
#endif
int	gpsee_isFalsy(JSContext *cx, jsval v)
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
 *  <tt><i>example:</i></tt>	if (jsval_CompareAndSwap(&v, wasThisVal, becomeThisVal) == JS_TRUE) { do_something(); };
 *
 *  @ingroup    utils
 *  @param	vp	jsval to change
 *  @param	oldv	Only change vp if it matches oldv
 *  @param	newv	Value for new vp to become
 *  @returns	JS_TRUE when values are swapped
 *
 */
#ifndef DOXYGEN
static inline 
#endif
JSBool jsval_CompareAndSwap(volatile jsval *vp, const jsval oldv, const jsval newv) 
{ 
  GPSEE_STATIC_ASSERT(sizeof(jsval) == sizeof(void *));
  /* jslock.c code: return js_CompareAndSwap(vp, oldv, newv) ? JS_TRUE : JS_FALSE; */
  return (atomic_cas_ptr((void  *)vp, (void *)oldv, (void *)newv) == (void *)oldv) ? JS_TRUE : JS_FALSE;
}
#else
/** Compare-And-Swap jsvals using code lifted from jslock.c ca. Spidermonkey 1.8.
 *
 *  <tt><i>example:<i/></tt>    if (jsval_CompareAndSwap(&v, wasThisVal, becomeThisVal) == JS_TRUE) { do_something(); };
 *
 *  @ingroup    utils
 *  @param      vp      jsval to change
 *  @param      oldv    Only change vp if it matches oldv
 *  @param      newv    Value for new vp to become
 *  @returns    JS_TRUE when values are swapped
 */
#include <gpsee_lock.c>
#ifndef DOXYGEN
static inline 
#endif
JSBool jsval_CompareAndSwap(jsval *vp, const jsval oldv, const jsval newv)
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

/** @ingroup bytethings
 *  @see GPSEE_DECLARE_BYTETHING_CLASS
 */
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
