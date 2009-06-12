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
 *  @file	binary_module.h		Symbols shared between classes/objects in the binary module.
 *  @author	Wes Garland, PageMail, Inc., wes@page.ca
 *  @date	March 2009
 *  @version	$Id: binary_module.h,v 1.2 2009/06/12 17:01:21 wes Exp $
 */

#define MODULE_ID GPSEE_GLOBAL_NAMESPACE_NAME ".module.ca.page.binary"

JSObject *ByteString_InitClass(JSContext *cx, JSObject *obj, JSObject *parentProto);
JSObject *ByteArray_InitClass(JSContext *cx, JSObject *obj, JSObject *parentProto);
JSObject *Binary_InitClass(JSContext *cx, JSObject *obj);

/** Structure which can be cast to either a ByteArray or a ByteString handle */
typedef struct
{
  size_t		length;			/**< Number of characters in buf */
  const unsigned char 	*buffer;		/**< Backing store */
} byteThing_handle_t;

JSBool 		copyJSArray_toBuf	(JSContext *cx, JSObject *arr, unsigned char **bufp, size_t *lenp, 
					 const char *throwPrefix);
JSBool 		transcodeString_toBuf	(JSContext *cx, JSString *string, const char *codec, unsigned char **bufp, size_t *lenp,
					 const char *throwPrefix);
JSBool 		transcodeBuf_toBuf	(JSContext *cx, const char *targetCharset, const char *sourceCharset, 
					 unsigned char **outputBuffer_p, size_t *outputBufferLength_p, 
					 const unsigned char *inputBuffer, size_t inputBufferLength,
					 const char *throwPrefix);
JSObject	*byteString_fromCArray	(JSContext *cx, const unsigned char *buffer, size_t length, JSObject *obj, int stealBuffer);
JSObject 	*byteArray_fromCArray	(JSContext *cx, const unsigned char *buffer, size_t length, JSObject *obj, int stealBuffer);
JSBool          byteThing_getLength(JSContext *cx, JSObject *obj, jsval id, jsval *vp, const char const * className);
/* TODO I would like to see more advanced argument processing available as part of GPSEE's core module support. */
const char * byteThing_val2size(JSContext *cx, jsval val, size_t *rval, const char const * methodName);
JSBool byteThing_arg2size(JSContext *cx, uintN argc, jsval *vp, size_t *rval, uintN argn, size_t min, size_t max,
                          JSBool mayDefault, size_t defaultSize, const char const *methodName);

extern JSClass *byteString_clasp;
extern JSClass *byteArray_clasp;

#ifdef HAVE_ICONV
# define _LIBICONV_H
# if defined GPSEE_SUNOS_SYSTEM
/* 
 * /usr/sfw gcc can find sunfreeware gnu libiconv header, 
 * then explode at runtime when solaris iconv lib gets used 
 */
#  include "/usr/include/iconv.h"	
# else
#  include <iconv.h>
# endif
# include "jsnum.h"
#endif
