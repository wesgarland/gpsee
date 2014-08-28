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
 *  @file	mime_charsets.c         Support routines for MIME "charsets" - 
 *                                      encoding schemes used by MIME to escape
 *                                      binary data. Based on work in 
 *                                      smapi_mime.c version 1.13.
 *
 *  @author	Wes Garland
 *              PageMail, Inc.
 *		wes@page.ca
 *  @date	Mar 2000, Aug 2014
 */

#include <gpsee.h>

static char hex[]="0123456789ABCDEF";
static char B64_ALPHABET[] = "ABCDEFGH"
                             "IJKLMNOP"
                             "QRSTUVWX"
		             "YZabcdef"
		             "ghijklmn"
		             "opqrstuv"
		             "wxyz0123"
		             "456789+/"
                             "=";

/** Decode a single (the first) byte from a quoted-printable character sequence.
 *
 *  @param      inBuf           The character sequence to decode.
 *  @param      conv_c  [out]   The decoded character, which may be a pointer into
 *                              inBuf, or a statically-allocated character which might
 *                              be reused on a subsequent call to this routine.
 *  @returns    An index into inBuf which is the first character of the next 
 *              character sequence.
 */
static const char *qp_decode_byte(const char *inBuf, unsigned char *conv_c, int *emittedByte_p)
{   
  /* See RFC 2045, section 6.7 */

  static char   newchar;
  char          *tmp;
 
  *emittedByte_p = 1;

  /* Rule #2, Literal representation */
  if ((inBuf[0] >= (char)33) && (inBuf[0] <= (char)126) && (inBuf[0] != '='))
  { 
    *conv_c = inBuf[0];
    return inBuf + 1;
  }

  /* Rule #3, Whitespace */ 
  if (strchr(" \t", inBuf[0]))
  {
    if (strspn(inBuf + 1, " \t\n\r") == strlen(inBuf + 1)) /* Rest of inBuf is whitespace? */
    {
      char	*cr, *lf;

      *emittedByte_p = 0;		/* 3.2, Trailing Whitespace */
      
      cr = strchr(inBuf, '\r');
      lf = strchr(inBuf, '\n');

      if (cr && (cr < lf))		/* Advance to CR */
	return cr;

      if (lf)				/* Advance to LF */
	return lf;
      
      return inBuf + strlen(inBuf);		/* Advance to EOL (NUL) */
    }
    else
    {
      *conv_c = inBuf[0];			/* 3.1, Literal Whitespace */
      return inBuf + 1;
    }
  }

  if (inBuf[0] == '=')
  {
    /* Rule #5.2, Soft Line Breaks after = signs, taking into 
    ** account Rule #3.2, Trailing Whitespace 
    */

    if (strspn(inBuf + 1, " \t\n\r") == strlen(inBuf + 1))
    {
      *emittedByte_p = 0;
      return inBuf + strlen(inBuf); /* Advance to EOL (the NUL) */
    }

    /* Rule #1, General 8bit representation */
    tmp=strchr(hex, inBuf[1]);
    if (tmp == NULL)
    {
      *conv_c = inBuf[0];
      return inBuf + 1;
    }
    else
      newchar = (tmp - hex) * 16;
     
    tmp=strchr(hex, inBuf[2]);
    if (tmp == NULL)
    {
      *conv_c = inBuf[0];
      return inBuf + 1;
    }
    else
      newchar += (tmp - hex);
    
    *conv_c = newchar;
    return inBuf + 3;
  }

  /* Catch-all, literal representation for non-compliant encoding and line breaks.. */
  *conv_c = inBuf[0];
  return inBuf + 1;
}

/** Encode a byte into quoted-printable encoding.
 *  @param      c	character (byte) to encode, if it needs to be encoded.
 *  @param      force	characters to encode, regardless of standard rules.
 *
 *  @returns    A statically allocated buffer which may be re-used on
 *              subsequent calls to this routine. Buf is always ASCIIZ.
 *
 *  @see        RFC 2045
 *
 *  @note       Must be able to "peek" one character ahead of c
 *              in order to make decisions! If c is end of string, then
 *              c+1 should be set to (char)0
 *
 *  @note       Input text is considered to have no CRs and that
 *              LFs are to be treated like RFC-822 line breaks.
 */
static const char *qp_encode_byte(unsigned char *c, const char *force)
{
  static char 	buf[8];

  if (force ? strchr(force, *c) : 0)
  {
    sprintf(buf, "=%02X", *c);			/* Sort of Rule 1 */
    return buf;
  }

  if ((*c >= 33) && (*c <= 126) && (*c != 61))	/* Rule 2 */
  {
    buf[0] = *c;
    buf[1] = (char)0;
    return buf;
  }

  if ((*c == 32) || (*c == 9))			/* Rule 3 */
  {
    if ((c[1] == '\n') || (c[1] == '\r'))
      sprintf(buf, "=%02X", *c);
    else
    {
      buf[0] = *c;
      buf[1] = (char)0;
    }
    return buf;
  }

  if (c[0] == '\r')				/* Non-conforming */
    return "";

  if (c[0] == '\n')				/* N/C Rule 4 */
    return "\r\n";

  sprintf(buf, "=%02X", *c);			/* Rule 1 */
  return buf;
}

/** Returns a C string describing
 * in_buf as a quoted-printable entity. Not quite compliant
 * w.r.t. CRLF sequences.
 *
 *  @param      force   array of characters to be encoded even if they can be sent plain
 *  @returns    A JS_malloc()'d buffer or NULL when exception is thrown
 */
char *binary_to_qp_alloc(JSContext *cx, unsigned char *in_buf, size_t n_els, const char *force)
{
  size_t        buf_used=0;
  size_t 	buf_alloc=0;
  size_t        x;
  char 	        *buf=NULL;
  const char    *tmp;
  int	        last_cr=0;

  if (n_els == 0)
    return JS_strdup(cx, "");

  buf_alloc = n_els + (n_els / 10);	/* Guess: alloc amount as 10% more than input */
  buf       = malloc(buf_alloc);

  if (buf == NULL)
    return NULL;

  for (x=0; x < n_els; x++)
  {
    char *btmp;

    if (buf_used + 8 >= buf_alloc)
    {
      buf_alloc += 256;			/* Guess: we're shy by about 256 bytes.. */
      btmp = realloc(buf, buf_alloc);

      if (btmp == NULL)
      {
	JS_free(cx, buf);
	return NULL;
      }
      else
	buf=btmp;
    }

    if (x == n_els)
    {
      unsigned char tmp_buf[2]={in_buf[x], (char)0};
      tmp = qp_encode_byte(tmp_buf, force);
    }
    else
      tmp = qp_encode_byte(in_buf + x, force);

    if (tmp[0] == '\r')
      last_cr = buf_used+2;
    else
      if ((buf_used - last_cr) >= (76 - 1))	/* Section 5, soft line breaks */
      {
	memcpy(buf + buf_used, "=\r\n", 3);
	buf_used += 3;
        last_cr = buf_used;
      }

    memcpy(buf + buf_used, tmp, strlen(tmp));
    buf_used += strlen(tmp);
  }

  *(buf + buf_used) = (char)0;	/* make ASCIZ */

  return buf;
}

/** Decode a quoted-printable buffer. Output buffer must be at least
 *  as long as the input buffer to avoid overflows.
 *
 *  @param      qp              buffer to decode (ASCIIZ)
 *  @param      outBuf          pre-allocated output buffer, or NULL to allocate with JS_malloc
 *  @param      outLen_p        pointer to number of bytes placed into the output buffer
 *  @returns    output buffer, or NULL when exception is thrown.
 */
unsigned char *qp_to_binary(JSContext *cx, const char *qp, unsigned char *outBuf, size_t *outLen_p)
{
  int emittedByte;

  if (!outBuf)
  {
    outBuf = JS_malloc(cx, strlen(qp));
    if (!outBuf)
      return NULL;
  }
  outBuf[0] = (char)0;

  while(*qp)
  {
    qp=qp_decode_byte(qp, outBuf + *outLen_p, &emittedByte); /* Note: qp advances by 1 or more chars */
    if (emittedByte)
      (*outLen_p)++;
  }

  outBuf[*outLen_p] = (char)0;

  return outBuf;
}

/** Decode a base64-encoded buffer. Output buffer must be at least
 * ((strlen(b64) / 4) * 3) + 2 bytes long to avoid overflow. 
 *
 *  @param      b64             buffer to decode (ASCIIZ)
 *  @param      buf             output buffer, or NULL to allocate it ourselves
 *  @param      outChars_p      pointer to number of characters placed into the output buffer
 *  @returns    buf          
 */
unsigned char *b64_to_binary(JSContext *cx, const char *b64, unsigned char *buf, size_t *outChars_p)
{
  unsigned char *buf_p;
  const char	*b64_p, *alpha_p;
  unsigned char	leftover=0, emit=0, value;

  if (!buf)
  {
    buf = JS_malloc(cx, ((strlen(b64) * 6) / 8) + 1);
    if (!buf)
      return NULL;
  }
    
  *buf = (char)0;
  for (buf_p=buf, b64_p = b64; 
       *b64_p && (alpha_p = strchr(B64_ALPHABET, *b64_p));
       b64_p++)
  {
    value = (alpha_p - B64_ALPHABET);

    if (*b64_p == '=')       		/* Time to finish up? */
    {
      if (((b64_p - b64) % 4) == 1)	/* A=== case */
	*buf_p++ = leftover;

      break;
    }

    switch((b64_p - b64) % 4)
    {
      case 0:				/* Start of a quartet; no character emitted */
	leftover = value << 2;
	continue;
      case 1:				/* Emit first character, result of 1st & 2nd encoded chars.. */
	emit	 = (value >> 4) | leftover;
	leftover = (value << 4);
	break;
      case 2:				/* Emit second character, result of 2nd & 3rd encoded chars.. */
	emit	 = (value >> 2) | leftover;
	leftover = (value << 6);
	break;
      case 3:				/* Emit last character, result of 3rd & 4th encoded chars.. */
	emit	 = value | leftover;
	leftover = 0;
	break;
    }
    
    *buf_p++ = emit;
  }

  *outChars_p = (size_t)(buf_p - buf);
  return buf;
}

/** Encode up to three bytes into base 64.
 */
static char *b64_encode_3bytes(int c_arr[3])
{
  static char enc_c[4];

  /* Encode left six bits of c_arr[0] into first character */
  enc_c[0] = B64_ALPHABET[c_arr[0] >> 2];
  
  if (c_arr[1] == EOF)
  {
    enc_c[1] = B64_ALPHABET[(c_arr[0] & 0x03) << 0x04];
    enc_c[2] = '=';
    enc_c[3] = '=';
    return enc_c;
  }

  /* Encode right two bits of c_arr[0] + left four bits of c_arr[1] into next character */
  enc_c[1] = B64_ALPHABET[(c_arr[0] & 0x03) << 0x04 | (c_arr[1] >> 0x04)];

  if (c_arr[2] == EOF)
  {
    enc_c[2] = B64_ALPHABET[(c_arr[1] & 0x0F) << 0x02];
    enc_c[3] = '=';
    return enc_c;
  }

  enc_c[2] = B64_ALPHABET[(c_arr[1] & 0x0F) << 0x02 | (c_arr[2] >> 0x06)];
  enc_c[3] = B64_ALPHABET[(c_arr[2] & 0x3F)];

  return enc_c;
}

/** Copy a buffer into a new buffer, converting to base-64 as we go.
 *  @param      in      The buffer to convert
 *  @param      inLen   The number of bytes in the input buffer
 *  @param      out     The buffer in which to write the output, or NULL
 *  @returns    The number of bytes written into out
 *
 *  @note       Out must be at least ((((inLen) + 2) / 3) * 4) + 1 bytes long,
 *              to hold the encoded data and the trailing NUL.
 *
 *              If out is NULL this routine returns the number of bytes
 *              which would have been written into out had it been non-NULL.
 */
size_t binary_to_b64(const unsigned char *in, size_t inLen, char *out)
{ 
  int           c[3];
  int           i;
  size_t        outLen = 0;

  for (i=0; i < inLen; i++)
  {
    if (i && i % 3 == 0)
    {
      if (out)
        memcpy(out + outLen, b64_encode_3bytes(c), 4);
      outLen += 4;
    }
    c[i % 3] = in[i];
  }

  switch(i % 3)
  {
    case 1:
      c[1] = 0;
    case 2:
      c[2] = 0;
    case 0:     
      if (out)
        memcpy(out + outLen, b64_encode_3bytes(c), 4);
      break;
  }

  switch(i % 3)
  {
    case 0:
      out[outLen + 4] = (char)0;
      break;
    case 1:
      out[outLen + 2] = '=';
    case 2:
      out[outLen + 3] = '=';
      out[outLen + 4] = (char)0;
      outLen += (i % 3) + 1;      
      break;
  }

  return outLen;
}
