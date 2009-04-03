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
 *  @file	mozshell_module.cpp	Wrapper to allow importing and pretzel-twisting of
 *					Mozilla's JS Shell such that it becomes a valid
 *					GPSEE module. Intercepts stdout and redirects
 *					it through JavaScript functions.
 *
 *  @author	Wes Garland, PageMail, Inc.,  wes@page.ca
 *  @date	Jan 2009
 *  @version	$Id: mozshell_module.cpp,v 1.2 2009/03/31 15:09:17 wes Exp $
 */

#define _GPSEE_INTERNALS
#include <gpsee.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#define GPSEE_DEBUG_MODULE_MAX_OUTPUT_BUFFER_SIZE		1024 * 64  /**< 64K oughtta be long enough for anybody */

#if defined(GPSEE_SUNOS_SYSTEM)
# define tmpnam(s) tmpnam_r(s)
#endif

#ifdef stdout
#undef stdout
#undef stderr
FILE *stdout = NULL;
FILE *stderr = NULL;
#endif

extern "C" { const char *mozshell_InitModule(JSContext *cx, JSObject *moduleObject); }

extern FILE *gOutFile;

static void capture_printf(JSContext *cx, JSObject *obj, jsval *vp, FILE *whatFile, const char *fmt, ...) __attribute__((format(printf,5,6)));
#define MODULE_ID GPSEE_GLOBAL_NAMESPACE_NAME ".module.ca.page.debug"

#if defined(__GNUC__)
# if (__GNUC__ < 2)
#  error "You must be kidding"
# endif
# if (__GNUC__ == 2)
#  define fprintf(a, fmt, args...) capture_printf(cx, obj, vp, gOutFile, fmt, ##args)
# else
#  define fprintf(a, fmt, args...) capture_printf(cx, obj, vp, gOutFile, fmt, ## args)
# endif
#else
# define fprintf(a, fmt, ...) capture_printf(cx, obj, vp, gOutFile, fmt, __VA_ARGS__) /* iso c99? */
#endif

#ifdef fputc
# undef fputc
#endif

#ifdef fputs
# undef fputs
#endif

#ifdef putc
# undef putc
#endif

#ifdef puts
# undef puts
#endif

#ifdef putchar
# undef putchar
#endif

#define fputc(ch,stream)	capture_printf(cx, obj, vp, stream, "%c", ch)
#define putc(ch,stream) 	capture_printf(cx, obj, vp, stream, "%c", ch)
#define putchar(ch)		capture_printf(cx, obj, vp, gOutFile, "%c", ch)
#define fputs(s,stream) 	capture_printf(cx, obj, vp, stream, "%s", s)
#define puts(s) 		capture_printf(cx, obj, vp, gOutFile, "%s", s)
#define fflush(stream)		capture_printf(cx, obj, vp, stream, " \b");

/* capture_printf macro'd functions will shadow these into non-nulled-ness when they can be used */
static JSContext *cx = NULL;
static JSObject *obj = NULL;
static jsval *vp = NULL;

#undef main
#define main(a,b,c) dummy(a,b,c)

extern "C" {
JS_EXTERN_API(char) *readline(const char *prompt)
{
  GPSEE_NOT_REACHED("Module does not allow interactive input");
  return NULL;
};
};

#undef EDITLINE
#include "shell/js.cpp"

#undef fputs
#undef putc
#undef putchar
#undef fputs
#undef puts
#undef fprintf

/* Use cx + obj to find JS function for screen output; fall back to stdio or gpsee_log */
void capture_outputString(JSContext *cx, JSObject *obj, int isError, const char *buf)
{
  char		*funName;
  jsval		v;

  /* We try to use JavaScript functions debugPrint() and errorPrint(); 
   * if not, we fall back to stdout/stderr and gpsee_log.
   * Using JS functions to handle the output provides handy hooks, especially 
   * when we are in curses mode, etc.
   */

  if (isError == (int)gErrFile)
    funName = (char *)"errorPrint";
  else
    funName = (char *)"debugPrint";

  v = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, buf));

  if (obj && JS_CallFunctionName(cx, obj, funName, 1, &v, &v) == JS_TRUE)
    return;

  if (isError && isatty(STDERR_FILENO))
    fputs(buf, stderr);
  else if (isatty(STDOUT_FILENO))
    fputs(buf, stdout);
  else
    gpsee_log(isError ? SLOG_NOTICE : SLOG_INFO, "%s", buf);

  return;
}

/** Scan the redirection caches for data that needs printing. */
static void capture_scanRedirCaches(JSContext *cx, JSObject *obj)
{
  char 	buf[256];

  GPSEE_ASSERT(cx && obj);

  if (ftell(gErrFile))
  {
    rewind(gErrFile);
    while (!ferror(gErrFile) && !fgets(buf, sizeof(buf), gErrFile))
      capture_outputString(cx, obj, 1, buf);
    rewind(gErrFile);
    ftruncate(fileno(gErrFile), 0);
  }

  if (ftell(gOutFile))
  {
    rewind(gOutFile);
    while (!ferror(gOutFile) && fgets(buf, sizeof(buf), gOutFile))
      capture_outputString(cx, obj, 1, buf);
    rewind(gOutFile);
    ftruncate(fileno(gOutFile), 0);
  }

  return;
}

/** Format captured output for printing, and determine best "this" object to use
 *  for locating JavaScript output functions
 */
static void capture_printf(JSContext *cx, JSObject *obj, jsval *vp, FILE *whatFile, const char *fmt, ...) 
{
#if defined(HAVE_APR)
# warning "Implementation with apr_psprintf() would be far superior"
#endif
  int		bufSize = 1;
  int		charsPrinted = 0;
  char 		*buf = NULL;
  JSObject	*thisObj;
  va_list	ap;

  if ((whatFile != gOutFile) && (whatFile != gErrFile))
  {
    /* odd - js.cpp must have new file-related features? */
    va_start(ap, fmt);
    vfprintf(whatFile, fmt, ap);
    va_end(ap);

    return;
  }

  GPSEE_ASSERT(cx);
  if (!cx || (!obj && !vp))
    return;

  if (!obj)
  {
    /*  bogus?: assume this is a fast native */
    thisObj = reinterpret_cast<JSObject *>(JS_THIS_OBJECT(cx, vp));
  }
  else
    thisObj = obj;

  GPSEE_ASSERT(thisObj);
  capture_scanRedirCaches(cx, thisObj);

  do
  {
    if (charsPrinted > bufSize)
      bufSize = charsPrinted + 1;
    else
      bufSize = bufSize * 2;

    
    buf = (char *)JS_realloc(cx, buf, bufSize);
    if (!buf)
      panic(MODULE_ID ".capture_printf: Out of memory allocating temporary buffer");
    
    va_start(ap, fmt);
    charsPrinted = vsnprintf(buf, bufSize, fmt, ap);
    va_end(ap);

  } while(((charsPrinted == -1) || (charsPrinted >= bufSize)) && 
	  GPSEE_DEBUG_MODULE_MAX_OUTPUT_BUFFER_SIZE >= bufSize);

  capture_outputString(cx, thisObj, whatFile == gErrFile, buf);

  JS_free(cx, buf);

  return;
}

/** Create a temp file for use by the redirection cache */
static FILE *capture_redirTempFile(FILE *which)
{
  char		tmpfn[L_tmpnam];
  FILE		*tmpFile;
  int		fd;

  fd = open(tmpnam(tmpfn), O_RDWR | O_CREAT | O_EXCL | O_NONBLOCK, 0600);	
  if ((fd == -1) && (errno == EEXIST)) /* highly improbable tmpnam collision? Try once more then bail. */
  {
    sleep(0);
    fd = open(tmpnam(tmpfn), O_RDWR | O_CREAT | O_EXCL | O_NONBLOCK, 0600);	
  }

  if (fd == -1)
    return NULL;

  tmpFile = fdopen(fd, "w+");	
#if 0
  tmpFile = fopen(tmpnam(tmpfn), "w+");
  if (!tmpFile)
    return NULL;
#endif

  if (unlink(tmpfn))
    goto fail;

  return tmpFile;

  fail:
  fclose(tmpFile);
  return NULL;
}

/** Module Init function. Sets up global variables for use by js.cpp,
 *  defines methods based on the shell_functions in js.cpp, and attaches
 *  the environment variable.
 */
const char *mozshell_InitModule(JSContext *cx, JSObject *moduleObject)
{
  JSObject	*envobj;
  extern	char **environ;
  FILE		**oldStreams;

  CheckHelpMessages();
  setlocale(LC_ALL, "");

#warning "FIXME Stack base implementation changed in js.cpp"
//  gStackBase = (jsuword)gpsee_stackBase;

  if (!JS_DefineFunctions(cx, moduleObject, shell_functions))
    return NULL;

  /* useless? */
  {
    JSObject *it = JS_DefineObject(cx, moduleObject, "it", &its_class, NULL, 0);
    if (!it || !JS_DefineProperties(cx, it, its_props) || !JS_DefineFunctions(cx, it, its_methods))
      return NULL;
  }

  envobj = JS_DefineObject(cx, moduleObject, "environment", &env_class, NULL, 0);
  if (!envobj || !JS_SetPrivate(cx, envobj, environ))
    return NULL;

  oldStreams = static_cast<FILE **>(JS_malloc(cx, sizeof(oldStreams[0]) * 2));
  if (!oldStreams)
    return NULL;

  oldStreams[0] = stdout;
  oldStreams[1] = stderr;
  JS_SetPrivate(cx, moduleObject, oldStreams);
		
  stdout = gOutFile = capture_redirTempFile(stdout);
  stderr = gErrFile = capture_redirTempFile(stderr);

#if defined(JS_TRACER) && defined(GPSEE_BUILD_DEBUG)
extern struct JSClass jitstats_class;
extern void js_InitJITStatsClass(JSContext *cx, JSObject *glob);
            js_InitJITStatsClass(cx, JS_GetGlobalObject(cx));
            JS_DefineObject(cx, JS_GetGlobalObject(cx), "tracemonkey",
                            &jitstats_class, NULL, 0);
#endif

  return MODULE_ID;
}

/** Module Fini function. Restores stdout/stderr and flushes the redir caches */
JSBool MozShell_FiniModule(JSContext *cx, JSObject *moduleObject)
{
  FILE **oldStreams = static_cast<FILE **>(JS_GetPrivate(cx, moduleObject));

  stdout = oldStreams[0];
  stderr = oldStreams[1];
  
  capture_scanRedirCaches(cx, moduleObject);
  return JS_TRUE;
}

