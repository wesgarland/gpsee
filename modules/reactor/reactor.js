/** 
 * @file 	reactor.js		A simple event-loop system for GPSEE.
 *					Based loosely on the reactor initally developed for the net module POC.
 * 
 * @author	Wes Garland, wes@page.ca
 * @date	Dec 2012
 */

var state;

/** Main reactor loop.  Loop terminates only when state.quitObject.quit is truey.
 *
 *  @param 	boostrap	Function to execute when reactor is started
 *  @param	maintenance	Function to execute each time the reactor loops
 */
exports.start = function reactor$$start(bootstrap, maintenance)
{
  if (typeof state !== "undefined")
    throw new Error("Reactor already started!");

  state = { quitObject: {} };

  if (bootstrap)
    bootstrap(state);

  do
  {
    maintenance(state);
  } while(!state.quitObject.quit);
}
