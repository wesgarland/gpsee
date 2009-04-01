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
 *  @version	$Id: gpsee_util.c,v 1.1 2009/03/30 23:55:43 wes Exp $
 *  @date	March 2009
 */

static const char __attribute__((unused)) rcsid[]="$Id: gpsee_util.c,v 1.1 2009/03/30 23:55:43 wes Exp $:";

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
    while (dst_size-- && (*dst++ = *src++));
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
  }

  if ((slash - fullpath) >= bufLen)
    return NULL;

  strncpy(buf, fullpath, slash - fullpath);
  buf[slash - fullpath] = (char)0;

  return buf;
}

int gpsee_resolvepath(const char *path, char *buf, size_t bufsiz)
{
#if defined(GPSEE_SUNOS_SYSTEM)
  return resolvepath(path, buf, bufsiz);
#else
  char  *mBuf;
  char  *rp;
  int   ret;

  errno = 0;

  if (bufsiz < PATH_MAX)
    mBuf = malloc(PATH_MAX);
  else
    mBuf = buf;

  if (!mBuf)
  {
    if (!errno)
      errno = EINVAL;
    return -1;
  }

  rp = realpath(path, mBuf);
  if (rp != buf)
  {
    char *s = gpsee_cpystrn(buf, rp, bufsiz);
    if (s - buf == bufsiz) /* overrun */
    {
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
