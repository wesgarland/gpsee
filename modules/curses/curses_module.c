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
 *  @file	curses_module.c		A GPSEE module for exposing curses to JS.
 *  @author	Wes Garland
 *              PageMail, Inc.
 *		wes@page.ca
 *  @date	Jan 2008
 *  @version	$Id: curses_module.c,v 1.3 2009/07/31 16:07:34 wes Exp $
 */

static const char __attribute__((unused)) rcsid[]="$Id: curses_module.c,v 1.3 2009/07/31 16:07:34 wes Exp $";

#include "gpsee.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <curses.h>
#include <signal.h>
#include "curses_module.h"

/** Curses-mode error logger.
 *  Writing directly to stdout/stderr once curses has control of the
 *  screen is iffy at best.
 */
void cursesStdScrErrorLogger(JSContext *cx, const char *prefix, const char *message)
{
  jsrefcount depth;

  depth = JS_SuspendRequest(cx);

  scrollok(stdscr, TRUE);
  move(LINES - 1, 0);
  addstr((char *)"\n");
  attron(A_BOLD);
  addstr((char *)prefix);
  attroff(A_BOLD);
  addstr(" ");
  addstr((char *)message);
  addstr("\n\n");
  refresh();

  JS_ResumeRequest(cx, depth);
}

/** Initialize the module, Window class, and keyboard object */
const char *curses_InitModule(JSContext *cx, JSObject *moduleObject)
{
  /** Classes & objects provided by module */
  gpsee_interpreter_t 	*jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));

  if (Window_InitClass(cx, moduleObject) == NULL)
    return NULL;

  if (keyboard_InitObject(cx, moduleObject) == NULL)
    return NULL;

  if (!isatty(fileno(stdout)))
    filter();

  initscr();
  raise(SIGWINCH);
  jsi->errorLogger = cursesStdScrErrorLogger;
  noecho();
  raw();
  nonl();

  return MODULE_ID;
}

JSBool curses_FiniModule(JSContext *cx, JSObject *moduleObject)
{
  endwin();
  puts("\n\n");

  return JS_TRUE;
}

