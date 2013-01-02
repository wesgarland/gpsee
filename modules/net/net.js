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
 * Copyright (c) 2007-2010, PageMail, Inc. All Rights Reserved.
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
 *
 *  @file	sockets.js	Simple sockets API for GPSEE. Very heavily influenced
 *				by the net API in Node.js.
 *  @author	Wes Garland
 *  @date	March 2010
 *  @version	$Id: net.js,v 1.6 2011/12/05 19:13:37 wes Exp $
 */

const fs = require("fs-base");
const binary = require("binary");
const ffi = require("gffi");
const dl = ffi;		/**< Dynamic lib handle for pulling symbols */
const dh = ffi.std	/**< Header collection for #define'd constants */

const _close		= new dl.CFunction(ffi.int,	"close",		ffi.int);
const _strerror		= new dl.CFunction(ffi.pointer, "strerror",		ffi.int);
const _socket		= new dl.CFunction(ffi.int, 	"socket",		ffi.int, ffi.int, ffi.int);
const _ntohl 		= new dl.CFunction(ffi.int, 	"ntohl",		ffi.uint32_t);
const _ntohs 		= new dl.CFunction(ffi.int, 	"ntohs",		ffi.uint16_t);
const _htonl 		= new dl.CFunction(ffi.int, 	"htonl",		ffi.uint32_t);
const _htons 		= new dl.CFunction(ffi.int, 	"htons",		ffi.uint16_t);
const _bind		= new dl.CFunction(ffi.int,	"bind",			ffi.int, ffi.pointer, ffi.int);
const _setsockopt	= new dl.CFunction(ffi.int, 	"setsockopt",		ffi.int, ffi.int, ffi.int, ffi.pointer, ffi.int);
const _listen		= new dl.CFunction(ffi.int,	"listen",		ffi.int, ffi.int);
const _accept		= new dl.CFunction(ffi.int, 	"accept",		ffi.int, ffi.pointer, ffi.pointer);
const _select		= new dl.CFunction(ffi.int,	"select",		ffi.int, ffi.pointer, ffi.pointer, ffi.pointer, ffi.pointer);
const _FD_ISSET		= new dl.CFunction(ffi.int,	"FD_ISSET",		ffi.int, ffi.pointer);
const _FD_SET		= new dl.CFunction(ffi.void,	"FD_SET",		ffi.int, ffi.pointer);
const _FD_CLR		= new dl.CFunction(ffi.void,	"FD_CLR",		ffi.int, ffi.pointer);
const _FD_ZERO		= new dl.CFunction(ffi.void,	"FD_ZERO",		ffi.pointer);
const _write		= new dl.CFunction(ffi.ssize_t,	"write",		ffi.int, ffi.pointer, ffi.size_t);
const _read		= new dl.CFunction(ffi.ssize_t,	"read",			ffi.int, ffi.pointer, ffi.size_t);
const _inet_pton	= new dl.CFunction(ffi.int,	"inet_pton",		ffi.int, ffi.pointer, ffi.pointer);
const _inet_ntop	= new dl.CFunction(ffi.pointer,	"inet_ntop",		ffi.int, ffi.pointer, ffi.pointer, ffi.size_t);
const _shutdown         = new dl.CFunction(ffi.int,     "shutdown",             ffi.int, ffi.int);
const _connect          = new dl.CFunction(ffi.int,     "connect",              ffi.int, ffi.pointer, ffi.socklen_t);
const _fcntl		= new dl.CFunction(ffi.int,	"fcntl",		ffi.int, ffi.int, ffi.int);

const FD_SETSIZE = ffi.std.FD_SETSIZE;

exports.config = 
{
  readBufferSize:		65536,
  defaultBacklog:		32,
  pollAllSocketsTimeout:	100
};

/**
 *  Return a string documenting the most recent OS-level error, if there was one.
 *  @param 	force	Always return an error, even if it's (No Error)
 *  @returns	A string, possibly empty.
 *
 *  @note	If this is ever ported to Windows, this routine should use the wsock32 errno
 */
function syserr(force)
{
  if (ffi.errno || force)
    return ' (' + _strerror(ffi.errno).asString() + ')';
  else
    return '';
}

/**
 *  IP Address class; can hold IPv4 or IPv6 addresses. Note that IPv6 is not yet implemented.
 *
 *  this.ipv6		True if ipv6 address
 *  this.in4_addr	IP address represented as a 32-bit number
 *  this.toString()	Presentation format (human-oriented string)
 */
exports.IP_Address = function IP_Address(address)
{
  this.ipv6 = false;

  var addrBuf = new ffi.CType(ffi.int32_t);

  if (typeof address === "string")
  {
    switch(address)
    {
      case "INADDR_ANY":
      case "any/0":
	address = 0;
	break;
      case "localhost":
	address = 0x7F000001;
	break;
    }
  }

  switch(typeof address)
  {
    case "number":
      this.in4_addr = htonl(address);
      break;
    case "object":
      address = address.toString();
    case "string":
      if (_inet_pton(dh.AF_INET, address, addrBuf) != 1)
	throw new Error("Cannot convert input string to IP address" + syserr());
      this.in4_addr = addrBuf.valueOf();
      break;
  }
}

exports.IP_Address.prototype.toString = function IP_Address_toString()
{
  var buf;

  switch(this.in4_addr)
  {
    case 0:
      return "INADDR_ANY";
    case 0x7F000001:
      return "localhost";
  }

  buf = new ffi.Memory(dh.INET_ADDRSTRLEN);
  if (_inet_ntop(dh.AF_INET, new ffi.CType(ffi.int, this.in4_addr), buf, buf.size) == null)
    throw new Error("Cannot convert IP address toString" + syserr());

  return buf.asString(-1);
}

function hton_detect()
{
  if (_htonl(0x1) == 0x1)
  {
    htonl = function(num) +num;		/* big endian host */
    htons = htonl;
    ntohl = htonl;
    ntohs = htonl;
  }
  else
  {
    htonl = function(num) _htonl(+num);
    htons = function(num) _htons(+num);
    ntohl = function(num) _ntohl(+num);
    ntohs = function(num) _ntohs(+num);
  }
}

var htonl = function htonl_thunk(num)
{
  hton_detect()
  return htonl(num);
}

var htons = function htons_thunk(num)
{
  hton_detect()
  return htons(num);
}

var ntohl = function ntohl_thunk(num)
{
  ntoh_detect()
  return ntohl(num);
}

var ntohs = function ntohs_thunk(num)
{
  ntoh_detect()
  return ntohs(num);
}

/**
 *  Constructor to create a new TCP Socket instance
 *
 *  @param	fd	[optional]	File descriptor associated with the socket object if the OS endpoint has already been created
 */
var Socket = exports.Socket = function net$$Socket(fd, options)
{
  if (typeof fd !== "undefined")
    this.setfd(fd, options);

  this.pendingWrites = [];			/* Array of ffi.Memory or binary.ByteArray */
}

Socket.prototype = new (require("./events").EventEmitter)();

Socket.prototype.toString = function Socket$toString()
{
  return "[Socket fd=" + this.fd + "]";
}

/** Associate a file descriptor with a Socket instance.  The file descriptor will be closed automatically during
 *  garbage collection if the Socket is no longer reachable.  This should be avoided, as closing the file descriptor
 *  alone does not invoke the shutdown() system call.
 *
 *  @param	fd		The file descriptor  (ffi boxed integer)
 *  @param	options		Socket options
 *				.nonBlocking		truey [default] to make the socket non-blocking
 */
Socket.prototype.setfd = function Socket$setfd(fd, options)
{
  if (this.fd)
    throw new Error("socket fd has already been set");

  this.fd	= +fd;
  this.gcFd	= fd;		/* keep the boxed fd as a GC root to prevent finalization */

  if (options.hasOwnProperty("nonBlocking") && options.nonBlocking)
  {
    flags = _fcntl(this.fd, dh.F_GETFL, 0);
    if (_fcntl(this.fd, dh.F_SETFL, flags | dh.O_NONBLOCK) == -1)
      throw new Error("Could not make socket with file descriptor " + this.fd + " non-blocking" + syserr());
    this.nonBlocking = true;
  }

  if (options.hasOwnProperty("reuse") && options.reuse != false)
  {
    i = new ffi.CType(ffi.int, 1);
    if (_setsockopt(this.fd, dh.SOL_SOCKET, dh.SO_REUSEADDR, i, i.size) != 0)
      throw new Error("Could not set SO_REUSEADDR option on socket with file descriptor " + this.fd + syserr());
  }

  addSocketToReactor(this);
}

/** Create a TCP socket endpoint (3 points of the 5D vector for a connection).
 *
 *  @param	port		TCP port number
 *  @param	address		IP address to bind to, or undefined for any/0
 *  @param	options		Socket options (passed to setfd)
 */
Socket.prototype.createEndpoint = function Socket$create(port, address, options)
{
  var fd;

  if (address && address.ipv6)
    throw new Error("IPv6 support not yet implemented");

  fd = _socket.call((address && address.ipv6) ? dh.PF_INET6 : dh.PF_INET, dh.SOCK_STREAM, 0);
  if (fd == -1)
    throw new Error("Could not create socket" + syserr());
  fd.finalizeWith(_close, +fd);

  if (port)
    this.port	= typeof port === "number" ? port : parseInt(port);
  else
    this.port 	= 0;

  if (address)
    this.address	= new exports.IP_Address(address);
  else
    this.address	= new exports.IP_Address(0);

  this.sockaddr		 	= new ffi.MutableStruct("struct sockaddr_in");
  this.sockaddr.sin_family 	= dh.AF_INET;
  this.sockaddr.sin_addr	= this.address.in4_addr;
  this.sockaddr.sin_port	= htons(this.port);

  this.setfd(fd, options);
}

/** Cause a socket to listen for incoming connections */
Socket.prototype.listen = function Socket$listen(backlog)
{
  if (_bind(this.fd, this.sockaddr, 16) == -1)
    throw new Error("Could not bind socket on " + this.address + ":" + this.port + syserr());

  this.bound = true;

  if (!backlog && backlog !== 0)
    backlog = exports.config.defaultBacklog;

  if (_listen(this.fd, backlog) == -1)
    throw new Error("Could not listen to socket on " + this.address + ":" + this.port + syserr());

  this.listening = true;
}

/** Accept the first pending connection in the queue and return a Socket. If there is no pending connection, 
 *  this routine will block, or return null in the case of a non-blocking socket.
 */
Socket.prototype.accept = function Socket$accept()
{
  var i = new ffi.CType(ffi.socklen_t);
  var fd;
  var el;
  var addrlen = new ffi.CType(ffi.socklen_t, 16);

  if (!this.listening)
    this.listen();

  fd = _accept.call(this.fd, this.sockaddr, addrlen);
  if (fd == -1)
  {
    if (ffi.errno == dh.EAGAIN)
      return null;
    throw new Error("Could not accept connection on " + this.address + ":" + this.port + syserr());
  }

  fd.finalizeWith(_close, +fd);
  return new Socket(fd, {nonBlocking: this.nonBlocking});
}

/** Disable/Enable the Nagle algorithm.
 *  @param	noDelay		When falsey, cause us to enable the Nagle algorithm
 */
Socket.prototype.setNoDelay = function Socket$$setNoDelay(noDelay)
{
  var i = new ffi.CType(ffi.int, noDelay == false ? 0 : 1);

  if (_setsockopt(this.fd, dh.IPPROTO_TCP, dh.TCP_NODELAY, i, i.size) != 0)
    throw new Error("Could not set SO_REUSEADDR option on socket with file descriptor " + this.fd + syserr());
}

/** Open a [client] connection to a server
 *  @param	options		Connection options
 *				- .address	host to connect to
 *				- .port		port to connect on
 *  @param	connectionListener	Event handler invoked when connection is established
 */
Socket.prototype.connect = function Socket$connect(options, connectionListener)
{
  var res;

  this.createEndpoint(options.port, options.address, {nonBlocking: true});

  res = _connect(this.fd, this.sockaddr, 16);
  if (res == -1 && ffi.errno != dh.EINPROGRESS)
    throw new Error("Could not connect socket to " + this.address + ":" + this.port + syserr());

  function connected()
  {
    delete this.onWritable;

    this.onReadable = socketReadThenEmit;
    this.readyState = "open";
    this.addListener("end", this.close );
    
    if (connectionListener)
      this.addListener("connect", connectionListener);
    
    this.emit("connect");
  }

  this.readyState = "connecting";
  this.onWritable = connected;
}

/** Close a socket, discarding any pending data */
Socket.prototype.close = function Socket$close()
{
  delete this.onReadable;
  delete this.onWritable;

  if (typeof this.fd != "undefined")
  {
    if (_shutdown(this.fd, dh.O_RDWR) != 0)
      if (ffi.errno != dh.ENOTCONN)
	throw new Error("Could not shutdown socket on fd " + (0+this.fd) + syserr());

    if (this.gcFd.destroy() != 0 ? false : false /* XXX GPSEE bug 101 */)
      throw new Error("Could not close socket on fd " + (+this.fd) + syserr());
  }

  this.readyState = "closed";
  this.emit("close", this.had_write_error ? true : false);
  delete this.fd;
  delete this.gcFd;

  removeSocketFromReactor(this);
}

Socket.prototype.end = function Socket$end(data, encoding)
{
  if (data)
  {
    if (!this.write(data, encoding))
    {
      this.addListener("drain", this.close);
      return;
    }
  }

  if (_shutdown(this.fd, dh.SHUT_WR) != 0)
    if (ffi.errno != dh.ENOTCONN)
      throw new Error("Could not half-close socket on fd "  + (+this.fd) + syserr());

  this.emit("end");
}

/** Connect this socket's reads to a socket's writes.  Modelled after Node's stream API call of the same name. */
Socket.prototype.pipe = function Socket$pipe(destination)
{
  this.addListener("data", destination.write);
}

Socket.prototype.write = socketWriteOrQueue;

/** Write an array of Strings or ByteThings to the socket in a single system call.
 *  @param 	dataArray	The array of things to write
 *  @returns 	true		if all data was written to the kernel's buffer, false if some data was queued for later writing.
 *  @see 	Socket.prototype.write()
 */
Socket.prototype.writev = function socket$writev(dataArray)
{
  var writevBuf = new binary.ByteArray(0);
  var i, tmp;

  if (dataArray.length === 1)
    return this.write(dataArray[0]);

  for (i = 0; i < dataArray.length; i++)
  {
    if (!require("gpsee").isByteThing(dataArray[i]))
    {
      tmp = new ffi.Memory(dataArray[i].length);
      tmp.copyDataString(dataArray[i]);
      dataArray[i] = tmp;
    }
    writevBuf.concat(dataArray[i]);
  }

  return this.write(writevBuf);
}
 

/**
 * Poll a series of Sockets, invoking their onReadable or onWritable methods
 * as appropriate.  It is not supported to use pollSockets in a re-entrant way
 * (i.e. do not use the same pollSockets from an event as the event trigger).
 *
 * Polling zero sockets will result in zero delay regardless of timeout.
 *
 * @param	pollSockets	Array of instances of Socket
 * @param	timeout		How long to wait before giving up (ms)
 *
 * @note	This routine may add a _poll_objCache property to pollSockets which should not be modified (but can be deleted)
 */
exports.poll = function poll(pollSockets, timeout)
{
  var tv, rfds, wfds;
  var maxfd = 0;
  var numfds;
  var i, socket;

  if (pollSockets.length === 0)
    return 0;

  if (!pollSockets.hasOwnProperty("_poll_objCache"))
    Object.defineProperty(pollSockets, "_poll_objCache", {value:{}, configurable: true, enumerable:false});

  if (arguments.length < 2 || timeout === null || +timeout === Infinity)
  { 
    tv = null;
  }
  else
  { 
    if (timeout instanceof Date)
      timeout -= Date.now();
    if (!isNaN(+timeout))
    { 
      if (!pollSockets._poll_objCache.tv)
	pollSockets._poll_objCache.tv = new ffi.MutableStruct("struct timeval");
      tv = pollSockets._poll_objCache.tv;
      tv.tv_sec = Math.floor(timeout / 1000);
      tv.tv_usec = (timeout % 1000) * 1000;
    }
    else
      throw new Error('invalid timeout');
  }

  if (!pollSockets._poll_objCache.rfds)
  {
    pollSockets._poll_objCache.rfds = new ffi.MutableStruct("fd_set");
    pollSockets._poll_objCache.wfds = new ffi.MutableStruct("fd_set");
  }

  rfds = pollSockets._poll_objCache.rfds;
  wfds = pollSockets._poll_objCache.wfds;
  _FD_ZERO(rfds);
  _FD_ZERO(wfds);

  for (i=0; i < pollSockets.length; i++)
  {
    socket = pollSockets[i];
    var fd = +socket.fd;
    if (isNaN(fd) || fd < 0 || fd >= FD_SETSIZE)
      throw new Error("Invalid file descriptor: "+fd+" from "+socket);
    if (fd > maxfd)
      maxfd = fd;
    _FD_SET(fd, rfds);
    if (socket.onWritable)
      _FD_SET(fd, wfds);
  }

  numfds = _select(maxfd + 1, rfds, wfds, null, tv);
  if (numfds == 0)
    return 0;

  if (numfds == -1)
  {
    if (ffi.errno == dh.EINTR)
      return 0;

    throw new Error("Error polling sockets" + syserr());
  }

  for (i=0; i < pollSockets.length; i++)
  {
    socket = pollSockets[i];
    var fd = +socket.fd; /* events could modify sock.fd */
    if (isNaN(fd) || fd < 0 || fd >= FD_SETSIZE)
      throw new Error("Invalid file descriptor: "+fd+" from "+socket);

    if (_FD_ISSET(fd, rfds) != 0)
    {
      if (socket.onReadable)
	socket.onReadable(socket);
    }

    if (_FD_ISSET(fd, wfds) != 0)
      if (socket.onWritable)
	socket.onWritable(socket);
  }

  return numfds;
}

exports.Server = function net$$Server()
{
  this.socket = new Socket();
}
exports.Server.prototype = new (require("./events").EventEmitter);

exports.Server.prototype.connections = [];

exports.Server.prototype.listen = function net$$Server$listen(port, host, backlog, callback)
{
  var listenSocket, clientSocket;
  var server = this;

  if (arguments.length === 2 && typeof host === "function")
  {
    callback = host;
    host = undefined;
  }

  if (arguments.length === 3 && typeof backlog === "function")
  {
    callback = backlog;
    backlog = undefined;
  }

  if (this.socket && this.socket.listening)
    throw new Error("Server is already listening to " + this.socket.address + ":" + ntohl(this.socket.sockaddr.sin_port));

  listenSocket = this.socket;
  listenSocket.createEndpoint(port, host, {nonBlocking: true, reuseAddr: true});
  listenSocket.listen(backlog);
  
  listenSocket.onReadable = function Server$onReadableHandler()
  {
    do
    {
      clientSocket = listenSocket.accept();
      if (clientSocket)
      {
	clientSocket.readyState		= "opening";
	clientSocket.server 		= server;
	clientSocket.onReadable 	= socketReadThenEmit;
	clientSocket.readyState 	= "open";
	clientSocket.addListener("end", this.close);
	clientSocket.addListener("close", 
				 function freshBindingWrapper(server, clientSocket) 
				 { 
				   return function removeSocketFromServer()
				          {
					    var idx = server.connections.indexOf(clientSocket);
					    if (idx == -1)
					      throw new Error("Server connection list corrupted");
					    server.connections.splice(idx, 1);
				          }
				 }(server, clientSocket));

	server.connections.push(clientSocket);
 	server.emit("connection", clientSocket);
	clientSocket.emit("connect");
      }
    } while (clientSocket && listenSocket.nonBlocking);
  }

  Object.defineProperty(listenSocket, "toString", {value: function() "[Socket fd=" + listenSocket.fd + " listening on " + (host ? host : "") + ":" + port + "]", enumerable:false});
}

exports.Server.prototype.close = function net$$Server$close()
{
  delete this.socket.onReadable;
  this.socket.close();

  for each (connection in this.connections)
    connection.addListener("close", function Server$TryClose(socket)
			   {
			     var server = socket.server;

			     if (server.connections.length)
			       return;

			     server.emit("close");
			   });
}

exports.Server.prototype.toString = function net$$Server$toString()
{
  var a=[];

  if (this.socket.listening)
    a.push(" listening");
  else
    if (this.socket.bound)
      a.push(" bound");

  if (this.socket)
    a.push(" on " + this.socket);

  return "[net$$Server" + a.join("; ") + "]";
}

exports.createServer = function createServer(connectionListener)
{
  var s = new exports.Server();
  s.addListener("connection", connectionListener);
  return s;
}

function socketReadThenEmit(socket)
{
  var bytesRead;

  if (!socket.readBuffer)
    socket.readBuffer = new ffi.Memory(exports.config.readBufferSize);

  bytesRead = 0 + _read(socket.fd, socket.readBuffer, socket.readBuffer.size);

  switch(bytesRead)
  {
    case 0:
      socket.end();
      break;
    case -1:
      socket.emit("error");
      break;
    default: 
      socket.emit("data", socket.readBuffer.asDataString(bytesRead));
      break;
  }
}

/** Write data to this.fd. Writes are non-blocking; unwritten 
 *  data is deferred to the reactor, which regularly invokes the
 *  socketDrainQueue function to drain the buffers. This function
 *  is normally used as the write() method of a Socket instance.
 *
 *  @param	data to write.  Must be either a String or an instace of a GPSEE ByteThing. 
 *				Buffer copies are reduced when data is a ByteString.
 *  @param	encoding	Ununused
 *  @param	callback	Unused
 */
function socketWriteOrQueue(data, encoding, callback)
{
  var bytesWritten;
  var privateMemory;

  if (this.had_write_error)
    throw new Error("Cannot write data; previous write had error " + this.had_write_error);

  if (encoding || callback)
    throw new Error("Not supported");

  switch (typeof data)
  {
    case "string":
      privateMemory = new ffi.Memory(data.length);
      privateMemory.length = data.length;
      privateMemory.copyDataString(data);
      data = privateMemory;
      break;
    case "object":
      if (require("gpsee").isByteThing(data))
	break;
    default:
	throw new Error("Cannot write data; invalid type");
  }

  /* Invariant here: data is either Memory or Binary */

  if (this.pendingWrites.length)
  {
    /* Since there is already a queue, we simulate "queue immediately, did not write"
     * as far as the application is concerned, but then we tickle the poll on this
     * particular file descriptor so that we can send more data down the pipeline
     * before control returns to the maintenance function in the event loop. This
     * gives us better performance for situations where the application writes a 
     * large amount of information in a tight loop, at a very marginal cost.
     */
    this.pendingWrites.push(data);
    exports.poll([this], 0);
    return false;
  }

  bytesWritten = _write(this.fd, data, data.length);
  if (bytesWritten == -1 && ffi.errno == dh.EAGAIN)		/* Kernel buffer was too full to write anything - treat as non-error */
    bytesWritten = 0;

  switch(bytesWritten)
  {
    case data.length:			/* Everything written */
      break;

    case -1:				/* Nothing written */
      switch (ffi.errno)
      {
	case dh.EPIPE:
	  delete this.onWritable;
	  this.readyState = "readOnly";
	default:
	  this.had_write_error = ffi.errno;
	  break;
	case dh.EINTR:
	  break;
      }
      break;

    default:				/* Partial write */
      if (!(data instanceof binary.ByteString))
	data = binary.ByteString(data);

      this.pendingWrites.push(data.slice(bytesWritten));
      this.onWritable = socketDrainQueue;
      break;
   }

  return bytesWritten == data.length;
}

function socketDrainQueue(socket)
{
  var res;
  var pendingWrites = socket.pendingWrites;

  socket.pendingWrites.length = 0;

  if (pendingWrites.length == 1)
    res = socket.write(pendingWrites[0]);
  else
    res = socket.writev(pendingWrites);
  
  if (res !== false)
  {
    delete socket.onWritable;
    socket.emit("drain");
  }
}

function flushAndCloseAllSockets(socketList)
{
  var socket;

  for (i=0; i < socketList.length; i++)
  {
    socket = socketList[i];
    if (socket.pendingWrites.length === 0)
      socket.close();
  }

  return socketList.length === 0;
}

function pollAllSockets(socketList)
{
  if (socketList.length === 0)
    return false;
  return exports.poll(socketList, exports.config.pollAllSocketsTimeout);
}

function setupReactorForSockets()
{
  var i;
  var socketList = setupReactorForSockets.socketList = [];
  
  require("reactor").registerMaintenance(function(){return pollAllSockets(socketList)});
  require("reactor").registerCleanup(function(){return flushAndCloseAllSockets(socketList)});

  exports.onSigPIPE = Function;
  require("signal").onPIPE = function() { exports.onSigPIPE };
}

function addSocketToReactor(socket)
{
  if (!setupReactorForSockets.socketList)
    setupReactorForSockets();

  setupReactorForSockets.socketList.push(socket);
}

function removeSocketFromReactor(socket)
{
  var list = setupReactorForSockets.socketList;
  var idx = list.indexOf(socket);

  if (idx === -1)
    throw new Error("Corrupted reactor socket list");

  list.splice(idx, 1);
}

/** Establish a socket connection to a server.
 *  @param	options		Connection options
 *				- .address	host to connect to
 *				- .port		port to connect on
 *  @param	connectionListener	Event handler invoked when connection is established
 */
exports.connect = function net$$connect(options, connectionListener)
{
  var socket = new Socket();

  socket.connect(options, connectionListener);

  return socket;
}
exports.createConnection = exports.connect;

