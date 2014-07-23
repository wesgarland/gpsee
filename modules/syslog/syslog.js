/** 
 *  @file	syslog.js	GPSEE module to wrap syslog calls
 *			   	with methods inspired by log4j. 
 *				Ported from $Id: surelog.js,v 1.1 2010/04/16 15:38:14 wes Exp $
 *
 *  @author 	Derek Traise, derek@page.ca
 *              Wes Garland, wes@page.ca
 *
 *  @date	Oct 2009, Feb 2011
 *
 */

const ffi = require("gffi");

var _openlog = new ffi.CFunction(ffi.void, "openlog", ffi.pointer, ffi.int, ffi.int);
var _log = new ffi.CFunction(ffi.void, "syslog", ffi.int, ffi.pointer, ffi.pointer);
var _closelog = new ffi.CFunction(ffi.void, "closelog");

var currentIdent = null;	/**<< Stores the current log identifier, if known. Also provides
				 *    a GC root for the C syslog() function's ident argument. 
				 */
/** Open a log file
 *  @param	ident		String identifying this program ("tag name")
 *  @param      facility	String naming initial facility to open. 
 *  @param	immediate	[Optional] Open the log socket immediately if not falsey
 *  @returns			an instance of exports.syslogAppender.
 */
exports.open = function open_log(ident, facility, immediate)
{
  var appender;

  if (!ident || !ident.length)
    throw(gpseeNamespace + ".module.syslog.open.arguments.ident: ident argument must be a string at least one character long");

  /** currentIdent pins the ident in memory so the GC doesn't clean it up. It
   *  needs to stay existing so that the associated ram from the JS->C string
   *  conversion doesn't get free'd before the log is closed. It is also used
   *  by the export.syslogAppender constructor make sure we're not calling 
   *  functions out-of-order.
   */
  currentIdent = new HeapCString(ident);

  try
  {
    appender = new exports.SyslogAppender(facility);
  }
  catch(e)
  {
    currentIdent = null;
    throw(e);
  }

  _openlog.call(currentIdent, ffi.gpsee.LOG_PID | (immediate ? ffi.gpsee.LOG_NDELAY : 0), 0);

  return appender;
}

/** Close syslog, releasing all open file descriptors and memory.
 */
exports.close = function close_log()
{
  currentIdent = null;
  _closelog.call();
}

/** SyslogAppender constructor. A class for sending messages to an arbitrary
 *  syslog facility.  Requires a previous call to exports.open().
 * 
 * @param	facilityLabel	Facility which class instances will use when
 *				writiing to the log.
 *
 *				Valid facilities are described by syslog(3);
 *				names are all lower-case
 */
function SyslogAppender(facilityLabel)
{
  if (!currentIdent)
    throw(gpseeNamespace + ".module.syslog.SyslogAppender.notOpen");

  if (!facilityLabel)
    throw(gpseeNamespace + ".module.syslog.SyslogAppender.facility.constructor.arguments.facility");

  this.facility = function getLogFacility()
  {
    var facilityString = "LOG_" + facilityLabel.toUpperCase();
    
    if (!facilityString in ffi.gpsee)
      throw(facilityString + " is not a valid syslog facility!");

    return ffi.gpsee[facilityString];
  }();
}

/** Write a message the debug level.
 *  @param	content		Message to write (uninterpreted string)
 */
SyslogAppender.prototype.debug = function log_debug(content)
{
  _log.call(this.facility | ffi.gpsee.LOG_DEBUG, "%s", content);
}

/** Write a message the debug level.
 *  @param	content		Message to write (uninterpreted string)
 */
SyslogAppender.prototype.info = function log_info(content)
{
  _log.call(this.facility | ffi.gpsee.LOG_NOTICE, "%s", content);
}

/** Write a message the debug level.
 *  @param	content		Message to write (uninterpreted string)
 */
SyslogAppender.prototype.notice = function log_notice(content)
{
  _log.call(this.facility | ffi.gpsee.LOG_NOTICE, "%s", content);
}

/** Write a message the debug level.
 *  @param	content		Message to write (uninterpreted string)
 */
SyslogAppender.prototype.error = function log_error(content)
{
  _log.call(this.facility | ffi.gpsee.LOG_ERR, "%s", content);
}

/** Write a message the debug level.
 *  @param	content		Message to write (uninterpreted string)
 */
SyslogAppender.prototype.warning = function log_warning(content)
{
  _log.call(this.facility | ffi.gpsee.LOG_ERR, "%s", content);
}

/** Write a message the debug level.
 *  @param	content		Message to write (uninterpreted string)
 */
SyslogAppender.prototype.alert = function log_alert(content)
{
  _log.call(this.facility | ffi.gpsee.LOG_ALERT, "%s", content);
}

/** Write a message the debug level.
 *  @param	content		Message to write (uninterpreted string)
 */
SyslogAppender.prototype.emerg = function log_emerg(content)
{
  _log.call(this.facility | ffi.gpsee.LOG_ALERT, "%s", content);
}

exports.SyslogAppender = SyslogAppender;

/** HeapCString class - create a C string (actually an instance of unsized Memory)
 *  with a free lifetime which matches the GC lifetime of the JS object.
 */
const _strdup = new ffi.CFunction(ffi.pointer, "strdup", ffi.pointer);
const _free = new ffi.CFunction(ffi.void, "free", ffi.pointer);
function HeapCString(s)
{
  var hs = _strdup.call(s);

  if (hs)
    hs.finalizeWith(_free, hs);

  return hs;
}



