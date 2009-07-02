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
 * Copyright (c) 2009, PageMail, Inc. All Rights Reserved.
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
 *  @file       bytethings.h    Useful header for interfacing with ByteString
 *                              and ByteArray native JSAPI classes.
 *  @author     Donny Viszneki, PageMail, Inc., donny.viszneki@gmail.com
 *  @date       June 2009
 *  @version    $Id$
 */

#ifndef _BYTETHINGS_H_09GU093WUG039UG0U
#define _BYTETHINGS_H_09GU093WUG039UG0U

/** Structure which can be cast to either a ByteArray or a ByteString handle */
typedef struct
{
  size_t                length;                 /**< Number of characters in buf */
  unsigned char         *buffer;                /**< Backing store */
} byteThing_handle_t;
typedef struct
{
  size_t                length;         /**< Number of characters in buffer */
  unsigned char         *buffer;        /**< Backing store */
} byteString_handle_t;

typedef struct
{
  size_t                length;         /**< Number of characters in buffer */
  unsigned char         *buffer;        /**< Backing store */
  unsigned char         *realBuffer;    /**< Larger block of memory encompassing 'buffer' */
  size_t                capacity;       /**< Amount of memory allocated for buffer */
} byteArray_handle_t;

byteThing_handle_t *byteThing_newHandle(JSContext *cx);
JSObject           *byteThing_fromCArray(JSContext *cx,
                                         const unsigned char *buffer, size_t length,
                                         JSObject *obj, JSClass *clasp,
                                        JSObject *proto, size_t btallocSize, int stealBuffer);
JSBool copyJSArray_toBuf(JSContext *cx, JSObject *arr, size_t start, unsigned char **bufp, size_t *lenp, JSBool *more, const char *throwPrefix);
JSBool          transcodeString_toBuf   (JSContext *cx, JSString *string, const char *codec, unsigned char **bufp, size_t *lenp,
                                         const char *throwPrefix);
JSBool          transcodeBuf_toBuf      (JSContext *cx, const char *targetCharset, const char *sourceCharset, 
                                         unsigned char **outputBuffer_p, size_t *outputBufferLength_p, 
                                         const unsigned char *inputBuffer, size_t inputBufferLength,
                                         const char *throwPrefix);
JSBool          byteThing_getLength(JSContext *cx, JSObject *obj, JSClass *clasp, jsval id, jsval *vp);
const char * byteThing_val2byte(JSContext *cx, jsval val, unsigned char *retval);
JSBool byteThing_val2bytes(JSContext *cx, jsval *vals, int nvals, unsigned char **resultBuffer, size_t *resultLen,
                           JSBool *stealBuffer, JSClass *clasp, const char const *methodName);
const char * byteThing_val2ssize(JSContext *cx, jsval val, ssize_t *rval, const char const * methodName);
JSBool byteThing_arg2ssize(JSContext *cx, uintN argc, jsval *vp, ssize_t *retval, uintN argn, ssize_t min, ssize_t max,
                          JSBool mayDefault, ssize_t defaultSize, JSClass *clasp, const char const *methodName);
const char * byteThing_val2size(JSContext *cx, jsval val, size_t *rval, const char const * methodName);
JSBool byteThing_arg2size(JSContext *cx, uintN argc, jsval *vp, size_t *retval, uintN argn, size_t min, size_t max,
                          JSBool mayDefault, size_t defaultSize, JSClass *clasp, const char const *methodName);
JSBool byteThing_toByteThing(JSContext *cx, uintN argc, jsval *vp, JSClass *classOfResult, JSObject *protoOfResult, size_t privAllocSizeOfResult);
JSBool byteThing_decodeToString(JSContext *cx, uintN argc, jsval *vp, JSClass *clasp);
JSBool byteThing_byarAt(JSContext *cx, uintN argc, jsval *vp, JSClass *clasp, JSClass *rtype, const char const * methodName);
JSBool byteThing_toString(JSContext *cx, uintN argc, jsval *vp);
/* TODO toArray() needs to be like toString above and accept the direct JSAPI call */
JSObject *byteThing_toArray(JSContext *cx, const unsigned char *bytes, size_t len);
inline byteThing_handle_t * byteThing_getHandle(JSContext *cx, JSObject *obj, JSClass **claspp, const char const * methodName);
JSBool byteThing_toSource(JSContext *cx, uintN argc, jsval *vp, JSClass *class);
JSBool byteThing_findChar(JSContext *cx, uintN argc, jsval *vp, void *memchr_fn(const void *, int, size_t), const char const * methodName);
JSBool byteThing_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp, JSClass *clasp);

#endif/*_BYTETHINGS_H*/

