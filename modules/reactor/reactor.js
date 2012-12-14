/** 
 * @file 	reactor.js		A simple event-loop system for GPSEE.
 *					Based loosely on the reactor initally developed for the net module POC.
 * 
 * @author	Wes Garland, wes@page.ca
 * @date	Dec 2012
 */

exports.config =
{
  intervalClamp: 4	/* Do not run recurring events more than once every 4ms, like the browser */
}

var maintenanceEvents	= [/* fn  */];	/**< Events which recurr every single event loop iteration - not for end-user code, only "friend"ly modules */
var pendingEvents	= [/* fn  */];	/**< One-time events to run ASAP */
var recurringEvents	= [/* obj */];	/**< Events which recurr every on a regular interval */
var scheduledEvents	= [/* obj */];	/**< One-time events which will happen at a certain time */

function whenCompare(a,b)
{
  return a.when - b.when;
}

/** Main reactor loop.  Loop terminates only when state.quitObject.quit is truey or there is
 *  nothing to do -- no pending events, no scheduled events, and all recurring events returning
 *  false.
 *
 *  @param	callback		Function to run inside the reactor. Reactor
 *					terminates after the function returns and there
 *					are no scheduled events, or when the quit object,
 *					supplied as the argument to the callback, has a
 *					property named 'quit' set to true.
 */
exports.activate = function reactor$$activate(callback)
{
  var i, ev, now, fn, didWork, res;
  var quitObject = {};

  callback(quitObject);

  do
  {
    now = Date.now();

    while(pendingEvents.length)
    {
      fn=pendingEvents.shift();
      fn();
    }

    if (scheduledEvents.dirty)
      scheduleEvents.sort(whenCompare);

    for (i=0; i < scheduledEvents.length; i++)
    {
      ev = scheduledEvents[i];
      if (ev.when >= now)
	break;

      ev.fn();
      scheduledEvents[i].splice(i--,1);
    }

    if (recurringEvents.dirty)
      recurringEvents.sort(whenCompare);

    for (i=0; i < recurringEvents.length; i++)
    {
      ev = recurringEvents[i];

      if (ev.when > now)
	break;

      ev.fn();
      ev.when = ev.delay + now;
      recurringEvents.dirty = true;
    }

    didWork = !!pendingEvents.length || !!scheduledEvents.length || !!recurringEvents.length;
    for (i=0; i < maintenanceEvents.length; i++)
    {
      res = maintenanceEvents[i]();
      didWork = didWork || res !== false;
    }

  } while(!quitObject.quit && didWork);
}

exports.runSoon	= function reactor$$runSoon(_this, fn, args)
{
  var callback = function() { fn.apply(_this, args) };
  pendingEvents.push(callback);
}

exports.runOnce = function reactor$$runOnce(fn)
{
  if (!exports.runOnce._ranOnce)
    exports.runOnce._ranOnce = [];

  if (exports.runOnce._ranOnce.indexOf(fn) !== -1)
    return;	/* ran already */

  exports.runOnce._ranOnce.push(fn);
  fn();
}

exports.registerMaintenance = function reactor$$registerMaintenance(callback, arg /* ... */)
{
  var args, fn, id;

  if (arg)
  {
    args = Array.prototype.slice.call(arguments);
    args.shift(2);
    fn = function(){ callback.apply(callback, args)};
  }
  else
    fn = callback;

  maintenanceEvents.push(fn);

  return id;
}

exports.setTimeout = function reactor$$setTimeout(callback, delay, arg /* ... */)
{
  var args, fn, id;

  if (arg)
  {
    args = Array.prototype.slice.call(arguments);
    args.shift(2);
    fn = function(){ callback.apply(callback, args)};
  }
  else
    fn = callback;

  scheduledEvents.dirty = 1;
  id = {fn: fn, when: +delay + Date.now()};
  pendingEvents.push(id);

  return id;
}

exports.clearTimeout = function reactor$$clearTimeout(id)
{
  var idx;

  scheduledEvents.dirty = 1;
  idx = scheduledEvents.indexOf(id);
  if (idx !== -1)
    scheduledEvents.splice(idx, 1);
}

exports.setInterval = function reactor$$setInterval(callback, delay, arg /* ... */)
{
  var args, fn, id;

  if (arg)
  {
    args = Array.prototype.slice.call(arguments);
    args.shift(2);
    fn = function(){ callback.apply(callback, args)};
  }
  else
    fn = callback;

  if (delay < exports.config.intervalClamp)
    delay = exports.config.intervalClamp;

  recurringEvents.dirty = 1;
  id = {fn: fn, when: +delay};
  recurringEvents.push(id);

  return id;
}

exports.clearInterval = function reactor$$clearInterval(id)
{
  var idx;

  recurringEvents.dirty = 1;
  idx = recurringEvents.indexOf(id);
  if (idx !== -1)
    recurringEvents.splice(idx, 1);
}
