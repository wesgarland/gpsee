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
 *  @file	gpsee_flock.c		Portable advisory file-locking for GPSEE's internal use.
 *  @author	Wes Garland, PageMail, Inc., wes@page.ca
 *  @version	$Id: gpsee_flock.c,v 1.3 2009/07/31 16:46:04 wes Exp $
 *  @date	March 2009
 */

static const char __attribute__((unused)) rcsid[]="$Id: gpsee_flock.c,v 1.3 2009/07/31 16:46:04 wes Exp $:";

#include <gpsee.h>
#if !defined(HAVE_FLOCK) && !defined(NO_GPSEE_FLOCK)

/** Implement advisory file lock with fcntl using flock semantics.
 *
 * @bug	errno codes match fcntl(), not flock(). They're
 *  	not portable anyhow, so more information probably
 *  	won't hurt.
 *
 * @bug	errno will probably never be set to EAWOULDBLOCK
 *  	under Solaris, because I can't figure out how to tell
 *  	whether or not failed because of blocking I/O or
 *  	something else. There does not appear to be an appropriate
 *  	errno return. That's okay, though, because nobody should
 *  	care *why* a non-blocking lock failed, just that it did.
 */
int gpsee_flock(int fd, int lockmode)
{
  int          cmd,
               result;
  struct flock fl;
  
  if (lockmode & GPSEE_LOCK_NB)
    cmd=F_SETLK;
  else
    cmd=F_SETLKW;
 
  memset(&fl, 0, sizeof(fl));

  switch(lockmode & ~GPSEE_LOCK_NB)
  {
    case GPSEE_LOCK_SH:
      fl.l_type = F_RDLCK;
      break;
    case GPSEE_LOCK_EX:
      fl.l_type = F_WRLCK;
      break;      
    case GPSEE_LOCK_UN:
      fl.l_type = F_UNLCK;
      break;      
 }

  fl.l_whence=SEEK_SET;

  result=fcntl(fd, cmd, &fl);
  return(result);
}
#endif





