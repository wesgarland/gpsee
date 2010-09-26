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
 * @file	gpsee-iconv.h	Formal way for GPSEE apps/modules to include iconv.h
 *				that matches the iconv library GPSEE links against.
 * @author	Wes Garland, wes@page.ca
 * @date	Nov 2009
 * @version	$Id: gpsee-iconv.h,v 1.2 2010/03/06 18:17:13 wes Exp $
 */

#ifdef HAVE_ICONV
/*
 * This file is required because of
 *  - changing prototype of SUSv2 / SUSv3 iconv
 *  - Macports vs. Apple iconv
 *  - SFW vs. Sun iconv
 *  - etc
 *
 * This file works in tandem with ./iconv.mk
 */

#if !defined(ICONV_HEADER)
# include <iconv.h>
#endif

static __attribute__((unused)) size_t gpsee_iconv(iconv_t cd, const char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft)
{
  size_t (*iconvSym)(iconv_t cd, const char **, size_t *, char **, size_t *) = (void *)iconv;

  return iconvSym(cd, inbuf, inbytesleft, outbuf, outbytesleft);
}

#undef iconv
#define iconv(a,b,c,d,e)       gpsee_iconv(a,b,c,d,e)
 
#endif
