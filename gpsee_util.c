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
 *  @file	gpsee_util.c	General utility functions which have nothing
 *				to do with GPSEE other than it uses them.
 *  @author	Wes Garland, PageMail, Inc., wes@page.ca
 *  @version	$Id: gpsee_util.c,v 1.8 2010/02/13 20:32:29 wes Exp $
 *  @date	March 2009
 */

static const char __attribute__((unused)) rcsid[]="$Id: gpsee_util.c,v 1.8 2010/02/13 20:32:29 wes Exp $:";

#include "gpsee.h"

/* ctype.h provides isspace() used by gpsee_reportErrorSourceCode()
 */
#include <ctype.h>

/** String copy function which insures NUL-terminated
 *  dst string and does not zero-pad. Works only on
 *  ASCIZ strings.
 *
 *  @param	dst		Memory to copy to, must be valid
 *  @param	src		ASCIZ-String to copy from
 *  @param	dst_size	Size of dst buffer.
 *  @returns	pointer to dst's terminating NUL char
 */
char *gpsee_cpystrn(char *dst, const char *src, size_t dst_size)
{
  if (dst_size)
  {
    while (dst_size-- && *src)
      *dst++ = *src++;
    *dst = (char)0;
  }

  return dst;
}

/** String concatenation function.
 * 
 *  Appends src to dst, without overrunning dst's buffer and
 *  guaranteeing NUL termination.
 *
 *  @param dst		The string to append to
 *  @param src		The string to append
 *  @param dst_size	The size of dst's memory allocation
 *  @returns		The number of unused bytes in dst, excluding the NLU
 */
size_t gpsee_catstrn(char *dst, const char *src, size_t dst_size)
{
  char *s;

  if (!dst_size)
    return 0;

  for (s = dst; *s; s++);
  dst_size -= (s - dst);
  dst = s;

  s = gpsee_cpystrn(dst, src, dst_size);
  dst_size -= (s - dst);

  return dst_size;
}

/** Returns a pointer to the filename portion of the passed argument.
 *  @param	fullpath	Path to examine
 *  @returns	Pointer inside fullpath to the base filename.
 */
const char *gpsee_basename(const char *fullpath)
{
  const char *slash = strrchr(fullpath, '/');

  if (!slash)
    return fullpath;

  if (!slash[1])
    return "/";

  return slash + 1;
}

/** Returns the directory name component of the passed argument.
 *  If the fullpath does not contain any slashes, it is assumed that
 *  it is in the current directory, and "." is returned.
 *
 *  @param	fullpath	Path to examine
 *  @param	buf		Buffer in which to copy by the name
 *  @param	bufLen		Number of bytes allocated for buf
 *  @returns	Pointer to buf, or NULL when buf is too small.
 */
const char *gpsee_dirname(const char *fullpath, char *buf, size_t bufLen)
{
  const char *slash = strrchr(fullpath, '/');

  if (bufLen < 2)
    return NULL;

  if (!slash)
  {
    buf[0] = '.';
    buf[1] = (char)0;
    return buf;
  }

  if ((slash - fullpath) >= bufLen)
    return NULL;

  strncpy(buf, fullpath, slash - fullpath);
  buf[slash - fullpath] = (char)0;

  return buf;
}

/**
 *  Resolve all symlinks and relative directory components (. and ..) to return a fully resolved path.
 *
 *  If bufsize < PATH_MAX, malloc() is used to allocate PATH_MAX bytes during the call to this function.
 *
 *  TODO remove use of malloc()
 *
 *  @param      path      Input path string to opreate upon
 *  @param      buf       Output buffer for resolved path
 *  @param      bufsize   Size of 'buf'
 *  @returns              Length of returned path on success, or -1 on failure
 */
int gpsee_resolvepath(const char *path, char *buf, size_t bufsiz)
{
  int   ret;
#if defined(GPSEE_SUNOS_SYSTEM)
  ret = resolvepath(path, buf, bufsiz);
  buf[min(ret, bufsiz - 1)] = (char)0;

  return ret;
#else
  char  *mBuf;
  char  *rp;

  errno = 0;

  /* Require buf (this behavior is *nearly* consistent with what was here before I came'a bugfixin') */
  if (!buf)
  {
    errno = EINVAL;
    return -1;
  }

  /* realpath() requires PATH_MAX sized buffer, but we buffer the caller from that restriction */
  if (bufsiz < PATH_MAX)
  {
    mBuf = malloc(PATH_MAX);
    if (!mBuf)
    {
      errno = ENOMEM;
      return -1;
    }
  }
  else
    mBuf = buf;

  rp = realpath(path, mBuf);
  if (!rp)
  {
    if (mBuf != buf)
      free(mBuf);
    /* Pass realpath() errno upstream */
    return -1;
  }

  /* Did we buffer the caller's buffer? */
  if (mBuf != buf)
  {
    char *s = gpsee_cpystrn(buf, mBuf, bufsiz);
    if (s - buf == bufsiz) /* overrun */
    {
      /* should be unreachable, because we would have already bailed on realpath() failure */
      errno = ENAMETOOLONG;
      ret = -1;
    }
    else
      ret = s - buf;
  }
  else
    ret = strlen(buf);

  if (mBuf != buf)
    free(mBuf);

  return ret;
#endif
}


/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright 2009 Page Mail, Inc.
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

//#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * Similar to popen(3S) but with pipe to cmd's stdin and from stdout.
 */

#include <sys/types.h>
#include <libgen.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
//#include "lib_gen.h"

int gpsee_p2open(const char *cmd, int *FDs, pid_t *pid)
{
  int tocmd[2];
  int fromcmd[2];

  if (pipe(tocmd) < 0 || pipe(fromcmd) < 0)
    return (-1);
#ifndef _LP64
  if (tocmd[1] >= 256 || fromcmd[0] >= 256) {
    (void) close(tocmd[0]);
    (void) close(tocmd[1]);
    (void) close(fromcmd[0]);
    (void) close(fromcmd[1]);
    return (-1);
  }
#endif  /*      _LP64   */
  if ((*pid = fork()) == 0) {
    (void) close(tocmd[1]);
    (void) close(0);
    (void) fcntl(tocmd[0], F_DUPFD, 0);
    (void) close(tocmd[0]);
    (void) close(fromcmd[0]);
    (void) close(1);
    (void) fcntl(fromcmd[1], F_DUPFD, 1);
    (void) close(fromcmd[1]);
    (void) execl("/bin/sh", "sh", "-c", cmd, (char *)0);
    _exit(1);
  }
  if (*pid == (pid_t)-1)
    return (-1);
  (void) close(tocmd[0]);
  (void) close(fromcmd[1]);

  FDs[0] = fromcmd[0];
  FDs[1] = tocmd[1];
  return (0);
}

int gpsee_p2close(int *fdp, FILE **tocmd, FILE **fromcmd, int kill_sig, pid_t pid)
{
  int             status;
  void            (*hstat)(int), (*istat)(int), (*qstat)(int);
  pid_t r;

  /* Kill process */

  if (pid == (pid_t)-1)
    return (-1);

  if (kill_sig != 0) {
    (void) kill(pid, kill_sig);
  }

  /* Finalize stdio */

  if (tocmd && *tocmd) {
    fclose(*tocmd);
    *tocmd = NULL;
  }
  if (fromcmd && *fromcmd) {
    fclose(*fromcmd);
    *fromcmd = NULL;
  }

  /* Wait for process to exit */

  istat = signal(SIGINT, SIG_IGN);
  qstat = signal(SIGQUIT, SIG_IGN);
  hstat = signal(SIGHUP, SIG_IGN);
  while ((r = waitpid(pid, &status, 0)) == (pid_t)-1 && errno == EINTR)
    ;
  if (r == (pid_t)-1)
    status = -1;
  (void) signal(SIGINT, istat);
  (void) signal(SIGQUIT, qstat);
  (void) signal(SIGHUP, hstat);

  /* Return the child process's exit status */
  return (status);
}

/** Returns a number >= 0 if there is a currently pending exception and that exception qualifies as
 *  a "SystemExit". Currently, to exit a process using the exception throwing mechanism, one must
 *  throw an system exit code. If uncaught, this should signify to the host application the
 *  success or failure of the program (zero, of course, meaning success.) Of course any other
 *  uncaught exception will result in the program exiting with failure as well, but often it is
 *  important to indicate to the parent process that a failure has occurred but bypass any
 *  noisy output that would normally occur.
 *
 *  As only the lowest 8 bits of exit status are used in POSIX, we don't consider any number for which
 *  (n & 0377 != n) to constitute a SystemExit.
 *
 *  In the future, this facility may be more formalized. The fact that you *can* throw any value in
 *  Javascript is only a coincidence that we took advantage of for providing the prototype of this
 *  facility, but it there could possibly be someone else out there throwing numbers with a different
 *  idea in mind as to the meaning of such an action.
 *
 *  @param    cx
 *  @returns  Returns an int greater than 
 *
 */
int gpsee_getExceptionExitCode(JSContext *cx)
{
  jsval v;
  jsdouble d;
  jsint i;

  /* Retrieve exception; none has been thrown if this returns JS_FALSE */
  if (!JS_GetPendingException(cx, &v))
    return -1;

  /* Try to coerce an object to a number */
  if (!JSVAL_IS_PRIMITIVE(v))
  {
    if (!JS_ValueToNumber(cx, v, &d))
      return -1;
    goto havedouble;
  }

  /* Try to get a double, or use a double we got from the last step */
  if (JSVAL_IS_DOUBLE(v))
  {
    d = *JSVAL_TO_DOUBLE(v);
havedouble:
    if (d != (jsdouble) (i = (jsint) d))
      return -1;
    goto haveint;
  }

  /* Evaluate an int */
  if (JSVAL_IS_INT(v))
  {
    int n;
    i = JSVAL_TO_INT(v);
haveint:
    n = (int) i; // not that this should ever matter...
    if ((n & 255) == n)
      return n;
  }

  return -1;
}

/** An error reporter used by gpsee_reportUncaughtException() because only an "error reporter" (see JS_SetErrorReporter()
 *  gets access to the script source that generated the error.
 */
static void gpsee_reportErrorSourceCode(JSContext *cx, const char *message, JSErrorReport *report)
{
  char prefix[strlen(report->filename) + 21]; /* Allocate enough room for "filename:lineno" */
  gpsee_interpreter_t *jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  size_t sz;
  
  sz = snprintf(prefix, sizeof(prefix), "%s:%d", report->filename, report->lineno);
  GPSEE_ASSERT(sz < sizeof(prefix));

  if (jsi->pendingErrorMessage)
  {
    fprintf(stderr, "%s: %s\n", prefix, jsi->pendingErrorMessage);
  }

  if (report->linebuf)
  {
    size_t start, len;
    const char *c = report->linebuf;

    start = 0;
    while (isspace(*(c++)))
      start++;

    len = 0;
    while (*(c))
    {
      if (!isspace(*c))
        len = (c - report->linebuf) - start;
      c++;
    }

    if (len > 0)
    {
      char linebuf[len+1];
      char *d = linebuf;
      int i;

      strncpy(linebuf, report->linebuf + start, len+1);
      fprintf(stderr, "%s: %s\n", prefix, linebuf);
      fprintf(stderr, "%s: ", prefix);
      for (i = report->tokenptr - report->linebuf - start; i; i--)
        fputc('.', stderr);
      fputs("^\n", stderr);
    }
    else
    {
      fprintf(stderr, "%s: %s\n", prefix, report->linebuf);
    }
  }
}

/** Report any pending exception as though it were uncaught.
 *  Renders a nice-looking error report to stderr.
 *
 *  @param    cx
 *  @param    exval     Exception value to be used (typically from JS_GetPendingException()) or JSVAL_NULL
 *                      to grab the exception value for you from JS_GetPendingException().
 * 
 *  @todo Should there be an option argument for publishing to gpsee_log()?
 */
JSBool gpsee_reportUncaughtException(JSContext *cx, jsval exval)
{
  gpsee_interpreter_t *jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  jsval                v;
  const char *longerror = NULL;
  JSString *jsstr;
  JSErrorReporter reporter;

  /* Must we look up the exception value ourselves? */
  if (exval == JSVAL_NULL)
  {
    if (!JS_GetPendingException(cx, &exval))
      /* no exception pending! */
      return JS_FALSE;
  }

  /* We'll be trying to output one of two sets of data. The first set is error.message, followed by error.stack.
   * The second set is just an attempt to stringify the exception value to a string.
   */

  /* Is exval an Object? */
  if (!JSVAL_IS_PRIMITIVE(exval))
  {
    /* Attempt to retrieve "message" property from exception object */
    if (JS_GetProperty(cx, JSVAL_TO_OBJECT(exval), "message", &v))
    {
      const char *error;
      if (JSVAL_IS_STRING(v))
      {
        /* Make char buffer from JSString* */
        error = JS_GetStringBytes(JSVAL_TO_STRING(v));

        /* This makes the message available to gpsee_reportErrorSourceCode() */
        jsi->pendingErrorMessage = error;
      }
    }

    /* Attempt to retrieve "stack" property from exception object */
    if (JS_GetProperty(cx, JSVAL_TO_OBJECT(exval), "stack", &v))
    {
      if (JSVAL_IS_STRING(v))
      {
        /* Make char buffer from JSString* */
        longerror = JS_GetStringBytes(JSVAL_TO_STRING(v));
      }
    }
  }

  /* The 'stack' property of the exception wasn't a string, so let's try stringifying the exception value */
  if (!longerror)
  {
    /* Convert exception value to string */
    jsstr = JS_ValueToString(cx, exval);
    /* Make char buffer from JSString* */
    longerror = JS_GetStringBytes(jsstr);
    /* Check for OOM error @todo report OOM here? that sort of error better get reported SOMEWHERE */
    if (!longerror)
      return JS_FALSE;
  }

  /* Some information is only available to an "error reporter" (see JS_SetErrorReporter()) so we report that part here
   * using an error reporter. Note that JS_ReportPendingException() also calls JS_ClearPendingException().
   */
  reporter = JS_SetErrorReporter(cx, (JSErrorReporter)gpsee_reportErrorSourceCode);
  JS_ReportPendingException(cx);
  JS_SetErrorReporter(cx, reporter);
  jsi->pendingErrorMessage = NULL;

  /* Output the exception information :) There are two possible sinks */
  if (longerror && *longerror)
  {
    fprintf(stderr, "stack trace:\n%s", longerror);
    if (longerror[strlen(longerror)-1] != '\n')
      fprintf(stderr, "\n");
  }

  return JS_TRUE;
}
