/** 
 * @file 	event.js		An event module for GPSEE intended to be API-compatible with Node's event module.
 * 					Loosely based around the GPSEE POC net module.
 * @author	Wes Garland, wes@page.ca
 * @date	Dec 2012
 */

var REACTOR = require("reactor");

exports.EventEmitter = function events$$EventEmitter()
{
}

exports.EventEmitter.prototype.addListener = function events$$EventEmitter$addListener(eventName, eventHandler)
{
  if (!this.hasOwnProperty("_listeners"))
    this._listeners = {};

  if (!this._listeners.hasOwnProperty(eventName))
    this._listeners[eventName] = [];

  this._listeners[eventName].push(eventHandler);
  this.emit("newListener", eventName, eventHandler);
}

exports.EventEmitter.prototype.on = exports.EventEmitter.prototype.addListener;

exports.EventEmitter.prototype.once = function events$$EventEmitter$once(eventName, eventHandler)
{
  function once_wrapper()
  {
    eventHandler();
    this.removeListener(eventName, eventHandler);
  }
  this.on(eventName, once_wrapper);
}

exports.EventEmitter.prototype.removeListener = function events$$EventEmitter$removeListener(eventName, eventHandler)
{
  var i;

  if (!this._listeners.hasOwnProperty(eventName))
    return;

  i = this._listeners[eventName].indexOf(eventHandler);
  if (i != -1)
  {
    this._listeners[eventName].splice(i, 1);
    if (this._listeners[eventName].length === 0)
      delete this._listeners[eventName];
  }
}

exports.EventEmitter.prototype.removeAllListeners = function events$$EventEmitter$removeAllListeners(eventName)
{
  if (this._listeners)
    delete this._listeners[eventName];
}

exports.EventEmitter.prototype.listeners = function events$$EventEmitter$listeners(eventName)
{
  if (!this._listeners.hasOwnProperty(eventName))
    this._listeners[eventName] = [];

  return this._listeners[eventName];
}

exports.EventEmitter.prototype.setMaxListeners = function events$$EventEmitter$setMaxListeners(number)
{
  throw new Error("not implemented");
}

exports.EventEmitter.prototype.vemit = function events$$EventEmitter$vemit(eventName, args)
{
  var i;

  if (!this._listeners || !this._listeners.hasOwnProperty(eventName))
    return;

  for (i=0; i < this._listeners[eventName].length; i++)
    REACTOR.runSoon(this, this._listeners[eventName][i], args);
}

exports.EventEmitter.prototype.emit = function events$$EventEmitter$emit(eventName /* ... */)
{
  var args;

  args = Array.prototype.slice.call(arguments);
  args.shift();

  return this.vemit(eventName, args);
}

Function.prototype._toString = Function.prototype.toString;
Function.prototype.toString = function() { return this.name ? "[Function " + this.name + "]" : this._toString() };
