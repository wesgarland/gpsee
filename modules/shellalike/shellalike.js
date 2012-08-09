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
 *  @file       shellalike_module.js    The shellalike module provides a convenient API for
 *                                      common "shell" programming idioms, mainly executing
 *                                      other processes and connecting their stdio streams
 *                                      to each other to "pipe" data from one to the next.
 *  @author     Donny Viszneki
 *              hdon@page.ca
 *  @date       Nov 2009
 *  @version    $Id: shellalike.js,v 1.4 2010/06/14 22:12:01 wes Exp $
 */

/* Module requirements */
const ffi = require('gffi');
const {ByteString,ByteArray} = require('binary');
const system = require('system');

/* GFFI decls */
const _pipe = new ffi.CFunction(ffi.int, 'pipe', ffi.pointer);
const _gpsee_p2open = new ffi.CFunction(ffi.int, 'gpsee_p2open', ffi.pointer, ffi.pointer, ffi.pointer);
const _p2close = new ffi.CFunction(ffi.int, 'gpsee_p2close', ffi.pointer, ffi.pointer, ffi.int, ffi.pid_t);
const _waitpid = new ffi.CFunction(ffi.pid_t, 'waitpid', ffi.pid_t, ffi.pointer, ffi.int);
const _open = new ffi.CFunction(ffi.int, 'open', ffi.pointer, ffi.int);
const _creat = new ffi.CFunction(ffi.int, 'creat', ffi.pointer, ffi.int);
const _close = new ffi.CFunction(ffi.int, 'close', ffi.int);
const _read = new ffi.CFunction(ffi.ssize_t, 'read', ffi.int, ffi.pointer, ffi.size_t);
const _write = new ffi.CFunction(ffi.ssize_t, 'write', ffi.int, ffi.pointer, ffi.size_t);
const _fsync = new ffi.CFunction(ffi.int, 'fsync', ffi.int);
const _memset = new ffi.CFunction(ffi.pointer, 'memset', ffi.pointer, ffi.int, ffi.size_t);
const _fdopen = new ffi.CFunction(ffi.pointer, 'fdopen', ffi.int, ffi.pointer);
const _fgets = new ffi.CFunction(ffi.pointer, 'fgets', ffi.pointer, ffi.int, ffi.pointer);
const _fwrite = new ffi.CFunction(ffi.size_t, 'fwrite', ffi.pointer, ffi.size_t, ffi.size_t, ffi.pointer);
const _fflush = new ffi.CFunction(ffi.int, 'fflush', ffi.pointer);
const _fclose = new ffi.CFunction(ffi.int, 'fclose', ffi.pointer);
const _feof = new ffi.CFunction(ffi.int, 'feof', ffi.pointer);
const _ferror = new ffi.CFunction(ffi.int, 'ferror', ffi.pointer);
const _strlen = new ffi.CFunction(ffi.size_t, 'strlen', ffi.pointer);

/* TODO add this sort of thing to GFFI proper */
const sizeofInt = 4;
const sizeofPtr = 4;
/* Convenience stuff */
/* numeric range generator */
function range(start,stop){if('undefined'===typeof stop){stop=start;start=0}for(var i=start;i<stop;i++)yield i};
/* Generator.prototype.toArray() */
(function(){yield})().__proto__.toArray = function(){var a=[];for(var v in this)a.push(v);return a}
/* Zap the new/apply contention with help from eval() */
Function.prototype.new = function(){ return eval('new this('+range(0,arguments.length).toArray().map(function(n)'arguments['+n+']')+')') }
/* Bind an instance method to some 'this' object */
function $P(f,t) {
  if (arguments.length==1) t = this;
  return function() f.apply(t, arguments);
}
/* Patch Memory with GPSEE's xintAt() ByteString method */
ffi.Memory.prototype.intAt = ByteString.prototype.xintAt;
/* Patch Memory with ptrAt() function; returns C-equivalent of ((void*)this)[offset]
 * TODO endiannize. implement in C?
 */
ffi.Memory.prototype.ptrAt = function Memory_ptrAt(offset) {
  var addr = '@0x';
  for(let i=sizeofPtr-1; i>=0; i--) {
    var s = ByteString.prototype.charCodeAt.call(this, i+offset).toString(16);
    if (s.length < 2)
      s = '0' + s;
    addr += s;
  }
  return ffi.Memory(addr);
}
/* Patch Memory with a function to return a pointer at a given offset of the given pointer.
 * TODO add to Memory proper? */
ffi.Memory.prototype.offset = function Memory_offset(offset) ffi.Memory('@0x'+(offset+parseInt(this.toString().substr(3), 16)).toString(16))
/* Recursively invoke each member of the stack (Array) 's' such that the return value of each function is supplied as
 * a single argument to the next deepest function. The deepest function is given no arguments.
 */
function $CS(s){var f=s.pop();return s.length?f(arguments.callee(s)):f()}

/* @jazzdoc shellalike.flines
 * @form for (line in flines(source)) {...}
 * Allows line-by-line iteration over a readable stdio FILE* source.
 */
const flines = (function(){
  var buflen = 4096, buf = new ffi.Memory(buflen);
  function flines(fdsrc) {
    /* fdopen(3) just for fgets(3) */
    var src = _fdopen.call(fdsrc, "r");
    var line;
    //print('flines buf', buf);
    //print('flines src', src);
    while ((line = _fgets.call(buf, buflen, src))) {
      yield ByteString(line, _strlen.call(line)).decodeToString('ascii');
    }
    /* Check ferror(3) */
    var err = _ferror.call(src);
    if (err|0)
      print('throw new Error("fgets() threw error "+err);');
    /* Check feof(3) */
    if (_feof.call(src)|0)
      return;
  }
  return flines;
})();

/* @jazzdoc shallalike.Process
 * @form new Process('command_sans_path');
 * @form new Process('command_sans_path with arguments');
 * @form new Process('command_sans_path with arguments', 'plus', 'these', 'arguments');
 * @form new Process('/this/command/not/searched/for/in/PATH');
 * @form new Process('tail -f', logfilename);
 * @form new Process('tail -f', logfilename, {onOutputHandler: outputHandler});
 * @form new Process('tail -f', logfilename, {inputSource: inputSource});
 *
 * nothing is set in stone, yet
 */
function Process(command) {
  //print('Process with', command);
  var FDs = new ffi.Memory(sizeofInt*2);
  var pidPtr = new ffi.Memory(sizeofInt);
  var result = _gpsee_p2open.call(command, FDs, pidPtr);
  //print('  result', result);

  var pid = pidPtr.intAt(0);
  var snk = FDs.intAt(sizeofInt);
  var src = FDs.intAt(0);

  //print('  pid', pid);
  //print('  src', src);
  //print('  snk', snk);

  //var {src,snk} = p2open(command);
  /* @jazzdoc shellalike.Process.__iterator__
   * @form for (line in new Process(command)) {...}
   * Allows line-by-line iteration over the output of a shell command.
   */
  function Process___iterator__() {
    for (let line in flines(src))
      yield line;
    if (this.check()) {
      let exitStatus = this.checkStatus();
      if (exitStatus)
        throw new Error("Command exited " + exitStatus);
    }
  }
  this.__iterator__ = Process___iterator__;

  /* @jazzdoc shellalike.Process.check
   * @form (instance of Process).check()
   * Returns 0 if the process is still running, and non-zero if the process has exited.
   */
  var statusVar = new ffi.Memory(sizeofInt);
  this.check = function Process_check() {
    var result = _waitpid.call(pid, statusVar, ffi.std.WNOHANG);
    //print('waitpid returns', result);
    return result|0;
  }
  /* @jazzdoc shellalike.Process.checkStatus
   * @form (instance of Process).checkStatus():Number
   * If the process has exited, returns the exit status. Otherwise throws an Error.
   */
  this.checkStatus = function Process_checkStatus() {
    if (!this.check())
      throw new Error('Process has not exited, so no exit status is available');
    return statusVar.intAt(0);
  }
  /* @jazzdoc shellalike.Process.write
   * @form (instance of Process).write(string)
   * Writes a ByteString to the Process's sink.
   * Other types will be coerced to a ByteString.
   */
  this.write = function Process_write(s) {
    //print("WRITING TO", snk, 'THIS', s);
    if (!s instanceof ByteString)
      s = new ByteString(String(s));
    var result = _write.call(snk, s, s.length);
    /* Write error */
    if (result < 0) {
      if (this.check())
        throw new Error("Attempt to write to a process which has already exited");
      throw new Error("write(2) error! TODO error report");
    }
    return result;
  }
  /* @jazzdoc shellalike.Process.flush
   * @form (instance of Process).flush()
   * Flushes the Process's sink
   * TODO figure out why fsync(2) is returning an error on simple cat test
   */
  this.flush = function() {
    print("SYNCING", snk);
    var result = _fsync.call(snk);
    if (result<0) {
      if (this.check())
        throw new Error("Attempt to flush to a process which has already exited");
      throw new Error("fsync(2) error! TODO error report");
    }
  }
  /* @jazzdoc shellalike.Process.close
   * @form (instance of Process).close()
   * Closes the Process's sink
   */
  var finalizer1 = new ffi.WillFinalize,
      finalizer2 = new ffi.WillFinalize;
  finalizer1.finalizeWith(_close, snk);
  finalizer2.finalizeWith(_close, src);
  this.close = function() {
    finalizer1.runFinalizer();
    finalizer2.runFinalizer();
  }

  this.finishWriting = function() {
    finalizer1.runFinalizer();
  }
}

/* Digraph class */
function Digraph() {
  /* Private variables */
  var m_graph = this;
  var m_elements = [];
  var m_elementData = [];
  var m_passkey = {};
  function pass(key, rval) {
    if (m_passkey !== key)
      throw new Error("Private access denied");
    return rval;
  }

  /* Privileged Digraph.toString() instance method */
  function Digraph_toString() {
    return '[Digraph with '+m_elements.length+' elements]';
  }
  this.toString = Digraph_toString;

  /* Element class */
  function Digraph_Element(data) {
    /* Private variables */
    var m_element = this;
    var m_elementData = this.data = data;
    var m_id = m_elements.length;
    var m_pads = {
      'src': [],
      'snk': []
    };

    /* Pad class */
    function Digraph_Element_Pad(type, data) {
      /* Private variables */
      var m_type = String(type);
      var m_pad = this;
      var m_padData = data;
      this.linked = false;

      /* There are only two types of Pad */
      if (type != 'src' && type != 'snk')
        throw new Error('invalid pad type "'+type+'"');

      /* Function to link two pads */
      function Digraph_Element_Pad_link(p) {
        if (this._private(m_passkey) !== m_pad)
          throw new Error("OOP violation");
        if (m_element.owner !== p.owner.owner)
          throw new Error("Pad link parameters must descend from the same Digraph instance");
        if (this.linked || p.linked)
          throw new Error("Pad link parameters must be unlinked");
        if (m_type == p.type)
          throw new Error("Pad link parameters must be heterogeneous (both pads are of type "+m_type+")");

        var priv = p._private(m_passkey);

        priv[m_type] = m_pad;
        m_pad[p.type] = p;

        priv.linked = true;
        m_pad.linked = true;
      }

      /* Private variable Digraph::Element::m_pads keeps track of its pads */
      m_pads[type].push(m_pad);
      /* Restricted variables */
      this.owner = m_element;
      /* Privileged toString() instance method */
      this.toString = function()"["+(m_pad.linked?"Linked":"Unlinked")+" "+m_type+"Pad ("+m_padData+") of "+m_element+"]"
      /* Public Digraph_Element_Pad interface */
      return m_pad.iface = {
        '__proto__':      Digraph_Element_Pad.prototype,
        get link()        Digraph_Element_Pad_link,
        get type()        m_type,
        get owner()       m_element,
        get toString()    function() m_pad.toString(),
        get linked()      Boolean(m_pad.linked),
        '_private':       function(key) pass(key, m_pad)
      };
    } /* Digraph_Element_Pad constructor */

    /* Function to add a pad to this element */
    function Digraph_Element_addPad(type, data) {
      return new Digraph_Element_Pad(type, data);
    }
    this.addPad = Digraph_Element_addPad;

    /* Digraph Element toString() privileged instance method */
    function Digraph_Element_toString() {
      return '[Element '+m_id+' ('+m_elementData+') of '+m_graph+']';
    }
    this.toString = Digraph_Element_toString;

    this._private = function(key) pass(key, {'pads':m_pads});
  } /* Digraph_Element constructor */

  /* Function to add a new element to the digraph */
  function Digraph_addElement(data) {
    var el = new Digraph_Element(data);
    m_elements.push(el);
    m_elementData.push(data);

    /* Return public Digraph_Element interface */
    el.owner = m_graph;
    return el.iface = {
      '__proto__':      data,
      get toString()    function() el.toString(),
      get addPad()      function() el.addPad.apply(el,arguments),
      get owner()       m_graph,
      get _private()    function(key) pass(key, el)
    };
  }

  function Digraph_isContinuous() {
    var visited = [];
    function visit(el) {
      if (visited.indexOf(el) < 0) {
        visited.push(el);
        for each(let [dir,rid] in [['src','snk'],['snk','src']])
        for each(let pad in el._private(m_passkey).pads[dir]) {
          if (pad.linked) {
            visit(pad[rid].owner, dir, rid);
          }
        }
      }
    }
    visit(m_elements[0]);
    for each(let el in m_elements)
      if (visited.indexOf(el) < 0)
        return false;
    return true;
  }

  /* Function to determine whether or not each pad is linked to only one other pad,
   * that the digraph is acyclic, that all of each elements' sources and sinks are
   * connected to the sinks and sources (respectively) of only one other element
   * (individually) and that there are no discontinuitities or unlinked pads.
   */
  function Digraph_isLinear() {
    if (!this.isContinuous())
      return false;
    if (this.isCyclic())
      return false;
    for each(let [dir,rid] in [['src','snk'],['snk','src']]) {
      let el = m_elements[0];
      while (1) {
        let pads = el._private(m_passkey).pads[dir];
        if (pads.length > 1)
          return false;
        else if (pads.length == 1) {
          if (pads[0].linked)
            el = pads[0][rid].owner;
          else break;
        }
        else break;
      }
    }
    return true;
  }

  /* Stop-gap means of building pipelines before the prop query system is
   * fully implemented.
   */
  function Digraph_linearize(userfun) {
    if (!this.isLinear())
      throw new Error("Cannot linearize non-linear graph");
    var rval = [userfun(m_elements[0].data)];
    for each(let [dir,rid,add] in [['src','snk','push'],['snk','src','unshift']]) {
      let el = m_elements[0];
      while (1) {
        let pads = el._private(m_passkey).pads[dir];
        if (pads.length > 1)
          throw new Error("graph appears unlinear!");
        else if (pads.length == 1) {
          if (pads[0].linked) {
            el = pads[0][rid].owner;
            rval[add](userfun(el.data));
          }
          else break;
        }
        else break;
      }
    }
    return rval;
  }

  /* Function to determine whether or not the digraph is cyclic */
  function Digraph_isCyclic() {
    var visited = [];
    function visit(el, dir, rid) {
      if (visited.indexOf(el) >= 0)
        throw visit;
      visited.push(el);
      for each(let pad in el._private(m_passkey).pads[dir]) {
        if (pad.linked) {
          //print('yay', rid, pad[rid]);
          visit(pad[rid].owner, dir, rid);
        }
      }
      visited.pop();
    }
    try {
      for each(let [dir,rid] in [['src','snk'],['snk','src']]) {
        visit(m_elements[0], dir, rid);
      }
    }
    catch (e if e == visit) {
      return true;
    }
    return false;
  }
  this.isCyclic = Digraph_isCyclic;

  /* Return public Digraph interface */
  return m_graph.iface = {
    '__proto__':            Digraph.prototype,
    get toString()          Digraph_toString,
    get addElement()        Digraph_addElement,
    get isLinear()          Digraph_isLinear,
    get isCyclic()          Digraph_isCyclic,
    get isContinuous()      Digraph_isContinuous,
    get linearize()         Digraph_linearize
  };
}

function SimplePad() {}
SimplePad.prototype.toString = function()'simple pipeline pad';

/* A pipeline is an extension of the Digraph class. Presently it is quite limited,
 * but the use cases it covers are very typical.
 */
function Pipeline() {
  /* Private variables */
  var m_graph;
  var m_topElement;
  var m_pipeline = this;

  /* Determine what sort of execution implementation is needed to run this pipeline
   */
  function Pipeline_shape() {
    var ie = m_graph.linearize(function(x)x.internal?'i':'e').join('');
    if (!m_graph.isLinear())
      throw new Error("Currently only linear pipelines are supported!");

    for each(var [nam,pat] in [
      ['empty',         /^$/],
      ['all internal',  /^i+$/],
      ['all external',  /^e+$/],
      ['to internal',   /^e+i+$/],
      ['to external',   /^e*i+e+$/],
                              ]) {
      if (ie.match(pat))
        return nam;
    }
    return 'interleaved';
  }

  function Pipeline_validateGraph() {
    if (!m_graph.isLinear())
      throw new Error("Currently only linear pipelines are supported!");
    return true;
  }

  function Pipeline_realizeGraph() {
    switch (Pipeline_shape()) {
      case 'empty':
        return;
      case 'all internal':
      case 'all external':
      case 'to internal':
      case 'to external':
    }
  }
  
  function Pipeline_append(el) {
    if ('undefined' !== typeof m_topElement) {
      var srcPad = m_topElement.addPad('src', new SimplePad);
      var snkPad = el.addPad('snk', new SimplePad);
      srcPad.link(snkPad);
    }
    m_topElement = el;
  }

  function Pipeline_addExternalCommand(command) {
    Pipeline_append(m_graph.addElement({
          'toString':     function()'[External Command '+command+']',
          'internal':     false,
          'command':      command
    }));
  }
  function Pipeline_addGenerator(generator) {
    Pipeline_append(m_graph.addElement({
          'toString':     function()'[Generator '+(generator.name||'anonymous')+']',
          'internal':     true,
          'generator':    generator
    }));
  }
  function Pipeline_addMap(func) {
    Pipeline_append(m_graph.addElement({
          'toString':     function()'[Mapping '+(func.name||'anonymous')+']',
          'internal':     true,
          'generator':    function(src){for(let y in src)yield func(y)}
    }));
  }
  function Pipeline_addIterator(iterator) {
    Pipeline_append(m_graph.addElement({
          'toString':     function()'[Iterator]',
          'internal':     true,
          'iterator':     iterator
    }));
  }
  function Pipeline_add(a) {
    switch (typeof a) {
      case 'string':
        return Pipeline_addExternalCommand(a);
      case 'function':
        return Pipeline_addGenerator(a);
      case 'object':
        switch (a.cmd) {
          case 'map':
            return Pipeline_addMap(a.func);
        }
    }
    throw new Error("Don't know how to add this type of argument to a Pipeline");
  }
  /* runs the pipeline */
  function Pipeline_run() {
    var shape = Pipeline_shape();
    //print('running a "'+shape+'"-type pipeline');
    switch (shape) {
      default:
        throw new Error('Unknown pipeline shape "'+shape+'"');
      case 'empty':
        // @todo is this the desired behavior?
        return null;
      case 'all internal':
        /* Get an array of all generator functions */
        var generators = m_graph.linearize(function(x)x.generator).filter(function(x)x);
        //print(generators.join('\nTHEN\n'));
        // Construct each generator (ie. for 3 generators: g[2](g[1](g[0])))
        return $CS(generators);
      case 'all external':
        var cmd = m_graph.linearize(function(x)x.command).join('|');
        var p = new Process(cmd);
        return p;
      case 'to internal':
        var cmd = m_graph.linearize(function(x)x).filter(function(x)x.hasOwnProperty('command')).map(function(x)x.command).join('|');
        var generators = m_graph.linearize(function(x)x.generator).filter(function(x)x);
        function process(){for(var x in new Process(cmd))yield x}
        generators.unshift(process);
        //print(generators.join('\nTHEN\n'));
        // Construct each generator (ie. for 3 generators: g[2](g[1](g[0])))
        return $CS(generators);
      case 'to external':
        var cmd = m_graph.linearize(function(x)x).filter(function(x)x.hasOwnProperty('command')).map(function(x)x.command).join('|');
        var generators = m_graph.linearize(function(x)x.generator).filter(function(x)x);
        function process(src){var p = new Process(cmd); for(var y in src) { p.write(y) } };
        generators.push(process);
        return $CS(generators);
    }
  }
  
  /* TODO note this style of digraph inheritance isn't actually implemented yet */
  m_graph = new Digraph({
    'validateGraph': Pipeline_validateGraph,
    'realizeGraph':  Pipeline_realizeGraph
  });

  return this.iface = {
    'shape':        Pipeline_shape,
    'add':          Pipeline_add,
    'addMap':       Pipeline_addMap,
    'run':          Pipeline_run
  };
}

function CAY_writeToFile(src) {
}
function CAY_appendToFile(src) {
}
var CAY = {
  /* @jazzdoc shellalike.cay.prototype.print
   * @form (cay initializer).print()
   * Terminates a CAY pipeline with a printer (with newlines. Use
   * shellalike.cay.prototype.rtrim() to remove excess newlines before
   * printing.
   */
  'print':  function() this(function(src){for each(let x in src)print(x)})(),
  /* @jazzdoc shellalike.cay.prototype.trim
   * @form (cay initializer).trim()
   * Trims whitespace from both ends of each chunk of data coming through
   * the CAY pipeline. This is accomplished by calling a .trim() method
   * on each value yielded.
   */
  'trim':   function() this(function(src){for each(let x in src)yield x.trim()}),
  /* @jazzdoc shellalike.cay.prototype.trim
   * @form (cay initializer).trim()
   * Trims whitespace from the end of each chunk of data coming through
   * the CAY pipeline. This is accomplished by regexp on each value yielded.
   */
  'rtrim':  function() this(function(src){for each(let x in src)yield x.match(/(.*)\s*/)[1]}),
  /* @jazzdoc shellalike.cay.prototype.fwrite
   * @form (cay initializer).fwrite(filename)
   * Sends CAY pipeline data directly to a file that is truncated upon execution
   * of this CAY pipeline. This dynamic CAY pipeline stage will avoid
   * extra trips between process boundaries by implementing itself as internal
   * if its source is internal, and external if its source is external.
   */
  'fwrite': CAY_writeToFile,
  /* @jazzdoc shellalike.cay.prototype.fappend
   * @form (cay initializer).fappend(filename)
   * Appends CAY pipeline data directly to a file. This dynamic CAY pipeline
   * stage will avoid extra trips between process boundaries by implementing
   * itself as internal if its source is internal, and external if its source
   * is external.
   */
  'fappend':CAY_appendToFile,

  /* @jazzdoc shellalike.cay.prototype.map
   * @form (cay initializer).map(func)
   * Invokes func() on all CAY data passing through this pipeline element,
   * yielding the return value of func(). Notionally similar to
   * Array.prototype.map, but no Array is ever materialized, therefore a data
   * set can be arbitrarily large and may be computed over time.
   */
  'map': function CAY_map(func) this({'cmd':'map', 'func':func}),

  /* A function will inherit from CAY, so we'll make it feel as much like a standard function as possible */

  /* @jazzdoc shellalike.cay.prototype.call
   * This is a reference to Function.prototype.call()
   */
  'call':   Function.prototype.call,
  /* @jazzdoc shellalike.cay.prototype.apply
   * This is a reference to Function.prototype.apply()
   */
  'apply':  Function.prototype.apply
}

function cay(cmd) {
  var m_pipeline = new Pipeline;
  m_pipeline.add(cmd);

  /* This closure will be our return value. We'll give it some properties
   * and a different __proto__, too.
   */
  function _cay(cmd) {
    if (arguments.length == 0) {
      return m_pipeline.run();
    }
    else {
      m_pipeline.add(cmd);
    }
    return _cay;
  }
  function CAY___iterator__() {
    /* This last stage guarantees that we have an internal stage at the end.
     * TODO check if the last one is internal first.
     * TODO do something more efficient than nesting another identity generator */
    m_pipeline.add(function(src){for(let x in src)yield x});
    return m_pipeline.run();
  }
  _cay.__proto__ = CAY;
  _cay._pipeline = m_pipeline;
  _cay.__iterator__ = CAY___iterator__;
  
  return _cay;
}

/* exports */
exports.Digraph = Digraph;
exports.Pipeline = Pipeline;
exports.Process = Process;
exports.flines = flines;
exports.cay = cay;
exports.CAY = CAY;
