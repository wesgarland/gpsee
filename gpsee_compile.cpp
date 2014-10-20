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
 *
 *  @author	Donny Viszneki, PageMail, Inc., hdon@page.ca
 *  @version	$Id: gpsee_compile.c,v 1.8 2011/12/05 19:13:36 wes Exp $
 *  @file	gpsee_compile.c		Code to compile JavaScript, and to transparently
 *					cache compiled byte code to/from disk.
 */

#include "gpsee.h"
#include "gpsee_private.h"
#include "gpsee_xdrfile.h"
#include <jsxdrapi.h>
#include <jsscript.h>

#if defined(GPSEE_DEBUG_BUILD)
# define dprintf(a...) do { if (gpsee_verbosity(0) >= GPSEE_XDR_DEBUG_VERBOSITY) gpsee_printf(cx, "> "), gpsee_printf(cx, a); } while(0)
#else
# define dprintf(a...) while(0) gpsee_printf(cx, a)
#endif

/** Reformat a filename such that /original/path/original.name becomes /original/path/.original.namec
 *
 *  @param      cx            JSContext needed for JS_strdup() et al
 *  @param      filename      Filename of Javascript code module
 *  @param      buf           Pointer to buffer into which the result is put
 *  @param      buflen        Size of buffer 'buf'
 *
 *  @returns    Zero on success, non-zero on failure
 */
static int make_jsc_filename(JSContext *cx, const char *filename, char *buf, size_t buflen)
{
  char dir[PATH_MAX];

  if (!filename || !filename[0])
    return -1;

  if (!gpsee_dirname(filename, dir, PATH_MAX))
    return -1;

  if ((size_t)snprintf(buf, buflen, "%s/.%sc", dir, gpsee_basename(filename)) >= buflen)
  {
    /* Report paths over PATH_MAX */
    gpsee_log(cx, GLOG_NOTICE, "Would-be compiler cache for source code filename \"%s\" exceeds buffer size (" GPSEE_SIZET_FMT ") bytes)",
              filename, buflen);
    return -1;
  }

  return 0;
}

#define CSERR "error in gpsee_compileScript(\"%s\"): "
/**
 *  Load a JavaScript-language source code file, passing back a compiled JSScript object and a "script object"
 *  instantiated for the benefit of the garbage collector, *or* an error message.
 *
 *  Returns zero on success; non-zero on failure.
 *
 *  For the compiler cache to be available, scriptFilename must be specified. To skip over the shebang (#!) header of a
 *  GPSEE program, the scriptFile parameter must be an open FILE with read access whose seek position reflects the end
 *  of the header and the beginning of Javascript code that Spidermonkey's Javascript parser will not be offended by.
 *  
 *  The JSScript returned has a well defined relationship with the garbage collector via the JSObject passed back
 *  through the scriptObject parameter created with JS_NewScriptObject().
 *  
 *  @param    cx                  The JSAPI context to be associated with the return values of this function.
 *  @param    scriptFilename      Filename to Javascript source code file. This argument is now required.
 *  @param    scriptFile          Open readable stdio FILE representing beginning of Javascript source code.
 *                                Can be non-null.
 *  @param    scriptCode          Actual source code to be compiled. Compiler cache is disabled if this is used.
 *                                Can be non-null.
 *  @param    script              The address where a pointer to the new JSScript will be stored.
 *  @param    scope               The address of the JSObject that will represent the 'this' variable for the script.
 *  @param    scriptObject        The address where a pointer to a new "scrobj" will be stored.
 *
 *  @returns  JS_FALSE on error, JS_TRUE on success
 */
JSBool gpsee_compileScript(JSContext *cx, const char *scriptFilename, FILE *scriptFile, const char *scriptCode,
                           JSScript **script, JSObject *scope, JSObject **scriptObject)
{
  JSBool                rval = JS_TRUE;
  char 			cache_filename[PATH_MAX];
  int 			haveCacheFilename = 0;
  int 			useCompilerCache;
  uint32 		fileHeaderOffset;
  struct stat 		source_st;
  struct stat 		cache_st;
  FILE 			*cache_file = NULL;
  JSBool                own_scriptFile = JS_FALSE;

  *script = NULL;
  *scriptObject = NULL;

  /* If literal script code was supplied ('scriptCode') then we must disable the compiler cache and skip over this
   * file-oriented code.
   */
  if (scriptCode)
  {
    useCompilerCache = 0;
    fileHeaderOffset = 0;
  }
  else
  {
    gpsee_runtime_t *grt;

    /* Open the script file if it hasn't been yet */
    if (scriptFile)
      own_scriptFile = JS_FALSE;
    else {
      if (!(scriptFile = fopen(scriptFilename, "r")))
        return gpsee_throw(cx, CSERR "fopen() error %m", scriptFilename);
      own_scriptFile = JS_TRUE;
    }

    /* Should we use the compiler cache at all? */
    /* Check the compiler cache setting in our gpsee_runtime_t struct */
    grt = (gpsee_runtime_t *)JS_GetRuntimePrivate(JS_GetRuntime(cx));
    useCompilerCache = grt->useCompilerCache;

    if (useCompilerCache)
    {
      /* Only lock script when using the cache - needed then for stability of invariants  */
      if (gpsee_flock(fileno(scriptFile), GPSEE_LOCK_SH) != 0)
      {
	fclose(scriptFile);
	return gpsee_throw(cx, CSERR "lock error %m", scriptFilename);
      }
    }

    /* One criteria we will use to verify that our source code is the same is to check for a "pre-seeked" stdio FILE. If
     * it has seeked/read past the zeroth byte, then we should store that in the cache file along with other metadta. */
    fileHeaderOffset = ftell(scriptFile);
  }

  /* Before we compile the script, let's check the compiler cache */
  if (useCompilerCache && make_jsc_filename(cx, scriptFilename, cache_filename, sizeof(cache_filename)) == 0)
  {
    /* We have acquired a filename */
    JSXDRState *xdr;
    unsigned int fho;

    haveCacheFilename = 1;

    /* Let's compare the last-modification times of the Javascript source code and its precompiled counterpart */
    /* stat source code file */
    if (fstat(fileno(scriptFile), &source_st))
    {
      /* We have already opened the script file, I can think of no reason why stat() would fail. */
      useCompilerCache = 0;
      gpsee_log(cx, GLOG_ERR, AT "could not fstat(filedes %d allegedly \"%s\") (%m)", fileno(scriptFile), scriptFilename);
      goto cache_read_end;
    }

    /* Open the cache file, in a rather specific way */
    if (!(cache_file = fopen(cache_filename, "r")))
    {
      dprintf(AT "could not load from compiler cache \"%s\" (%s)\n", cache_filename, strerror(errno));
      goto cache_read_end;
    }

    if (gpsee_flock(fileno(cache_file), GPSEE_LOCK_SH) != 0)
    {
      gpsee_log(cx, GLOG_ERR, AT "could not lock cache file '%s' (%m)", cache_filename);
      fclose(cache_file);
      cache_file = NULL;
      goto cache_read_end;
    }

    if (fstat(fileno(cache_file), &cache_st))
    {
      dprintf(AT "could not stat() compiler cache \"%s\"\n", cache_filename);
      fclose(cache_file);
      cache_file = NULL;
      goto cache_read_end;
    }

    /* Compare the owners and privileges of the source file and the cache file */
    if (source_st.st_uid != cache_st.st_uid
    ||  source_st.st_gid != cache_st.st_gid
    ||  ((source_st.st_mode & 0666) != (cache_st.st_mode & 0777)))
    {
      /* This invalidates the cache file. We will try to delete the cache file after goto cache_read_end */
      fclose(cache_file);
      cache_file = NULL;
      if (gpsee_verbosity(0))
	gpsee_log(cx, GLOG_NOTICE,
          "ownership/mode on cache file \"%s\" (%d:%d@0%o) does not match the expectation (%d:%d@0%o).",
          cache_filename,
          (int)cache_st.st_uid,  (int)cache_st.st_gid,  (unsigned int)cache_st.st_mode & 0777,
          (int)source_st.st_uid, (int)source_st.st_gid, (unsigned int)source_st.st_mode & 0666);
      goto cache_read_end;
    }

    /* Compare last modification times to see if the compiler cache has been invalidated by a change to the
     * source code file */
    if (cache_st.st_mtime < source_st.st_mtime)
      goto cache_read_end;

    /* Let's ask Spidermonkey's XDR API for a deserialization context */
    if ((xdr = gpsee_XDRNewFile(cx, JSXDR_DECODE, cache_filename, cache_file)))
    {
      uint32 ino, size, mtime, cstrRutf8;
      /* These JS_XDR*() functions de/serialize (in our case: deserialize) data to/from the underlying cache file */
      /* We match some metadata embedded in the cache file against the source code file as it exists now to determine
       * if the source code file has been changed more recently than its compiler cache file was updated. */
      JS_XDRUint32(xdr, &ino);
      JS_XDRUint32(xdr, &size);
      JS_XDRUint32(xdr, &mtime);
      JS_XDRUint32(xdr, &fho);
      /* SpiderMonkey has an option to interpret "C strings" (ostensibly, 8-bit character strings passed between
       * spidermonkey and external code) as UTF-8 (as opposed to some default character encoding I have been un-
       * able to ascertain.) This option can only be set *before* the instantiation of the SpiderMonkey runtime
       * singleton, and so is constant during the lifecycle of the main program module. It is thought by Wes that
       * it may affect the XDR form of a compiled script, although I am not as sure about that. Below, then, we
       * invalidate any compiler cache resulting from a different "cstrRutf8" setting. */
      JS_XDRUint32(xdr, &cstrRutf8);
      /* Compare the results of the deserialization */
      if (ino != (uint32)source_st.st_ino || size != (uint32)source_st.st_size || mtime != (uint32)source_st.st_mtime
      ||  fileHeaderOffset != fho || cstrRutf8 != (uint32)JS_CStringsAreUTF8())
      {
        /* Compiler cache invalidated by change in inode / filesize / mtime */
        gpsee_log(cx, GLOG_DEBUG, "cache file \"%s\" invalidated"
                  " (inode %d %d size %d %d mtime %d %d fho %d %d cstrRutf8 %d %d)",
                  cache_filename,
                  ino, (int)source_st.st_ino,
                  size, (int)source_st.st_size,
                  mtime, (int)source_st.st_mtime,
                  fho, fileHeaderOffset,
                  cstrRutf8, JS_CStringsAreUTF8());
      } else {
        /* Now we attempt to deserialize a JSScript */
        if (!JS_XDRScript(xdr, script))
        {
          const char *exception = "(exception missing!)";
          jsval v;
          /* We should have an exception waiting for us */
          if (JS_GetPendingException(cx, &v))
          {
            JSString *exstr;
            JS_ClearPendingException(cx);
            exstr = JS_ValueToString(cx, v);
            exception = exstr ? JS_GetStringBytes(exstr) : "(nothing from JS_ValueToString())";
          }

          /* Failure */
          gpsee_log(cx, GLOG_NOTICE, "JS_XDRScript() failed deserializing \"%s\" from cache file \"%s\": \"%s\"", scriptFilename,
                    cache_filename, exception);
        } else {
          /* Success */
	  if (gpsee_verbosity(0) >= GPSEE_XDR_DEBUG_VERBOSITY)
	    gpsee_log(cx, GLOG_DEBUG, "JS_XDRScript() succeeded deserializing \"%s\" from cache file \"%s\"", scriptFilename,
		      cache_filename);
        }
      }
      /* We are done with our deserialization context */
      JS_XDRDestroy(xdr);
      fclose(cache_file);
      cache_file = NULL;
    }
  }

  cache_read_end:
  errno = 0;	/* Reset errno; diagnostics from cache misses etc. are not useful */

  /* Was the precompiled script thawed from the cache? If not, we must compile it */
  if (!*script)
  {
    /* Actually compile the script! */
    *script = scriptCode ?
      JS_CompileScript(cx, scope, scriptCode, strlen(scriptCode), scriptFilename, 0):
      JS_CompileFileHandle(cx, scope, scriptFilename, scriptFile);  

    if (!*script) {
      rval = JS_FALSE;
      goto finish;
    }

    /* Should we freeze the compiled script for the compiler cache? */
    if (useCompilerCache && haveCacheFilename)
    {
      JSXDRState *xdr;
      int cache_fd;

      if (unlink(cache_filename))
      {
        if (errno != ENOENT)
        {
          useCompilerCache = 0;
          /* Report that we could not unlink the cache file */
	  if (gpsee_verbosity(0))
	    gpsee_log(cx, GLOG_NOTICE, "unlink(\"%s\") error: %m", cache_filename);
        }
      }
      errno = 0;

      /* Open the cache file atomically; fails on collision with other process */
      mode_t oldumask = umask(~0666);
      if ((cache_fd = open(cache_filename, O_WRONLY|O_CREAT|O_EXCL, source_st.st_mode & 0666)) < 0)
      {
        umask(oldumask);
	if (gpsee_verbosity(0))
	  gpsee_log(cx, GLOG_NOTICE, "Could not create compiler cache '%s' (%m)", cache_filename);
        goto cache_write_end;
      }
      umask(oldumask);

      /* Acquire write lock for file */
      if (gpsee_flock(cache_fd, GPSEE_LOCK_EX) != 0)
      {
	if (gpsee_verbosity(0))
	  gpsee_log(cx, GLOG_NOTICE, "Could not lock compiler cache '%s'", cache_filename);
        goto cache_write_bail;
      }

      /* Ensure the owner of the cache file is the owner of the source file */
      if (fchown(cache_fd, source_st.st_uid, source_st.st_gid))
      {
	if (gpsee_verbosity(0))
	  gpsee_log(cx, GLOG_NOTICE, "Could not set compiler cache ownership for '%s' to uid/gid %i/%i (%m)", cache_filename, (int)source_st.st_uid, (int)source_st.st_gid);
        goto cache_write_bail;
      }

      /* Truncate file again now that we have the lock */
      if (ftruncate(cache_fd, 0))
      {
        close(cache_fd);
        gpsee_log(cx, GLOG_NOTICE, "Could not truncate compiler cache file '%s' (%m)", cache_filename);
        goto cache_write_bail;
      }

      /* Create a FILE* for XDR */
      if ((cache_file = fdopen(cache_fd, "w")) == NULL)
      {
        close(cache_fd);
        gpsee_log(cx, GLOG_NOTICE, "Could not write compiler cache file '%s' (%m)", cache_filename);
        goto cache_write_bail;
      }
      /* Let's ask Spidermonkey's XDR API for a serialization context */
      if ((xdr = gpsee_XDRNewFile(cx, JSXDR_ENCODE, cache_filename, cache_file)))
      {
        /* See above in the deserialization routine for a description of "cstrRutf8." */
        uint32 cstrRutf8 = JS_CStringsAreUTF8();
        /* We will mark the file with some data about its source code file to aid in detecting whether the source code
         * has changed more recently than it has been recompiled to its cache */
        JS_XDRUint32(xdr, (uint32*)&source_st.st_ino);
        JS_XDRUint32(xdr, (uint32*)&source_st.st_size);
        JS_XDRUint32(xdr, (uint32*)&source_st.st_mtime);
        JS_XDRUint32(xdr, &fileHeaderOffset);
        /* The return value of JS_CStringsAreUTF8() affects the XDR subsystem */
        JS_XDRUint32(xdr, &cstrRutf8);
        /* Now we attempt to serialize a JSScript to the compiler cache file */
        if (!JS_XDRScript(xdr, script))
        {
          const char *exception = "(exception missing!)";
          jsval v;
          /* We should have an exception waiting for us */
          if (JS_GetPendingException(cx, &v))
          {
            JSString *exstr;
            JS_ClearPendingException(cx);
            exstr = JS_ValueToString(cx, v);
            exception = exstr ? JS_GetStringBytes(exstr) : "(nothing from JS_ValueToString())";
          }

          /* Failure */
          gpsee_log(cx, GLOG_NOTICE, "JS_XDRScript() failed serializing \"%s\" to cache file \"%s\": \"%s\"", scriptFilename,
                    cache_filename, exception);
        } else {
          /* Success */
	  if (gpsee_verbosity(0) >= GPSEE_XDR_DEBUG_VERBOSITY)
	    gpsee_log(cx, GLOG_DEBUG, "JS_XDRScript() succeeded serializing \"%s\" to cache file \"%s\"",
		      scriptFilename, cache_filename);
        }
        /* We are done with our serialization context */
        JS_XDRDestroy(xdr);
      }

      /* We'll close the cache file's FILE handle. fclose(3) also calls close(2) on cache_fd for us */
      fclose(cache_file);
      cache_file = NULL;
      goto cache_write_end;

      cache_write_bail:
      unlink(cache_filename);
      if (cache_file)
	fclose(cache_file);
      else if (cache_fd >= 0)
	close(cache_fd);
      cache_file = NULL;
    }
  }

  cache_write_end:
  /* We must associate a GC object with the JSScript to protect it from the GC */
  if ((*script)->length < 2)
  {
    if (gpsee_verbosity(0))
      gpsee_log(cx, GLOG_DEBUG, "Script at '%s' is empty", scriptFilename);
    goto finish;
  }

  *scriptObject = JS_NewScriptObject(cx, *script);
  if (!*scriptObject)
    goto fail;

  goto finish;

  fail:
  rval = JS_FALSE;

  finish:
  if (own_scriptFile)
    fclose(scriptFile);
  return rval;
}
