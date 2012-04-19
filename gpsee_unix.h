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
 *  @version	$Id: gpsee_unix.h,v 1.10 2010/06/14 22:11:59 wes Exp $
 */

#define GLOG_EMERG	0,LOG_EMERG
#define GLOG_ERR	0,LOG_ERR
#define GLOG_WARNING	0,LOG_WARNING
#define GLOG_NOTICE	0,LOG_NOTICE
#define GLOG_INFO	0,LOG_INFO
#define GLOG_DEBUG	0,LOG_DEBUG

#define GLOG_NOTTY_EMERG	1,LOG_EMERG
#define GLOG_NOTTY_ERR	        1,LOG_ERR
#define GLOG_NOTTY_WARNING	1,LOG_WARNING
#define GLOG_NOTTY_NOTICE	1,LOG_NOTICE
#define GLOG_NOTTY_INFO	        1,LOG_INFO
#define GLOG_NOTTY_DEBUG	1,LOG_DEBUG

#if !defined GPSEE_LOG_FACILITY
# define GPSEE_LOG_FACILITY	LOG_USER
#endif

#if defined(GPSEE_NO_JS_DEBUGGER)
# define gpsee_printf(cx, a...)         printf(a)
# define gpsee_fprintf(cx, f, s, a...)  fprintf(f, s, a)
# define gpsee_vfprintf(cx, f, s, a)    vfprintf(f, s, a)
# define gpsee_fwrite(cx, p, s, n, f)   fwrite(p, s, n, f)
# define gpsee_fread(cx, p, s, n, f)    fread(p, s, n, f)
# define gpsee_fgets(cx, s, n, f)       fgets(s, n, f)
# define gpsee_fputs(cx, s, f)          fputs(s, f)
# define gpsee_fputc(cx, c, f)          fputc(c, f)
# define gpsee_puts(cx, s)              puts(s)
#else
static inline gpsee_runtime_t *_grt(JSContext *cx)
{
  return (gpsee_runtime_t *)JS_GetRuntimePrivate(JS_GetRuntime(cx));
}
# define gpsee_printf(cx, a...)	        _grt(cx)->user_io.printf(cx, a)
# define gpsee_fprintf(cx, f, a...)     _grt(cx)->user_io.fprintf(cx, f, a)
# define gpsee_vfprintf(cx, f, s, a)    _grt(cx)->user_io.vfprintf(cx, f, s, a)
# define gpsee_fwrite(cx, p, s, n, f)   _grt(cx)->user_io.fwrite(p, s, n, f, cx)
# define gpsee_fread(cx, p, s, n, f)    _grt(cx)->user_io.fread(p, s, n, f, cx)
# define gpsee_fgets(cx, s, n, f)       _grt(cx)->user_io.fgets(s, n, f, cx)
# define gpsee_fputs(cx, s, f)          _grt(cx)->user_io.fputs(s, f, cx)
# define gpsee_fputc(cx, c, f)          _grt(cx)->user_io.fputc(c, f, cx)
# define gpsee_puts(cx, s)              _grt(cx)->user_io.puts(s, cx)
# define gpsee_fgetc(cx, s)             _grt(cx)->user_io.fgetc(s, cx)
#endif

#define gpsee_openlog(ident)		openlog(ident, LOG_ODELAY | LOG_PID, GPSEE_LOG_FACILITY)
JS_EXTERN_API(void) gpsee_log(JSContext *cx, unsigned int extra, signed int pri, const char *fmt, ...)  __attribute__((format(printf,4,5)));
#define gpsee_closelog()		closelog()

typedef void * cfgHnd;     					/**< opaque dictionary */
typedef void * cfgFILE;					        /**< opaque dictionary I/O handle */
typedef enum { cfg_false, cfg_true, cfg_undef } cfg_bool;	/**< Possible values for a boolean test */

const char *    cfg_value(cfgHnd cfg, const char *key);		                                /**< Dictionary lookup */
const char *    cfg_default_value(cfgHnd cfg, const char *key, const char *defaultValue);	/**< Dictionary lookup w/ default */
cfg_bool	cfg_bool_value(cfgHnd cfg, const char *key);					/**< Dictionary bool lookup */
cfgFILE *       cfg_openfile(int argc, char * const argv[]);					/**< Open based on progname */
int 		cfg_close(cfgFILE *cfgFile);							/**< Close what we opened */
cfgHnd   	cfg_readfile(cfgFILE *cfgFile);							/**< Read what we opened */

#if !defined(min)
# define min(a,b) ({__typeof__(a) _a=(a); __typeof__(b) _b=(b); _a < _b ? _a : _b;})
#endif

#if !defined(max)
# define max(a,b) ({__typeof__(a) _a=(a); __typeof__(b) _b=(b); _a > _b ? _a : _b;})
#endif
