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
 *  @version	$Id: gpsee_util.c,v 1.15 2010/12/02 21:59:42 wes Exp $
 *  @date	March 2009
 */

static const char __attribute__((unused)) rcsid[]="$Id: gpsee_util.c,v 1.15 2010/12/02 21:59:42 wes Exp $:";

#include "gpsee.h"
#define NO_FUNCTION_NAME "<global scope>"

/* ctype.h provides isspace() used by gpsee_reportErrorSourceCode()
 */
#include <ctype.h>

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
    while (dst_size-- && *src)
      *dst++ = *src++;
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

  if (!slash[1])
    return "/";

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
    return buf;
  }

  if (((size_t)(slash - fullpath)) >= bufLen)
    return NULL;

  strncpy(buf, fullpath, slash - fullpath);
  buf[slash - fullpath] = (char)0;

  return buf;
}

/**
 *  Resolve all symlinks and relative directory components (. and ..) to return a fully resolved path.
 *
 *  If bufsize < PATH_MAX, malloc() is used to allocate PATH_MAX bytes during the call to this function.
 *
 *  TODO remove use of malloc()
 *
 *  @param      path      Input path string to opreate upon
 *  @param      buf       Output buffer for resolved path
 *  @param      bufsize   Size of 'buf'
 *  @returns              Length of returned path on success, or -1 on failure
 */
int gpsee_resolvepath(const char *path, char *buf, size_t bufsiz)
{
  int   ret;
#if defined(GPSEE_SUNOS_SYSTEM)
  ret = resolvepath(path, buf, bufsiz);
  buf[min(ret, bufsiz - 1)] = (char)0;

  return ret;
#else
  char  *mBuf;
  char  *rp;

  errno = 0;

  /* Require buf (this behavior is *nearly* consistent with what was here before I came'a bugfixin') */
  if (!buf)
  {
    errno = EINVAL;
    return -1;
  }

  /* realpath() requires PATH_MAX sized buffer, but we buffer the caller from that restriction */
  if (bufsiz < PATH_MAX)
  {
    mBuf = malloc(PATH_MAX);
    if (!mBuf)
    {
      errno = ENOMEM;
      return -1;
    }
  }
  else
    mBuf = buf;

  rp = realpath(path, mBuf);
  if (!rp)
  {
    if (mBuf != buf)
      free(mBuf);
    /* Pass realpath() errno upstream */
    return -1;
  }

  /* Did we buffer the caller's buffer? */
  if (mBuf != buf)
  {
    char *s = gpsee_cpystrn(buf, mBuf, bufsiz);
    if (s - buf == bufsiz) /* overrun */
    {
      /* should be unreachable, because we would have already bailed on realpath() failure */
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

/** Returns a number >= 0 if there is a currently pending exception and that exception qualifies as
 *  a "SystemExit". Currently, to exit a process using the exception throwing mechanism, one must
 *  throw an system exit code. If uncaught, this should signify to the host application the
 *  success or failure of the program (zero, of course, meaning success.) Of course any other
 *  uncaught exception will result in the program exiting with failure as well, but often it is
 *  important to indicate to the parent process that a failure has occurred but bypass any
 *  noisy output that would normally occur.
 *
 *  As only the lowest 8 bits of exit status are used in POSIX, we don't consider any number for which
 *  (n & 0377 != n) to constitute a SystemExit.
 *
 *  In the future, this facility may be more formalized. The fact that you *can* throw any value in
 *  Javascript is only a coincidence that we took advantage of for providing the prototype of this
 *  facility, but it there could possibly be someone else out there throwing numbers with a different
 *  idea in mind as to the meaning of such an action.
 *
 *  @param    cx
 *  @returns  Returns an int greater than 
 *
 */
int gpsee_getExceptionExitCode(JSContext *cx)
{
  jsval v;
  jsdouble d;
  jsint i;

  /* Retrieve exception; none has been thrown if this returns JS_FALSE */
  if (!JS_GetPendingException(cx, &v))
    return -1;

  /* Try to coerce an object to a number */
  if (!JSVAL_IS_PRIMITIVE(v))
  {
    if (!JS_ValueToNumber(cx, v, &d))
      return -1;
    goto havedouble;
  }

  /* Try to get a double, or use a double we got from the last step */
  if (JSVAL_IS_DOUBLE(v))
  {
    d = *JSVAL_TO_DOUBLE(v);
havedouble:
    if (d != (jsdouble) (i = (jsint) d))
      return -1;
    goto haveint;
  }

  /* Evaluate an int */
  if (JSVAL_IS_INT(v))
  {
    int n;
    i = JSVAL_TO_INT(v);
haveint:
    n = (int) i; // not that this should ever matter...
    if ((n & 255) == n)
      return n;
  }

  return -1;
}

/**
 *  Return a pointer to the relative filename of the passed long_filename. The returned
 *  filename will be in relation to the realm's program module, and will always be
 *  a substring of long_filename (pointing inside it - no copying).
 *
 *  @param      cx              Any context in the current realm
 *  @param      long_filename   The filename to shorten
 * 
 *  @returns    The shortened filename, or long_filename.
 *
 *  @note	Passing long_filename = NULL to this function will cause it to return NULL.
 */
const char *gpsee_programRelativeFilename(JSContext *cx, const char *long_filename)
{
  moduleHandle_t        *programModule;
  gpsee_realm_t         *realm = gpsee_getRealm(cx);
  const char            *cname;
  const char            *bname;

  if (!realm || !long_filename)
    return long_filename;

  gpsee_enterAutoMonitor(cx, &realm->monitors.programModule);
  programModule = realm->monitored.programModule;
  if (!programModule)
    goto out;

  cname = gpsee_getModuleCName(programModule);
  if (!cname)
    goto out;

  bname = strrchr(cname, '/');
  if (!bname)
    goto out;

  if (strncmp(long_filename, cname, (bname-cname) + 1) == 0)
  {
    gpsee_leaveAutoMonitor(realm->monitors.programModule);
    return long_filename + (bname-cname) + 1;
  }

  out:
  gpsee_leaveAutoMonitor(realm->monitors.programModule);
  return long_filename;
}

#define VT_BOLD "\33[1m"
#define VT_UNBOLD "\33[22m"
/** An error reporter used by gpsee_reportUncaughtException() because only an "error reporter" (see JS_SetErrorReporter()
 *  gets access to the script source that generated the error.
 */
static void gpsee_reportErrorSourceCode(JSContext *cx, const char *message, JSErrorReport *report)
{
  gpsee_runtime_t	*grt = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  const char		*filename = gpsee_programRelativeFilename(cx, report->filename) ?: "<unknown filename>";
  char 			prefix[strlen(filename) + 21]; /* Allocate enough room for "filename:lineno" */
  size_t 		sz;
  int			bold = gpsee_isatty(STDERR_FILENO);

  if (bold)
  {
    const char *term = getenv("TERM");

    if (!term || ( 
		  (strncmp(term, "vt10", 4) != 0) &&
		  (strncmp(term, "xterm", 5) != 0) &&
		  (strcmp(term, "dtterm") != 0) &&
		  (strcmp(term, "ansi") != 0)))
      bold = 0;
  }

  sz = snprintf(prefix, sizeof(prefix), "%s:%d", filename, report->lineno);
  GPSEE_ASSERT(sz < sizeof(prefix));

  if (grt->pendingErrorMessage && gpsee_verbosity(0) >= GPSEE_ERROR_OUTPUT_VERBOSITY)
  {
    gpsee_fprintf(cx, stderr, "\n%sUncaught exception in %s: %s%s\n", bold?VT_BOLD:"", prefix, grt->pendingErrorMessage, bold?VT_UNBOLD:"");
    gpsee_log(cx, SLOG_NOTTY_NOTICE, "Uncaught exception in %s: %s", prefix, grt->pendingErrorMessage);
  }

  if (report->linebuf && (gpsee_verbosity(0) >= GPSEE_ERROR_POINTER_VERBOSITY) && gpsee_isatty(STDERR_FILENO))
  {
    size_t start, len;
    const char *c = report->linebuf;

    start = 0;
    while (isspace(*(c++)))
      start++;

    len = 0;
    while (*(c))
    {
      if (!isspace(*c))
        len = (c - report->linebuf) - start;
      c++;
    }

    if (len > 0)
    {
      char linebuf[len+1];
      int i;

      gpsee_cpystrn(linebuf, report->linebuf + start, len+1);
      gpsee_fprintf(cx, stderr, "%s: %s\n", prefix, linebuf);
      gpsee_fprintf(cx, stderr, "%s: ", prefix);
      for (i = report->tokenptr - report->linebuf - start; i > 1; i--)
        gpsee_fputc(cx, '.', stderr);
      gpsee_fputs(cx, "^\n", stderr);
    }
    else
    {
      gpsee_fprintf(cx, stderr, "%s: %s\n", prefix, report->linebuf);
    }
  }
}

static const char *rev_strchr(const char *start, char search, const char *limit)
{
  const char *s;

  for (s = start; s >= limit; s--)
    if (*s == search)
      return s;

  return NULL;
}

/** Report any pending exception as though it were uncaught.
 *  Renders a nice-looking error report to stderr.
 *
 *  @param    cx
 *  @param    exval     Exception value to be used (typically from JS_GetPendingException()) or JSVAL_NULL
 *                      to grab the exception value for you from JS_GetPendingException().
 * 
 *  @todo Should there be an option argument for publishing to gpsee_log()?
 */
JSBool gpsee_reportUncaughtException(JSContext *cx, jsval exval, int dumpStack)
{
  gpsee_runtime_t 	*grt = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  jsval                	v;
  char 			*longerror = NULL;
  JSErrorReporter 	reporter;

  /* Must we look up the exception value ourselves? */
  if (exval == JSVAL_NULL)
  {
    if (!JS_GetPendingException(cx, &exval))
      /* no exception pending! */
      return JS_FALSE;
  }

  /* We'll be trying to output one of two sets of data. The first set is error.message, followed by error.stack.
   * The second set is just an attempt to stringify the exception value to a string.
   */

  /* Is exval an Object? */
  if (!JSVAL_IS_PRIMITIVE(exval))
  {
    /* Attempt to retrieve "message" property from exception object */
    if (JS_GetProperty(cx, JSVAL_TO_OBJECT(exval), "message", &v))
    {
      const char *error;
      if (JSVAL_IS_STRING(v))
      {
        /* Make char buffer from JSString* */
        error = JS_GetStringBytes(JSVAL_TO_STRING(v));

        /* This makes the message available to gpsee_reportErrorSourceCode() */
        grt->pendingErrorMessage = error;
      }
    }

    /* Attempt to retrieve "stack" property from exception object */
    if (JS_GetProperty(cx, JSVAL_TO_OBJECT(exval), "stack", &v))
    {
      if (JSVAL_IS_STRING(v))
      {
        char *stack;
        /* Make char buffer from JSString* */
        stack = JS_GetStringBytes(JSVAL_TO_STRING(v));
        if (!stack) // OOM
          panic("Out of memory reporting an uncaught exception in " __FILE__);
        longerror = JS_strdup(cx, stack);
      }
    }
  }

  /* Some information is only available to an "error reporter" (see JS_SetErrorReporter()) so we report that part here
   * using an error reporter. Note that JS_ReportPendingException() also calls JS_ClearPendingException().
   */
  reporter = JS_SetErrorReporter(cx, (JSErrorReporter)gpsee_reportErrorSourceCode);
  JS_ReportPendingException(cx);
  JS_SetErrorReporter(cx, reporter);
  grt->pendingErrorMessage = NULL;

  if (!dumpStack)
    return JS_TRUE;

  /* Tweak the exception output format without re-allocating buffers */
  if (longerror && *longerror)
  {
    const char		*pm_dir;
    size_t		pm_dir_len;
    char		*nl, *line_start, *tok;
    size_t		extraBytes = 0;
    gpsee_realm_t       *realm = gpsee_getRealm(cx);

    /* Look for filenames which can be shortened */
    gpsee_enterAutoMonitor(cx, &realm->monitors.programModuleDir);
    pm_dir = realm->monitored.programModuleDir;
    if (!pm_dir)
      panic("Out of memory reporting an uncaught exception in " __FILE__);
    if (!pm_dir[0])
    {
      gpsee_leaveAutoMonitor(realm->monitors.programModuleDir);
      goto done;
    }
    pm_dir_len = strlen(pm_dir);

    for (line_start = longerror, nl=strchr(line_start, '\n'); 
	 nl; 
	 line_start = nl + 1, nl = strchr(line_start, '\n'))
    {
      tok = (char *)rev_strchr(nl, '@', line_start);
      if (!tok)
	continue;
      if (strncmp(tok + 1, pm_dir, pm_dir_len) == 0)
      {
	memmove(tok + 1, tok + 1 + pm_dir_len, strlen(tok + 1 + pm_dir_len) + 1);
	nl -= pm_dir_len;
	extraBytes += pm_dir_len;
      }
    }
    gpsee_leaveAutoMonitor(realm->monitors.programModuleDir);

    /* Lines beginning with @ need a fake function name for the table to render right */
    for (nl = strchr(longerror, '\n'); 
	 nl; 
	 nl = strchr(nl + 1, '\n'))
    {
      if (nl[1] == '@')
      {
	/* realloc longerror only if we didn't make enough space above */
	if (extraBytes < (sizeof(NO_FUNCTION_NAME) - 1))
	{
	  char *new_longerror;

	  GPSEE_ASSERT(sizeof(NO_FUNCTION_NAME) < 256);
	  extraBytes += 128 + (128 - extraBytes);
	  new_longerror = JS_malloc(cx, strlen(longerror) + extraBytes);
	  strcpy(new_longerror, longerror);
	  nl = new_longerror + (nl - longerror);
	  JS_free(cx, longerror);
	  longerror = new_longerror;
	}

	memmove(nl + sizeof(NO_FUNCTION_NAME) - 1, nl, strlen(nl) + 1);
	memcpy(nl + 1, NO_FUNCTION_NAME, sizeof(NO_FUNCTION_NAME) - 1);
	extraBytes -= (sizeof(NO_FUNCTION_NAME) - 1);
      }
    }
  }
  done:

  /* Output the exception information :) There are two possible sinks */
  if (longerror && *longerror)
  {
    static const char *columnPrefixes[3] = {"   ","in ","at "};
    char *c;
    int phase;
    if (longerror[strlen(longerror)-1] != '\n')
      gpsee_fprintf(cx, stderr, "\n");

    c = longerror;
    phase = 0;
    while (*c)
    {
      if (phase == 0)
      {
        if (*c == '@')
        {
          *c = '\t';
          phase = 1;
        }
      }
      else if (phase == 1)
      {
        if (*c == ':')
        {
          *c = '\t';
          phase = 2;
        }
      }
      else if (phase == 2)
      {
        if (*c == '\n')
          phase = 0;
      }
      c++;
    }

    /* More output tweaking: pull out @:0 from first exception */
    if (longerror && *longerror)
    {
      char		*nl, *tok;

      nl = strchr(longerror, '\n');
      if (nl)
      {
	tok = (char *)rev_strchr(nl, '\t', longerror);

        if (tok && tok != longerror && strncmp(tok-1, "\t\t0\n", 4) == 0)
        {
          tok--;
          memmove(tok, tok + 1, strlen(tok + 1) + 1);
          memcpy(tok, "\t\t\n", 3);
        }
      }
    }

    gpsee_printTable(cx, stderr, longerror, 3, columnPrefixes, 1, 9);
    JS_free(cx, longerror);
  }

  return JS_TRUE;
}

static const char *spaces(size_t n, char *buf, size_t len)
{
  if (!len)
    return NULL;

  if (n >= len)
    n = len - 1;

  memset(buf, ' ', n);
  buf[n] = (char)0;
  return buf;
}

/** A convenient function for rendering fixed-width font tables. Like this one:
 *  @param  out   The place to print the table to.
 *  @param  s     The text to print.
 *  @param  ncols The number of columns there are.
 *  @param  pfix  A pointer to ncols-many strings that will be used to prefix each column (NULLs allowed.)
 *  @param  shrnk >=0 to identify a shrinkable column (vs COLUMNS) or -1 to disable.
 *  @param  maxshrnk is the SMALLEST the shrnk column will shrink to.
 *  @notes  The buffer may be modified. Repeated lines will be absorbed and a message like "repeats 99 times"
 *          will be printed in their place.
 */
void gpsee_printTable(JSContext *cx, FILE *out, char *s, int ncols, const char **pfix, int shrnk, size_t maxshrnk)
{
  char		spaceBuf[32];
  size_t 	shrinkamount;
  int 		screencols;
  size_t 	cols[ncols];
  size_t 	cols1[ncols];
  size_t 	widecol;
  size_t 	tablewidth;
  int 		i;
  char 		*c;
  
  /* How many characters wide is the terminal? */
  if (getenv("COLUMNS"))
    screencols = atoi(getenv("COLUMNS"));
  else
    screencols = 70;

  /* Measure the width of each column */

  /* Initialize column width values with length of column prefix texts */
  for (i=0; i<ncols; i++)
  {
    cols[i] =
    cols1[i] = 0;//strlen(pfix[i])+1;
  }

  /* Iterate over entire table, incrementing the column length as necessary.
   * cols1[] is used for measurement results on each line, whereas cols[]
   * holds the final result of column width measurement. */
  i = 0;
  c = s;
  widecol = 1; // widest column
  do
  {
    if (*c == '\t')
      i++;
    else if (*c == '\n' || *c == '\0')
    {
      for (i=0; i<ncols; i++)
      {
        if (cols1[i] > cols[i])
        {
          cols[i] = cols1[i];
          if (cols[i] > widecol)
            widecol = cols[i];
        }
        cols1[i] = 0;//strlen(pfix[i])+1;
      }
      i = 0;
    }
    else
      cols1[i]++;
  } while (*(++c));

  /* Determine the width of the entire table */
  tablewidth = 0;
  for (i=0; i<ncols; i++)
    tablewidth += cols[i] + strlen(pfix[i]) + 1;

  /* Several conditions must be checked to activate column abbreviation */
  if (tablewidth > screencols                           // table overflows COLUMNS
  &&  shrnk > -1                                        // shrink enabled by caller
  &&  cols[shrnk] > maxshrnk                            // shrink column has room to shrink
  &&  tablewidth - screencols < cols[shrnk] - maxshrnk) // shrink would get us inside COLUMNS
  {
    shrinkamount = cols[shrnk] - max(maxshrnk, cols[shrnk] - (tablewidth - screencols));
    cols[shrnk] -= shrinkamount;
  }
  else
  {
    shrnk = -1; // disable shrink
    shrinkamount = 0;
  }

  if (widecol)
  {
    int repeats = 0;
    int done = 0;
    char space[widecol+1];
    char *d, *e;
    memset(space, ' ', widecol);
    space[widecol] = '\0';

    /* Begin tokenizing and printing */
    i = 0;
    c = s;
    do
    {
      d = c;
      while (*d != '\0' && *d != '\t' && *d != '\n')
        d++;
      if (*d == '\0') /* End of table */
        done = 1;
      else
      {
        char *e;
        /* Look for repeated lines if we're at the beginning of the line */
        if (i == 0)
        {
          e = strchr(d, '\n');
          size_t walk = e-c;
          repeats = 0;
          /* First, null-terminate this line, saving the old char */
          while (*e && strncmp(c, e+1, walk) == 0)
          {
            repeats++;
            e += walk + 1;
          }
        }
        /* Null-terminate this column. */
        *d = '\0';
      }
      
      /* Shrink this column if necessary */
      if (shrnk == i && strlen(c) > cols[i])
        c += shrinkamount;

      /* Output column prefix, column content, and whitespace padding */
      gpsee_fprintf(cx, out, "%s%s%s", c[0] ? pfix[i] : spaces(strlen(pfix[i]), spaceBuf, sizeof(spaceBuf)), 
                    c, space + widecol - cols[i] + strlen(c) - 1);

      /* Advance the column counter */
      if (++i >= ncols)
      {
        /* End of a line */
        gpsee_fprintf(cx, out, "\n");
        /* Reset column counter */
        i = 0;
        /* Were there repeats? */
        if (repeats > 1)
        {
          gpsee_fprintf(cx, out, "   ^ repeats %d times\n", repeats);
          /* Adjust the cursor to absorb the repeated lines */
          d = e;        /* GCC warns e unused; in fact repeats > 1 guards */
        }
        repeats = 0;
      }

      /* Advance the cursor */
      c = d+1;
    }
    while (!done && *c);
  }
  gpsee_fprintf(cx, out, "\n");
}

int gpsee_isatty(int fd)
{
  const char *s;

  if (fd == STDOUT_FILENO)
  {
    if ((s = getenv("GPSEE_STDOUT_ISATTY")))
      return atoi(s) ? 1 : 0;
  }

  if (fd == STDERR_FILENO)
  {
    if ((s = getenv("GPSEE_STDERR_ISATTY")))
      return atoi(s) ? 1 : 0;
  }

  return isatty(fd);
}

