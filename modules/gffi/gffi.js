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
 *  @file	gffi_module.c  		JavaScript portions GPSEE's libFFI module, providing
 *                                      access to C programming abstractions such as pointers
 *                                      and C ABI function invocation.
 *  @author	Donny Viszneki
 *		donny.viszneki@gmail.com
 *  @date	Aug 2009
 *  @version	$Id: gffi.js,v 1.4 2010/04/14 00:38:39 wes Exp $
 */

/* We now box up return values with private instances of WillFinalize, a native class that has a finalizer, so that
 * when the GC gets to it, the notional value can get cleaned up. This is useful for calling free() or fclose(), for
 * instance. Because the garbage collector is a bad place to clean up, however, GPSEE will issue a warning to the log
 * stream each time it gets used. */

/* The BoxedPrimitive can have a WillFinalize instance. It also has a valueOf() method to keep acting like a
 * primitive data type. */
exports.BoxedPrimitive = function(value) {
  this.value = value;
}
exports.BoxedPrimitive.prototype.valueOf = function() {
  return this.value;
}
exports.BoxedPrimitive.prototype.finalizeWith = function() {
  if (!this.hasOwnProperty('finalizer'))
    this.finalizer = new exports.WillFinalize;
  return this.finalizer.finalizeWith.apply(this.finalizer, arguments);
}
/* This will run and remove a finalizer */
exports.BoxedPrimitive.prototype.destroy = function() {
  if (this.hasOwnProperty('finalizer'))
    this.finalizer.runFinalizer();
}
exports.BoxedPrimitive.prototype.toString = function() {
  return '[gpsee.module.ca.page.gffi.BoxedPrimitive ' + typeof this + ' ' + this.value + ']';
}

exports.CFunction.prototype.call = function() {
  var rval = exports.CFunction.prototype.unboxedCall.apply(this, arguments);
  if (rval === null) return null;
  if (rval === undefined) return undefined;
  /* If we get an object back, it doesn't need boxing */
  if ('object' === typeof rval) {
    /* Add the finalizeWith() and destroy() instance methods to object return values. TODO is this the best way to do this? */
    rval.finalizeWith = exports.BoxedPrimitive.prototype.finalizeWith;
    rval.destroy = exports.BoxedPrimitive.prototype.destroy;
    return rval;
  } else {
    /* Box primitive values so that they can be garbage collected, and support a finalizeWith() instance method */
    return new exports.BoxedPrimitive(rval);
  }
}
