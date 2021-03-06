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
 * @file	minimal.c           A minimal GPSEE embedding: "Hello, world"
 * @author	Wes Garland, wes@page.ca
 * @date	Jan 2008
 * @version	$Id: minimal.c,v 1.4 2010/09/01 18:12:35 wes Exp $
 */
 
static __attribute__((unused)) const char rcsid[]="$Id: minimal.c,v 1.4 2010/09/01 18:12:35 wes Exp $";

#include <gpsee.h>

int main (int argc, char *argv[])
{
  jsval			rval;
  gpsee_interpreter_t	*jsi;
  const char            *scriptCode;

#if defined(__SURELYNX__) 
  apr_initRuntime();		/* PageMail-proprietary requirement */
#endif

  gpsee_openlog("minimal");

  jsi = gpsee_createInterpreter();

  scriptCode = "print('Hello, World!');";
  JS_EvaluateScript(jsi->cx, jsi->globalObject, scriptCode, strlen(scriptCode), "anonymous", 1, &rval);
  rval = JSVAL_NULL;	/* rval is a potential GC root with the conservative stack-scanning collector */

  gpsee_destroyInterpreter(jsi);
  gpsee_closelog();
  return 0;
}

