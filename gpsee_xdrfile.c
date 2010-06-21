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
 *  @version	$Id: gpsee_xdrfile.c,v 1.4 2010/06/14 22:11:59 wes Exp $
 *  @date	May 2009
 *  @file	gpsee_xdrfile.c         JSXDR implementation using stdio.h FILE API (fwrite()
 *                                      et al.) and kernel API memory mapping when possible
 */

static __attribute__((unused)) const char rcsid[]="$Id: gpsee_xdrfile.c,v 1.4 2010/06/14 22:11:59 wes Exp $";

#include "gpsee.h"
#include "jsapi.h"
#include "jsxdrapi.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#define dprintf(a...) do { if (gpsee_verbosity(0) > 2) gpsee_printf(cx, "> "), gpsee_printf(cx, a); } while(0)

/* TODO JSXDR_FREE support? */

/* XDRFile member variables */
struct XDRFile {
  JSXDRState xdr; /* JSXDR base class */
  FILE *f;      /* the underlying file */
  int own_file; /* are we responsible for fclose()? */
#ifdef XDR_USES_MMAP
  void *map;    /* pointer to a real memory map provided by the kernel VM manager */
  void *mapend; /* pointer to the first bit beyond the memory map (mappos <= mapend) */
  size_t maplen;/* length of mapped memory */
  void *mappos; /* pointer to the current read/write position within 'map' */
#endif
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
    default:
      gpsee_throw(cx, "gpsee_XDRNewFile() error: invalid mode argument (%d as opposed to %d, %d, or %d)",
                  mode, JSXDR_ENCODE, JSXDR_DECODE, JSXDR_FREE);
      return NULL;
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
  xdr->raw = NULL;
  xdr->rawlive = 0;
  xdr->rawlen = 0;
  xdr->rawsize = 0;
  xdr->own_file = own_file;

  /* Teach the new XDR object our XDROps implementation */
  xdr->xdr.ops = &xdrfile_ops;

  /* Map file into memory if we're in read mode */
  #ifdef XDR_USES_MMAP
  xdr->map = NULL;
  xdr->maplen = 0;
  xdr->mappos = NULL;
  xdr->mapend = NULL;
  /* Map read-only files into memory if possible */
  if (mode != JSXDR_ENCODE)
  {
    /* Get the size of the file */
    struct stat st;
    if (fstat(fileno(f), &st))
    {
      /* Should never ever ever happen! */
      dprintf("warning: fstat(\"%s\" fd=%d) failed: %m\n", filename, fileno(f));
    }
    else
    {
      /* Map the file into memory! */
      xdr->maplen = st.st_size;
      xdr->map = mmap(NULL, xdr->maplen, PROT_READ, MAP_PRIVATE, fileno(f), 0);
      if (xdr->map == MAP_FAILED)
      {
        dprintf("warning: mmap() failed: %m\n");
      }
      xdr->mappos = xdr->map;
      xdr->mapend = xdr->map + xdr->maplen;
    }
  }
  #endif

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

  /* Un-map the file from memory if necessary */
#ifdef XDR_USES_MMAP
  if (self->map)
  {
    if (munmap(self->map, self->maplen))
      gpsee_log(cx, SLOG_NOTICE, "munmap(%p, %u) failure: %m\n", self->map, self->maplen);
    self->map = NULL;
    self->maplen = 0;
    self->mappos = NULL;
    self->mapend = NULL;
  }
#endif

  /* Release the "raw" memory allocation if necessary */
  if (self->raw)
  {
    JS_free(xdr->cx, self->raw);
    self->raw = NULL;
    self->rawsize = 0;
  }

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
  if (self->rawlive && self->raw && xdr->mode == JSXDR_ENCODE)
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
  /* If mmap is enabled, this is a rather simple case */
  #ifdef XDR_USES_MMAP
  if (self->mappos)
  {
    void *rval = self->mappos;
    /* Bounds check */
    if (self->mappos + len > self->mapend)
    {
      JS_ReportError(xdr->cx, "unexpected end of file");
      return NULL;
    }
    self->mappos = (unsigned char*)self->mappos + len;
    return rval;
  }
  #endif

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
  /* TODO should we commit on tell op? */
  xdrfile_commit_raw(xdr);

  #ifdef XDR_USES_MMAP
  if (self->map)
    return (uint32) (self->mappos - self->map);
  #endif
  return (uint32) ftell(self->f);
}
static JSBool xdrfile_seek (JSXDRState *xdr, int32 offset, JSXDRWhence whence)
{
  xdrfile_commit_raw(xdr);

  #ifdef XDR_USES_MMAP
  if (self->mappos)
  {
    switch (whence)
    {
      /* TODO add seek-underflow checking. right now it will be reported as EOF */
      case JSXDR_SEEK_SET:
        self->mappos = self->map + offset;
        break;
      case JSXDR_SEEK_CUR:
        self->mappos += offset;
        break;
      case JSXDR_SEEK_END:
        self->mappos = self->map + self->maplen - offset;
        break;
      default:
        JS_ReportError(xdr->cx, "invalid \"whence\" value");
        return JS_FALSE;
    }
    /* Boundary check */
    if (self->mappos > self->mapend || self->mappos < self->map)
    {
      JS_ReportError(xdr->cx, "unexpected end of file");
      return JS_FALSE;
    }
    return JS_TRUE;
  }
  #endif

  switch (whence)
  {
    case JSXDR_SEEK_SET:
      whence = SEEK_SET;
      break;
    case JSXDR_SEEK_CUR:
      whence = SEEK_CUR;
      break;
    case JSXDR_SEEK_END:
      whence = SEEK_END;
      break;
    default:
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

  #ifdef XDR_USES_MMAP
  if (self->mappos)
  {
    /* Bounds check */
    if (self->mappos + len > self->mapend)
    {
      JS_ReportError(xdr->cx, "unexpected end of file");
      return JS_FALSE;
    }
    memcpy(self->mappos, buf, len);
    self->mappos += len;
    return JS_TRUE;
  }
  #endif

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

  #ifdef XDR_USES_MMAP
  if (self->mappos)
  {
    /* Bounds check */
    if (self->mappos + len > self->mapend)
    {
      JS_ReportError(xdr->cx, "unexpected end of file");
      return JS_FALSE;
    }
    memcpy(buf, self->mappos, len);
    self->mappos += len;
    return JS_TRUE;
  }
  #endif

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

  #ifdef XDR_USES_MMAP
  if (self->mappos)
  {
    /* Bounds check */
    if (self->mappos + sizeof(uint32) > self->mapend)
    {
      JS_ReportError(xdr->cx, "unexpected end of file");
      return JS_FALSE;
    }
    *(uint32*)self->mappos = *lp;
    self->mappos = (uint32*)self->mappos + 1;
    return JS_TRUE;
  }
  #endif

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

  #ifdef XDR_USES_MMAP
  if (self->mappos)
  {
    /* Bounds check */
    if (self->mappos + sizeof(uint32) > self->mapend)
    {
      JS_ReportError(xdr->cx, "unexpected end of file");
      return JS_FALSE;
    }
    *lp = *(uint32*)self->mappos;
    self->mappos = (uint32*)self->mappos + 1;
    return JS_TRUE;
  }
  #endif

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

