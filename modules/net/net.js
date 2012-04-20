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
 *  @file	sockets.js	Simple sockets API for GPSEE
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

/* If FD_SETSIZE is not defined, we'll just use 2^15 as a sane value */
const FD_SETSIZE = exports.FD_SETSIZE = ffi.std.FD_SETSIZE;

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
    return ' (' + _strerror.call(ffi.errno).asString() + ')';
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
      if (_inet_pton.call(dh.AF_INET, address, addrBuf) != 1)
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
  if (_inet_ntop.call(dh.AF_INET, this.in4_addr, buf, buf.size) == null)
    throw new Error("Cannot convert IP address toString" + syserr());

  return buf.asString(-1);
}

function hton_detect()
{
  if (_htonl.call(0x1) == 0x1)
  {
    htonl = function(num) num;		/* big endian host */
    htons = htonl;
    ntohl = htonl;
    ntohs = htonl;
  }
  else
  {
    htonl = function(num) _htonl.call(num);
    htons = function(num) _htons.call(num);
    ntohl = function(num) _ntohl.call(num);
    ntohs = function(num) _ntohs.call(num);
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

var sockStream_methods = 
{
  bind: function bind(reuse)
  {
    var i;

    if (reuse != false)
    {
      i = new ffi.CType(ffi.int, 1);
      if (_setsockopt.call(this.sockfd, dh.SOL_SOCKET, dh.SO_REUSEADDR, i, i.size) != 0)
      throw new Error("Could not set SO_REUSEADDR option on socket with file descriptor " + this.sockfd + syserr());
    }

    if (_bind.call(this.sockfd, this.sockaddr, 16) == -1)
    throw new Error("Could not bind socket on " + this.address + ":" + this.port + syserr());

    this.bound = true;
  },

  listen: function listen(backlog)
  {
    if (!this.bound)
      this.bind(true);

    if (!backlog && backlog !== 0)
      backlog = 32;

    if (_listen.call(this.sockfd, backlog) == -1)
      throw new Error("Could not listen to socket on " + this.address + ":" + this.port + syserr());

    this.listening = true;
  },

  accept: function accept()
  {
    var i = new ffi.CType(ffi.socklen_t);
    var fd;
    var el;
    var sockStream;
    var addrlen = new ffi.CType(ffi.socklen_t, 16);

    if (!this.listening)
      this.listen();

    fd = _accept.call(this.sockfd, this.sockaddr, addrlen);
    if (fd == -1)
      throw new Error("Could not accept connection on " + this.address + ":" + this.port + syserr());

    sockStream = fs.openDescriptor(fd, "r+");
    return sockStream;
  },

  connect: function connect()
  {
    if (_connect.call(this.sockfd, this.sockaddr, 16) == -1)
      throw new Error("Could not connect socket to " + this.address + ":" + this.port + syserr());
  }
}

/**
 *  Constructor to create a new TCP Socket
 * 
 *  @param	ipv6		Truey if we want an IPV6 socket
 */
exports.socketStream = function (port, address, ipv6)
{
  var sockfd;
  var sockStream;

  ipv6 = ipv6 ? true : false;

  sockfd = _socket.call(ipv6 ? dh.PF_INET6 : dh.PF_INET, dh.SOCK_STREAM, 0);
  if (sockfd == -1)
    throw new Error("Could not create socket" + syserr());

  sockStream = fs.openDescriptor(sockfd, "r+");

  sockStream.ipv6 	= ipv6;
  sockStream.sockfd 	= sockfd;

  if (port)
    sockStream.port	= typeof port === "number" ? port : parseInt(port);
  else
    sockStream.port 	= 0;

  if (address)
    sockStream.address	= new exports.IP_Address(address);
  else
    sockStream.address	= new exports.IP_Address(0);

  if (sockStream.ipv6)
    throw new Error("IPv6 support not yet implemented");

  sockStream.sockaddr		 = new ffi.MutableStruct("struct sockaddr_in");
  sockStream.sockaddr.sin_family = dh.AF_INET;
  sockStream.sockaddr.sin_addr	 = sockStream.address.in4_addr;
  sockStream.sockaddr.sin_port 	 = htons(sockStream.port);

  graft(sockStream, sockStream_methods);

  return sockStream;
}

exports.poll = function poll(pollSockets, timeout)
{
  //print('poll(timeout='+timeout+')')
  var tv;
  var rfds = new ffi.MutableStruct("fd_set");
  var wfds = new ffi.MutableStruct("fd_set");
  var maxfd = 0;
  var numfds;

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
      tv = new ffi.MutableStruct("struct timeval");
      tv.tv_sec = Math.floor(timeout / 1000);
      tv.tv_usec = timeout % 1000 * 1000;
    }
    else throw new Error('invalid timeout');
  }
  //print('  tv = '+tv)


  _FD_ZERO.call(rfds);
  _FD_ZERO.call(wfds);

  for each (let socket in pollSockets)
  {
    var fd = +socket.fd;
    if ('number' != typeof fd || fd < 0 || fd >= FD_SETSIZE)
      throw new Error("Invalid file descriptor: "+fd+" from "+socket);
    if (fd > maxfd)
      maxfd = fd;
    _FD_SET.call(fd, rfds);
    if (socket.onWriteable)
      _FD_SET.call(fd, wfds);
  }

  numfds = _select.call(maxfd + 1, rfds, wfds, null, tv);
  if (numfds == 0)
    return numfds;

  if (numfds == -1)
  {
    if (ffi.errno == dh.EINTR)
      return numfds;

    throw new Error("Error polling sockets" + syserr());
  }

  for each (let socket in pollSockets)
  {
    var fd = +socket.fd; /* events could modify sock.fd */
    if ('number' != typeof fd || fd < 0 || fd >= FD_SETSIZE)
      throw new Error("Invalid file descriptor: "+fd+" from "+socket);

    if (_FD_ISSET.call(fd, rfds) != 0)
    {
      if (socket.onReadable)
	socket.onReadable(socket);
    }

    if (_FD_ISSET.call(fd, wfds) != 0)
      if (socket.onWritable)
	socket.onWritable(socket);
  }
  return numfds
}

/*** Node.js Compatibility Functions ***/
function graft(dst, src)
{
  for (let el in src)
    dst[el] = src[el];
}

var eventSystem =
{
  addListener: function addListener(eventName, eventHandler)
  {
    if (!this._listeners)
      this._listeners = {};

    if (!this._listeners[eventName])
      this._listeners[eventName] = [];

    this._listeners[eventName].push(eventHandler);
  },

  removeListener: function removeListener(eventName, eventHandler)
  {
    var i;

    if (!this._listeners || !this._listeners[eventName])
      return;

    i = this._listeners[eventName].indexOf(eventHandler);
    if (i != -1)
      this._listeners[eventName].splice(i, 1);
  },

  removeAllListeners: function removeAllListeners(eventname)
  {
    if (this._listeners)
      delete this._listeners[eventName];
  },

  listeners: function listeners(eventName)
  {
    if (!this._listeners)
      this._listeners = {};

    if (!this._listeners[eventName])
      this._listeners[eventName] = [];

    return this._listeners[eventName];
  },

  vemit: function vemit(eventName, args)
  {
    if (!this._listeners || !this._listeners[eventName])
      return;

    for each(let event in this._listeners[eventName])
      event.apply(this, args);
  },

  emit: function emit(eventName /* ... */)
  {
    var args;

    args = Array.prototype.slice.call(arguments);
    args.shift();

    return this.vemit(eventName, args);
  }
}

function EventSystem()
{
  this._listeners = {};
}

EventSystem.prototype = eventSystem;

function Server()
{
  graft(this, new EventSystem());
}

Server.prototype.connections = [];

Server.prototype.listen = function Server_listen(port, host, backlog)
{
  if (this.socket && this.socket.listening)
    throw new Error("Cannot listen again; server is already listening to " + this.socket.address + ", on port " + ntohl(this.socket.sockaddr.sin_port));

  this.socket = new exports.socketStream(port, host);
  this.socket.listen(backlog);
  this.ident = "the server";
  this.socket.ident = "the socket";
  this.socket.server = this;
  this.socket.onReadable = reactorConnectEvent;
}

Server.prototype.close = function Server_close()
{
  delete this.socket.onReadable;
  this.socket.close();

  for each (connection in this.connections)
    connection.addListener("close", reactorTryCloseServer);
}

Server.prototype.toString = function Server_toString()
{
  var a=[];

  if (this.socket.listening)
    a.push("listening");

  if (this.socket.bound)
    a.push("bound");

  if (this.socket)
    a.push("on " + this.socket.address + ", port " + ntohl(this.socket.sockaddr.sin_port));

  return "[Object net.Server" + a.join("; ") + "]";
}

exports.createServer = function createServer(connection_listener)
{
  var s = new Server();

  s.addListener("connection", connection_listener);

  return s;
}

exports.reactor = function(servers, quitObject, extraSockets)
{
  var pollSockets;

  if (!quitObject)
  {
    quitObject = { quit: false };

    if (!require("system").global.quit)
      require("signal").onINT = function () { quitObject.quit = true; };	/* gpsee-js crashes when trapping signals */
  }

  do 
  {
    pollSockets = extraSockets ? extraSockets.slice(0) : [];

    for each (let server in servers)
    {
      pollSockets.push(server.socket);

      for each (let connection in server.connections)
      {
        if (connection.readyState === "closed")
        {
          let idx = server.connections.indexOf(connection);

          if (idx === -1)
            throw new Error("Socket on fd " + (0+this.fd) + "is not in server's connection list!");

          server.connections.splice(idx, 1);
          continue;
        }

	connection.onReadable = reactorRead;
	if (connection.pendingWrites.length)
	  connection.onWriteable = reactorDrain;
	else
	  delete connection.onWriteable;

	pollSockets.push(connection);
      }
    }

    exports.poll(pollSockets);
  } while(!quitObject.quit);
}

function reactorConnectEvent(socket)
{
  var new_socket = socket.accept();

  new_socket.readyState = "opening";
  new_socket.server = socket.server;
  new_socket.write = reactorWrite;
  new_socket.pendingWrites = [];

  graft(new_socket, new EventSystem());

  new_socket.readyState = "open";
  new_socket.addListener("end", reactSocketEnd);

  socket.server.connections.push(new_socket);
  socket.server.emit("connection", new_socket);
  new_socket.emit("connect");
}

function reactorRead(socket)
{
  var bytesRead;

  if (!socket.readBuffer)
    socket.readBuffer = new ffi.Memory(8192);

  bytesRead = 0 + _read.call(socket.fd, socket.readBuffer, socket.readBuffer.size);

  switch(bytesRead)
  {
    case 0:
      socket.emit("end");
      break;
    case -1:
      socket.emit("error");
      break;
    default: 
      socket.emit("data", socket.readBuffer.asDataString(bytesRead));
      break;
  }
}

function reactorWrite(data)
{
  var bytesWritten;
  var tmp;

  switch (typeof data)
  {
    case "string":
      tmp = new ffi.Memory(data.length);
      tmp.length = data.length;
      tmp.copyDataString(data);
      data = tmp;
      break;
    case "object":
      if (data instanceof binary.Binary)
	break;
    default:
	throw("Cannot write data; invalid type");
  }

  if (this.pendingWrites.length)
  {
    this.pendingWrites.push(data);
    return;
  }

  bytesWritten = _write.call(this.fd, data, data.length);

  if (bytesWritten != data.length)
  {
    if (bytesWritten != -1)
      this.pendingWrites.push(data.slice(bytesWritten));
    else
    {
      if (ffi.errno == dh.EPIPE)
      {
        delete this.onWriteable;
        this.readyState = "readOnly";
      }
      this.had_write_error = ffi.errno;
    }
  }
}

function reactSocketEnd()
{
  var socket = this;

  delete this.onReadable;
  delete this.onWriteable;

  if (_shutdown.call(this.fd, dh.O_RDWR) != 0)
    if (ffi.errno != dh.ENOTCONN)
      throw new Error("Could not shutdown socket on fd " + (0+this.fd) + syserr());

  if (_close.call(this.fd) != 0)
    throw new Error("Could not close socket on fd " + (0+this.fd) + syserr());

  this.readyState = "closed";
  delete this.fd;
}

function reactorDrain(socket)
{
  var bytesWritten;

  while(socket.pendingWrites.length)
  {
    bytesWritten = _write.call(socket.fd, socket.pendingWrites[0], socket.pendingWrites[0].length);
    if (bytesWritten != socket.pendingWrites[0].length)
    {
      if (bytesWritten != -1)
	socket.pendingWrites[0] = socket.pendingWrites[0].slice(bytesWritten);
      else
	socket.had_write_error = ffi.errno;

      return;
    }
  }

  socket.emit("drain");
}

function reactorTryCloseServer(socket)
{
  var server = socket.server;

  if (server.connections.length)
    return;

  this.emit("close");
}

