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
 *  @version	$Id: gpsee.h,v 1.19 2009/09/21 21:33:41 wes Exp $
 *
 *  $Log: gpsee.h,v $
 *  Revision 1.19  2009/09/21 21:33:41  wes
 *  Implemented Memory equality operator in gffi module
 *
 *  Revision 1.18  2009/09/17 20:57:11  wes
 *  Fixed %m support for gpsee_log in surelynx stream
 *
 *  Revision 1.17  2009/08/06 14:17:51  wes
 *  Corrected comment-in-comment false warning
 *
 *  Revision 1.16  2009/08/05 18:40:26  wes
 *  Adjusted _GNU_SOURCE definition location and tweaked build system to make debugging modules/gffi/\* easier
 *
 *  Revision 1.15  2009/08/04 20:22:38  wes
 *  Work towards resolving build-system circular dependencies et al
 *
 *  Revision 1.14  2009/07/31 16:08:20  wes
 *  C99
 *
 *  Revision 1.13  2009/07/31 14:56:08  wes
 *  Removed printf formats, now in gpsee_formats.h
 *
 *  Revision 1.11  2009/07/28 15:52:27  wes
 *  Updated isByteThing functions
 *
 *  Revision 1.10  2009/07/28 15:21:52  wes
 *  byteThing memoryOwner patch
 *
 *  Revision 1.9  2009/07/27 21:05:37  wes
 *  Corrected gpsee_getInstancePrivate macro
 *
 *  Revision 1.8  2009/07/23 21:19:01  wes
 *  Added ByteString_Cast
 *
 *  Revision 1.7  2009/07/23 19:00:40  wes
 *  Merged with upstream
 *
 *  Revision 1.6  2009/07/23 18:35:13  wes
 *  Added *gpsee_getInstancePrivateNTN
 *
 *  Revision 1.5  2009/06/12 17:01:20  wes
 *  Merged with upstream
 *
 *  Revision 1.4  2009/05/27 04:38:44  wes
 *  Improved build configuration for out-of-tree GPSEE embeddings
 *
 *  Revision 1.3  2009/05/08 18:18:38  wes
 *  Added fieldSize, offsetOf and gpsee_isFalsy()
 *
 *  Revision 1.2  2009/04/01 22:30:55  wes
 *  Bugfixes for getopt, linux build, and module-case in tests
 *
 *  Revision 1.1  2009/03/30 23:55:43  wes
 *  Initial Revision for GPSEE. Merges now-defunct JSEng and Open JSEng projects.
 *
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
# define JS_GetGlobalObject(cx)			deprecated(gpsee_getModuleScope)		/**< @deprecated in favour of gpsee_getModuleScope () */
# define JS_GetContextPrivate(cx,data)     	deprecated(gpsee_getContextPrivate)		/**< @deprecated in favour of gpsee_getContextPrivate() */
# define JS_SetContextPrivate(cx,data)      	deprecated(gpsee_getContextPrivate)		/**< @deprecated in favour of gpsee_setContextPrivate() */
#endif

#if defined(JS_THREADSAFE)
# include <prinit.h>
#endif

#if !defined(JS_DLL_CALLBACK)
# define JS_DLL_CALLBACK /* */
#endif

#define GPSEE_MAX_THROW_MESSAGE_SIZE		256
#define GPSEE_GLOBAL_NAMESPACE_NAME 		"gpsee"
#define GPSEE_CURRENT_VERSION_STRING		"0.2-pre1"
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

typedef enum
{
  et_unknown	= 0,

  et_finished 		= 1 << 0,			/**< Script simply finished running */
  et_requested		= 1 << 1,			/**< e.g. System.exit(), Thread.exit() */

  et_compileFailure	= 1 << 16,			
  et_execFailure	= 1 << 17,
  et_exception		= 1 << 18,

  et_errorMask 		= et_exception,
  et_successMask	= et_finished | et_requested
} exitType_t;

typedef JSBool (* JS_DLL_CALLBACK GPSEEBranchCallback)(JSContext *cx, JSScript *script, void *_private);
typedef struct moduleHandle moduleHandle_t; /**< Handle describing a loaded module */

/** Signature for callback functions that can be registered by gpsee_addAsyncCallback() */
typedef JSBool (*GPSEEAsyncCallbackFunction)(JSContext*, void*);
/** Data struct for entries in the asynchronous callback multiplexor */
typedef struct GPSEEAsyncCallback
{
  GPSEEAsyncCallbackFunction  callback;   /**< Callback function */
  void                        *userdata;  /**< Opaque data pointer passed to the callback */
  JSContext                   *cx;        /**< Pointer to JSContext for the callback */
  struct GPSEEAsyncCallback   *next;      /**< Linked-list link! */
} GPSEEAsyncCallback;

/** Handle describing a gpsee interpreter important details and state */
typedef struct
{
  JSRuntime 		*rt;			/**< Handle for this interpreter's JavaScript runtime */
  JSContext 		*cx;			/**< Context for use by primoridial thread. */
  JSObject		*globalObj;		/**< Global object, for binding things like parseInt() */
  size_t		stackChunkSize;		/**< For calls to JS_NewContext() */

  signed int		linenoOffset;		/**< Added line number in error reports, in case script doesn't start at top of file. */
  int 			exitCode;		/**< Exit Code from System.exit() etc */
  exitType_t		exitType;		/**< Why the script stopped running */
  errorReport_t		errorReport;		/**< What errors to report? 0=all unless RC file overrides */
  void			(*errorLogger)(JSContext *cx, const char *pfx, const char *msg); /**< Alternate logging function for error reporter */

  moduleHandle_t *	*modules;		/**< List of loaded modules and their shutdown requirements etc */
  moduleHandle_t 	*unreachableModule_llist;/**< List of nearly-finalized modules waiting only for final free & dlclose */
  size_t		modules_len;		/**< Number of slots allocated in modules */
  const char 		*moduleJail;		/**< Top-most UNIX directory allowed to contain modules, excluding libexec dir */
  const char		*programModule_dir;	/**< Directory JS program is in, based on its cname, used for absolute module names */

#if defined(JS_THREADSAFE)
  PRThread	*primordialThread;
#endif
  /** Pointer to linked list of OPCB entries */
  GPSEEAsyncCallback    *asyncCallbacks;
  PRLock                *asyncCallbacks_lock;
  PRThread              *asyncCallbackTriggerThread;
  unsigned int          useCompilerCache:1;     /**< Option: Do we use the compiler cache? */
} gpsee_interpreter_t;

GPSEEAsyncCallback *gpsee_addAsyncCallback(JSContext *cx, GPSEEAsyncCallbackFunction callback, void *userdata);
void gpsee_removeAsyncCallbacks(gpsee_interpreter_t *jsi);
void gpsee_removeAsyncCallback(JSContext *cx, GPSEEAsyncCallback *c);

/* core routines */
gpsee_interpreter_t *	gpsee_createInterpreter(char * const argv[], char * const script_environ[]);
int 			gpsee_destroyInterpreter(gpsee_interpreter_t *interpreter);
JSBool 			gpsee_throw(JSContext *cx, const char *fmt, ...) __attribute__((format(printf,2,3)));;
int			gpsee_addBranchCallback(JSContext *cx, GPSEEBranchCallback cb, void *_private, size_t oneMask);
JSBool 			gpsee_branchCallback(JSContext *cx, JSScript *script);
void 			gpsee_errorReporter(JSContext *cx, const char *message, JSErrorReport *report);
void *			gpsee_getContextPrivate(JSContext *cx, void *id, size_t size, JSContextCallback cb);
JSContextCallback       gpsee_setContextCallback(JSContext *cx, JSContextCallback cb);
int                     gpsee_compileScript(JSContext *cx, const char *scriptFilename, FILE *scriptFile, 
                        JSScript **script, JSObject *scope, JSObject **scriptObject, const char **errorMessage);
JSBool 			gpsee_loadModule(JSContext *cx, JSObject *parentObject, uintN argc, jsval *argv, jsval *rval);
JSObject *		gpsee_InitClass(JSContext *cx, JSObject *obj, JSObject *parent_proto,
					JSClass *clasp, JSNative constructor, uintN nargs,
					JSPropertySpec *ps, JSFunctionSpec *fs,
					JSPropertySpec *static_ps, JSFunctionSpec *static_fs,
					const char *moduleID);
JSObject *		gpsee_getModuleObject(JSContext *cx, const char *moduleID);
const char *		gpsee_runProgramModule(JSContext *cx, const char *scriptFilename, FILE *scriptFile);
JSBool 			gpsee_initGlobalObject(JSContext *cx, JSObject *obj, char * const script_argv[], char * const script_environ[]);
JSClass	*		gpsee_getGlobalClass(void);

/* support routines */
signed int		gpsee_verbosity(signed int changeBy);
void			gpsee_assert(const char *s, const char *file, JSIntn ln);
int 			gpsee_printf(const char *format, /* args */ ...) __attribute__((format(printf,1,2)));
JSBool 			gpsee_global_print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
char *			gpsee_cpystrn(char *dst, const char *src, size_t dst_size);
size_t 			gpsee_catstrn(char *dst, const char *src, size_t dst_size);
const char *		gpsee_basename(const char *filename);
const char *		gpsee_dirname(const char *filename, char *buf, size_t bufLen);
int			gpsee_resolvepath(const char *path, char *buf, size_t bufsiz);

/* GPSEE JSAPI idiom extensions */
void			*gpsee_getInstancePrivateNTN(JSContext *cx, JSObject *obj, ...); 
#define 		gpsee_getInstancePrivate(cx, obj, ...) gpsee_getInstancePrivateNTN(cx, obj, __VA_ARGS__, NULL)
void 			gpsee_byteThingTracer(JSTracer *trc, JSObject *obj);

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
  GPSEE_STATIC_ASSERT(sizeof(jsval) == 4);
  /* jslock.c code: return js_CompareAndSwap(vp, oldv, newv) ? JS_TRUE : JS_FALSE; */
  return (atomic_cas_32((uint32_t  *)vp, oldv, newv) == (uint32_t)oldv) ? JS_TRUE : JS_FALSE;
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

#if defined(GPSEE_DEBUG_BUILD)
extern int *gpsee_stackBase;
# define main(c,v)	gpsee_main(c,v); int main(int argc, char *argv[]) { int i; gpsee_stackBase = &i; return gpsee_main(argc,argv); }; int gpsee_main(c,v)
#endif

#if defined(GPSEE_DEBUG_BUILD)
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
 */
typedef struct
{
  size_t                length;                 /**< Number of characters in buf */
  unsigned char         *buffer;                /**< Backing store */
  JSObject		*memoryOwner;		/**< What JS Object owns the memory? NULL means "nobody" */
} byteThing_handle_t;

/** Macro to insure a proper byteThing is being set up. Requirements;
 *  - called before JS_InitClass, but after JSClass is defined
 *  - Type X has x_handle_t private data type
 *  - Type X has x_class JSClass
 */
#define GPSEE_DECLARE_BYTETHING_CLASS(cls)	 								\
GPSEE_STATIC_ASSERT(offsetOf(cls ## _handle_t, length) == offsetOf(byteThing_handle_t, length)); 		\
GPSEE_STATIC_ASSERT(offsetOf(cls ## _handle_t, buffer) == offsetOf(byteThing_handle_t, buffer)); 		\
GPSEE_STATIC_ASSERT(offsetOf(cls ## _handle_t, memoryOwner) == offsetOf(byteThing_handle_t, memoryOwner)); 	\
GPSEE_ASSERT(cls ## _class.finalize != JS_FinalizeStub);							\
cls ## _class.mark = (JSMarkOp)gpsee_byteThingTracer;								\
cls ## _class.flags |= JSCLASS_HAS_PRIVATE | JSCLASS_MARK_IS_TRACE;


#define GPSEE_DECLARE_BYTETHING_EXTCLASS(ecls) { JSClass ecls ## _class = ecls ##_eclass.base; GPSEE_DECLARE_BYTETHING_CLASS(ecls) }

#endif /* GPSEE_H */
