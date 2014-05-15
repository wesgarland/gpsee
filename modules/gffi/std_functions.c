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
 * ***** END LICENSE BLOCK ***** */

/**
 *  @file	std_functions.c	Support code for GPSEE's gffi module which
 *				exposes standard C function symbols to 
 *				CFunction.
 *
 *				This is necessary to effectively de-reference
 *				symbols which are implemented as macros to other
 *				symbols by the host's standard C library. 
 *				GNU libc is a major offender in this regard;
 *				"stat" makes a good test case. 
 *
 *  @author	Wes Garland
 *              PageMail, Inc.
 *		wes@page.ca
 *  @date	Nov 2009
 *  @version	$Id: std_functions.c,v 1.4 2009/11/10 22:04:54 wes Exp $
 */

#include "std_gpsee_no.h"
#if defined(_GNU_SOURCE)
# define __deprecated__  
# define __attribute_deprecated__  
# define GPSEE_NO_gets
#endif
#include "std_functions.h"

struct fn_s
{
  const char 	*name;
  void		*address;	
};

static inline int vstrcmp(const void *s1, const void *s2)
{
  return strcmp((const char *)s1, ((struct fn_s *)s2)->name);
}

void *findPreDefFunction(const char *name)
{
  void 		*p;
  static struct fn_s	unixFunctions[] =
  {
#define defineFunction(fn)	{ #fn, (void *)fn },
#include "unix_api_v3.incl"
#undef defineFunction
  };

#if defined(GPSEE_XCURSES)
  static struct fn_s	cursesFunctions[] =
  {
#define defineFunction(fn)	{ #fn, (void *)fn },
#include "xcurses_api.incl"
#undef defineFunction
  };
#endif

#if defined(GPSEE_DEBUG_BUILD) || defined(GPSEE_DRELEASE_BUILD)
  assert(sizeof(unixFunctions));
#endif
  p = bsearch(name, unixFunctions, sizeof(unixFunctions) / sizeof(unixFunctions[0]), sizeof(unixFunctions[0]), vstrcmp);

#if defined(GPSEE_XCURSES)
  assert(sizeof(cursesFunctions));
  if (!p)
    p = bsearch(name, cursesFunctions, sizeof(cursesFunctions) / sizeof(cursesFunctions[0]), sizeof(cursesFunctions[0]), vstrcmp);
#endif

  if (!p)
    return NULL;

  return ((struct fn_s *)p)->address;
}

