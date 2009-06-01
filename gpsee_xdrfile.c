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
 *  @author	Donny Viszneki, PageMail, Inc., donny.viszneki@gmail.com
 *  @version	$Id:$
 *  @date	May 2009
 *  @file	gpsee_xdrfile.c         JSXDR implementation using stdio.h FILE API (fwrite()
 *                                      et al.) and kernel API memory mapping when possible
 */

static __attribute__((unused)) const char rcsid[]="$Id:$";

#include "jsapi.h"
#include "jsxdrapi.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

/* TODO JSXDR_FREE support */

/* XDRFile member variables */
struct XDRFile {
  JSXDRState xdr; /* JSXDR base class */
  FILE *f;      /* the underlying file */
  int own_file; /* are we responsible for fclose()? */
  void *map;    /* pointer to a real memory map provided by the kernel VM manager */
  void *raw;    /* 'raw' chunk of memory mapped to the underlying media */
  int rawlen;   /* requested 'raw' chunk size */
  int rawsize;  /* real size of 'raw' buffer */
  int rawlive;  /* non-zero if the 'raw' buffer needs to be committed to the disk before the next operation */
};

/* XDRFile member functions */
static JSBool xdrfile_get32 (JSXDRState *xdr, uint32 *lp);
static JSBool xdrfile_set32 (JSXDRState *xdr, uint32 *lp);
static JSBool xdrfile_getbytes (JSXDRState *xdr, char *buf, uint32 len);
static JSBool xdrfile_setbytes (JSXDRState *xdr, char *buf, uint32 len);
static void * xdrfile_raw (JSXDRState *xdr, uint32 len);
static JSBool xdrfile_seek (JSXDRState *xdr, int32 offset, JSXDRWhence whence);
static uint32 xdrfile_tell (JSXDRState *xdr);
static void xdrfile_finalize (JSXDRState *xdr);
inline static void xdrfile_commit_raw (JSXDRState *xdr);

/* XDRFile vtable */
static JSXDROps xdrfile_ops = {
  xdrfile_get32,   
  xdrfile_set32,   
  xdrfile_getbytes,  
  xdrfile_setbytes,
  xdrfile_raw,     
  xdrfile_seek,    
  xdrfile_tell,      
  xdrfile_finalize
};

/* Convenience macro for XDRFile member function implementations */
#define self ((struct XDRFile*)xdr)

/* XDRFile constructor */
JSXDRState * gpsee_XDRNewFile(JSContext *cx, JSXDRMode mode, const char *filename, FILE *f)
{
  struct XDRFile *xdr;
  const char *fopen_mode;
  int own_file = 0;

  switch (mode)
  {
    case JSXDR_ENCODE:
      fopen_mode = "w";
      break;
    case JSXDR_DECODE:
      fopen_mode = "r";
      break;
    case JSXDR_FREE:
      fopen_mode = "w+";
      break;
  }

  if (!f)
  {
    if (!filename || !(f = fopen(filename, fopen_mode)))
      return NULL;
    own_file = 1;
  }

  /* Allocate our XDRState and also our internal XDRFile struct */
  xdr = (struct XDRFile*) JS_malloc(cx, sizeof(struct XDRFile));
  if (!xdr)
  {
    if (own_file)
      fclose(f);
    return NULL;
  }

  /* Initialize our XDRState */
  JS_XDRInitBase(&xdr->xdr, mode, cx);

  /* Initialize our internal XDRFile members */
  xdr->f = f;
  xdr->map = NULL;
  xdr->raw = NULL;
  xdr->rawlive = 0;
  xdr->rawlen = 0;
  xdr->rawsize = 0;
  xdr->own_file = own_file;

  /* Teach the new XDR object our XDROps implementation */
  xdr->xdr.ops = &xdrfile_ops;

  return (JSXDRState*) xdr;
}

/* Return the file descriptor of the file backing this XDR */
int gpsee_XDRFileNo(JSXDRState *xdr)
{
  if (xdr->ops != &xdrfile_ops)
    return -1;
  return fileno(self->f);
}

/* XDRFile destructor */
static void xdrfile_finalize (JSXDRState *xdr)
{
  /* Do we have a raw chunk needing to be freed? */
  xdrfile_commit_raw(xdr);

  /* Close the underlying file */
  if (self->f)
  {
    if (self->own_file)
      fclose(self->f);
    self->f = NULL;
  }
}
/* jorendorff describes how the 'raw' operation is supposed to work:
 *    <jorendorff> hdon: so if you're writing ops that will be writing to a file, your file_raw function will need to
 *    do this:
 *    <jorendorff> hdon: if xdr->mode is ENCODE, malloc the requested number of bytes, store that pointer and size
 *    somewhere, and return the pointer.  Then before the next file operation, write that buffer to your file and free
 *    it.
 *    <jorendorff> hdon: If xdr->mode is DECODE, read the next n bytes into a temporary buffer and return that. (You
 *    have to remember to free the buffer later.)
 */
inline static void xdrfile_commit_raw (JSXDRState *xdr)
{
  /* Do we have a raw chunk that needs to be committed? */
  if (self->raw && xdr->mode == JSXDR_ENCODE && self->rawlive)
  {
    /* Write raw chunk to file */
    fwrite(self->raw, 1, self->rawlen, self->f);

    /* Mark self 'raw' chunk as committed */
    self->rawlive = 0;
  }
}

/* Return a chunk of memory which is mapped to the underlying media */
static void * xdrfile_raw (JSXDRState *xdr, uint32 len)
{
  /* Commit 'raw' chunk if necessary */
  xdrfile_commit_raw(xdr);

  /* Do we need a bigger buffer than what we've already allocated? */
  if (len > self->rawsize)
  {
    /* Free any previous buffer */
    if (self->raw) JS_free(xdr->cx, self->raw);

    /* Allocate a new buffer */
    self->raw = (void*)JS_malloc(xdr->cx, len);

    /* Make sure our buffer was successfully allocated */
    if (!self->raw)
    {
      self->rawlen = 0;
      self->rawlive = 0;
      self->rawsize = 0;
      return NULL;
    }

    /* rawsize reflects the size of our actual buffer */
    self->rawsize = len;
  }

  /* rawlen reflects the requested 'raw' chunk size */
  self->rawlen = len;

  /* mark this 'raw' buffer as needing to be committed */
  self->rawlive = 1;

  /* If we're deserializing, we need to read the file into the buffer */
  if (xdr->mode != JSXDR_ENCODE)
  {
    int n;
    /* Fill our 'raw' chunk from the underlying file */
    n = fread(self->raw, 1, len, self->f);
    if (n < len)
    {
      JS_ReportError(xdr->cx, "unexpected end of file");
      return NULL;
    }
  }

  return self->raw;
}

static uint32 xdrfile_tell (JSXDRState *xdr)
{
  xdrfile_commit_raw(xdr);
  return ftell(self->f);
}
static JSBool xdrfile_seek (JSXDRState *xdr, int32 offset, JSXDRWhence whence)
{
  xdrfile_commit_raw(xdr);

  if (whence == JSXDR_SEEK_CUR) whence = SEEK_CUR;
  else if (whence == JSXDR_SEEK_SET) whence = SEEK_SET;
  else if (whence == JSXDR_SEEK_END) whence = SEEK_END;
  else {
    JS_ReportError(xdr->cx, "invalid \"whence\" value");
    return JS_FALSE;
  }
  if (fseek(self->f, offset, whence))
  {
    JS_ReportError(xdr->cx, "seek operation failed with error %d: %s", errno, strerror(errno));
    return JS_FALSE;
  }
  return JS_TRUE;
}
static JSBool xdrfile_setbytes (JSXDRState *xdr, char *buf, uint32 len)
{
  int n;
  xdrfile_commit_raw(xdr);

  if (self->f == NULL)
    return JS_FALSE;
  n = fwrite(buf, 1, len, self->f);
  if (n < len)
  {
    JS_ReportError(xdr->cx, "unexpected end of file");
    return JS_FALSE;
  }
  return JS_TRUE;
}
static JSBool xdrfile_getbytes (JSXDRState *xdr, char *buf, uint32 len)
{
  int n;
  xdrfile_commit_raw(xdr);

  if (self->f == NULL)
    return JS_FALSE;
  n = fread(buf, 1, len, self->f);
  if (n < len)
  {
    JS_ReportError(xdr->cx, "unexpected end of file");
    return JS_FALSE;
  }
  return JS_TRUE;
}
static JSBool xdrfile_set32 (JSXDRState *xdr, uint32 *lp)
{
  int n;
  xdrfile_commit_raw(xdr);

  if (self->f == NULL)
    return JS_FALSE;
  n = fwrite(lp, 1, 4, self->f);
  if (n != 4)
  {
    JS_ReportError(xdr->cx, "unexpected end of file");
    return JS_FALSE;
  }
  return JS_TRUE;
}
static JSBool xdrfile_get32 (JSXDRState *xdr, uint32 *lp)
{
  int n;
  xdrfile_commit_raw(xdr);

  if (self->f == NULL)
    return JS_FALSE;
  n = fread(lp, 1, 4, self->f);
  if (n != 4)
  {
    JS_ReportError(xdr->cx, "unexpected end of file");
    return JS_FALSE;
  }
  return JS_TRUE;
}

/* This shouldn't matter, but let's play nice */
#undef self

