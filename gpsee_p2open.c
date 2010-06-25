/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at licenses/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at licenses/OPENSOLARIS.LICENSE.
 *
 * Portions Copyright 2009 Page Mail, Inc.
 *
 * CDDL HEADER END
 */

/**
 *  @file       gpsee_p2open.c
 *  @brief      An cross-platform implementation of p2open for GPSEE. Based on the p2open implementation
 *              from the SunOS 5.11 Operating System  ("OpenSolaris").
 *  @author     AT&T, Sun Microsystems, Donny Viszneki <hdon@page.ca>
 */

/*
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

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

/**
 *  Open (fork/exec) a process for read/write, yielding file descriptor pair.
 *
 *  @param      cmd     The shell command used to launch the process
 *  @param      FDs     [out]   An array of [read,write]able file descriptors
 *  @param      pid     [out]   The process ID of the process launched
 * 
 *  @returns    0 on success, -1 on failure, setting errno
 */
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

/**
 *  Close a file descriptor pair, and associated FILE streams, resulting from a call to gpsee_p2open().
 * 
 *  @param      fdp             The array of file descriptors returned from gpsee_p2open()
 *  @param      tocmd           A pointer to a FILE stream fdopen()ed from fds[1] or NULL
 *  @param      fromcmd         A pointer to a FILE stream fdopen()ed from fds[0] or NULL
 *  @param      kill_sig        The signal to use to kill the process. Passing zero means we wait for it to exit on its own.
 *  @param      pid             The pid from gpsee_p2open()
 *
 *  @returns    0 on success, -1 on failure, setting errno
 */
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
