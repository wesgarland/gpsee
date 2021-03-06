/** 
 * @file 	reactor.js		A simple event-loop system for GPSEE.
 *					Based loosely on the reactor initally developed for the net module POC.
 * 
 * @author	Wes Garland, wes@page.ca
 * @date	Dec 2012
 */

exports.config =
{
  intervalClamp: 4,	       /* Do not run recurring events more than once every 4ms, like the browser */
  idleSleepTime: 10,           /* How long to sleep when the loop did no work */
  minLoopTime: 4,              /* Minimum amount of time one pass of the loop is allowed to take */
};

var maintenanceEvents	= [/* fn  */];	/**< Events which recurr every single event loop iteration - not for end-user code, only "friend"ly modules */
var pendingEvents	= [/* fn  */];	/**< One-time events to run ASAP */
var cleanupEvents	= [/* fn  */];	/**< One-time events to run during finally clause; exception order is LIFO */
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
 *  Once the loop terminates, all cleanups are run.  If a cleanup returns false and there are
 *  maintenance functions defined, the cleanup will be re-run after the maintenance functions,
 *  until it does not return false.  No cleanups will be run in the meantime, preserving the
 *  LIFO semantic. This means that cleanups must NOT have interdependencies; it also means
 *  that it is possible for a maintenance function to be run after its corresponding cleanup 
 *  is called. During the cleanup phase, any maintenance function returning false will be
 *  removed from the maintenance event list.
 *
 *  @param	initializer		Function to run as we initialize the reactor. Reactor
 *					terminates after the initializer returns and there
 *					are no scheduled events, or when the quit object,
 *					supplied as the argument to the callback, has a
 *					property named 'quit' set to true.
 *
 *  @param	exceptionHandler [opt]	Function to pass all exceptions to for handling
 */
exports.activate = function reactor$$activate(initializer, exceptionHandler)
{
  var i, ev, now, fn, didWork, res, loopTime;
  var quitObject = {};

  initializer(quitObject);
  try
  {
    while(!quitObject.quit)
    {
      now = Date.now();

      while(pendingEvents.length)
	pendingEvents.shift()();

      if (scheduledEvents.dirty)
      {
	scheduledEvents.sort(whenCompare);
	scheduledEvents.dirty = false;
      }

      for (i=0; i < scheduledEvents.length; i++)
      {
	ev = scheduledEvents[i];
	if (ev.when >= now)
	  break;

	scheduledEvents.splice(i--,1);
	ev.fn();
      }

      if (recurringEvents.dirty)
      {
	recurringEvents.sort(whenCompare);
	recurringEvents.dirty = false;
      }

      for (i=0; i < recurringEvents.length; i++)
      {
	ev = recurringEvents[i];
	if (ev.when > now)
	  break;

	ev.fn();
	ev.when = ev.delay + now;
	recurringEvents.dirty = true;
      }

      didWork = false;
      for (i=0; i < maintenanceEvents.length; i++)
      {
	res = maintenanceEvents[i]();
	didWork = didWork || res !== false;
      }

      if (pendingEvents.length === scheduledEvents.length === recurringEvents.length === 0)
        break; /* nothing to do! */

      if (!didWork)
	require('gpsee').sleep(exports.config.idleSleepTime / 1000);
      else
      {
        loopTime = Date.now() - now;
        if (loopTime < exports.config.minLoopTime)
          require('gpsee').sleep((exports.config.minLoopTime - loopTime) / 1000);
      }
    }
  }
  catch (e) 
  {
    if (exceptionHandler)
      exceptionHandler(e);
    else
      throw e;
  }
  finally
  {
    while(cleanupEvents.length)
    {
      fn = cleanupEvents.pop();
      if (fn() === false && maintenanceEvents.length)
      {
	cleanupEvents.unshift(fn);
	for (i=0; i < maintenanceEvents.length; i++)
	  if (maintenanceEvents[i]() === false)
	    maintenanceEvents.splice(i,1);
      }
    }
  }
}

/** Run this supplied function ASAP in the reactor loop, with the provided 'this' and arguments */
exports.runSoon	= function reactor$$runSoon(_this, fn, args)
{
  var callback = function() { fn.apply(_this, args) };
  pendingEvents.push(callback);
}

/** Run the supplied function once, no matter how many times runOnce() is invoked. */
exports.runOnce = function reactor$$runOnce(fn)
{
  if (!exports.runOnce._ranOnce)
    exports.runOnce._ranOnce = [];

  if (exports.runOnce._ranOnce.indexOf(fn) !== -1)
    return;	/* ran already */

  exports.runOnce._ranOnce.push(fn);
  fn();
}

/** Register a maintenance function. Maintenance functions are run once per 
 *  iteration of the reactor loop, in FIFO order.  A maintenance function should
 *  return false when it does no work, to help with CPU resource management.
 */
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

/** Register a cleanup function. Cleanup functions are run once per reactor,
 *  either as it exits normally or after the exception handler has been invoked.
 *  Cleanups are run in LIFO order.
 */
exports.registerCleanup = function reactor$$registerCleanup(callback, arg /* ... */)
{
  var args, fn;

  if (arg)
  {
    args = Array.prototype.slice.call(arguments);
    args.shift(2);
    fn = function(){ callback.apply(callback, args)};
  }
  else
    fn = callback;

  cleanupEvents.push(fn);

  return;
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

  scheduledEvents.dirty = true;
  id = {fn: fn, when: +delay + Date.now()};
  scheduledEvents.push(id);

  return id;
}

exports.clearTimeout = function reactor$$clearTimeout(id)
{
  var idx;

  scheduledEvents.dirty = true;
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

  recurringEvents.dirty = true;
  id = {fn: fn, when: +delay, delay: +delay};
  recurringEvents.push(id);

  return id;
}

exports.clearInterval = function reactor$$clearInterval(id)
{
  var idx;

  recurringEvents.dirty = true;
  idx = recurringEvents.indexOf(id);
  if (idx !== -1)
    recurringEvents.splice(idx, 1);
}
