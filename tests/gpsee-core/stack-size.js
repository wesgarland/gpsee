#! /usr/bin/gsr -dd

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
// The Initial Developer of the Original Code is Ash Berlin
//
// Portions created by the Initial Developer are 
// Copyright (c) 2007-2009, PageMail, Inc. All Rights Reserved.
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

///////////////////////////////////////////////////////////
// This test succeeds if the embedding does not segfault //
///////////////////////////////////////////////////////////

if (typeof exports === "undefined") this.exports = {};

exports.get_caller = function() {
}

exports.deprecation_warning = function () {
    var stack = exports.getCaller(2);
};

function compat_alias(obj, display, old_name, new_name ) {
  if ( new_name === undefined ) {
    // Not passed, guess it
    new_name = old_name.replace( /([a-z])([A-Z]+)/g, 
      function(all, l1, l2) { return l1 + "_" + l2.toLowerCase() } 
    )
  }

  obj.__defineGetter__( old_name, function() { 
      exports.deprecation_warning(display + old_name , "0.3");
      return obj[ new_name ]; 
  } );
  obj.__defineSetter__( old_name, function( x ) { 
      exports.deprecation_warning(display + old_name , "0.3");
      return obj[ new_name ] = x; 
  } );
}
compat_alias(exports, "obj.", "getCaller");

try
{
  exports.getCaller(1);
}
catch(e)
{
  if (e.toString().indexOf("too much recursion"))
    print("Test passes (fail is usually segfault)");
  else
    throw e;
}

