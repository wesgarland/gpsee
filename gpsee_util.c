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
 *  @version	$Id: gpsee_util.c,v 1.5 2009/05/27 04:51:44 wes Exp $
 *  @date	March 2009
 */

static const char __attribute__((unused)) rcsid[]="$Id: gpsee_util.c,v 1.5 2009/05/27 04:51:44 wes Exp $:";

#include "gpsee.h"

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
#if defined(GPSEE_SUNOS_SYSTEM)
  return resolvepath(path, buf, bufsiz);
#else
  char  *mBuf;
  char  *rp;
  int   ret;

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

/* functions in libc */
//extern int _insert(pid_t pid, int fd);
//extern pid_t _delete(int fd);
int __gpsee_p2open(const char *cmd, int fds[2]);
int __gpese_p2close(int *fdp, FILE **fpp, int kill_sig);

int
gpsee_p2open(const char *cmd, FILE *fp[2])
{
	int	fds[2];

	if (__gpsee_p2open(cmd, fds) == -1)
		return (-1);

	fp[0] = fdopen(fds[0], "w");
	fp[1] = fdopen(fds[1], "r");
	return (0);
}

int
gpsee_p2close(FILE *fp[2])
{
	return (__gpsee_p2close(NULL, fp, 0));
}

int
__gpsee_p2open(const char *cmd, int fds[2])
{
	int	tocmd[2];
	int	fromcmd[2];
	pid_t	pid;

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
#endif	/*	_LP64	*/
	if ((pid = fork()) == 0) {
		(void) close(tocmd[1]);
		(void) close(0);
		(void) fcntl(tocmd[0], F_DUPFD, 0);
		(void) close(tocmd[0]);
		(void) close(fromcmd[0]);
		(void) close(1);
		(void) fcntl(fromcmd[1], F_DUPFD, 1);
		(void) close(fromcmd[1]);
		(void) execl("/bin/sh", "sh", "-c", cmd, 0);
		_exit(1);
	}
	if (pid == (pid_t)-1)
		return (-1);
	//(void) _insert(pid, tocmd[1]);
	//(void) _insert(pid, fromcmd[0]);
	(void) close(tocmd[0]);
	(void) close(fromcmd[1]);
	fds[0] = tocmd[1];
	fds[1] = fromcmd[0];
	return (0);
}

int
__gpsee_p2close(int *fdp, FILE **fpp, int kill_sig)
{
	int		fds[2];
	int		status;
	void		(*hstat)(int), (*istat)(int), (*qstat)(int);
	pid_t pid, r;

	if (fdp != NULL) {
		fds[0] = fdp[0];
		fds[1] = fdp[1];
	} else if (fpp != NULL) {
		fds[0] = fileno(fpp[0]);
		fds[1] = fileno(fpp[1]);
	} else {
		return (-1);
	}

	//pid = _delete(fds[0]);
	//if (pid != _delete(fds[1]))
		//return (-1);

	if (pid == (pid_t)-1)
		return (-1);

	if (kill_sig != 0) {
		(void) kill(pid, kill_sig);
	}

	if (fdp != NULL) {
		(void) close(fds[0]);
		(void) close(fds[1]);
	} else {
		(void) fclose(fpp[0]);
		(void) fclose(fpp[1]);
	}

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
	return (status);
}
