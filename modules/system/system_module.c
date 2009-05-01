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
 *  @file       system_module.c	General holding class for operating system-level stuff
 *				which isn't available from JavaScript.
 *  @author     Wes Garland
 *  @date       Oct 2007
 *  @version    $Id: system_module.c,v 1.3 2009/05/01 19:01:46 wes Exp $
 */
 
static __attribute__((unused)) const char rcsid[]="$Id: system_module.c,v 1.3 2009/05/01 19:01:46 wes Exp $";
 
#include "gpsee.h"
#include <prinit.h>
#if defined(GPSEE_SUNOS_SYSTEM)
# include <sys/loadavg.h>
#endif
#include <math.h>

#define MODULE_ID GPSEE_GLOBAL_NAMESPACE_NAME	".module.ca.page.system"

/** loadavg getter */
static JSBool system_loadavg_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  double 	loadavg[1];

  if (getloadavg(loadavg, 1) == -1)
    return gpsee_throw(cx, MODULE_ID ".loadavg.getter.errno.%i: %m", errno);	/* Impossible, at least on Solaris 10 */

  return JS_NewDoubleValue(cx, loadavg[0], vp);
}

/** ppid getter */
static JSBool system_pid_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  *vp = INT_TO_JSVAL(getpid());
  return JS_TRUE;
}

/** ppid getter */
static JSBool system_ppid_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  *vp = INT_TO_JSVAL(getppid());
  return JS_TRUE;
}

/** pgrp getter */
static JSBool system_pgrp_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  *vp = INT_TO_JSVAL(getpgrp());
  return JS_TRUE;
}

/** pgid getter */
static JSBool system_pgid_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  *vp = INT_TO_JSVAL(getpgid(0));
  return JS_TRUE;
}

/** pgid setter */
static JSBool system_pgid_setter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  pid_t		pgid;
  JSBool	res;

  res = JS_ValueToInt32(cx, *vp, (int32 *)&pgid);
  if (res != JS_TRUE)
    return gpsee_throw(cx, MODULE_ID ".pgid.setter: Cannot convert value to int32!"); 

  if (setpgid(0, pgid))
    return gpsee_throw(cx, MODULE_ID ".pgid.setter.error.%i: Cannot set process group ID", errno);

  return JS_TRUE;
}

/** errno getter */
static JSBool system_errno_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  *vp = INT_TO_JSVAL(errno);
  return JS_TRUE;
}

/** errno setter */
static JSBool system_errno_setter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  int		num;
  JSBool	res;

  res = JS_ValueToInt32(cx, *vp, &num);

  if (res != JS_TRUE)
    return gpsee_throw(cx, MODULE_ID ".errno_setter: Cannot convert value to int32!"); 

  errno = num;

  return JS_TRUE;
}

/** Translate a system error number into a string.
 */
static JSBool system_strerror(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  char		buf[256];
  int		errnum = JSVAL_TO_INT(argv[0]);

  if (argc != 1)
    return gpsee_throw(cx, MODULE_ID ".arguments.count");

  if (strerror_r(errnum, buf, sizeof(buf)) != 0)
  {
    switch(errno)
    {
      default:
	return gpsee_throw(cx, MODULE_ID ".strerror.noString: Out of memory?");

      case EINVAL:
	snprintf(buf, sizeof(buf), "Invalid error #%i", errnum);
	break;

      case ERANGE:
	snprintf(buf, sizeof(buf), "Error number #%i (description more than %u bytes)", errnum, sizeof(buf) -1);
	break;
    }
  }

  *rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, buf));

  return JS_TRUE;
}

/** XXX Supports fractional seconds only when built with APR */
static JSBool system_sleep(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  jsdouble 		d;
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

  if (d == JS_GetNaNValue(cx))
    return gpsee_throw(cx, MODULE_ID ".sleep.arguments.0.NaN");

  saveDepth = JS_SuspendRequest(cx);

#if defined(HAVE_APR)
  apr_sleep((apr_interval_time_t)d * APR_USEC_PER_SEC);
#else
  sleep((time_t)round(d));
#endif

  JS_ResumeRequest(cx, saveDepth);

  return JS_TRUE; 
}

static size_t strcpylen(char *target, const char *source)
{
  char *t = target;
  const char *s = source;

  do
  {
    *t++ = *s;
  } while(*s++);

  return (t - source) - 1;
}

/** Load and interpreter a script in the caller's context */
static JSBool system_include(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  JSScript 	*script;
  JSObject	*scrobj;
  JSObject	*thisObj;
  jsval		*fnArg;
  const char	*scriptFilename;
  JSString	*scriptFilename_jsstr;
  JSBool	retval;

  switch(argc)
  {
    default:
      return gpsee_throw(cx, MODULE_ID ".include.arguments.count");
      break;
    case 1:
      thisObj = ((gpsee_interpreter_t *)(JS_GetRuntimePrivate(JS_GetRuntime(cx))))->globalObj;
      fnArg = argv + 0;
      break;
    case 2:
      thisObj = JSVAL_TO_OBJECT(argv[0]);
      fnArg = argv + 1;

      if (JSVAL_IS_OBJECT(argv[0]) != JS_TRUE)
	return gpsee_throw(cx, MODULE_ID ".include.arguments.0.notObject");

      if (JSVAL_IS_NULL(thisObj) == JS_TRUE)
	return gpsee_throw(cx, MODULE_ID ".include.arguments.0.isNull");

      break;
  }

  scriptFilename_jsstr = JS_ValueToString(cx, *fnArg);
  scriptFilename = JS_GetStringBytes(scriptFilename_jsstr);
  if (!scriptFilename[0])
    return gpsee_throw(cx, MODULE_ID ".include.filename: Unable to determine script filename");

  errno = 0;
  if (access(scriptFilename, F_OK))
    return gpsee_throw(cx, MODULE_ID ".include.file: %s - %s", scriptFilename, strerror(errno));

  JS_AddNamedRoot(cx, &scriptFilename_jsstr, "System.include.scriptFilename_jsstr");

  errno = 0;
  script = JS_CompileFile(cx, thisObj, scriptFilename);
  if (!script)
  {
    JS_RemoveRoot(cx, &scriptFilename_jsstr);
    return gpsee_throw(cx, MODULE_ID ".include.compile: Error compiling %s (OS reports %s)", scriptFilename, strerror(errno));
  }
  JS_RemoveRoot(cx, &scriptFilename_jsstr);

  scrobj = JS_NewScriptObject(cx, script);
  JS_AddNamedRoot(cx, &scrobj, "include_scrobj");
  retval = JS_ExecuteScript(cx, thisObj, script, rval);
  JS_RemoveRoot(cx, &scrobj);

  return retval;
}

/** Issue a shell command */
static JSBool system_system(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  int		exitCode;
  int		i;
  JSString 	*str;
  jsrefcount 	depth;
  size_t	len, totalLen = 0;
  char		*buf, *buf_p;

  if (argc == 0)
    return gpsee_throw(cx, MODULE_ID ".system.arguments.count");

  /** Coerce all args .toString() and calculate total buffer size */
  for (i = 0; i < argc; i++)
  {
    if (!JSVAL_IS_STRING(argv[i]))
    {
      str = JS_ValueToString(cx, argv[i]);
      argv[i] = STRING_TO_JSVAL(str);
    }
    else
      str = JSVAL_TO_STRING(argv[i]);

    len = JS_GetStringLength(JSVAL_TO_STRING(argv[i]));
      totalLen += len;
  }

  buf_p = buf = JS_malloc(cx, totalLen + 1);
  if (!buf)
    return gpsee_throw(cx, MODULE_ID ".system.buf.malloc");

  for (i = 0; i < argc; i++)
  {
    str = JSVAL_TO_STRING(argv[i]);
    if (!str)
      return JS_FALSE;

    buf_p += strcpylen(buf_p, JS_GetStringBytes(str));
  }

  if (*buf)
  {
    depth = JS_SuspendRequest(cx);
    exitCode = system(buf);
    exitCode = WEXITSTATUS(exitCode);
    JS_ResumeRequest(cx, depth);

    *rval = INT_TO_JSVAL(exitCode);
  }
  else
    *rval = JSVAL_FALSE;

  JS_free(cx, buf);

  return JS_TRUE; /* not reached */
}

/** Terminate the running application. 
 *  @see	exit()
 *  @see	PR_ProcessExit()
 *  @warning 	Experimental
 */
static JSBool system_underscoreExit(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  int exitCode;

  if (argc)
    exitCode = JSVAL_TO_INT(argv[0]);
  else
    exitCode = 0;

  PR_ProcessExit(exitCode);

  return JS_TRUE; /* not reached */
}

static JSBool system_exit(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  gpsee_interpreter_t	*jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));

  if (jsi->primordialThread != PR_GetCurrentThread())
  {
    return gpsee_throw(cx, MODULE_ID ".exit.thread: System.exit() may not be called by any other thread "
		       "than the thread which created the run time!");
  }

  if (argc)
    jsi->exitCode = JSVAL_TO_INT(argv[0]);
  else
    jsi->exitCode = 0;

  jsi->exitType = et_requested;

  return JS_FALSE; /* not reached */
}

/** Fork the running application. 
 *  @see	fork()
 *
 *  @note	This function doesn't know anything at all about SpiderMonkey or APR.
 *		So you can pretty much bet it's not safe to use when you have an open
 *		SNPAF_Datagram, or at least, when one is open for write.
 */
static JSBool system_fork(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  *rval = INT_TO_JSVAL(fork());

  return JS_TRUE;
}

const char *system_InitModule(JSContext *cx, JSObject *moduleObject)
{
  static JSFunctionSpec system_static_methods[] = 
  {
    { "include",		system_include,			0, 0, 0 },	/* char: filename */
    { "system",			system_system,			0, 0, 0 },	/* char: cmd str returns int exit code */
    { "exit",			system_exit,			0, 0, 0 },	/* int: exit code */
    { "_exit",			system_underscoreExit,		0, 0, 0 },	/* int: exit code */
    { "sleep",			system_sleep,			0, 0, 0 },	/* int: seconds */
    { "fork",			system_fork,			0, 0, 0 },
    { "strerror",		system_strerror,		0, 0, 0 },	/* string: error number */
    { NULL,			NULL,				0, 0, 0 },
  };

  static JSPropertySpec system_static_props[] = 
  {
    { "errno",		0, JSPROP_ENUMERATE | JSPROP_PERMANENT, 			system_errno_getter, 	system_errno_setter },
    { "loadavg", 	0, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY, 	system_loadavg_getter, 	JS_PropertyStub },
    { "pid",		0, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY, 	system_pid_getter, 	JS_PropertyStub },
    { "ppid",		0, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY, 	system_ppid_getter, 	JS_PropertyStub },
    { "pgrp",		0, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY, 	system_pgrp_getter, 	JS_PropertyStub },
    { "pgid",		0, JSPROP_ENUMERATE | JSPROP_PERMANENT, 			system_pgid_getter, 	system_pgid_setter },
   { NULL, 0, 0, NULL, NULL }
  };

  if (!JS_DefineFunctions(cx, moduleObject, system_static_methods) || !JS_DefineProperties(cx, moduleObject, system_static_props))
    return NULL;

  return MODULE_ID;
}

JSBool system_FiniModule(JSContext *cx, JSObject *moduleObject)
{
  return JS_TRUE;
}
