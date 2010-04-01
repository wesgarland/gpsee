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
 * @file	gsr.c		GPSEE Script Runner ("scripting host")
 * @author	Wes Garland
 * @date	Aug 27 2007
 * @version	$Id: gsr.c,v 1.21 2010/04/01 13:43:19 wes Exp $
 *
 * This program is designed to interpret a JavaScript program as much like
 * a shell script as possible.
 *
 * @see exec(2) system call
 *
 * When launching as a file interpreter, a single argument may follow the
 * interpreter's filename. This argument starts with a dash and is a series
 * of argumentless flags.
 *
 * All other command line options will be passed along to the JavaScript program.
 *
 * The "official documentation" for the prescence and meaning of flags and switch
 * is the usage() function.
 */
 
static __attribute__((unused)) const char rcsid[]="$Id: gsr.c,v 1.21 2010/04/01 13:43:19 wes Exp $";

#define PRODUCT_SHORTNAME	"gsr"
#define PRODUCT_VERSION		"1.0-pre2"

#include <prinit.h>
#include "gpsee.h"
#if defined(GPSEE_DARWIN_SYSTEM)
#include <crt_externs.h>
#endif

#if defined(__SURELYNX__)
# define NO_APR_SURELYNX_NAMESPACE_POISONING
# define NO_SURELYNX_INT_TYPEDEFS	/* conflicts with mozilla's NSPR protypes.h */
#endif

#if defined(__SURELYNX__)
static apr_pool_t *permanent_pool;
#endif

#if defined(__SURELYNX__)
# define whenSureLynx(a,b)	a
#else
# define whenSureLynx(a,b)	b
#endif


extern rc_list rc;

/** Handler for fatal errors. Generate a fatal error
 *  message to surelog, stdout, or stderr depending on
 *  whether our controlling terminal is a tty or not.
 *
 *  @param	message		Arbitrary text describing the
 *				fatal condition
 *  @note	Exits with status 1
 */
static void __attribute__((noreturn)) fatal(const char *message)
{
  int 		haveTTY;
#if defined(HAVE_APR)
  apr_os_file_t	currentStderrFileno;

  if (!apr_stderr || (apr_os_file_get(&currentStderrFileno, apr_stderr) != APR_SUCCESS))
#else
  int currentStderrFileno;
#endif
    currentStderrFileno = STDERR_FILENO;

  haveTTY = isatty(currentStderrFileno);

  if (!message)
    message = "UNDEFINED MESSAGE - OUT OF MEMORY?";

  if (haveTTY)
  {
#if defined(HAVE_APR)
    if (apr_stderr)
      apr_file_printf(apr_stderr, "\007Fatal Error in " PRODUCT_SHORTNAME ": %s\n", message);
    else
#endif
      fprintf(stderr, "\007Fatal Error in " PRODUCT_SHORTNAME ": %s\n", message);
  }
  else
    gpsee_log(SLOG_EMERG, "Fatal Error: %s", message);

  exit(1);
}

/** GPSEE uses panic() to panic, expects embedder to provide */
void __attribute__((noreturn)) panic(const char *message)
{
  fatal(message);
}

/** Help text for this program, which doubles as the "official"
 *  documentation for the command-line parameters.
 *
 *  @param	argv_zero	How this program was invoked.
 *
 *  @note	Exits with status 1
 */
static void __attribute__((noreturn)) usage(const char *argv_zero)
{
  char spaces[strlen(argv_zero) + 1];

  memset(spaces, (int)(' '), sizeof(spaces) -1);
  spaces[sizeof(spaces) - 1] = (char)0;

  gpsee_printf(
                  "\n"
#if defined(__SURELYNX__)
                  "SureLynx "
#endif
                  PRODUCT_SHORTNAME " " PRODUCT_VERSION " - GPSEE Script Runner for GPSEE " GPSEE_CURRENT_VERSION_STRING "\n"
                  "Copyright (c) 2007-2009 PageMail, Inc. All Rights Reserved.\n"
                  "\n"
                  "As an interpreter: #! %s {-/*flags*/}\n"
                  "As a command:      %s "
#if defined(__SURELYNX__)
                                        "{-r file} [-D file] "
#endif
                                                              "[-z #] [-n] <[-c code]|[-f filename]>\n"
                  "                   %s {-/*flags*/} {[--] [arg...]}\n"
                  "Command Options:\n"
                  "    -c code     Specifies literal JavaScript code to execute\n"
                  "    -f filename Specifies the filename containing code to run\n"
                  "    -F filename Like -f, but skip shebang if present.\n"
                  "    -h          Display this help\n"
                  "    -n          Engine will load and parse, but not run, the script\n"
#if defined(__SURELYNX__)
                  "    -D file     Specifies a debug output file\n"
                  "    -r file     Specifies alternate interpreter RC file\n"
#endif
                  "    flags       A series of one-character flags which can be used\n"
                  "                in either file interpreter or command mode\n"
                  "    --          Arguments after -- are passed to the script\n"
                  "\n"
                  "Valid Flags:\n"
                  "    a - Allow (read-only) access to caller's environment\n"
		  "    C - Disables compiler caching via JSScript XDR serialization\n"
                  "    d - Increase verbosity\n"
                  "    e - Do not limit regexps to n^3 levels of backtracking\n"
                  "    J - Disable nanojit\n"
                  "    S - Disable Strict mode\n"
                  "    R - Load RC file for interpreter (" PRODUCT_SHORTNAME ") based on\n"
                  "        script filename.\n"
		  "    U - Disable UTF-8 C string processing\n"
                  "    W - Do not report warnings\n"
                  "    x - Parse <!-- comments --> as E4X tokens\n"
                  "    z - Increase GC Zealousness\n"
                  "\n",
                  argv_zero, argv_zero, spaces);
  exit(1);
}

/** Process the script interpreter flags.
 *
 *  @param	flags	An array of flags, in no particular order.
 */
static void processFlags(gpsee_interpreter_t *jsi, const char *flags)
{
  int			gcZeal = 0;
  int			jsOptions;
  const char 		*f;

  jsOptions = JS_GetOptions(jsi->cx) | JSOPTION_ANONFUNFIX | JSOPTION_STRICT | JSOPTION_RELIMIT | JSOPTION_JIT;

  /* Iterate over each flag */
  for (f=flags; *f; f++)
  {
    switch(*f)
    {
      /* 'C' flag disables compiler cache */
      case 'C':
        jsi->useCompilerCache = 0;
        break;

      case 'a':	/* Handled in prmain() */
      case 'R':	/* Handled in loadRuntimeConfig() */
      case 'U': /* Must be handled before 1st JS runtime */
	break;
	  
      case 'x':	/* Parse <!-- comments --> as E4X tokens */
	jsOptions |= JSOPTION_XML;
	break;

      case 'S':	/* Disable Strict JS */
	jsOptions &= ~JSOPTION_STRICT;
	break;

      case 'W':	/* Suppress JS Warnings */
	jsi->errorReport |= er_noWarnings;
	break;

      case 'e':	/* Allow regexps that are more than O(n^3) */
	jsOptions &= ~JSOPTION_RELIMIT;
	break;

      case 'J':	/* Disable Nanojit */
	jsOptions &= ~JSOPTION_JIT;
	break;

      case 'z':	/* GC Zeal */
	gcZeal++;
	break;

      case 'd':	/* increase debug level */
	gpsee_verbosity(+1);
	break;	

      default:
	gpsee_log(SLOG_WARNING, "Error: Unrecognized option flag %c!", *f);
	break;
    }
  }

#ifdef JSFEATURE_GC_ZEAL
  if (JS_HasFeature(JSFEATURE_GC_ZEAL) == JS_TRUE)
#endif
    JS_SetGCZeal(jsi->cx, gcZeal);

  JS_SetOptions(jsi->cx, jsOptions);
}

static void processInlineFlags(gpsee_interpreter_t *jsi, FILE *scriptFile)
{
  char	buf[256];
  off_t	offset;

  offset = ftello(scriptFile);

  while(fgets(buf, sizeof(buf), scriptFile))
  {
    char *s, *e;

    if ((buf[0] != '/') || (buf[1] != '/'))
      break;

    for (s = buf + 2; *s == ' ' || *s == '\t'; s++);
    if (strncmp(s, "gpsee:", 6) != 0)
      continue;

    for (s = s + 6; *s == ' ' || *s == '\t'; s++);

    for (e = s; *e; e++)
    {
      switch(*e)
      {
	case '\r':
	case '\n':
	case '\t':
	case ' ':
	  *e = (char)0;
	  break;
      }
    }

    if (s[0])
      processFlags(jsi, s);
  }

  fseeko(scriptFile, offset, SEEK_SET);
}

static FILE *openScriptFile(gpsee_interpreter_t *jsi, const char *scriptFilename, int skipSheBang)
{
  FILE 	*file = fopen(scriptFilename, "r");
  char	line[64]; /* #! args can be longer than 64 on some unices, but no matter */

  if (!file)
    return NULL;

  gpsee_flock(fileno(file), GPSEE_LOCK_SH);

  if (!skipSheBang)
    return file;

  if (fgets(line, sizeof(line), file))
  {
    if ((line[0] != '#') || (line[1] != '!'))
    {
      gpsee_log(SLOG_NOTICE, PRODUCT_SHORTNAME ": Warning: First line of "
		"file-interpreter script does not contain #!");
      rewind(file);
    }
    else
    {
      jsi->linenoOffset += 1;

      do  /* consume entire first line, regardless of length */
      {
        if (strchr(line, '\n'))
          break;
      } while(fgets(line, sizeof(line), file));
    }
  }

  return file;
}

/**
 * Detect if we've been asked to run as a file interpreter.
 *
 * File interpreters always have the script filename within 
 * the first two arguments. It is never "not found", except
 * due to an annoying OS race in exec(2), which is very hard
 * to exploit.
 *
 * @returns - 0 if we're not a file interpreter
 *	    - 1 if the script filename is on argv[1]
 *          - 2 if the script filename is on argv[2]
 */
PRIntn findFileToInterpret(PRIntn argc, char **argv)
{
  if (argc == 1)
    return 0;

  if ((argv[1][0] == '-') && 
      ((argv[1][1] == '-')
       || strchr(argv[1] + 1, 'c')
       || strchr(argv[1] + 1, 'f')
       || strchr(argv[1] + 1, 'F')
       || strchr(argv[1] + 1, 'h')
       || strchr(argv[1] + 1, 'n')
#if defined(__SURELYNX__)
       || strchr(argv[1], 'r')
       || strchr(argv[1], 'D')
#endif	
       )
      )
    return 0;	/* -h, -c, -f, -F, -r will always be invalid flags & force command mode */

  if ((argc > 1) && argv[1][0] != '-')
    if (access(argv[1], F_OK) == 0)
      return 1;

  if ((argc > 2) && (argv[2][0] != '-'))
    if (access(argv[2], F_OK) == 0)
      return 2;

  return 0;
}

/** Load RC file based on filename.
 *  Filename to use is script's filename, if -R option flag is thrown.
 *  Otherwise, file interpreter's filename (i.e. gsr) is used.
 */
void loadRuntimeConfig(const char *scriptFilename, const char *flags, int argc, char * const *argv)
{ 
  rcFILE	*rc_file;
  
  if (strchr(flags, 'R'))
  {
    char	fake_argv_zero[FILENAME_MAX];
    char 	*fake_argv[2];
    char	*s;

    if (!scriptFilename)
      fatal("Cannot use script-filename RC file when script-filename is unknown!");

    fake_argv[0] = fake_argv_zero;

    gpsee_cpystrn(fake_argv_zero, scriptFilename, sizeof(fake_argv_zero));

    if ((s = strstr(fake_argv_zero, ".js")) && (s[3] == (char)0))
      *s = (char)0;

    if ((s = strstr(fake_argv_zero, ".es3")) && (s[4] == (char)0))
      *s = (char)0;

    if ((s = strstr(fake_argv_zero, ".e4x")) && (s[4] == (char)0))
      *s = (char)0;

    rc_file = rc_openfile(1, fake_argv);
  }
  else
  {
    rc_file = rc_openfile(argc, argv);
  }

  if (!rc_file)
    fatal("Could not open interpreter's RC file!");
  
  rc = rc_readfile(rc_file);
  rc_close(rc_file);
}

PRIntn prmain(PRIntn argc, char **argv)
{
  gpsee_interpreter_t	*jsi;				/* Handle describing JS interpreter */
  const char		*scriptCode = NULL;		/* String with JavaScript program in it */
  const char		*scriptFilename = NULL;		/* Filename with JavaScript program in it */
  char * const		*script_argv;			/* Becomes arguments array in JS program */
  char * const  	*script_environ = NULL;		/* Environment to pass to script */
  char			*flags = malloc(8);		/* Flags found on command line */
  int			noRunScript = 0;		/* !0 = compile but do not run */

  int			fiArg = 0;
  int			skipSheBang = 0;
  int			exitCode;
  int			preloadVerbosity;		/* Verbosity to use before flags are processed */

#if defined(__SURELYNX__)
  permanent_pool = apr_initRuntime();
#endif
  preloadVerbosity = isatty(STDERR_FILENO) ? 1 : 0;
  gpsee_openlog(gpsee_basename(argv[0]));

  /* Print usage and exit if no arguments were given */
  if (argc < 2)
    usage(argv[0]);

  flags[0] = (char)0;

  fiArg = findFileToInterpret(argc, argv);

  if (fiArg == 0)	/* Not a script file interpreter (shebang) */
  {
    int 	c;
    char	*flag_p = flags;

    while ((c = getopt(argc, argv, whenSureLynx("D:r:","") "v:c:hnf:F:aCRxSUWdeJ"
#ifdef JS_GC_ZEAL
		       "z"
#endif
		       )) != -1)
    {
      switch(c)
      {
#if defined(__SURELYNX__)
	case 'D':
	  redirect_output(permanent_pool, optarg);
	  break;
	case 'r': /* handled by rc_openfile() */
	  break;
#endif

	case 'c':
	  scriptCode = optarg;
	  break;

	case 'h':
	  usage(argv[0]);
	  break;

	case 'n':
	  noRunScript = 1;
	  break;

	case 'F':
	  skipSheBang = 1;
	case 'f':
	  scriptFilename = optarg;
	  break;

	case 'a':
	case 'C':
	case 'd':
	case 'e':
	case 'J':
	case 'S':
	case 'R':
	case 'U':
	case 'W':
	case 'x':
	case 'z':
	{
	  char *flags_storage = realloc(flags, (flag_p - flags) + 1 + 1);
	    
	  if (flags_storage != flags)
	  {
	    flag_p = (flag_p - flags) + flags_storage;
	    flags = flags_storage;
	  }

	  *flag_p++ = c;
	  *flag_p = '\0';
	}
	break;

	case '?':
	{
	  char buf[32];

	  snprintf(buf, sizeof(buf), "Invalid option: %c\n", optopt);
	  fatal(buf);
	}

	case ':':
	{
	  char buf[32];

	  snprintf(buf, sizeof(buf), "Missing parameter to option '%c'\n", optopt);
	  fatal(buf);
	}

      }
    } /* getopt wend */

    /* Create the script's argument vector with the script name as arguments[0] */
    {
      char **nc_script_argv = malloc(sizeof(argv[0]) * (2 + (argc - optind)));
      nc_script_argv[0] = (char *)scriptFilename;
      memcpy(nc_script_argv + 1, argv + optind, sizeof(argv[0]) * ((1 + argc) - optind));
      script_argv = nc_script_argv;
    }

    *flag_p = (char)0;
  }
  else 
  {
    scriptFilename = argv[fiArg];

    if (fiArg == 2)
    {
      if (argv[1][0] != '-')
	fatal("Invalid syntax for file-interpreter flags! (Must begin with '-')");

      flags = realloc(flags, strlen(argv[1]) + 1);
      strcpy(flags, argv[1] + 1);
    }

    /* This is a file interpreter; script_argv is correct at offset fiArg. */
    script_argv = argv + fiArg;
  }

  if (strchr(flags, 'a'))
  {
#if defined(GPSEE_DARWIN_SYSTEM)
    script_environ = (char * const *)_NSGetEnviron();
#else
    extern char **environ;
    script_environ = (char * const *)environ;
#endif
  }

  loadRuntimeConfig(scriptFilename, flags, argc, argv);
  if (strchr(flags, 'U') || (rc_bool_value(rc, "gpsee_force_no_utf8_c_strings") == rc_true) || getenv("GPSEE_NO_UTF8_C_STRINGS"))
  {
    JS_DestroyRuntime(JS_NewRuntime(1024));
    putenv((char *)"GPSEE_NO_UTF8_C_STRINGS=1");
  }

  jsi = gpsee_createInterpreter(script_argv, script_environ);
  gpsee_setThreadStackLimit(jsi->cx, &jsi);
  processFlags(jsi, flags);
  free(flags);

  /* Set the correct verbosity level, then temporarily increase by preloadVerbosity if 0 */
#if defined(__SURELYNX__)
  sl_set_debugLevel(gpsee_verbosity(0));
  enableTerminalLogs(permanent_pool, gpsee_verbosity(0) > 0, NULL);
#else
  if (!gpsee_verbosity(0) && isatty(STDERR_FILENO))
    gpsee_verbosity(1);
#endif

  if (!gpsee_verbosity(0) && preloadVerbosity)
    gpsee_verbosity(preloadVerbosity);
  else
    preloadVerbosity = 0;

  /* Run JavaScript specified with -c */
  if (scriptCode) 
  {
    jsval v;
    JS_EvaluateScript(jsi->cx, jsi->globalObj, scriptCode, strlen(scriptCode), "command_line", 1, &v);
    if (JS_IsExceptionPending(jsi->cx))
      goto out;
  }

#if !defined(SYSTEM_GSR)
#define	SYSTEM_GSR	"/usr/bin/gsr"
#endif
  if ((argv[0][0] == '/') && (strcmp(argv[0], SYSTEM_GSR) != 0) && rc_bool_value(rc, "no_gsr_preload_script") != rc_true)
  {
    char preloadScriptFilename[FILENAME_MAX];
    char mydir[FILENAME_MAX];
    int i;

    i = snprintf(preloadScriptFilename, sizeof(preloadScriptFilename), "%s/.%s_preload", gpsee_dirname(argv[0], mydir, sizeof(mydir)), 
		 gpsee_basename(argv[0]));
    if ((i == 0) || (i == (sizeof(preloadScriptFilename) -1)))
      gpsee_log(SLOG_EMERG, PRODUCT_SHORTNAME ": Unable to create preload script filename!");
    else
      errno = 0;

    if (access(preloadScriptFilename, F_OK) == 0)
    {
      jsval 		v;
      JSScript		*script;
      JSObject		*scrobj;

      if (!gpsee_compileScript(jsi->cx, preloadScriptFilename, NULL, NULL, &script, jsi->globalObj, &scrobj))
      {
	gpsee_log(SLOG_EMERG, PRODUCT_SHORTNAME ": Unable to compile preload script '%s'", preloadScriptFilename);
	goto out;
      }

      if (!script || !scrobj)
	goto out;

      if (!noRunScript)
      {
        JS_AddNamedRoot(jsi->cx, &scrobj, "preload_scrobj");
        JS_ExecuteScript(jsi->cx, jsi->globalObj, script, &v);
        if (JS_IsExceptionPending(jsi->cx))
        {
          jsi->exitType = et_exception;
          JS_ReportPendingException(jsi->cx);
        }
        JS_RemoveRoot(jsi->cx, &scrobj);
      }
    }

    if (jsi->exitType & et_exception)
      goto out;
  }

  /* Setup for main-script running -- cancel preload verbosity and use our own error reporting system in gsr that does not use error reporter */
  gpsee_verbosity(-1 * preloadVerbosity);
  JS_SetOptions(jsi->cx, JS_GetOptions(jsi->cx) | JSOPTION_DONT_REPORT_UNCAUGHT);

  if (!scriptFilename)
  {
    exitCode = scriptCode ? 0 : 1;
  }
  else
  {
    FILE 	*scriptFile = openScriptFile(jsi, scriptFilename, skipSheBang || (fiArg != 0));

    if (!scriptFile)
    {
      gpsee_log(SLOG_NOTICE, PRODUCT_SHORTNAME ": Unable to open' script '%s'! (%m)", scriptFilename);
      exitCode = 1;
      goto out;
    }

    processInlineFlags(jsi, scriptFile);

    /* Just compile and exit? */
    if (noRunScript)
    {
      JSScript        *script;
      JSObject        *scrobj;

      if (!gpsee_compileScript(jsi->cx, scriptFilename, scriptFile, NULL, &script, jsi->globalObj, &scrobj))
      {
        gpsee_reportUncaughtException(jsi->cx, JSVAL_NULL, isatty(STDERR_FILENO));
	exitCode = 1;
      }
      else
      {
	exitCode = 0;
      }
    }
    else /* noRunScript is false; run the program */
    {
      if (!gpsee_runProgramModule(jsi->cx, scriptFilename, NULL, scriptFile))
      {
        int code = gpsee_getExceptionExitCode(jsi->cx);
        if (code >= 0)
        {
          exitCode = code;
        }
        else
        {
          gpsee_reportUncaughtException(jsi->cx, JSVAL_NULL, isatty(STDERR_FILENO));
          exitCode = 1;
        }
      }
      else
      {
      }
    }
      fclose(scriptFile);
    goto out;
  }

  out:
  gpsee_destroyInterpreter(jsi);
  JS_ShutDown();

  return exitCode;
}

int main(int argc, char *argv[])
{
  return PR_Initialize(prmain, argc, argv, 0);
}

