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
 *  @version    $Id$
 */

/* Module requirements */
const ffi = require('gffi');
const {ByteString,ByteArray} = require('binary');

/* GFFI decls */
const _pipe = new ffi.CFunction(ffi.int, 'pipe', ffi.pointer);
const _p2open = new ffi.CFunction(ffi.int, '__gpsee_p2open', ffi.pointer, ffi.pointer);
const _p2close = new ffi.CFunction(ffi.int, 'gpsee_p2close', ffi.pointer);
const _open = new ffi.CFunction(ffi.int, 'open', ffi.pointer, ffi.int);
const _creat = new ffi.CFunction(ffi.int, 'creat', ffi.pointer, ffi.int);
const _close = new ffi.CFunction(ffi.int, 'close', ffi.int);
const _read = new ffi.CFunction(ffi.ssize_t, 'read', ffi.int, ffi.pointer, ffi.size_t);
const _write = new ffi.CFunction(ffi.ssize_t, 'write', ffi.int, ffi.pointer, ffi.size_t);
const _memset = new ffi.CFunction(ffi.pointer, 'memset', ffi.pointer, ffi.int, ffi.size_t);
const _fdopen = new ffi.CFunction(ffi.pointer, 'fdopen', ffi.int, ffi.pointer);
const _fgets = new ffi.CFunction(ffi.pointer, 'fgets', ffi.pointer, ffi.int, ffi.pointer);
const _fwrite = new ffi.CFunction(ffi.size_t, 'fwrite', ffi.pointer, ffi.size_t, ffi.size_t, ffi.pointer);
const _fflush = new ffi.CFunction(ffi.int, 'fflush', ffi.pointer);
const _fclose = new ffi.CFunction(ffi.int, 'fclose', ffi.pointer);
const _strlen = new ffi.CFunction(ffi.size_t, 'strlen', ffi.pointer);


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
ffi.Memory.prototype.intAt = ByteString.prototype.xintAt;


/* @jazzdoc shellalike.p2open
 * Mostly intended as an internal function.
 * TODO add finalizer! this will require repairing the missing
 * linked-list functionality in __gpsee_p2open() et al.
 */
const p2open = (function() {
  var FDs = new ffi.Memory(8); // TODO sizeof(int)*2
  //FDs.finalizeWith(_free, FDs);
  function p2open(command) {
    //print('p2open() with', command);
    var n = _p2open.call(command, FDs);
    if (n|0)
      throw new Error('p2open() failed (TODO better error)');
    var psink = _fdopen.call(FDs.intAt(0), "w"),
        psrc  = _fdopen.call(FDs.intAt(4), "r");
    /* GFFI MAGIC! See gffi.BoxedPrimitive for help! */
    //n.finalizeWith(_p2close, FDs.intAt(0), FDs.intAt(4), 9);
    return {'src':psrc, 'snk':psink, /*'__gcreqs__': [n]*/ };
  }
  return p2open;
})();
/* @jazzdoc shellalike.flines
 * @form for (line in flines(source)) {...}
 * Allows line-by-line iteration over a readable stdio FILE* source.
 */
const flines = (function(){
  var buf = new ffi.Memory(4096);
  function flines(src) {
    var line;
    while ((line = _fgets.call(buf, 4096, src)))
      yield ByteString(line, _strlen.call(line)).decodeToString('ascii');
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
  var {src,snk} = p2open(command);
  /* @jazzdoc shellalike.Process.__iterator__
   * @form for (line in new Process(command)) {...}
   * Allows line-by-line iteration over the output of a shell command.
   */
  this.__iterator__ = function()flines(src);

  /* @jazzdoc shellalike.Process.write
   * @form (instance of Process).write(string)
   * Writes a ByteString to the Process's sink.
   * Other types will be coerced to a ByteString.
   */
  this.write = function(s) {
    if (!(s instanceof ByteString))
      s = new ByteString(String(s));
    return _fwrite.call(s, 1, s.length, snk);
  }
  /* @jazzdoc shellalike.Process.flush
   * @form (instance of Process).flush()
   * Flushes the Process's sink
   */
  this.flush = function() {
    return _fflush.call(snk);
  }
  /* @jazzdoc shellalike.Process.close
   * @form (instance of Process)f.flush()
   * Closes the Process's sink
   */
  this.close = function() {
    return _fclose.call(snk);
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
      'snk': [],
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
        '_private':       function(key) pass(key, m_pad),
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
      get _private()    function(key) pass(key, el),
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
    get linearize()         Digraph_linearize,
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
          'command':      command,
    }));
  }
  function Pipeline_addGenerator(generator) {
    Pipeline_append(m_graph.addElement({
          'toString':     function()'[Generator '+(generator.name||'anonymous')+']',
          'internal':     true,
          'generator':    generator,
    }));
  }
  function Pipeline_addIterator(iterator) {
    Pipeline_append(m_graph.addElement({
          'toString':     function()'[Iterator]',
          'internal':     true,
          'iterator':     iterator,
    }));
  }
  function Pipeline_add(a) {
    if ('string'===typeof a)
      Pipeline_addExternalCommand(a);
    else if ('function'===typeof a)
      Pipeline_addGenerator(a);
    else throw new Error("Don't know how to add this type of argument to a Pipeline");
  }
  /* runs the pipeline */
  function Pipeline_run() {
    //print('running a "'+Pipeline_shape()+'"-type pipeline');
    switch (Pipeline_shape()) {
      case 'empty':
        return;
      case 'all internal':
        /* Get an array of all generator functions */
        var generators = m_graph.linearize(function(x)x.generator).filter(function(x)x);
        //print(generators.join('\nTHEN\n'));
        // Construct each generator (ie. for 3 generators: g[2](g[1](g[0])))
        return $CS(generators);
      case 'all external':
        var cmd = m_graph.linearize(function(x)x.command).join('|');
        var p = new Process(cmd);
        p.close();
        break;
      case 'to internal':
        var cmd = m_graph.linearize(function(x)x).filter(function(x)x.hasOwnProperty('command')).map(function(x)x.command).join('|');
        var generators = m_graph.linearize(function(x)x.generator).filter(function(x)x);
        function process(){for(var x in new Process(cmd))yield x}
        generators.unshift(process);
        //print(generators.join('\nTHEN\n'));
        // Construct each generator (ie. for 3 generators: g[2](g[1](g[0])))
        return $CS(generators);
      case 'to external':
    }
  }
  
  /* TODO note this style of digraph inheritance isn't actually implemented yet */
  m_graph = new Digraph({
    'validateGraph': Pipeline_validateGraph,
    'realizeGraph':  Pipeline_realizeGraph,
  });

  return this.iface = {
    'shape':        Pipeline_shape,
    'add':          Pipeline_add,
    'run':          Pipeline_run,
  };
}

function ExecAPI_writeToFile(src) {
}
function ExecAPI_appendToFile(src) {
}
var ExecAPI = {
  'print':  function() this(function(src){for each(let x in src)print(x)})(),
  'trim':   function() this(function(src){for each(let x in src)yield x.trim()}),
  'rtrim':  function() this(function(src){for each(let x in src)yield x.match(/(.*)\s*/)[1]}),
  'fwrite': ExecAPI_writeToFile,
  'fappend':ExecAPI_appendToFile,
  /* A function will inherit from ExecAPI, so we'll make it feel as much like a standard function as possible */
  'call':   Function.prototype.call,
  'apply':  Function.prototype.apply,
}

function exec(cmd) {
  var m_pipeline = new Pipeline;
  m_pipeline.add(cmd);

  /* This closure will be our return value. We'll give it some properties
   * and a different __proto__, too.
   */
  function _exec(cmd) {
    if (arguments.length == 0) {
      return m_pipeline.run();
    }
    else {
      m_pipeline.add(cmd);
    }
    return _exec;
  }
  function ExecAPI___iterator__() {
    /* This last stage guarantees that we have an internal stage at the end.
     * TODO check if the last one is internal first */
    m_pipeline.add(function(src){for(let x in src)yield x});
    return m_pipeline.run();
  }
  _exec.__proto__ = ExecAPI;
  _exec._pipeline = m_pipeline;
  
  return _exec;
}

/* exports */
exports.Digraph = Digraph;
exports.Pipeline = Pipeline;
exports.Process = Process;
exports.flines = flines;
exports.p2open = p2open;
exports.exec = exec;
exports.ExecAPI = ExecAPI;
