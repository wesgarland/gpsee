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
 *  @file 	gpsee_formats.h		Portable printf-style formats
 *  @author	Wes Garland
 *  @date	Jul 2009
 *  @version	$Id: gpsee_formats.h,v 1.3 2010/04/14 00:37:58 wes Exp $
 */

#define GPSEE_SIZET_FMT       "%zu"
#define GPSEE_SSIZET_FMT      "%zd"
#if defined(GPSEE_SUNOS_SYSTEM)
#define GPSEE_PTR_FMT         "0x%p"
#else
#define GPSEE_PTR_FMT         "%p"
#endif
#define GPSEE_INT_FMT         "%d"
#define GPSEE_UINT_FMT        "%u"
#define GPSEE_INT32_FMT       "%ld"
#define GPSEE_INT64_FMT       "%lld"
#define GPSEE_UINT32_FMT      "%lu"
#define GPSEE_UINT64_FMT      "%llu"
#define GPSEE_HEX_UINT32_FMT  "0x%lx"
#define GPSEE_HEX_UINT_FMT    "0x%x"
#define GPSEE_PTRDIFF_FMT     "%li"
