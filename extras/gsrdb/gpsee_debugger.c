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
 * @file	gpsee_debugger.c
 * @brief       Support for adding debugger functionality to GPSEE embeddings.
 * @ingroup     debugger
 * @author	Wes Garland
 * @date	June 2010
 * @version	$Id: gpsee_debugger.c,v 1.1 2010/06/14 22:23:49 wes Exp $
 */
 
static __attribute__((unused)) const char rcsid[]="$Id: gpsee_debugger.c,v 1.1 2010/06/14 22:23:49 wes Exp $";

#include <prinit.h>
#include "gpsee.h"
#include "jsdebug.h"
#include "jsdb.h"

static void SendSourceToJSDebugger(const char *filename, uintN lineno,
                                   jschar *str, size_t length,
                                   void **listenerTSData, void *closure)
{
  JSDSourceText *jsdsrc = (JSDSourceText *) *listenerTSData;
  JSDContext *jsdc  = (JSDContext *)closure;

  if (!jsdsrc) {
    if (!filename)
      filename = "typein";
    if (1 == lineno) {
      jsdsrc = JSD_NewSourceText(jsdc, filename);
    } else {
      jsdsrc = JSD_FindSourceForURL(jsdc, filename);
      if (jsdsrc && JSD_SOURCE_PARTIAL !=
          JSD_GetSourceStatus(jsdc, jsdsrc)) {
        jsdsrc = NULL;
      }
    }
  }
  if (jsdsrc) {
    jsdsrc = JSD_AppendUCSourceText(jsdc,jsdsrc, str, length,
                                    JSD_SOURCE_PARTIAL);
  }
  *listenerTSData = jsdsrc;
}     

/** Initialize the debugger for this embedding. Not sure if this is per-cx or per-rt.
 *  Must be called before the embedding processes JavaScript to be debugged.
 *
 *  @param      cx              The current JavaScript context
 *  @param      realm           The current GPSEE realm
 *  @param      debugger        The fully-qualified path to the debugger.js we will invoke
 *  @returns    A new JSD Context
 */
JSDContext *gpsee_initDebugger(JSContext *cx, gpsee_realm_t *realm, const char *debugger)
{
  JSDContext    *jsdc;

  jsdc = JSD_DebuggerOnForUser(realm->grt->rt, realm, NULL, NULL);
  if (!jsdc)
    panic("Could not start JSD debugger layer");
  JSD_JSContextInUse(jsdc, cx);
  JS_SetSourceHandler(realm->grt->rt, SendSourceToJSDebugger, jsdc);

  if (JSDB_InitDebugger(realm->grt->rt, jsdc, 0, debugger) != JS_TRUE)
    panic("Could not start JSDB debugger layer");

  realm->grt->useCompilerCache = 0;       /* Interacts poorly with JSD source-code transmitter */

  return jsdc;
}

/** Turn off the debugger.
 *  @see gpsee_initDebugger()
 *
 *  @param      jsdc    The JSD Context to finalize
 */
void gpsee_finiDebugger(JSDContext *jsdc)
{
  JSDB_TermDebugger(jsdc);       
  JSD_DebuggerOff(jsdc);
}
