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
 */

/** 
 * @file	gpsee_precompiler.c	GPSEE script precompiler, part of the build/install processes
 * @author	Wes Garland
 * @date	Apr 2010
 * @version	$Id: gpsee_precompiler.c,v 1.5 2010/12/02 21:59:42 wes Exp $
 */
static __attribute__((unused)) const char rcsid[]="$Id: gpsee_precompiler.c,v 1.5 2010/12/02 21:59:42 wes Exp $";

#include "./gpsee.h"
#if defined(GPSEE_DARWIN_SYSTEM)
#include <crt_externs.h>
#endif
static void __attribute__((noreturn)) usage(const char *argv_zero)
{
  printf(
	 "Script precompiler for GPSEE " GPSEE_CURRENT_VERSION_STRING "\n"
	 "Copyright (c) 2007-2010 PageMail, Inc. All Rights Reserved.\n"                 
	 "\n"
	 "Usage: %s <filename>\n"
	 "\n"
	 "This program creates precompiled cache files for scripts without\n"
	 "executing them. It is intended to be used during the GPSEE install\n"
	 "process, so that system libraries can be precompiled with sudo,\n"
	 "rather than requiring that first-time users of said libraries be\n"
	 "privileged users.\n"
	 "\n", argv_zero
	 );
  exit(2);
};

int main(int argc, char *argv[])
{
  gpsee_interpreter_t	*jsi;				/* Handle describing JS interpreter */
  const char		*scriptFilename;		/* Filename with JavaScript program in it */
  FILE			*scriptFile;
  JSScript        	*script;
  JSObject        	*scrobj;
  int			exitCode;
  int			jsOptions;

  if (argc != 2)
    usage(argv[0]);

  gpsee_verbosity(2);

  jsi = gpsee_createInterpreter();

  jsOptions = JS_GetOptions(jsi->cx) | JSOPTION_ANONFUNFIX | JSOPTION_STRICT | JSOPTION_RELIMIT | JSOPTION_JIT;	/* match GSR baseline */
  JS_SetOptions(jsi->cx, jsOptions | JSOPTION_WERROR);

  scriptFilename = argv[1];
  scriptFile = fopen(scriptFilename, "r");

  if (!scriptFile)
  {
    fprintf(stderr, "Unable to open' script '%s'! (%s)\n", scriptFilename, strerror(errno));
    exitCode = 1;
    goto out;
  }

  if (fgetc(scriptFile) == '#' && fgetc(scriptFile) == '!' && fgetc(scriptFile) == ' ')
    while(fgetc(scriptFile) != '\n');
  else
    rewind(scriptFile);

  printf(" * Precompiling %s\n", scriptFilename);
  if (gpsee_compileScript(jsi->cx, scriptFilename, scriptFile, NULL, &script, jsi->globalObject, &scrobj) == JS_FALSE)
  {
    fprintf(stderr, "Could not compile %s (%s)\n", scriptFilename, errno ? strerror(errno) : "no system error");
    exitCode = 1;
  }
  else
  {
    exitCode = 0;
  }

  fclose(scriptFile);
 out:
  scrobj = NULL;
  script = NULL;
  gpsee_destroyInterpreter(jsi);
  JS_ShutDown();

  return exitCode;
}

