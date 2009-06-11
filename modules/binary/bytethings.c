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
 *  @version	$Id: bytethings.c,v 1.1 2009/05/27 04:48:36 wes Exp $
 */

static const char __attribute__((unused)) rcsid[]="$Id: bytethings.c,v 1.1 2009/05/27 04:48:36 wes Exp $";

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

extern JSClass *byteArray_class;
extern JSClass *byteString_class;

/** Copy a JavaScript array-of-numbers into an unsigned char buffer.
 *  If we return successfully, buf is allocated with JS_malloc, unless it is
 *  zero bytes long in which case it will be NULL. If we do not return successfully,
 *  buf will be free and have an unspecified value.
 *
 *  @throws	.array.range If any array element is <0 or >255
 *  @param	bufp	[out]	malloc'd buffer with array copy or NULL
 *  @param	arr	[in]	Array to copy from
 *  @param	lenp	[out]	Length of buffer
 *  @returns	JS_TRUE on success
 */
JSBool copyJSArray_toBuf(JSContext *cx, JSObject *arr, unsigned char **bufp, size_t *lenp, const char *throwPrefix)
{
  /* Algorithm may seem sub-optimal, but we have to consider the
   * MT case where the array is being length-modified in another
   * thread. Ouch.
   *
   * We also can't use enumeration as the trivial case, because
   * enumeration order is not necessarily the same as array order.
   */
  jsuint 	arrLen, allocArrLen, arrEl;

  if (JS_GetArrayLength(cx, arr, &arrLen) == JS_FALSE)
    return JS_FALSE;

  if (arrLen == 0)
  {
    *bufp = NULL;
    *lenp = 0;
    return JS_TRUE;
  }

  allocArrLen = arrLen;
  *bufp = JS_malloc(cx, allocArrLen);

  for(arrEl = 0; arrEl < arrLen; arrEl++)
  {
    jsid 	id;
    jsval	idval;
    jsval	v;

    if (INT_FITS_IN_JSVAL(arrEl))
      idval = INT_TO_JSVAL(arrEl);
    else
    {
      jsdouble d = arrEl;
      if (JS_NewNumberValue(cx, d, &idval) == JS_FALSE)
	goto failOut;
    }

    if (JS_ValueToId(cx, idval, &id) == JS_FALSE)
      goto failOut;

    if (JS_LookupPropertyById(cx, arr, id, &v) == JS_FALSE)
      goto failOut;

    if (JSVAL_IS_INT(v))
    {
      int i = JSVAL_TO_INT(v);
      
      if (i < 0 || i > 255)
      {
	if (JSVAL_IS_INT(arrEl))
	  (void)gpsee_throw(cx, "%s.array.range: element %i does not fit in a byte", throwPrefix, JSVAL_TO_INT(arrEl));
	else
	  (void)gpsee_throw(cx, "%s.array.range: element %f does not fit in a byte", throwPrefix, JSVAL_TO_DOUBLE(arrEl));

	goto failOut;
      }

      *bufp[arrEl] = (unsigned char)i;
    }
    else
    {
      jsdouble d;

      if (JS_ValueToNumber(cx, v, &d) == JS_FALSE)
	goto failOut;

      if (d < 0 || d > 255)
      {
	if (JSVAL_IS_INT(arrEl))
	  (void)gpsee_throw(cx, "%s.array.range: element %i does not fit in a byte", throwPrefix, JSVAL_TO_INT(arrEl));
	else
	  (void)gpsee_throw(cx, "%s.array.range: element %f does not fit in a byte", throwPrefix, JSVAL_TO_DOUBLE(arrEl));

	goto failOut;
      }
    }

    if (JS_GetArrayLength(cx, arr, &arrLen) == JS_FALSE) /* Keep re-checking length in case it changes */
      goto failOut;

    if (arrLen != allocArrLen)
    {
      unsigned char *newBuf;
      allocArrLen = arrLen;

      newBuf = JS_realloc(cx, *bufp, allocArrLen);
      if (!newBuf)
      {
	JS_ReportOutOfMemory(cx);
	goto failOut;
      }

      *bufp = newBuf;
    }
  }

  return JS_TRUE;

  failOut:
  free(*bufp);

  return JS_FALSE;
}

/**
 *  Transcode from one character encoding to another. This routine works on "buffers", i.e.
 *  C arrays of unsigned char. 
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
  iconv_t	cd;
  const char 	*inbuf;
  char	        *outbuf, *outbufStart;
  size_t	inbytesleft, outbytesleft;
  size_t	allocBytes, result;
  jsrefcount	depth;
  size_t	approxChars;

  if ((sourceCharset == targetCharset) || (sourceCharset && targetCharset && (strcasecmp(sourceCharset, targetCharset) == 0)))
  {
    *outputBuffer_p = JS_malloc(cx, inputBufferLength);
    if (!outputBuffer_p)
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

  inbytesleft  = inputBufferLength;
  inbuf 	 = (const char *)inputBuffer;
  outbuf	 = outbufStart;

  do
  {
    outbytesleft = allocBytes - (outbuf - outbufStart);

    depth = JS_SuspendRequest(cx);
    result = iconv(cd, (char **/*GNU has wrong prototype*/)&inbuf, &inbytesleft, &outbuf, &outbytesleft);
    JS_ResumeRequest(cx, depth);

    if (result == -1)
    {
      switch(errno)
      {
	case E2BIG:
	{
	  char *newBuf = JS_realloc(cx, outbufStart, inbytesleft + inbytesleft / 4 + 32);  /* WAG -- +32 implies output charset cannot exceed 32 bytes per character */
	  
	  if (!newBuf)
	  {
	    JS_free(cx, outbufStart);
	    iconv_close(cd);
	    JS_ReportOutOfMemory(cx);
	    return JS_FALSE;
	  }

	  outbuf += (newBuf - outbufStart);
	  outbufStart = newBuf;
	  break;
	}

	default:
	  JS_free(cx, outbufStart);
	  iconv_close(cd);
	  return gpsee_throw(cx, "%s.transcode: Transcoding error at source byte %i (%m)", 
			     throwPrefix, ((unsigned char *)inbuf - inputBuffer));
      }
    }
  } while (result == -1);

  *outputBufferLength_p = outbuf - outbufStart;
  if (*outputBufferLength_p != allocBytes)
  {
    char *newBuf = JS_realloc(cx, outbufStart, *outputBufferLength_p);
    if (newBuf)
      outbufStart = newBuf;
  }

  *outputBuffer_p = (unsigned char *)outbufStart;
  return JS_TRUE;
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

    if (JS_EncodeCharacters(cx, string_chars, string_length, *bufp, lenp) == JS_FALSE)
    {
      JS_free(cx, *bufp);
      return JS_FALSE;
    }

    return JS_TRUE;
  }

#if !defined(HAVE_ICONV)
  return gpsee_throw(cx, "%s.transcode.iconv.missing: Could not transcode UTF-16 String to %s charset; GPSEE was compiled without iconv "
		     "support.", throwPrefix, charset ?: "(null)");
#else
   if (string_length * 2 & 1 << ((sizeof(size_t) * 8) - 1))   /* JSAPI limit is currently lower than this; may 2009 wg from shaver */
    return gpsee_throw(cx, "%s.transcode.length: String length exceeds maximum characters, cannot convert", throwPrefix);
  else
    return transcodeBuf_toBuf(cx, charset, DEFAULT_UTF_16_FLAVOUR, bufp, lenp, (const unsigned char *)string_chars, string_length * 2, throwPrefix);
#endif
}
