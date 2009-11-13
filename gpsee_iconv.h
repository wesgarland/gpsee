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
 *
 * @file	gpsee_iconv.h	Formal way for GPSEE apps/modules to include iconv.h
 *				that matches the iconv library GPSEE links against.
 * @author	Wes Garland, wes@page.ca
 * @date	Nov 2009
 * @version	$Id: gpsee_iconv.h,v 1.1 2009/11/13 19:34:48 wes Exp $
 */

#ifdef HAVE_ICONV
# if defined GPSEE_SUNOS_SYSTEM && !defined(GPSEE_DONT_PREFER_SUN_ICONV)
/*
 * /usr/sfw gcc can find sunfreeware gnu libiconv header,
 * then explode at runtime when solaris iconv lib gets used
 */
#  define _LIBICONV_H
#  include "/usr/include/iconv.h"
# else
#  include <iconv.h>
 
static __attribute__((unused)) size_t gpsee_non_susv3_iconv(iconv_t cd, const char **inbuf, size_t *inbytesleft, char **out$
{
  return iconv(cd, (char **)inbuf, inbytesleft, outbuf, outbytesleft);
}
# define iconv(a,b,c,d,e)       gpsee_non_susv3_iconv(a,b,c,d,e)
 
# endif
#endif
