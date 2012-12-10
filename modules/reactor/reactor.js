/** 
 * @file 	reactor.js		A simple event-loop system for GPSEE.
 *					Based loosely on the reactor initally developed for the net module POC.
 * 
 * @author	Wes Garland, wes@page.ca
 * @date	Dec 2012
 */

var quitObject;
var pollSockets;

const NET = require("./net");

exports.quitObject = { quit: false };		/**< Quit object; can be overridden at any time. Making quitObject.quit truey will halt the reactor at the bottom of the loop. */
exports.granularity = 1000;			/**< Maximum amount of "waiting for nothing" time at the top of the event loop, in milliseconds */

if (!require("system").global.quit)
  require("signal").onINT = function () { exports.quitObject.quit = true; };	/* gpsee-js crashes when trapping signals */

/** Main reactor loop.  Loop terminates only when quitObject.quit is truey.
 */
exports.start = function reactor$$start(servers, extraSockets)
{
  var idx, server, connection;

  do 
  {
    pollSockets = extraSockets ? extraSockets.slice(0) : [];

    for each (server in servers)
    {
      pollSockets.push(server.socket);

      for each (connection in server.connections)
      {
        if (connection.readyState === "closed")
        {
          idx = server.connections.indexOf(connection);

          if (idx === -1)
            throw new Error("Socket on fd " + (0+this.fd) + "is not in server's connection list!");

          server.connections.splice(idx, 1);
          continue;
        }

	pollSockets.push(connection);
      }
    }

    NET.poll(pollSockets, exports.granularity);
  } while(!exports.quitObject.quit);
}

