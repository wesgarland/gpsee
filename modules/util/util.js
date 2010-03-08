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
 *  @file       util_module.c           GPSEE utilities
 *  @author     Donny Viszneki
 *              donny.viszneki@gmail.com
 *  @date       Sep 2009
 *  @version    $Id: util.js,v 1.1 2010/03/06 18:37:28 wes Exp $
 */

/* Definitions */

/** ParentError() can be used to throw an advisory error to the caller of your function. The 'stack' property of the
 *  new object that is returned excludes stack frames not relevant to your user. The stack frames which are removed
 *  from the backtrace of the error object are saved in a new property, 'hidden.'
 *
 *  ParentError() accepts invocations of the following two forms:
 *
 *  new ParentError("message string");
 *  new ParentError(ErrorConstructor, constructor arguments...);
 */
function ParentError() {
  var error;
  /* Instantiate an error */
  if ('function' == typeof arguments[0])
    error = arguments[0].apply(this, Array(arguments).splice(1));
  else
    error = new Error(arguments[0]);
  var stack = error.stack.split('\n');
  error.stack = stack.splice(2).join('\n');
  error.hidden = stack.join('\n');
  return error;
}

/* Exports */

exports.ParentError = ParentError;