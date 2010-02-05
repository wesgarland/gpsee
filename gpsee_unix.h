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
 *  @file 	gpsee_unix.h		Definitions for UNIX world normally provided by SureLynx headers.
 *  @author	Wes Garland
 *  @date	Feb 2009
 *  @version	$Id: gpsee_unix.h,v 1.6 2009/09/17 20:57:11 wes Exp $
 */

#define SLOG_EMERG	LOG_EMERG
#define SLOG_ERR	LOG_ERR
#define SLOG_WARNING	LOG_WARNING
#define SLOG_NOTICE	LOG_NOTICE
#define SLOG_INFO	LOG_INFO
#define SLOG_DEBUG	LOG_DEBUG

#if !defined GPSEE_LOG_FACILITY
# define GPSEE_LOG_FACILITY	LOG_USER
#endif

#define	gpsee_printf(a...)		printf(a)
#define gpsee_openlog(ident)		openlog(ident, LOG_ODELAY | LOG_PID, GPSEE_LOG_FACILITY)
JS_EXTERN_API(void) gpsee_log(signed int pri, const char *fmt, ...)  __attribute__((format(printf,2,3)));

typedef void * rc_list;					/**< opaque dictionary */
typedef void * rcFILE;					/**< opaque dictionary I/O handle */
typedef enum { rc_false, rc_true, rc_undef } rc_bool;	/**< Possible values for a boolean test */

const char 	*rc_value(rc_list rc, const char *key);						/**< Dictionary lookup */
const char 	*rc_default_value(rc_list rc, const char *key, const char *defaultValue);	/**< Dictionary lookup w/ default */
rc_bool		rc_bool_value(rc_list rc, const char *key);					/**< Dictionary bool lookup */
rcFILE 		*rc_openfile(int argc, char * const argv[]);					/**< Open based on progname */
int 		rc_close(rcFILE *rcFile);							/**< Close what we opened */
rc_list 	rc_readfile(rcFILE *rcFile);							/**< Read what we opened */

#if !defined(min)
# define min(a,b) ({__typeof__(a) _a=(a); __typeof__(b) _b=(b); _a < _b ? _a : _b;})
#endif

#if !defined(max)
# define max(a,b) ({__typeof__(a) _a=(a); __typeof__(b) _b=(b); _a > _b ? _a : _b;})
#endif

#if defined(__SURELYNX__)
# error "SureLynx environment not compatible with UNIX<>SureLynx shim!"
#endif
