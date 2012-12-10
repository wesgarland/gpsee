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

static __attribute__((unused)) const char rcsid[]="$Id: gpsee_unix.c,v 1.6 2011/12/05 19:13:36 wes Exp $";

/**
 *  @file 	gpsee_unix.c		Functions for UNIX world normally provided by SureLynx libs.
 *  @author	Wes Garland
 *  @date	Jan 2008
 *  @version	$Id: gpsee_unix.c,v 1.6 2011/12/05 19:13:36 wes Exp $
 */

#include "gpsee.h"

/** Format a log message, expanding %m as necessary,
 *  and returning a buffer suitable for processing
 *  printf() and friends.
 *
 *  @ingroup            netpaging_logging
 *  @see                addPercentM_Handler()
 *  @param      fmt     Format string which may have %m in it
 *  @param      fmtNew  pre-allocated buffer, GPSEE_MAX_LOG_MESSAGE_SIZE bytes long.
 *  @returns            Format string with %m pre-expanded, at address either fmt or fmtNew
 */  

const char *gpsee_makeLogFormat(const char *fmt, char *fmtNew)
{
  char          *s;
 
  if ((s=strstr(fmt, "%m")))
  {
    size_t              amtToCopy;
 
    amtToCopy = min((size_t)GPSEE_MAX_LOG_MESSAGE_SIZE, (size_t)(s - fmt));
 
    memcpy(fmtNew, fmt, amtToCopy);
    if (amtToCopy < GPSEE_MAX_LOG_MESSAGE_SIZE)
    {
      int error_number = errno;
      strncpy(fmtNew + amtToCopy, strerror(error_number), GPSEE_MAX_LOG_MESSAGE_SIZE - amtToCopy);
      errno = error_number;
 
      amtToCopy = GPSEE_MAX_LOG_MESSAGE_SIZE - strlen(fmtNew);
      strncat(fmtNew, s + 2, amtToCopy);
    }
 
    return fmtNew;
  }
 
  return fmt;
}

void gpsee_log(JSContext *cx, unsigned int extra, signed int pri, const char *fmt, ...)
{
  va_list	ap;
  int 		printToStderr;
  char		buf[GPSEE_MAX_LOG_MESSAGE_SIZE];
  int           n;

  if (extra & 1) /* 1==suppress TTY output */
  {
    printToStderr = 0;
  }
  else
  {
    switch(gpsee_verbosity(0))
    {
      case 0:
	printToStderr = 0;
	break;
      case 1:
	if ((pri == LOG_DEBUG) || (pri == LOG_INFO))
	  printToStderr = 0;
	else
	  printToStderr = 1;
	break;
      case 2:
	if (pri == LOG_DEBUG)
	  printToStderr = 0;
	else
	  printToStderr = 1;
	break;
      default:
	printToStderr = 1;
	break;
    }
  }

  va_start(ap, fmt);
  n = vsnprintf(buf, GPSEE_MAX_LOG_MESSAGE_SIZE, fmt, ap);
  va_end(ap);

  if (n >= GPSEE_MAX_LOG_MESSAGE_SIZE)
  {
    buf[GPSEE_MAX_LOG_MESSAGE_SIZE-3] = '.';
    buf[GPSEE_MAX_LOG_MESSAGE_SIZE-2] = '.';
    buf[GPSEE_MAX_LOG_MESSAGE_SIZE-1] = '.';
    buf[GPSEE_MAX_LOG_MESSAGE_SIZE-0] = '\0';
  }

  if (cx && printToStderr)
  {

    gpsee_fputs(cx, " *** ", stderr);
    gpsee_fputs(cx, buf,     stderr);
    gpsee_fputc(cx, '\n',    stderr);
  }

  syslog(pri, buf, n);

  return;
}

/**
 *  Runtime Configuration for GPSEE applications.
 *  GPSEE was originally developed using PageMail's
 *  proprietary SmartRC library for runtime configuration.
 *  Rather than remove tunables from this distribution, we 
 *  have provided reasonable defaults and these functions 
 *  to act as shims to whatever configuration paradigm you
 *  prefer.
 */

cfgHnd cfg;

/** Return a value associated with a key.
 *  @param	cfg	Runtime Configuration handle
 *  @param	key	The configuration parameter to look up
 *  @returns	The value, or NULL if not found
 */
const char *cfg_value(cfgHnd cfg, const char *key)
{
  return NULL;
}

/** Return a value associated with a key.
 *  @param	cfg		Runtime Configuration handle
 *  @param	key		The configuration parameter to look up
 *  @param	defaultValue	Application-supplied default
 *  @returns	The value, or defaultValue if not found.
 */
const char *cfg_default_value(cfgHnd cfg, const char *key, const char *defaultValue)
{
  return defaultValue;
}

/** Return a value, interpreted as boolean, associated with a key.
 *  @param	cfg		Runtime Configuration handle
 *  @param	key		The configuration parameter to look up
 *  @returns	cfg_true if the value is true, cfg_false is the value is false, 
 *              or cfg_undef if the value is not found.
 */
cfg_bool cfg_bool_value(cfgHnd cfg, const char *key)
{
  return cfg_undef;
}

/** Open a runtime configuration file based on the calling program's arguments */
cfgFILE *cfg_openfile(int argc, char * const argv[])
{
  return (cfgFILE *)1;	/* 1 != NULL */
}

/** Close a runtime configuration file */
int cfg_close(cfgFILE *cfgFile)
{
  return 0;
}

/** Read a runtime configuration file */
cfgHnd cfg_readfile(cfgFILE *cfgFile)
{
  return NULL;
}

