/** @file	readline.js
 *  @author	Eddie Roosenmaallen <eddie@page.ca>
 *  @date	May 2014
 *
 *  This module provides a quick wrapper for GNU Readline
 */

const FFI	 = require('gffi');

const RL	 = new FFI.Library('libreadline.so');
const _readline		 = new RL.CFunction(FFI.pointer,	"readline", 	FFI.pointer);
const _add_history	 = new RL.CFunction(FFI.sint,		"add_history",	FFI.pointer);


exports.readline = function $$readline(prompt)
{
  var buf = null;
  
  buf = _readline(prompt || '>');
  
  if (buf === null)
    return null;

  return buf.asString();
} /* readline() */

exports.add_history = function readline$$add_history(history)
{
  return _add_history( history );
} /* $$add_history */
