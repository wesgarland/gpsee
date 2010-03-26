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
 * Copyright (c) 2007-2010, PageMail, Inc. All Rights Reserved.
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
 *   @file	mk_std_flags.c		Turn standards-indicators from gpsee_config.h
 *                                      into standard-selecting CPPFLAGS. This cannot
 *					be done inside a regular header file, as the
 *					headers specified by the gcc -include directive 
 *					would get the wrong defines, potentially 
 *                                      poisoning further includes.
 *   @author	Wes Garland
 *   @date	March 2010
 *   @version	$Id: mk_std_cppflags.c,v 1.2 2010/03/26 00:19:32 wes Exp $
 */

#include "gpsee_config.h"

#if defined(GPSEE_STD_SUSV3)
# define _XOPEN_SOURCE 600
# if !defined(_POSIX_SOURCE)
#  define _POSIX_SOURCE
#  define _POSIX_C_SOURCE 200112L
# endif
#endif

#if defined(GPSEE_STD_SUSV2)
# define _XOPEN_SOURCE 500
# if !defined(_POSIX_SOURCE)
#  define _POSIX_SOURCE
#  define _POSIX_C_SOURCE 200112L
# endif
#endif

#if defined(GPSEE_STD_P92)
# define _POSIX_SOURCE
# define _POSIX_C_SOURCE 2
#endif

#if defined(GPSEE_STD_U95)
# define _POSIX_SOURCE
# define _POSIX_C_SOURCE 199506L
#endif

#if defined(GPSEE_STD_SUSV3)
# define GPSEE_STD_C99
#endif
#if defined(GPSEE_STD_C99)
# define _ISOC99_SOURCE
# define _ISOC9X_SOURCE
#endif

#if defined(GPSEE_STD_SVID)
# define _SVID_SOURCE
#endif

#if defined(GPSEE_STD_BSD)
# define _BSD_SOURCE
#endif

/* Pull feature-detection code in with unistd / stdlib */
#include <unistd.h>
#include <stdlib.h>

/* Pull in stdio.h for puts */
#include <stdio.h>

/* Enable POSIX realtime and thread extensions if OS standard supports them */
#if defined(_POSIX_SOURCE) && (_POSIX_C_SOURCE >= 199309L)
# define GPSEE_STDEXT_POSIX_REALTIME
#endif
#if defined(_POSIX_SOURCE) && (_POSIX_C_SOURCE >= 199506L)
# define GPSEE_STDEXT_POSIX_THREADS
# if !defined(_REENTRANT)
#  define _REENTRANT
# endif
#endif

/* Sanity checking */
#if !defined(GPSEE_STD_P92) && !defined(GPSEE_STD_U95) && !defined(GPSEE_STD_SUSV3) && !defined(GPSEE_STD_C99) && !defined(GPSEE_STD_SVID) && !defined(GPSEE_STD_BSD)
# warning "Could not figure out what kind of C standard your platform supports"
#endif

#define xstr(s) str(s)
#define str(s) #s

int main(int argc, const char *argv[])
{
  while(++argv,--argc)
    printf("%s ", *argv);

  puts(
#ifdef _REENTRANT
	" -D_REENTRANT"
#endif

#ifdef _XOPEN_SOURCE
	" -D_XOPEN_SOURCE=" xstr(_XOPEN_SOURCE)
#endif

#ifdef _POSIX_SOURCE
	" -D_POSIX_SOURCE"
	" -D_POSIX_C_SOURCE=" xstr(_POSIX_C_SOURCE)
#endif

#ifdef _ISOC99_SOURCE
	" -D_ISOC99_SOURCE"
#endif

#ifdef _ISOC9X_SOURCE
	" -D_ISOC9X_SOURCE"
#endif

#ifdef _SVID_SOURCE
	" -D__SOURCE"
#endif

#ifdef _BSD_SOURCE
	" -D__SOURCE"
#endif

#ifdef _GNU_SOURCE
	" -D__SOURCE"
#endif

#ifdef __EXTENSIONS__
	" -D__EXTENSIONS__=" xstr(__EXTENSIONS__)
#endif
  	"");

  return 0;
}
