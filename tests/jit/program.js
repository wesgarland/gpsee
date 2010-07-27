// ***** BEGIN LICENSE BLOCK *****
// Version: MPL 1.1/GPL 2.0/LGPL 2.1
//
// The contents of this file are subject to the Mozilla Public License Version
// 1.1 (the "License"); you may not use this file except in compliance with
// the License. You may obtain a copy of the License at
// http://www.mozilla.org/MPL/
//
// Software distributed under the License is distributed on an "AS IS" basis,
// WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
// for the specific language governing rights and limitations under the
// License.
//
// The Initial Developer of the Original Code is PageMail, Inc.
//
// Portions created by the Initial Developer are 
// Copyright (c) 2007-2010, PageMail, Inc. All Rights Reserved.
//
// Contributor(s):
// 
// Alternatively, the contents of this file may be used under the terms of
// either of the GNU General Public License Version 2 or later (the "GPL"),
// or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
// in which case the provisions of the GPL or the LGPL are applicable instead
// of those above. If you wish to allow use of your version of this file only
// under the terms of either the GPL or the LGPL, and not to allow others to
// use your version of this file under the terms of the MPL, indicate your
// decision by deleting the provisions above and replace them with the notice
// and other provisions required by the GPL or the LGPL. If you do not delete
// the provisions above, a recipient may use your version of this file under
// the terms of any one of the MPL, the GPL or the LGPL.
//
// ***** END LICENSE BLOCK ***** 
//

/* 
 * @author	Wes Garland, wes@page.ca
 * @date	Jan 2010
 * @version	$Id: program.js,v 1.1 2010/01/26 22:37:30 wes Exp $
 * @file	program.js	Unit test to insure program modules can JIT.
 *
 * Tries to detect the JIT by looking at wall clock. We run
 * the same loop twice, once with and once without JIT option,
 * and compare execution times. We expect the JIT to be at
 * least 5 times faster.
 */

const jit_factor = 5;
var jit_start
var jit_end;
var start;
var end;
const loop_iterations = 500000;

jit_start = Date.now();
for (let i=0, loop_iterations=loop_iterations; i < loop_iterations; i++);
jit_end = Date.now();

require("vm").jit = false;
start = Date.now();
for (let i=0, loop_iterations=loop_iterations; i < loop_iterations; i++);
end = Date.now();

if (((end - start) / jit_factor) < (jit_end - jit_start))
{
  print("JIT does not appear to be functioning correctly")
  print("With JIT,    the loop ran in " + (jit_end - jit_start) + "ms.");
  print("Without JIT, the loop ran in " + (end - start) + "ms.");
  print("Expected time with JIT is less than " + Math.floor((end - start) / jit_factor) + "ms.");
  throw 1;
}
else
{
  print("JIT looks good");
  print("With JIT,    the loop ran in " + (jit_end - jit_start) + "ms.");
  print("Without JIT, the loop ran in " + (end - start) + "ms.");
}
