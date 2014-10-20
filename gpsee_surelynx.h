extern void surelog(int pri, const char *fmt, ...) __attribute__((format(printf,2,3)));

#define NO_SURELYNX_INT_TYPEDEFS
#define NO_APR_SURELYNX_NAMESPACE_POISONING
#define GPSEE_LOG_FACILITY SLOG_USER

#define GLOG_DEBUG	SLOG_DEBUG
#define GLOG_INFO	SLOG_INFO
#define GLOG_NOTICE	SLOG_NOTICE
#define GLOG_WARNING	SLOG_WARNING
#define GLOG_ERR	SLOG_ERR
#define GLOG_ALERT	SLOG_ALERT
#define GLOG_EMERG	SLOG_EMERG

#define GLOG_NOTTY 0x10000000
#define GLOG_NOTTY_NOTICE	(GLOG_NOTICE | GLOG_NOTTY)
#define GLOG_NOTTY_INFO		(GLOG_INFO   | GLOG_NOTTY)

#define gpsee_log(cx, pri, fmt...) do 		\
{						\
  int _verbosity = gpsee_verbosity(0);		\
  int notty = pri & GLOG_NOTTY;			\
						\
  switch(pri & ~GLOG_NOTTY)			\
  {						\
    case GLOG_DEBUG:				\
      if (_verbosity < 2)			\
	break;					\
    case GLOG_INFO:				\
      if (_verbosity < 1)			\
	break;					\
    default:					\
       notty 					\
	? apr_surelynx_log(pri, fmt)		\
	: surelog(pri,fmt);	 		\
  }						\
} while(0)
#define gpsee_closelog()		closesurelog()
#define gpsee_openlog(ident)		rc_quiet(1), opensurelog(ident, SLOG_ODELAY | SLOG_PID, GPSEE_LOG_FACILITY)


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
#define _grt(cx)       ((gpsee_runtime_t *)JS_GetRuntimePrivate(JS_GetRuntime(cx)))
# define gpsee_printf(cx, a...)	        _grt(cx)->user_io.printf(cx, a)
# define gpsee_fprintf(cx, f, a...)     _grt(cx)->user_io.fprintf(cx, f, a)
# define gpsee_vfprintf(cx, f, s, a)    _grt(cx)->user_io.vfprintf(cx, f, s, a)
# define gpsee_fwrite(cx, p, s, n, f)   _grt(cx)->user_io.fwrite(p, s, n, f, cx)
# define gpsee_fread(cx, p, s, n, f)    _grt(cx)->user_io.fread(p, s, n, f, cx)
# define gpsee_fgets(cx, s, n, f)       _grt(cx)->user_io.fgets(s, n, f, cx)
# define gpsee_fputs(cx, s, f)          _grt(cx)->user_io.fputs(s, f, cx)
# define gpsee_fputc(cx, c, f)          _grt(cx)->user_io.fputc(c, f, cx)
# define gpsee_puts(cx, s)              _grt(cx)->user_io.puts(s, f, cx)
#endif

#define	cfg_value(cfg, key)			rc_value(cfg, key)
#define	cfg_default_value(cfg, key, default)	rc_default_value(cfg, key, default)
#define	cfg_bool_value(cfg, key)		rc_bool_value(cfg, key)
#define	cfg_openfile(argc, argv)		rc_openfile(argc, argv)
#define cfg_close(file)				rc_close(file)
#define cfg_readfile(file)			rc_readfile(file)
#define cfgFILE 				rcFILE
#define cfgHnd 					rc_list
#define cfg_false 				rc_false
#define cfg_true 				rc_true
#define cfg_undef 				rc_undef

