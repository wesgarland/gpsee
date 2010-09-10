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
 * Copyright (c) 2006-2009, PageMail, Inc. All Rights Reserved.
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
 *  @file	phpsess.c	PHP Session library. A method for C programmers to access
 *				PHP session data.
 *
 *  @author	Wes Garland
 *  @date	Apr 2006	
 *  @version	$Id: phpsess.c,v 1.2 2009/08/06 14:22:58 wes Exp $
 *
 *  @note	Determined primarily by reverse engineering. May have some
 *		unexpected consequences/bugs...  A full read of the PHP C source
 *		is really warranted. Testing against PHP 5.0.4.
 *
 *  @note	We only support string and integer types, not dates (yet).
 */

/*
 * $Log: phpsess.c,v $
 * Revision 1.2  2009/08/06 14:22:58  wes
 * Updated non-SureLynx phpsess.c to compile in APR environment
 *
 * Revision 1.1  2009/03/31 15:12:14  wes
 * Updated module name case, patched to build UNIX stream with local cgihtml and PHPSession only in apr stream
 *
 * Revision 1.5  2008/10/11 01:53:25  wes
 * Updated for apr_surelynx.c 1.6 logging changes
 *
 * Revision 1.4  2007/01/25 16:53:21  wes
 *  - Improved logging
 *
 * Revision 1.3  2007/01/25 16:36:24  wes
 *  - Added RCS ident keyword
 *
 * Revision 1.2  2007/01/25 16:35:29  wes
 *  - Modified session-locking to be more aggressive during non-blocking
 *    phase, including a sleep timeout (rc.php_session_lock_sleep) before
 *    going into blocking-lock mode
 *  - Added ability to not go into block-lock mode (rc.block_for_php_session_locks)
 *  - Added ability to touch session files, to get PHP to increase session lifetime
 *    as the result of our reads. Off by default, on by rc.touch_php_session_files.
 *
 * Revision 1.1  2007/01/25 15:47:30  wes
 * Initial Revision (stable)
 *
 */

/*
 * s = string
 * i = integer
 * d = date
 * N = NULL
 */

#include <gpsee.h>
#include "phpsess.h"
#include <sys/mman.h>

#if !defined TEST_DRIVER
extern
#else
static
#endif
rc_list rc;

const char rcsid[]="$Id: phpsess.c,v 1.2 2009/08/06 14:22:58 wes Exp $";

/** Return the best ASCIZ string version of a phpSession variable that we can.
 *  @param	*pool		Pool for allocating return value (only if needed)
 *  @param	stringName	Name of the string to find in the session file
 *  @param	phpSession	Handle from phpSession_load()
 *
 *  @note	There is no way to differentiate between NULL and NOT FOUND conditions.
 */
const char *phpSession_getString(apr_pool_t *pool, apr_hash_t *phpSession, const char *stringName)
{
  php_value_t *value_p = apr_hash_get(phpSession, stringName, APR_HASH_KEY_STRING);

  if (!value_p)
    return NULL;

  if (value_p->type == php_integer)
    return apr_psprintf(pool, "%i", value_p->data.integer);

  return value_p->data.string;
}

/** Load a PHP Session from disk, and store a copy of it in an APR hash table.
 *  
 *  @param	pool		Pool for allocating session storage (hash table and data)
 *  @param	tempPool	Pool for very temporary storage (e.g. session file descriptor)
 *  @param	sessionID	The PHP SessionID
 *  @param	session_p	[out]	The hash table full of PHP variables
 *
 *  @note	Parse errors WILL NOT make this function fail. This is because, due to the
 *		absense of documentation, we believe it may have trouble parsing the odd
 *		exotic setting. This means that testing needs to be very coverage-oriented,
 *		and log files should be monitored. (Bad parse info is logged at SLOG_NOTICE).
 *
 *  @note	rc.php_session_dir stores the location of the session directory. 
 *
 *  @returns	APR_SUCCESS On success (see qualifiers)
 */
apr_status_t	phpSession_load(apr_pool_t *pool, apr_pool_t *tempPool, 
				const char *sessionID, apr_hash_t **phpSession_p)
{
  apr_hash_t	*session;
  apr_file_t	*sessionFile;
  apr_mmap_t	*sessionImage;
  const char	*sessionFilename = apr_pstrcat(tempPool, 
					       rc_default_value(rc, "php_session_dir", "/var/www/sessions"),
					       "/sess_", sessionID, NULL);
  apr_status_t	status;
  char		errbuf[128];
  apr_finfo_t	finfo;

  /* For use by parsing loop: */
  char		*s;
  int		integrous;
  char		*varname;
  char		*pipeSymbol;
  char		*endOfImage;
  char		*dataString; /* Shadows value.data.string */
  size_t	stringLen;
  php_value_t	*value_p;
  int		count = 0;
  extern rc_list rc;

  *phpSession_p = NULL;

  session = apr_hash_make(pool);

  status = apr_file_open(&sessionFile, sessionFilename, APR_READ, 0, tempPool);
  if (status != APR_SUCCESS)
  {
    apr_surelynx_log(SLOG_ERR, "PHP Session Parse: Unable to open session file '%s' (%s)!", 
	sessionFilename, apr_strerror(status, errbuf, sizeof(errbuf)));
    return status;
  }

  if (rc_bool_value(rc, "lock_php_session_files") != rc_false)
  {
    status = apr_file_lock(sessionFile, APR_FLOCK_SHARED | APR_FLOCK_NONBLOCK);
    if (status != APR_SUCCESS)
    {
      apr_sleep(0);
      status = apr_file_lock(sessionFile, APR_FLOCK_SHARED | APR_FLOCK_NONBLOCK);
    }
    
    if (status != APR_SUCCESS)
    {
      apr_surelynx_log(SLOG_NOTICE, "PHP Session Parser: Sleeping for lock; session %s", sessionID);
      apr_sleep(atof(rc_default_value(rc, "php_session_lock_sleep", "1.0")) * APR_USEC_PER_SEC);
      status = apr_file_lock(sessionFile, APR_FLOCK_SHARED | APR_FLOCK_NONBLOCK); 
    }

    if ((status != APR_SUCCESS) && (rc_bool_value(rc, "block_for_php_session_locks") != rc_false))
    {
      if (status != APR_SUCCESS)
      {
	apr_surelynx_log(SLOG_NOTICE, "PHP Session Parser: Blocking for lock; session %s", sessionID);
	status = apr_file_lock(sessionFile, APR_FLOCK_SHARED);
	apr_surelynx_log(SLOG_INFO, "PHP Session Parser: Got blocked lock; session %s", sessionID);
      }
    }
  }

  if (status != APR_SUCCESS)
  {
    apr_surelynx_log(SLOG_NOTICE, "PHP Session Parser: Unable to acquire lock for session %s (%s)!", 
	sessionID, apr_strerror(status, errbuf, sizeof(errbuf)));
    apr_file_close(sessionFile);
    return status;
  }

  status = apr_file_info_get(&finfo, APR_FINFO_SIZE, sessionFile);
  if (status == APR_SUCCESS)
  {
    if (finfo.size)
      status = apr_mmap_create(&sessionImage, sessionFile, 0, finfo.size, APR_MMAP_READ, tempPool);
  }

  if ((status != APR_SUCCESS) || (sessionImage->mm == NULL) || (sessionImage->mm == MAP_FAILED))
  {
    apr_surelynx_log(SLOG_NOTICE, "PHP Session Parser: Unable to mmap session file! (%s)", apr_strerror(status, errbuf, sizeof(errbuf)));
    apr_file_close(sessionFile);
    return status;
  }

  apr_surelynx_log(SLOG_DEBUG, "PHP Session Parser: Mapped %" APR_OFF_T_FMT "-byte file", finfo.size);

  for (s = sessionImage->mm, endOfImage = ((char *)sessionImage->mm + finfo.size), integrous = 1;
       (s < endOfImage) && (s != NULL);
       integrous = 1 /* inside propels s */)
  {
    varname = s;
    pipeSymbol = strchr(s, '|');
    if (!pipeSymbol)
    {
      apr_surelynx_log(SLOG_NOTICE, "PHP Session Parser: Parse Error -- Unable to find end of variable name '%20s'", 
	  log_string(varname, endOfImage - s));
      break;
    }

    /* Parse out the variable name */
    varname = apr_palloc(pool, (pipeSymbol - s) + 1);
    strncpy(varname, s, pipeSymbol - s);
    varname[pipeSymbol - s] = (char)0;

    value_p = apr_pcalloc(pool, sizeof(*value_p));
    value_p->type = pipeSymbol[1];

    /* s points to start of variable name on the way into this switch, 
     * and pipeSymbol points to the end of the variable name + 1.
     * Once we exit the switch, we expect either integrous==0 (parse error),
     * or s to be pointing at the semicolon one byte before the start of
     * the next variable name.
     */
    switch(value_p->type)
    {
      case php_date:
	apr_surelynx_log(SLOG_NOTICE, "PHP Session Parser: Warning -- Date types are not supported!");
	integrous = 0;
      case php_string:
	if (pipeSymbol[2] != ':')
	{
	  apr_surelynx_log(SLOG_NOTICE, "PHP Session Parser: Parse Error -- Unable to find length of string '%s'!", varname);
	  integrous = 0;
	  break; /* switch */
	}

	stringLen = atoi(pipeSymbol + 3);
	s = strchr(pipeSymbol + 3, ':');

	if ((s == NULL) || (s[1] != '"'))
	{
	  apr_surelynx_log(SLOG_NOTICE, "PHP Session Parser: Parse Error -- Unable to find value for string '%s'!", varname);
	  integrous = 0;
	  break; /* switch */
	}

	value_p->data.string = dataString = apr_palloc(pool, stringLen + 1);
	strncpy(dataString, s + 2 /* :" */, stringLen);
	dataString[stringLen] = (char)0;
	
	if (strncmp(s + 2 + stringLen, "\";", 2) != 0)
	{
	  apr_surelynx_log(SLOG_NOTICE, "PHP Session Parser: Parse Error -- Unable to find end of value for string '%s'!", varname);
	  integrous = 0;
	  break; /* switch */
	}
	
	s = s + 3 + stringLen; /* points to terminating semicolon */

	if (integrous && (value_p->type == php_date))
	{
	  /* strptime should be called here */
	  value_p->data.date.aprTime = 0;
	}
	break;
      case php_integer:
	value_p->data.integer = atoi(pipeSymbol + 2);
	s = strchr(pipeSymbol + 2, ';');
	break;
      case php_null:
	s = strchr(pipeSymbol, ';');
	if (s)
	  s--;
	break;
      default:
	apr_surelynx_log(SLOG_NOTICE, "PHP Session Parser: Unknown variable type '%c'!", value_p->type);
	integrous = 0;
	break;
    }

    if (s != NULL)
    {
      if (*s != ';')
	integrous = 0;
      else
	s++;
    }

    if (!integrous && (s != NULL))
    {
      apr_surelynx_log(SLOG_INFO, "PHP Session Parser: Performing recovery at offset %i", s - (char *)sessionImage->mm);
      s = strchr(pipeSymbol + 1, ';');
      if (s)
	s++;
    }

    if (!s)
    {
      apr_surelynx_log(SLOG_NOTICE, "PHP Session Parser: Warning -- Unable to continue due to invalid state!");
      break; /* loop */
    }

    if (integrous)
    {
      if (varname[0])	/* We don't do unnamed variables... why does PHP?!?! */
      {
	apr_hash_set(session, varname, APR_HASH_KEY_STRING, value_p);
	count++;
#ifdef TEST_DRIVER
	apr_surelynx_log(SLOG_TTY, "%s=%s (type %c)", varname, value_p->data.string, value_p->type);
#endif
      }
    }
    else
      apr_surelynx_log(SLOG_NOTICE, "PHP Session Parser: Unable to parse entry for variable '%s'; ignoring",
	  (varname ?: "(null)"));
  }

  if (finfo.size)
    apr_mmap_delete(sessionImage);	/* Wierd core dumps here when size == 0. ?!?! APR 1.2.27 */


  if (rc_bool_value(rc, "touch_php_session_files") == rc_true)
  {
    status = apr_file_mtime_set(sessionFilename, apr_time_now(), pool);
    if (status != APR_SUCCESS)
      apr_surelynx_log(SLOG_NOTICE, "PHP Session Parser: Warning -- Unable to touch session file %s! (%s)", sessionFilename, apr_strerror(status, errbuf, sizeof(errbuf)));
  }

  apr_file_close(sessionFile);
  *phpSession_p = session;

  apr_surelynx_log(SLOG_INFO, "Loaded PHP SessionID %s; %i variables recorded",
      sessionID, count);

  return APR_SUCCESS;
}

#ifdef TEST_DRIVER

int main(int argc, char *argv[])
{
  apr_pool_t 	*pool;
  apr_hash_t	*phpSession;

  pool = apr_initRuntime();

  enableTerminalLogs(pool, 0, NULL);
  phpSession_load(pool, pool, argv[1], &phpSession);
  return 0;
}

#endif
