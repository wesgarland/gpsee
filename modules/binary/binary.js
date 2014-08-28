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
 *  @file	binary_module.c		JavaScript portions of GPSEE implementation of the
 *					ServerJS binary module. Thismodule provides ByteString 
 *					and ByteArray, which are both instances of Binary.
 *  @author	Donny Viszneki
 *		donny.viszneki@gmail.com
 *  @date	Jun 2009
 *  @version	$Id: binary.js,v 1.3 2010/03/06 18:17:13 wes Exp $
 */

/** ByteArray.forEach(callback)
 *  @name           ByteArray.forEach
 *  @param          value
 *  @function
 *  @public
 *  
 *  Calling this function is equivalent to calling "callback(a[n], n, a)" where
 *  'a' is this instance of ByteArray, 'a[n]' is the nth member of this ByteArray,
 *  and 'callback' is the argument to forEach().
 */
exports.ByteArray.prototype.forEach = function(f) {
    for (var i=0, l=this.length; i<l; i++)
      f(this[i], i, this);
}
/** ByteArray.every(callback)
 *  @name           ByteArray.every
 *  @param          callback
 *  @function
 *  @public
 *
 *  Returns true if every element in this ByteArray satisfies the provided testing function.
 */
exports.ByteArray.prototype.every = function(f) {
    for (var i=0, l=this.length; i<l; i++)
      if (!f(this[i], i, this))
        return false;
    return true;
}
/** ByteArray.some(callback)
 *  @name           ByteArray.some
 *  @param          callback
 *  @function
 *  @public
 *
 *  Returns true if at least one element in this ByteArray satisfies the provided testing function.
 */
exports.ByteArray.prototype.some = function(f) {
    for (var i=0, l=this.length; i<l; i++)
      if (f(this[i], i, this))
        return true;
    return false;
}
/** ByteArray.filter(callback)
 *  @name           ByteArray.filter
 *  @param          callback
 *  @function
 *  @public
 *
 *  Returns a new array with the results of calling a provided function on every element in this
 *  ByteArray.
 *
 *  TODO Option for an Array result
 */
exports.ByteArray.prototype.filter = function(f) {
  var rval = new ByteArray();
  for (var i=0, l=this.length; i<l; i++)
    if (f(this[i], i, this))
      rval.push(this[i]);
  return rval;
}
/** ByteArray.map(callback)
 *  @name           ByteArray.map
 *  @param          callback
 *  @function
 *  @public
 *
 *  Returns a new array with the results of calling a provided function on every element in this
 *  ByteArray.
 *
 *  TODO Option for an Array result
 */
exports.ByteArray.prototype.map = function(f) {
  var rval = new ByteArray();
  rval.length = this.length;
  for (var i=0, l=this.length; i<l; i++)
    rval[i] = f(this[i], i, this);
  return rval;
}
/** ByteArray.reduce(callback)
 *  @name           ByteArray.reduce
 *  @param          callback
 *  @function
 *  @public
 *
 *  Apply a function simultaneously against two values of this ByteArray (from left-to-right) as
 *  to reduce it to a single value.
 */
exports.ByteArray.prototype.reduce = function(f) {
  var rval = this[0];
  for (var i=1, l=this.length; i<l; i++)
    rval = f(rval, this[i]);
  return rval;
}
/** ByteArray.reduceRight(callback)
 *  @name           ByteArray.reduceRight
 *  @param          callback
 *  @function
 *  @public
 *
 *  Apply a function simultaneously against two values of this ByteArray (from right-to-left) as
 *  to reduce it to a single value.
 */
exports.ByteArray.prototype.reduceRight = function(f) {
  var rval = this[this.length-1];
  for (var i=this.length-2; i>=0; i--)
    rval = f(rval, this[i]);
  return rval;
}

/** Converts a string to a ByteArray encoded in charset. */
function binary$$String$toByteArray(charset)
{
  return new exports.ByteArray(this, charset ? charset : "utf-16");
}
Object.defineProperty(String.prototype, 'toByteArray', {enumerable: false, value: binary$$String$toByteArray});

/** Converts a string to a ByteString encoded in charset.  */
function binary$$String$toByteString(charset)
{
  return new exports.ByteString(this, charset ? charset : "utf-16");
}
Object.defineProperty(String.prototype, 'toByteString', {enumerable: false, value: binary$$String$toByteString});


/** Returns an array of Unicode code points (as numbers).  */
function binary$$String$charCodes()
{
  throw new Error("not implemented");
}
Object.defineProperty(String.prototype, 'charCodes', {enumerable: false, value: binary$$String$charCodes});

/** Converts an array of Unicode code points to a ByteArray encoded in charset.  */
function binary$$Array$toByteArray(charset)
{
  return new exports.ByteArray(this, charset);
}
Object.defineProperty(Array.prototype, 'toByteArray', {enumerable: false, value: binary$$Array$toByteArray});


/** Converts an array of Unicode code points to a ByteString encoded in charset.  */
function binary$$Array$toByteString(charset)
{
  return new exports.ByteString(this, charset);
}
Object.defineProperty(Array.prototype, "toByteString", {enumerable: false, value: binary$$Array$toByteString});
