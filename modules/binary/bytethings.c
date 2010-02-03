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
 * ***** END LICENSE BLOCK ***** */

/**
 *  @file	bytethings.c		Support routines for "Byte Things" -- ByteString, ByteArray, etc.
 *  @author	Wes Garland
 *              PageMail, Inc.
 *		wes@page.ca
 *  @date	Jan 2008
 *  @version	$Id: bytethings.c,v 1.9 2009/10/29 18:35:05 wes Exp $
 */

static const char __attribute__((unused)) rcsid[]="$Id: bytethings.c,v 1.9 2009/10/29 18:35:05 wes Exp $";

#include "gpsee.h"
#include "binary_module.h"

/** BE and LE flavours of UTF-16 do not emit the BOM. JS Strings are BOM-less and in machine order. */
#if defined(_BIG_ENDIAN) || (defined(BYTE_ORDER) && defined(BIG_ENDIAN) && BYTE_ORDER == BIG_ENDAN)
# define DEFAULT_UTF_16_FLAVOUR "utf-16BE"
#else
# define DEFAULT_UTF_16_FLAVOUR "utf-16LE"
#endif

/** A neutral character set that anything can be transcoded to/from */
#define NEUTRAL_CHARSET	"utf-8"

/** Returns a pointer to a byteString_handle_t, or NULL on error
 *  @param      cx          Your JSContext
 *  @param      obj         ByteString instance
 *  @param      claspp      A pointer to either byteString_clasp, a pointer to byteArray_clasp, a pointer to NULL, or the
 *                          value NULL itself. In the first two cases, the type is constrained to the given type. If a 
 *                          pointer to NULL is the type is stored at the given address.
 *  @param      methodName  Name of ByteString member method (used for error reporting)
 *  @returns    A pointer to a byteString_handle_t or NULL if an exception is thrown
 *  @throws     type
 *
 *  An error will be thrown if 'obj' has no "private" data, if obj's clasp does not match that pointed to by claspp (if it
 *  is not NULL and does not point to NULL) or if obj's class is neither byteString_clasp nor byteArray_clasp.
 */
inline byteThing_handle_t * byteThing_getHandle(JSContext *cx, JSObject *obj, JSClass **claspp, const char const * methodName)
{
  JSClass *clasp;
  const char *cn;
  const char *cn2;
  byteThing_handle_t * hnd;

  /* Determine a "requested class name" to be used in naming the exception to be thrown */
  cn = claspp && *claspp ? (*claspp)->name : MODULE_ID ".Binary";

  /* Check that obj is not NULL */
  if (!obj)
  {
    gpsee_throw(cx, "%s.%s.internalerror: byteThing_getHandle() called with NULL JSObject pointer!", cn, methodName);
    return NULL;
  }

  /* Retrieve 'obj's class */
  clasp = JS_GET_CLASS(cx, obj);
  if (!clasp)
  {
    gpsee_throw(cx, "%s.%s.internalerror: object %p has no class!", cn, methodName, obj);
    return NULL;
  }
  cn2 = clasp->name;

  /* If claspp is not NULL and does not point to a NULL value, then obj's class must match the class pointed to by claspp.
   * If claspp is NULL or it points to a NULL value, then obj's class must match either byteString_clasp or byteArray_clasp. */
  if ((claspp && *claspp && clasp != *claspp) || ((!claspp || !*claspp) && clasp != byteString_clasp && clasp != byteArray_clasp))
  {
    if (clasp != *claspp)
    {
      gpsee_throw(cx, MODULE_ID ".%s.%s.invalid: %s instance method cannot be applied to a %s", cn, methodName, cn, cn2);
      return NULL;
    }
  }

  /* Fetch our private data (byteThing_handle_t*, our return value) */
  hnd = JS_GetPrivate(cx, obj);
  if (!hnd)
  {
    gpsee_throw(cx, MODULE_ID ".%s.%s.internalerror: native instance method applied to object with good class but no private data",
                cn, methodName);
    return NULL;
  }

  /* Success! */
  /* Return clasp if they asked for it */
  if (claspp && !*claspp)
    *claspp = clasp;
  /* Return byteThing_handle_t */
  return hnd;
}
/** Copy a JavaScript array-of-numbers into an unsigned char buffer.
 *  If we return successfully, buf is allocated with JS_malloc, unless it is
 *  zero bytes long in which case it will be NULL. If we do not return successfully,
 *  buf will be free and have an unspecified value.
 *
 *  @throws	.array.range If any array element is <0 or >255
 *  @param      cx
 *  @param	arr	[in]	Array to copy from
 *  @param      start   [i/o]   Specifies an offset into 'arr' from which to start processing array members. Useful
 *                              for successive calls on the same array when providing your own buffer.
 *  @param      bufp    [i/o]   If 'bufp' points to a non-NULL address, it is assumed that our result buffer has
 *                              already been allocated, and the value pointed to by 'lenp' is the number of results
 *                              we can store there before returning. Any value at bufp[n] will be derived from the
 *                              value arr[start+n], so on successive calls you should increment the address pointed
 *                              to by 'bufp' by 'start' bytes. If 'bufp' points to a NULL address, then that
 *                              address will be set to the address of a newly created buffer that the caller is
 *                              responsible for freeing. This buffer is filled with result bytes from reading the
 *                              array 'arr'.
 *  @param      lenp    [i/o]   If 'bufp' points to a non-NULL address, this function will not write to any byte past
 *                              the address *bufp + *lenp. If the address pointed to by 'bufp' is NULL when this
 *                              function is called then the value pointed to by 'lenp' will contain the size of the 
 *                              buffer represented by 'bufp' when this function returns. In either case, the value
 *                              pointed to by 'lenp' will
 *                              contain the number of bytes returned by this function.
 *  @param      more    [out]   If 'bufp' points to a non-NULL address, the value pointed to by 'more' will be set
 *                              to. NULL is an acceptable
 *                              value, since you know the array has been exhausted if 'bufp' points to a NULL value.
 *                              JS_FALSE or if the end of the array was reached, or JS_TRUE if it was not.
 *  @param      throwPrefix [in]
 *  @returns	JS_TRUE on success
 */
JSBool copyJSArray_toBuf(JSContext *cx, JSObject *arr, size_t start, unsigned char **bufp, size_t *lenp, JSBool *more,
                         const char *throwPrefix)
{
  /* We can't use enumeration as the trivial case, because
   * enumeration order is not necessarily the same as array order.
   */

  /* Ok, a surprisingly large amount of book-keeping is done here:
   *
   *   *lenp    This is potentially the length of a buffer we are not free to reallocate
   *   bufLen   This is always the real size of the result buffer. This will always reflect
   *            the highest value of 'i' we'll be doing in this call. This value may change
   *            when then array size changes. It should reflect the maximum amount of work
   *            can feasibly do in this call. I guess this means either arrLen or *lenp
   *   arrLen   This stores the array length of 'arr'
   */
  jsuint        arrLen;       /* stores the array length of 'arr' */
  jsuint        bufLen;       /* stores the length of the buffer 'buf' */
  jsuint        i;            /* iterates from 0 to 'stop' */
  jsuint        stop;         /* stores one greater than the index of the last 'buf' member we're asked to fulfill.
                               * this value may come from *lenp, or arrLen. */
  JSBool        ownBuffer;    /* stores whether we have allocated our own buffer, or one has been provided by the
                               * caller. in the former case, we can realloc the buffer to any size. in the latter
                               * case, we cannot realloc, and we must respect the buffer length (*lenp). */
  unsigned char *buf = *bufp;

  /* Has the caller provided our buffer? */
  ownBuffer = buf ? JS_FALSE : JS_TRUE;

  /* Get the array length of 'arr' */
  if (JS_GetArrayLength(cx, arr, &arrLen) == JS_FALSE)
    return JS_FALSE;

  /* Well, this would be boring, but it can happen */
  if (arrLen == 0)
  {
    /* If we were not given a buffer, we are obliged to return a NULL pointer for the pointer we're expected to give */
    if (ownBuffer)
      *bufp = NULL;
    *lenp = 0;
    /* We have exhausted the array's contents :) */
    if (more)
      *more = JS_FALSE;
    return JS_TRUE;
  }

  /* Provide our own buffer? */
  if (ownBuffer)
  {
    /* Create a buffer long enough to represent one byte from each member of the array */
    bufLen = arrLen;
    /* Process all members */
    stop = bufLen;
    /* Allocate our buffer */
    buf = JS_malloc(cx, bufLen);
    if (!buf)
      goto failOut;
  }
  /* Caller provided buffer? */
  else
  {
    /* Know our buffer's size */
    bufLen = *lenp;
    /* Exceed neither the length of 'buf' nor the array length of 'arr' */
    stop = min(bufLen, arrLen);
  }

  /* Iterate over some portion (probably all) of the array contents */
  for (i=0; i<stop; i++)
  {
    jsval       v;
    jsdouble    d;

    /* Fetch array member from arr */

    /* jsval from array index */
    if (!JS_GetElement(cx, arr, start+i, &v))
      goto failOut;

    /* jsdouble from jsval */
    if (JS_ValueToNumber(cx, v, &d) == JS_FALSE)
      goto failOut;

    /* Validate array member */
    if (d != (unsigned char)d)
    {
      gpsee_throw(cx, "%s.array.range: element " GPSEE_SIZET_FMT " (value %lf) does not fit in a byte", throwPrefix, start+i, d);
      goto failOut;
    }

    /* Store result */
    buf[i] = (unsigned char) d;

    /* Check for asynchronous changes to 'arr's length
     * this may seem sub-optimal, but we have to consider the
     * MT case where the array is being length-modified in another
     * thread. Ouch. */
    if (JS_GetArrayLength(cx, arr, &arrLen) == JS_FALSE)
      goto failOut;

    if (arrLen != bufLen)
    {
      /* Reallocate our buffer if we can */
      if (ownBuffer)
      {
        buf = JS_realloc(cx, buf, arrLen);
        if (!buf)
        {
          JS_ReportOutOfMemory(cx);
          goto failOut;
        }
        bufLen = arrLen;
        stop = arrLen;
      }
      else
      {
        /* If we're using a caller-provided buffer, we need to ensure that we don't overrun it */
        stop = min(bufLen, arrLen); /* bufLen should be equal to *lenp */
      }
    }
  }/* i++ */

  /* Return time! */

  if (ownBuffer)
    /* Buffer! */
    *bufp = buf;
  /* Array contents exhausted? */
  if (more)
    *more = stop > arrLen ? JS_TRUE : JS_FALSE;
  /* Buffer length! */
  *lenp = stop;
  return JS_TRUE;

  /* Failure time */

  failOut:
  if (ownBuffer)
    free(buf);
  else
    *bufp = NULL;
  *lenp = 0;
  return JS_FALSE;
}

/**
 *  Transcode from one character encoding to another. This routine works on "buffers", i.e.
 *  C arrays of unsigned char. 
 *
 *  If sourceCharset and targetCharset point to the same address, or point to equivalent strings (as
 *  judged by strcasecmp()) then no conversion is performed, the buffer is only copied.
 *
 *  @param	cx			[in]  JavaScript context
 *  @param	targetCharset		[in]  Target character set or NULL for utf-16
 *  @param	sourceCharset		[in]  Source character set or NULL for utf-16
 *  @param     	outputBuffer_p		[out] Result of conversion, placed in buffer allocated with JS_malloc()
 *  @param	outputBufferLength_p	[out] Number of bytes in outputBuffer_p
 *  @param	inputBuffer		[in]  Data to transcode
 *  @param	inputBufferLength	[in]  Number of bytes in inputBuffer
 *  @param	throwPrefix		[in]  Prefix to prepend to any calls to gpsee_throw().
 *  @returns	JS_TRUE on success, JS_FALSE on throw
 */
JSBool transcodeBuf_toBuf(JSContext *cx, const char *targetCharset, const char *sourceCharset, 
			  unsigned char **outputBuffer_p, size_t *outputBufferLength_p, 
			  const unsigned char *inputBuffer, size_t inputBufferLength,
			  const char *throwPrefix)
{
#if defined(HAVE_ICONV)
  iconv_t	cd;
  const char	*inbuf;
  char		*outbuf, *outbufStart;
  size_t	inbytesleft, outbytesleft;
  size_t	allocBytes, result;
  jsrefcount	depth;
#endif
  size_t	approxChars;

  /* Empty string? */
  if (inputBufferLength == 0) {
    *outputBuffer_p = NULL;
    *outputBufferLength_p = 0;
    return JS_TRUE;
  }

  if ((sourceCharset == targetCharset) || (sourceCharset && targetCharset && (strcasecmp(sourceCharset, targetCharset) == 0)))
  {
    *outputBuffer_p = JS_malloc(cx, inputBufferLength);
    if (!*outputBuffer_p)
    {
      JS_ReportOutOfMemory(cx);
      return JS_FALSE;
    }

    memcpy(*outputBuffer_p, inputBuffer, inputBufferLength);
    *outputBufferLength_p = inputBufferLength;

    return JS_TRUE;
  }

  if (!sourceCharset || (strcasecmp(sourceCharset, DEFAULT_UTF_16_FLAVOUR) == 0))
    approxChars = inputBufferLength / 2;
  else
    approxChars = inputBufferLength;

  if (!sourceCharset)
    sourceCharset = DEFAULT_UTF_16_FLAVOUR;
  if (!targetCharset)
    targetCharset =  DEFAULT_UTF_16_FLAVOUR;

#if !defined(HAVE_ICONV)
  return gpsee_throw(cx, "%s.transcode.iconv.missing: Could not transcode charset %s to %s charset; GPSEE was compiled without iconv "
		     "support.", throwPrefix, sourceCharset, targetCharset);
#else
  depth = JS_SuspendRequest(cx);
  cd = iconv_open(targetCharset, sourceCharset);
  JS_ResumeRequest(cx, depth);
  
#if !defined(HAVE_IDENTITY_TRANSCODING_ICONV)
  /* Some iconv() implementations cannot do identity or near-identity transcode operations,
   * such as transcoding from UTF-16BE to UCS2 or UCS2 to UTF-16. Solaris 10 is an offender.
   * 
   * In order to make this less painful for the programmer, we make it more expensive by 
   * transcoding to/from an intermediate neutral character set, such as UCS4 or UTF-8.
   */
  if (cd == (iconv_t)-1)
  {
    iconv_t	cd1;
    iconv_t	cd2; 

    /* Here we'll iconv_open() just as a feature test */
    depth = JS_SuspendRequest(cx);
    if ((cd1 = iconv_open(targetCharset, NEUTRAL_CHARSET)) != (iconv_t)-1)
      iconv_close(cd1);

    if ((cd2 = iconv_open(NEUTRAL_CHARSET, sourceCharset)) != (iconv_t)-1)
      iconv_close(cd2);
    JS_ResumeRequest(cx, depth);

    if (cd1 != (iconv_t)-1 && cd2 != (iconv_t)-1)
    {
      JSBool		b;
      unsigned char	*ibufp;
      size_t		ibufsz;

      if (transcodeBuf_toBuf(cx, NEUTRAL_CHARSET, sourceCharset, &ibufp, &ibufsz, inputBuffer, inputBufferLength, throwPrefix) == JS_TRUE)
      {
	b = transcodeBuf_toBuf(cx, targetCharset, NEUTRAL_CHARSET, outputBuffer_p, outputBufferLength_p, ibufp, ibufsz, throwPrefix);
	JS_free(cx, ibufp);
	if (b == JS_TRUE)
	  return JS_TRUE;
      }      

      return JS_TRUE;
    }
  }
#endif

  if (cd == (iconv_t)-1)
    return gpsee_throw(cx, "%s.transcode: Cannot transcode from %s to %s", throwPrefix, sourceCharset, targetCharset);

  allocBytes   = (approxChars) + (approxChars / 8) + 32;	/* WAG */
  outbufStart  = JS_malloc(cx, allocBytes);
  if (!outbufStart)
  {
    iconv_close(cd);
    JS_ReportOutOfMemory(cx);
    return JS_FALSE;
  }
  outbytesleft = allocBytes;

  inbytesleft  = inputBufferLength;
  inbuf 	 = (const char*) inputBuffer;
  outbuf	 = outbufStart;

  do
  {
    depth = JS_SuspendRequest(cx);
    result = iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
    JS_ResumeRequest(cx, depth);

    if (result == -1)
    {
      switch(errno)
      {
	case E2BIG:
	{
	  char *newBuf;
          size_t newAllocBytes;

          /* Estimate a new buffer size */
          newAllocBytes = allocBytes + inbytesleft + inbytesleft / 4 + 32; /* WAG -- +32 implies output charset cannot exceed 32 bytes per character */
          /* Allocate the new buffer */
          newBuf = JS_realloc(cx, outbufStart, newAllocBytes);
	  if (!newBuf)
	  {
	    JS_free(cx, outbufStart);
	    iconv_close(cd);
	    JS_ReportOutOfMemory(cx);
	    return JS_FALSE;
	  }

          /* Transfer iconv write cursor from position relative to old buffer to same position relative to new buffer */
          outbuf = newBuf + (outbuf - outbufStart);
          /* Update the pointer to our buffer */
	  outbufStart = newBuf;
          /* Update the number of bytes remaining */
          outbytesleft += newAllocBytes - allocBytes;
          allocBytes = newAllocBytes;
	  break;
	}

	default:
	  JS_free(cx, outbufStart);
	  iconv_close(cd);
	  return gpsee_throw(cx, "%s.transcode: Transcoding error at source byte " GPSEE_SIZET_FMT " (%m)", 
			     throwPrefix, ((unsigned char *)inbuf - inputBuffer));
      }
    }
  } while (result == -1);

  iconv_close(cd);
  *outputBufferLength_p = outbuf - outbufStart;
  if (*outputBufferLength_p != allocBytes)
  {
    char *newBuf = JS_realloc(cx, outbufStart, *outputBufferLength_p);
    if (newBuf)
      outbufStart = newBuf;
  }

  *outputBuffer_p = (unsigned char*)outbufStart;
  return JS_TRUE;
#endif /* HAVE_ICONV */
}

/**
 *  Decode a JavaScript String into a ByteString buffer, via charset. JavaScript String is assumed to be UTF-8.
 *  If charset is not specified, charset's behaviour will follow the behaviour JS_Encharsetharacters(); that is to
 *  say, if JS_CStringsAreUTF8(), buf will be UTF-8-encoded; otherwise, buf will be created by simply throwing
 *  away the 16-bit characters' high bytes.
 *
 *  @param	string	[in]	String to decode
 *  @param	code	[in]	iconv character set name
 *  @param	bufp	[out]	malloc'd buffer with transcoded string or NULL if empty
 *  @param	lenp	[out]	Length of buffer
 *  @param	throwPrefix [in] Prefix to append to thrown exceptions
 *
 *  @returns	JS_TRUE on success
 */
JSBool transcodeString_toBuf(JSContext *cx, JSString *string, const char *charset, unsigned char **bufp, size_t *lenp,
			     const char *throwPrefix)
{
  jschar	*string_chars = JS_GetStringChars(string);
  size_t	string_length = JS_GetStringLength(string);

  if (!string_chars || !string_length)
  {
    *lenp = 0;
    return JS_TRUE;
  }

  if (!charset)
  {
    *lenp = 0;

    if (JS_EncodeCharacters(cx, string_chars, string_length, NULL, lenp) == JS_FALSE)
      return JS_FALSE;

    *bufp = JS_malloc(cx, *lenp);
    if (!*bufp)
    {
      JS_ReportOutOfMemory(cx);
      return JS_FALSE;
    }

    if (JS_EncodeCharacters(cx, string_chars, string_length, (char*)*bufp, lenp) == JS_FALSE)
    {
      JS_free(cx, *bufp);
      return JS_FALSE;
    }

    return JS_TRUE;
  }

#if !defined(HAVE_ICONV)
# warning Iconv support not detected: Binary module will throw rather than convert
  return gpsee_throw(cx, "%s.transcode.iconv.missing: Could not transcode UTF-16 String to %s charset; GPSEE was compiled without iconv "
		     "support.", throwPrefix, charset ?: "(null)");
#else
   if (string_length >=  1L << ((sizeof(size_t) * 8) - 1))   /* JSAPI limit is currently lower than this; may 2009 wg from shaver */
     return gpsee_throw(cx, "%s.transcode.length: String length exceeds maximum characters, cannot convert", throwPrefix);
  else
    return transcodeBuf_toBuf(cx, charset, DEFAULT_UTF_16_FLAVOUR, bufp, lenp, (const unsigned char *)string_chars, string_length * 2, throwPrefix);
#endif
}

/** Implements Binary::length getter.
 *
 *  @param	cx		JavaScript context
 *  @param	obj		A ByteString object
 *  @param      clasp           A pointer to either byteString_clasp or byteArray_clasp
 *  @param	id		unused
 *  @param	vp		The length, on successful return.
 *  @returns	JS_TRUE on success; otherwise JS_FALSE and a pending exception is set.
 */
JSBool byteThing_getLength(JSContext *cx, JSObject *obj, JSClass *clasp, jsval id, jsval *vp)
{
  byteThing_handle_t *hnd = JS_GetPrivate(cx, obj);

  if (!gpsee_isByteThing(cx, obj))
    return gpsee_throw(cx, "%s.length.get.invalid: property getter applied the wrong object type", clasp->name);

  if (((jsval)hnd->length == hnd->length) && INT_FITS_IN_JSVAL(hnd->length))
  {
    *vp = INT_TO_JSVAL(hnd->length);
    return JS_TRUE;
  }

  return JS_NewNumberValue(cx, hnd->length, vp);
}

/** Coerce a jsval to a ssize_t.
 *
 *  @returns  NULL on success; pointer to error message on error
 */
const char * byteThing_val2ssize(JSContext *cx, jsval val, ssize_t *retval, const char const * methodName)
{
  //static char errmsg[256];
  jsdouble temp;

  /* TODO more detailed error reporting! */
  if (!JS_ValueToNumber(cx, val, &temp))
  {
    return "invalid type";
  }

  /* Ensure value fits into ssize_t */
  if (temp != (ssize_t)temp)
  {
    return "invalid value";
  }

  /* Success! */
  *retval = (ssize_t)temp;
  return NULL;
}

/** Coerce a jsval to a size_t.
 *
 *  TODO remove methodName?
 *
 *  @returns  NULL on success; pointer to error message on error
 */
const char * byteThing_val2size(JSContext *cx, jsval val, size_t *retval, const char const * methodName)
{
  jsdouble temp;

  if (!JS_ValueToNumber(cx, val, &temp))
  {
    return "invalid type";
  }

  /* Reject negative values */
  if (temp < 0.0)
    return "negative number is invalid";

  /* Ensure value fits into size_t */
  if (temp != (size_t)temp)
  {
    return "invalid value";
  }

  /* Success! */
  *retval = (size_t)temp;
  return NULL;
}

/** Coerce a jsval to a byte.
 *
 *  @returns  NULL on success; pointer to error message on error
 */
const char * byteThing_val2byte(JSContext *cx, jsval val, unsigned char *retval)
{
  static char errmsg[256];
  jsdouble temp;

  /* Single-member binary types are generally usable as a byte */
  if (JSVAL_IS_OBJECT(val))
  {
    /* TODO support toByteString/Array ducktyping? */
    byteThing_handle_t * hnd;
    JSObject *obj = JSVAL_TO_OBJECT(val);
    JSClass *clasp = JS_GET_CLASS(cx, obj);

    /* Validate object's class */
    if (clasp != byteString_clasp && clasp != byteArray_clasp)
    {
      snprintf(errmsg, sizeof(errmsg), "cannot convert %s-type to byte value", clasp->name);
      return errmsg;
    }
    hnd = JS_GetPrivate(cx, obj);

    /* Binary types must contain only one byte */
    if (hnd->length != 1)
    {
      snprintf(errmsg, sizeof(errmsg), "Binary-type object contains too %s bytes (" GPSEE_SIZET_FMT " bytes) where it must contain exactly one byte",
               hnd->length < 1 ?"few":"many", hnd->length);
      return errmsg;
    }
  }

  /* TODO better error reporting? */
  if (!JS_ValueToNumber(cx, val, &temp) || temp!=(unsigned char)temp)
  {
    snprintf(errmsg, sizeof(errmsg), "%lf is not a valid byte value (only integers in the range 0<=n<=255 are valid)", temp);
    return errmsg;
  }

  /* Success! */
  *retval = (size_t)temp;
  return NULL;
}

/** Coerce a jsval into an unsigned char[]
 *  @param      cx              [in ]
 *  @param      val             [in ]   pointer to jsval to be converted
 *  @param      nvals           [in ]   number of jsvals at the address indicated by 'val'
 *  @param      resultBuffer    [out]   pointer to address of results
 *  @param      resultLen       [out]   pointer to number of results
 *  @param      stealBuffer     [i/o]   If 'stealBuffer' points to a value that is initially set to JS_TRUE, then the
 *                                      results returned through 'resultBuffer' will always be "new," and the caller
 *                                      is responsible for freeing it. If it is initially set to JS_FALSE, this func-
 *                                      tion will set it to JS_TRUE to indicate that caller is responsible for freeing
 *                                      'resultBuffer' after they're done with it.
 *  @param      clasp           [in ]   JSClass of calling object; used for error reporting
 *  @param      methodName      [in ]   Name of calling method; used for error reporting
 *
 *  @returns    JS_TRUE on success
 *
 *  This function is useful for handling arguments for functions which need to infer some kind of a byte array from
 *  one or more of its arguments. Of particular usefulness is its ability to process a jsval array and "linearize"
 *  them into a single result vector, which is the behavior I *guess* that is meant by specifications like these:
 *
 *  From https://wiki.mozilla.org/ServerJS/Binary/B
 *
 *     extendRight(...variadic Numbers / Arrays / ByteArrays / ByteStrings ...)
 *     extendLeft(...variadic Numbers / Arrays / ByteArrays / ByteStrings ...)
 *     displace(begin, end, values/ByteStrings/ByteArrays/Arrays...)
 *
 *  The Binary/B specification really is an awful piece of documentation. But this is what we have to deal with.
 *  Part of the reason this function is designed the way it is is so that it can be made recursive if that was
 *  the intention of Binary/B.
 */
JSBool byteThing_val2bytes(JSContext *cx, jsval *vals, int nvals, unsigned char **resultBuffer, size_t *resultLen,
                           JSBool *stealBuffer, JSClass *clasp, const char const *methodName)
{
  jsval *end = vals + nvals;
  unsigned char *buf=NULL, *bufpos;
  size_t buflen = 0; /* actual size of 'buf' */

  /* Given a single byte thing as an argument list, this simple case can just return a pointer to that bytething's buffer */
  if (nvals == 1 && !*stealBuffer && JSVAL_IS_OBJECT(*vals))
  {
    /* TODO support toByteString/Array ducktyping? */
    byteThing_handle_t *  hnd;
    JSObject *            obj =     JSVAL_TO_OBJECT(*vals);
    JSClass *             oclasp =  JS_GET_CLASS(cx, obj);

    if (oclasp == byteString_clasp || oclasp == byteArray_clasp) {
      hnd = JS_GetPrivate(cx, obj);
      if (!hnd)
        return gpsee_throw(cx, MODULE_ID ".%s.%s.internalerror: %s object missing private data!", 
                           clasp->name, methodName, oclasp->name);
      *resultBuffer = hnd->buffer;
      *resultLen = hnd->length;
      return JS_TRUE;
    }
  }

  /* Begin with a liberal allocation */
  *stealBuffer = JS_TRUE;
  buflen = 64;
  buf = JS_malloc(cx, buflen);
  if (!buf)
  {
    JS_ReportOutOfMemory(cx);
    return JS_FALSE;
  }
  bufpos = buf;

  /* Iterate over each member of 'vals' */
  for (end = vals + nvals; vals < end; vals++)
  {
    /* Binary types and arrays are usable */
    if (JSVAL_IS_OBJECT(*vals))
    {
      /* TODO support toByteString/Array ducktyping? */
      JSObject * obj = JSVAL_TO_OBJECT(*vals);
      JSClass * oclasp = JS_GET_CLASS(cx, obj);

      /* Arrays can work */
      if (JS_IsArrayObject(cx, obj))
      {
        size_t bytesRead = 0;
        JSBool more;
        jsuint arrLen;

        /* Get array length of val */
        if (JS_GetArrayLength(cx, obj, &arrLen) == JS_FALSE)
          return JS_FALSE;

        do
        {
          size_t len;

          /* Grow buffer if necessary */
          if (bufpos + arrLen > buf + buflen)
          {
            unsigned char *oldbuf = buf;
            buflen = bufpos - buf + arrLen;
            buf = JS_realloc(cx, buf, buflen);
            if (!buf)
            {
              JS_ReportOutOfMemory(cx);
              return JS_FALSE;
            }
            bufpos = bufpos - oldbuf + buf;
          }

          /* Ask another function to convert the Array for us */
          len = (size_t)(buflen - (bufpos - buf)); /* i/o var. limits results. reports bytes read. */
          if (!copyJSArray_toBuf(cx, obj, bytesRead, &bufpos, &len, &more, methodName))
            return JS_FALSE;
          bytesRead += len;
        } while (more);

        /* Advance buffer write cursor; reallocate buffer if necessary */
        bufpos += bytesRead;
      }
      /* Binary types work */
      else if (oclasp == byteString_clasp || oclasp == byteArray_clasp)
      {
        byteThing_handle_t * hnd;

        /* This should never return NULL */
        hnd = JS_GetPrivate(cx, obj);
        if (!hnd)
          return gpsee_throw(cx, MODULE_ID ".%s.%s.internalerror: %s object missing private data!", 
                             clasp->name, methodName, oclasp->name);

        /* Grow buffer if necessary */
        if (bufpos + hnd->length > buf + buflen)
        {
          unsigned char *oldbuf = buf;
          buflen = bufpos - buf + hnd->length;
          buf = JS_realloc(cx, buf, buflen);
          if (!buf)
          {
            JS_ReportOutOfMemory(cx);
            return JS_FALSE;
          }
          bufpos = bufpos - oldbuf + buf;
        }

        /* Copy bytes into buffer */
        memcpy(bufpos, hnd->buffer, hnd->length);
        bufpos += hnd->length;
      }
    }
    /* Try numeric coercion */
    else
    {
      jsdouble d;

      /* jsdouble from jsval */
      if (JS_ValueToNumber(cx, *vals, &d) == JS_FALSE)
        return gpsee_throw(cx, MODULE_ID ".%s.%s.byte.invalid: cannot convert argument to numeric value",
                           clasp->name, methodName);

      /* validate byte value */
      if (d != (unsigned char)d)
        return gpsee_throw(cx, MODULE_ID ".%s.%s.byte.invalid: %lf is not a valid byte value",
                           clasp->name, methodName, d);

      /* Grow buffer if necessary */
      if (bufpos >= buf + buflen) {
        unsigned char *oldbuf = buf;
        buflen *= 2; /* quite a liberal amount of growth! */
        buf = JS_realloc(cx, buf, buflen);
        if (!buf)
        {
          JS_ReportOutOfMemory(cx);
          return JS_FALSE;
        }
        bufpos = bufpos - oldbuf + buf;
      }

      /* store result */
      *bufpos++ = (unsigned char) d;
    }
  }

  /* Reallocate buffer in case it's a bit big :) */
  buflen = (size_t) (bufpos - buf);
  buf = JS_realloc(cx, buf, buflen);
  GPSEE_ASSERT(buf); /* should never happen */

  /* Return! */
  *resultBuffer = buf;
  *resultLen = buflen;
  return JS_TRUE;
}

/** Coerce a function argument to a size_t.
 *  GPSEE's core module support.
 *
 *  @param      cx            Your JSContext
 *  @param      argc          Number of JSFastNative arguments
 *  @param      vp            JSAPI stack
 *  @param      retval          Address at which to store the result
 *  @param      argn          Index of argument to process
 *  @param      mayDefault    Whether or not this argument has a default value
 *  @param      defaultSize   A default value for this argument
 *  @param      methodName    Used for error reporting
 *
 *  @returns    JS_TRUE on success
 *
 *  This function is to be called from within the execution of a JSFastNative. It will prepare an argument of type size_t,
 *  and is very convenient in that it is quite thorough and throws exceptions in a uniform way.
 */
JSBool byteThing_arg2size(JSContext *cx, uintN argc, jsval *vp, size_t *retval, uintN argn, size_t min, size_t max,
                          JSBool mayDefault, size_t defaultSize, JSClass *clasp, const char const *methodName)
{
  ssize_t temp;
  const char * errmsg;
  jsval * argv = JS_ARGV(cx, vp);

  /* Is the argument vector shorter than expected? */
  if (argn >= argc)
  {
    /* Is there a default value to fall back on? */
    if (!mayDefault)
    {
      return gpsee_throw(cx, "%s.%s.arguments.%d: too few arguments", clasp->name, methodName, argn);
    }
    /* Default */
    *retval = defaultSize;
    return JS_TRUE;
  }

  /* Try to convert jsval to size_t */
  if ((errmsg = byteThing_val2ssize(cx, argv[argn], &temp, methodName)))
    return gpsee_throw(cx, "%s.%s.arguments.%d: %s", clasp->name, methodName, argn, errmsg);

  /* Check lower bound */
  if (temp < 0 || temp < min)
    return gpsee_throw(cx, "%s.%s.arguments.%d.underflow: expected value not less than " GPSEE_SIZET_FMT ", got " GPSEE_SSIZET_FMT,
                       clasp->name, methodName, argn, min, temp);

  /* Check upper bound */
  if (temp > max)
    return gpsee_throw(cx, "%s.%s.arguments.%d.overflow: expected value not greater than " GPSEE_SIZET_FMT ", got " GPSEE_SSIZET_FMT,
                       clasp->name, methodName, argn, max, temp);

  /* Success! */
  *retval = (size_t) temp;
  return JS_TRUE;
}

/** Coerce a function argument to an ssize_t.
 *  part of GPSEE's core module support.
 *
 *  @param      cx            Your JSContext
 *  @param      argc          Number of JSFastNative arguments
 *  @param      vp            JSAPI stack
 *  @param      retval        Address at which to store the result
 *  @param      argn          Index of argument to process
 *  @param      mayDefault    Whether or not this argument has a default value
 *  @param      defaultSize   A default value for this argument
 *  @param      methodName    Used for error reporting
 *
 *  @returns    JS_TRUE on success
 *
 *  This function is to be called from within the execution of a JSFastNative. It will prepare an argument of type
 *  ssize_t and is very convenient in that it is quite thorough and throws exceptions in a uniform way.
 */
JSBool byteThing_arg2ssize(JSContext *cx, uintN argc, jsval *vp, ssize_t *retval, uintN argn, ssize_t min, ssize_t max,
                          JSBool mayDefault, ssize_t defaultSize, JSClass *clasp, const char const *methodName)
{
  const char * errmsg;
  jsval * argv = JS_ARGV(cx, vp);

  /* Is the argument vector shorter than expected? */
  if (argn >= argc)
  {
    /* Is there a default value to fall back on? */
    if (!mayDefault)
    {
      return gpsee_throw(cx, "%s.%s.arguments.%d: too few arguments", clasp->name, methodName, argn);
    }
    /* Default */
    *retval = defaultSize;
    return JS_TRUE;
  }

  /* Try to convert jsval to ssize_t */
  if ((errmsg = byteThing_val2ssize(cx, argv[argn], retval, methodName)))
    return gpsee_throw(cx, "%s.%s.arguments.%d: %s", clasp->name, methodName, argn, errmsg);

  /* Check lower bound */
  if (*retval < min)
    return gpsee_throw(cx, "%s.%s.arguments.%d.underflow: expected value not less than " GPSEE_SSIZET_FMT ", got " GPSEE_SSIZET_FMT,
                       clasp->name, methodName, argn, min, *retval);

  /* Check upper bound */
  if (*retval > max)
    return gpsee_throw(cx, "%s.%s.arguments.%d.overflow: expected value not greater than " GPSEE_SSIZET_FMT ", got " GPSEE_SSIZET_FMT,
                       clasp->name, methodName, argn, max, *retval);

  /* Success! */
  return JS_TRUE;
}

/**
 *  Create a new ByteString or ByteArray instance from a provided buffer.
 *
 *  @param	cx          JavaScript context
 *  @param	buffer      The raw contents of the byte string; if buffer is NULL we initialize to all-zeroes
 *  @param	length      The length of the data in buf.
 *  @param	obj         New object of correct class to decorate, or NULL. If non-NULL, must match clasp.
 *  @param      clasp       A pointer to either byteString_clasp or byteArray_clasp
 *  @param      proto       A pointer to either byteString_proto or byteArray_proto
 *  @param      btallocSize Either sizeof(byteString_handle_t) or sizeof(byteArray_handle_t)
 *  @param	stealBuffer If non-zero, we steal the buffer rather than allocating and copying our own
 *
 *  @returns	NULL on OOM, otherwise a new ByteString.
 */
JSObject *byteThing_fromCArray(JSContext *cx, const unsigned char *buffer, size_t length, JSObject *obj, JSClass *clasp,
                               JSObject *proto, size_t btallocsize, int stealBuffer)
{
  /* Allocate and initialize our private ByteThing handle */
  byteThing_handle_t 	*hnd;
  
  if (!gpsee_isByteThingClass(cx, clasp))
  {
    gpsee_throw(cx, MODULE_ID ".internalerror: byteThing_fromCArray() asked to instantiate a new %s", clasp?clasp->name:"(NULL CLASP)");
    return NULL;
  }

  hnd = (byteThing_handle_t*) JS_malloc(cx, btallocsize);
  if (!hnd)
    return NULL;
  memset(hnd, 0, btallocsize);
  hnd->length = length;

  if (stealBuffer)
  {
    hnd->buffer = (unsigned char *)buffer;
  }
  else
  {
    if (length)
    {
      hnd->buffer = JS_malloc(cx, length);

      if (!hnd->buffer)
      {
        JS_free(cx, hnd);
	return NULL;
      }
    }

    if (buffer)
      memcpy(hnd->buffer, buffer, length);
    else
      memset(hnd->buffer, 0, length);
  }

  GPSEE_ASSERT(!obj || (JS_GET_CLASS(cx, obj) == clasp));

  if (!obj)
    obj = JS_NewObject(cx, clasp, proto, NULL);

  if (obj)
  {
    JS_SetPrivate(cx, obj, hnd);
    hnd->memoryOwner = obj;
  }

  return obj;
}

/** Allocate and initialize a new byteThing_handle_t. Returned handle is allocated
 *  with JS_malloc().  Caller is responsible for releasing memory; if it winds up
 *  as the private data in a ByteString or ByteArray instance, the finalizer will
 *  handle the clean up during garbage collection.
 *
 *  @param      cx  JavaScript context
 *  @returns        A pointer to a zero-initialized byteString_handle_t or NULL on OOM
 */
byteThing_handle_t *byteThing_newHandle(JSContext *cx)
{
  byteThing_handle_t	*hnd;

  hnd = JS_malloc(cx, sizeof(*hnd));
  if (!hnd)
    return NULL;

  memset(hnd, 0, sizeof(*hnd));

  return hnd;
}

/** Implements ByteString.toByteString(), ByteString.toByteArray(), ByteArray.toByteString(), and ByteArray.toByteArray() instance methods.
 *  @param    cx
 *  @param    argc
 *  @param    vp
 *  @param    classOfResult         Either byteString_clasp or byteArray_clasp to indicate toByteString() or toByteArray() respectively.
 *  @param    protoOfResult         Either byteString_proto or byteArray_proto (or perhaps something that inherits those)
 *  @param    privAllocSizeOfResult Either sizeof(byteString_handle_to) or sizeof(byteArray_handle_t)
 *  @param    throwPrefix
 *  @returns  JS_TRUE on success
 *  */
JSBool byteThing_toByteThing(JSContext *cx, uintN argc, jsval *vp, JSClass *classOfResult, JSObject *protoOfResult, size_t privAllocSizeOfResult,
                             const char *throwPrefix)
{
  jsval			*argv = JS_ARGV(cx, vp);
  JSObject              *self = JSVAL_TO_OBJECT(JS_THIS(cx, vp));
  byteThing_handle_t    *hnd;
  int                   mustBeNew = 0; // in some cases we don't need to return a new object, we can return a reference ourself
  const char *          className = NULL;
  const char *          methodName;

  /* Deduce our method name as well as an appropriate prototype object from the JSClass we're given */
  if (classOfResult == byteString_clasp)
    methodName = "toByteString";
  else if (classOfResult == byteArray_clasp) {
    methodName = "toByteArray";
    /* Mutable ByteArray result means we must return a new instance */
    mustBeNew = 1;
  }

  /* Deduce our source class */
  if (JS_InstanceOf(cx, self, byteString_clasp, NULL))
  {
    /* ByteString.toByteString() might be accomplished by simply returning a reference to ourself.
     * This is the only possible source,target class pair appropriate for returning a reference to
     * ourself because ByteStrings are immutable */
    className = "ByteString";
  }
  else if (JS_InstanceOf(cx, self, byteArray_clasp, NULL))
  {
    /* Mutable ByteArray source object means we must return a new instance */
    mustBeNew = 1;
    className = "ByteArray";
  }
  /* Not an instance of ByteString or ByteArray! AAAHHH!!! */
  else
    return gpsee_throw(cx, MODULE_ID "%s.%s.invalid: native instance method applied to incompatible object", className, methodName);

  /* Retrieve our pointer! */
  hnd = JS_GetPrivate(cx, self);
  if (!hnd)
    return gpsee_throw(cx, MODULE_ID "%s.%s.invalid: native instance method applied to incompatible object (missing private data)", className, methodName);

  /* Validate argument count */
  if (argc != 0 && argc != 2)
    return gpsee_throw(cx, MODULE_ID "%s.%s.arguments.count: expected 0 or 2 arguments, got %d arguments.", className, methodName, argc);

  JS_EnterLocalRootScope(cx);

  /* If it is possible that we will have to transcode our character buffer, we will have exactly two arguments of different values */
  if (mustBeNew || (argc == 2 && argv[0] != argv[1]))
  {
    const char *sourceCharset = NULL, *targetCharset = NULL;
    unsigned char *newBuf = NULL;
    size_t newBufLength;
    JSObject *rval = NULL;
    /* We'll also compare their string representations. transcodeBuf_toBuf() actually does this same check, but it *always* returns
     * a *newly* malloc'd buffer, which we may not always need */
    if (argc == 2)
    {
      sourceCharset = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
      targetCharset = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));
    }
    if (mustBeNew || strcasecmp(sourceCharset, targetCharset))
    {
      /* Create a copy of the buffer, perhaps transcoding during the process */
      if (!transcodeBuf_toBuf(cx, targetCharset, sourceCharset, &newBuf, &newBufLength, hnd->buffer, hnd->length, throwPrefix))
        goto fail;
      /* Instantiate a new ByteThing from the transcoded character buffer */
      rval = byteThing_fromCArray(cx, newBuf, newBufLength, rval,
                                  classOfResult, protoOfResult, privAllocSizeOfResult, 1);
      if (!rval)
        goto fail;
      /* Return value! */
      JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(rval));
      goto succeed;
    }
  }
  /* ByteString.toByteString() was called with no transcoding needed! Return a reference to ourselves! */
  JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(self));

  succeed:
  JS_LeaveLocalRootScope(cx);
  return JS_TRUE;

  fail:
  JS_LeaveLocalRootScope(cx);
  return JS_FALSE;
}

/** Implements ByteString.decodeToString() and ByteArray.decodeToString() instance methods
 *  @param    cx
 *  @param    argc
 *  @param    vp
 *  @param    clasp   Either byteString_clasp or byteArray_clasp
 */
JSBool byteThing_decodeToString(JSContext *cx, uintN argc, jsval *vp, JSClass *clasp)
{
  byteThing_handle_t    *hnd;
  JSString		*s;
  unsigned char		*buf;
  size_t		length;
  const char		*sourceCharset;
  jsval			*argv = JS_ARGV(cx, vp);
  JSObject		*obj = JS_THIS_OBJECT(cx, vp);
  JSClass		*objClasp = JS_GET_CLASS(cx, obj);

  if (!obj)
    return JS_FALSE;

  if (!gpsee_isByteThingClass(cx, objClasp))
    return gpsee_throw(cx, MODULE_ID ".%s.decodeToString.type: Cannot call decodeToString on a %s object!", clasp->name, objClasp->name);

  /* Acquire our byteThing_handle_t */
  hnd = JS_GetPrivate(cx, obj);
  if (!hnd->length)
  {
    JS_SET_RVAL(cx, vp, JS_GetEmptyStringValue(cx));
    return JS_TRUE;
  }

  GPSEE_ASSERT(hnd->buffer);
    
  switch(argc)
  {
    case 0:
      sourceCharset = NULL;
      break;
    case 1:
      sourceCharset = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
      break;
    default:
      return gpsee_throw(cx, MODULE_ID ".%s.decodeToString.arguments.count", clasp->name);
  }

  /* TODO not always necessary to do this! */
  /* Transcode from one character encoding to another */
  if (!transcodeBuf_toBuf(cx, NULL, sourceCharset, &buf, &length, hnd->buffer, hnd->length,
                          clasp == byteString_clasp ? "ByteString.decodeToString" : "ByteArray.decodeToString"))
    return JS_FALSE;

  /* Instantiate a JSString return value */
  s = JS_NewUCStringCopyN(cx, (jschar *)buf, length / 2);
  if (!s)
    return JS_FALSE;

  JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(s));
  return JS_TRUE;
}

/** Implements ByteString.toString() and ByteArray.toString() */
JSBool byteThing_toString(JSContext *cx, uintN argc, jsval *vp)
{
  char 			buf[128];
  byteThing_handle_t 	*hnd;
  JSString		*s;
  const char            *className;
  JSObject              *self;

  /* Fetch our JSObject */
  self = JS_THIS_OBJECT(cx, vp);
  if (!self)
    return JS_FALSE;

  /* Make sure this is a binary.Binary instance, or at least compatible for this call */
  if (!gpsee_isByteThing(cx, self))
    return gpsee_throw(cx, MODULE_ID "binary.toString.invalid: native instance method applied to incompatible object");

  /* Fetch class name */
  className = JS_GET_CLASS(cx, self)->name;

  /* Fetch our byteThing_handle_t */
  hnd = JS_GetPrivate(cx, self);
  if (!hnd)
    return gpsee_throw(cx, MODULE_ID ".%s.toString.invalid: %s.toString applied the wrong object type", className, className);

  /* Compose the abbreviated string representation of our object */
  snprintf(buf, sizeof(buf), "[object %s " GPSEE_SIZET_FMT "]", className, hnd->length);
  /* Make a JSString from our C string */
  s = JS_NewStringCopyZ(cx, buf);
  if (!s)
  {
    JS_ReportOutOfMemory(cx);
    return JS_FALSE;
  }

  /* Return the string! */
  JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(s));
  return JS_TRUE;
}

/** ByteString_toSource() implements the ByteString member method toSource() */
JSBool byteThing_toSource(JSContext *cx, uintN argc, jsval *vp, JSClass *clasp)
{
  const char front[] = "(new (require(\"binary\").Byte";
  const char back[]  =                                      "]))";
  char *source_string, *c;
  unsigned char *buf;
  byteThing_handle_t * hnd;
  int i, l;
  JSString *retval;

  /* Acquire a pointer to our internal bytestring data */
  /* TODO would it be nice for the Binary spec if you could .apply() and .call() this method from the
   * ByteArray and ByteString prototype to an instance of either type, thus getting the source to instantiate
   * an instance of a different type? */
  hnd = byteThing_getHandle(cx, JS_THIS_OBJECT(cx, vp), &clasp, "toSource");
  if (!hnd)
    return JS_FALSE;
  l = hnd->length;
  buf = hnd->buffer;

  /* Allocate a string for */
  source_string = JS_malloc(cx, sizeof(front) + sizeof(back) + l*4 + 6);
  strcpy(source_string, front);
  c = source_string + sizeof(front) - 1;
  if (clasp == byteString_clasp)
  {
    strcat(c, "String)([");
    c += 9;
  } else if (clasp == byteArray_clasp)
  {
    strcat(c, "Array)([");
    c += 8;
  }
  i = 0;
  if (l) do
  {
    c += sprintf(c, "%d", buf[i]);
  } while (++i<l && (*(c++)=','));
  strcpy(c, back);

  retval = JS_NewString(cx, source_string, strlen(source_string));
  if (retval)
  {
    JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(retval));
    return JS_TRUE;
  }

  return JS_FALSE;
}
/** Implements ByteString.byteAt(), ByteString.charAt(), ByteString.charCodeAt(), ByteString.get(), ByteArray.byteAt(), and ByteArray.get()
 *  @param    cx
 *  @param    argc
 *  @param    vp
 *  @param    clasp           Either byteString_clasp or byteArray_clasp
 *  @param    rtype           If NULL, returns a Javascript Number to the Javascript caller. The other allowed
 *                            value at the moment is byteString_clasp.
 *  @param    fullMethodName  A string of the form "className.methodName" for error reporting
 */
JSBool byteThing_byarAt(JSContext *cx, uintN argc, jsval *vp, JSClass *clasp, JSClass *rtype, const char const *methodName)
{
  byteThing_handle_t    *hnd;
  size_t                index;
  JSObject		*obj = JS_THIS_OBJECT(cx, vp);
  JSClass		*objClasp = JS_GET_CLASS(cx, obj);

  if (!obj)
    return JS_FALSE;

  if (!gpsee_isByteThingClass(cx, objClasp))
    return gpsee_throw(cx, MODULE_ID ".%s.%s.type: Cannot call byteAt/charAt on a %s object!", clasp->name, methodName, objClasp->name);

  /* Acquire our byteThing_handle_t */
  hnd = JS_GetPrivate(cx, obj);
  if (!hnd)
    return JS_FALSE;

  /* Coerce argument and do bounds checking */
  /* TODO methodName argument here needs to include class name as well */
  if (!byteThing_arg2size(cx, argc, vp, &index, 0, 0, hnd->length-1, JS_FALSE, 0, clasp, methodName))
    return JS_FALSE;

  /* Return a new ByteString? */
  if (rtype == byteString_clasp)
  {
    JSObject *retval;

    /* Instantiate a new ByteString for the return value */
    retval = byteThing_fromCArray(cx, hnd->buffer + index, 1, NULL, byteString_clasp, byteString_proto, sizeof(byteString_handle_t), 0);
    if (!retval)
      return JS_FALSE;

    /* Return the new ByteString! */
    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(retval));
  }
  /* Default behavior is to return a Number */
  else {
    JS_SET_RVAL(cx, vp, INT_TO_JSVAL(hnd->buffer[index]));
  }
  return JS_TRUE;
}

/** Implements ByteString.indexOf(), ByteString.lastIndexOf(), ByteArray.indexOf(), and ByteArray.lastIndexOf() instance methods 
 *
 *  @param	cx		JavaScript Context
 *  @param	argc		Number of arguments
 *  @param	vp		JSFastNative value pointer
 *  @param	memchr_fn	Function to use to find bytes in memory
 *  @param	methodName	What this method is called (for display purposes only)
 *  @param	clasp		Class that method was called as, regardless of call() object type
 */
JSBool byteThing_findChar(JSContext *cx, uintN argc, jsval *vp, void *memchr_fn(const void *, int, size_t), const char const *methodName, JSClass *clasp)
{
  byteThing_handle_t    *hnd;
  jsval			*argv = JS_ARGV(cx, vp);
  unsigned char         theByte;
  size_t  		start;
  size_t  		len;
  const unsigned char 	*found;
  const char            *errmsg;
  JSObject		*obj = JS_THIS_OBJECT(cx, vp);
  JSClass		*objClasp = JS_GET_CLASS(cx, obj);

  if (!obj)
    return JS_FALSE;

  if (!gpsee_isByteThingClass(cx, objClasp))
    return gpsee_throw(cx, MODULE_ID ".%s.byteAt.type: Cannot call %s on a %s object!", clasp->name, methodName, objClasp->name);

  hnd = JS_GetPrivate(cx, obj);
  if (!hnd)
    return JS_FALSE;

  /* Validate argument count */
  if (argc < 1 || argc > 3)
    return gpsee_throw(cx, "%s.arguments.count", methodName);

  /* Process 'needle' argument */
  if ((errmsg = byteThing_val2byte(cx, argv[0], &theByte)))
    return gpsee_throw(cx, "%s.%s.arguments.0.byte.invalid: %s", clasp->name, methodName, errmsg);

  /* Convert JS args to C args */
  if (!byteThing_arg2size(cx, argc, vp, &start, 1, 0, hnd->length-1, JS_TRUE, 0, clasp, methodName))
    return JS_FALSE;
  if (!byteThing_arg2size(cx, argc, vp, &len, 2, 0, hnd->length-start, JS_TRUE, hnd->length-start, clasp, methodName))
    return JS_FALSE;

  /* Search for needle */
  if ((found = memchr_fn(hnd->buffer + start, theByte, len)))
  {
    if (INT_FITS_IN_JSVAL(found - hnd->buffer))
      JS_SET_RVAL(cx, vp, INT_TO_JSVAL(found - hnd->buffer));
    else
    {
      jsval v;

      if (JS_NewNumberValue(cx, found - hnd->buffer, &v) == JS_FALSE)
	return JS_FALSE;

      JS_SET_RVAL(cx, vp, v);
    }
  }
  else
    JS_SET_RVAL(cx, vp, INT_TO_JSVAL(-1));
  

  return JS_TRUE;
}

#ifndef HAVE_MEMRCHR
void *gpsee_memrchr(const void *s, int c, size_t n)
{
  const char *p;

  for (p = (char *)s + n; p != s; p--)
  {
    if (*p == c)
      return (void *)p;
  }

  return NULL;
}
#endif

/** Returns a Javascript Array object containing 'len' Numbers taken from 'bytes' */
JSObject *byteThing_toArray(JSContext *cx, const unsigned char *bytes, size_t len)
{
  JSObject *retval;
  int i;
  /* Instantiate an array to hold our results */
  retval = JS_NewArrayObject(cx, 0, NULL);
  if (!retval)
    return NULL;
  /* Protect our array from garbage collector during calls to JS_SetElement() */
  JS_AddRoot(cx, &retval);
  /* Iterate over each byte */
  for (i=0; i<len; i++)
  {
    /* Push the byte value into our result array */
    jsval val = INT_TO_JSVAL(bytes[i]);
    if (!JS_SetElement(cx, retval, i, &val))
      return NULL;
  }
  /* Un-GC-protect */
  JS_RemoveRoot(cx, &retval);
  /* Return the array object containing results */
  return retval;
}

/** Implements default property getter for ByteString and Byte Array. Used to implement to implement Array-like
 *  property lookup ([]) */
JSBool byteThing_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp, JSClass *clasp)
{
  byteThing_handle_t *  hnd;
  size_t                index;

  /* The ByteArray getter may also be applied to ByteArray.prototype. Returning JS_TRUE without modifying the value at
   * 'vp' will allow Javascript consumers to modify the properties of ByteArray.prototype. */
  if (obj == byteArray_proto || obj == byteString_proto)
    return JS_TRUE;

  /* The ByteThing getter may also be applied to a property name that is not an integer. We are unconcerned with these
   * on the native/JSAPI side. To give Javascript consumers free reign to assign arbitrary non-numeric properties to
   * an instance of ByteArray, we just return JS_TRUE as in the case of application to ByteArray.prototype. Thus, if
   * we cannot get a valid 'size_t' type from the property key, we pass through by returining JS_TRUE. */

  /* Coerce index argument and do bounds checking upon it */
  if (byteThing_val2size(cx, id, &index, "getProperty"))
    return JS_TRUE;

  /* Acquire our byteArray_handle_t */
  hnd = byteThing_getHandle(cx, obj, &clasp, "getProperty");
  if (!hnd)
    return JS_FALSE;

  /* If the property key is out of bounds, just return undefined */
  if (index >= hnd->length)
  {
    *vp = JSVAL_VOID;
    return JS_TRUE;
  }

  /* Return a new one-length ByteArray instance */
  if (clasp == byteString_clasp)
  {
    /* ByteString[n] returns a new ByteString containing one byte */
    JSObject *rval = byteThing_fromCArray(cx, hnd->buffer + index, 1, NULL,
                                          byteString_clasp, byteString_proto, sizeof(byteString_handle_t), 0);
    if (!rval)
      return JS_FALSE;
    *vp = OBJECT_TO_JSVAL(rval);
  }
  else
    /* ByteArray[n] returns a Number */
    *vp = INT_TO_JSVAL(hnd->buffer[index]);
  return JS_TRUE;
}

/**
 *  When byteThing-like constructors are used as functions, we want to cast the
 *  argument into the data type represented by the constructor.
 *
 *  Casting in GPSEE is different than casting in C.  In GPSEE, the backing store
 *  will be copied, not shared, unless you we can verify that both the source and
 *  the target of the cast are immutable.
 *
 *  The first argument must have a private slot which is castable to a 
 *  byteThing_handle_t pointer.
 *
 *  The second argument is an alternate length.  If the alternate length is -1, we 
 *  assume the source is an ASCIZ string.  The length argument is needed because we 
 *  don't always know how much memory is in hnd->buffer, for example, in the case
 *  where the memory is returned by an FFI function.
 *
 *  @param	cx 		JavaScript Context
 *  @param	argc		Number of arguments passed to cast function
 *  @param	argv		Arguments passed to cast function
 *  		- argv[0]	ByteThing we are casting
 *  		- argv[1]	Optional length parameter
 *  @param	clasp		The type we're casting into (i.e. ByteString or ByteArray)
 *  @param	proto		Prototype for the new object
 *  @param	threwPrefix	Prefix for exception messages
 *
 *  @returns	JS_TRUE unless an exception is thrown
 */
JSBool byteThing_Cast(JSContext *cx, uintN argc, jsval *argv, jsval *rval,
		      JSClass *clasp, JSObject *proto, size_t hndSize, const char *throwPrefix)
{
  byteThing_handle_t	*hnd;
  ssize_t		length;
  JSObject		*obj;

  if ((argc != 1) && (argc != 2))
    return gpsee_throw(cx, "%s.cast.arguments.count", throwPrefix);

  if (!JSVAL_IS_OBJECT(argv[0]))
  {
    if (JS_ValueToObject(cx, argv[0], &obj) == JS_FALSE)
      return JS_FALSE;
  }
  else
    obj = JSVAL_TO_OBJECT(argv[0]);

  if (!gpsee_isByteThing(cx, obj))
    return gpsee_throw(cx, "%s.cast.type: %s objects are not castable to %s", throwPrefix, (obj ? JS_GET_CLASS(cx, obj)->name : "null"), clasp->name);
  
  hnd = JS_GetPrivate(cx, obj);

  if (argc == 1)
    length = hnd->length;
  else
  {
    if (JSVAL_IS_INT(argv[1]))
      length = JSVAL_TO_INT(argv[1]);
    else
    {
      jsdouble d;

      if (JS_ValueToNumber(cx, argv[1], &d) == JS_FALSE)
	return JS_FALSE;

      length = d;
      if (d != length)
	return gpsee_throw(cx, "%s.cast.length.overflow", throwPrefix);
    }

    if (length == -1)
      length = strlen((const char *)hnd->buffer);
  }

  if ((clasp == byteString_clasp) && (hnd->btFlags & bt_immutable))
  {
    /* casting from immutable to immutable: can avoid copy */
    byteThing_handle_t	*newHnd = JS_malloc(cx, sizeof(*newHnd));
    
    if (!newHnd)
      return JS_FALSE;

    *newHnd = *hnd;
    obj = JS_NewObject(cx, clasp, proto, NULL);
    JS_SetPrivate(cx, obj, newHnd);
  }
  else
  {
    obj = byteThing_fromCArray(cx, hnd->buffer, length, NULL, clasp, proto, hndSize, 0);
    if (!obj)
      return JS_FALSE;
  }

  *rval = OBJECT_TO_JSVAL(obj);
  return JS_TRUE;
}

/**
 *  This function casts an int-sized portion of a byteThing's memory as an int
 *  and then returns it, optionally performing byte-order conversion.
 *
 *  @param      cx
 *  @param      argc
 *  @param      vp
 *  @param      throwPrefix     Prefix for exception messages
 */
JSBool byteThing_intAt(JSContext *cx, uintN argc, jsval *vp, const char *throwPrefix)
{
  byteThing_handle_t *hnd;
  size_t idx;
  JSObject *self;
  jsval *argv;
  int rval;
  const char *err;

  self = JS_THIS_OBJECT(cx, vp);
  if (!self)
    goto invalid;

  hnd = JS_GetPrivate(cx, self);
  if (!hnd)
    goto invalid;

  argv = JS_ARGV(cx, vp);

  err = byteThing_val2size(cx, argv[0], &idx, NULL);
  if (err)
    return gpsee_throw(cx, "%s.argument.0.invalid: %s", throwPrefix, err);

  rval = *((int*)&hnd->buffer[idx]);
  JS_SET_RVAL(cx, vp, INT_TO_JSVAL(rval));
  return JS_TRUE;

  invalid:
    return gpsee_throw(cx, "%s.invalid: method applied to non-bytething", throwPrefix);
}
