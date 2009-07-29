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
 *  @file	gpsee_flock.h		Cross-platform flock header for GPSEE.
 *  @author	Wes Garland, PageMail, Inc., wes@page.ca
 *  @version	$Id: gpsee_flock.h,v 1.1 2009/03/30 23:55:43 wes Exp $
 *  @date	Feb 2009
 */
 
#ifndef HAVE_FLOCK
# define GPSEE_LOCK_SH         0x01            /* shared file lock */
# define GPSEE_LOCK_EX         0x02            /* exclusive file lock */
# define GPSEE_LOCK_NB         0x04            /* don't block when locking */
# define GPSEE_LOCK_UN         0x08            /* unlock file */

# ifdef  __cplusplus
extern "C" 
{
# endif
int gpsee_flock(int fd, int lockmode);
# ifdef __cplusplus
}
# endif
#else /* HAVE_FLOCK */
# include <sys/file.h>	/* might be flock.h under linux? */
# define gpsee_flock(fd, lockmode) flock(fd, lockmode) 
# define GPSEE_LOCK_SH LOCK_SH
# define GPSEE_LOCK_EX LOCK_EX
# define GPSEE_LOCK_NB LOCK_NB
# define GPSEE_LOCK_UN LOCK_UN
#endif








