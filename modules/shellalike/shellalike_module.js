/* GPSEE shellalike module
 *
 * nothing is set in stone, yet.
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
function range(start,stop){for(var i=start;i<stop;i++)yield i}
/* Generator.prototype.toArray() */
(function(){yield})().__proto__.toArray = function(){var a=[];for(var v in this)a.push(v);return a}
/* Zap the new/apply contention with help from eval() */
Function.prototype.new = function(){ return eval('new this('+range(0,arguments.length).toArray().map(function(n)'arguments['+n+']')+')') }
/* Bind an instance method to some 'this' object */
function $P(f,t) {
  if (arguments.length==1) t = this;
  return function() f.apply(t, arguments);
}

ffi.Memory.prototype.intAt = (function() {
  function Memory_intAt(idx) {
    var f = ByteString.prototype.charCodeAt;
    return  f.call(this, idx+0)        |
           (f.call(this, idx+1) << 8)  |
           (f.call(this, idx+2) << 16) |
           (f.call(this, idx+3) << 24) ;
  }
  return Memory_intAt;
})();

/* @jazzdoc shellalike.p2open
 * Mostly intended as an internal function.
 * TODO add finalizer! this will require repairing the missing
 * linked-list functionality in __gpsee_p2open() et al.
 */
const p2open = (function() {
  var FDs = new ffi.Memory(8); // TODO sizeof(int)*2
  //FDs.finalizeWith(_free, FDs);
  function p2open(command) {
    print('p2open() with', command);
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
    while (line = _fgets.call(buf, 4096, src))
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
  print('Process with', command);
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

/* @jazzdoc shallalike.ExecAPI
 * @form exec(command)
 * @form exec(command).exec(command2)
 * @form exec(command).cat(file1...).match(/hdon/)
 * etc.
 * nothing is set in stone, yet
 */
function ExecAPI() {
  /* Call superclass constructor */
  //Process.apply(this, arguments);
  //not yet!
  //this.command = cmd;
}
ExecAPI.prototype = {'__proto__': Process.prototype};

/* @jazzdoc ExecAPI.prototype.initialized
 * The 'initialized' member of an ExecAPI instance is true if the pipeline has
 * begun working. If it has not begun working, the instance inherits a value of
 * false for its 'initialized' property.
 *
 * While an ExecAPI instance inherits from shellalike.Process, it does not call
 * its superclass constructor until the pipeline is read from (by iterating over
 * it.) Then, each of the members of the pipeline are initialized at once, and
 * data begins flowing.
 */
ExecAPI.prototype.initialized = false;
ExecAPI.prototype.initialize = function() {
  if (this.command)
    Process.call(this, this.command);
  else if (this.generator)
    throw new Error("TODO implement ExecAPI generators!");
  else if (this.iterator)
    throw new Error("TODO impelment ExecAPI iterators!");
  else
    throw new Error("This ExecAPI instance appears empty!");

  this.initialize = function(){};
}
ExecAPI.prototype.write = function() {
  this.initialize();
  if (this.write == ExecAPI.prototype.__iterator)
    throw new Error(this+" has no write method");
  return this.write.apply(this, arguments);
}
ExecAPI.prototype.flush = function() {
  this.initialize();
  if (this.flush == ExecAPI.prototype.__iterator)
    throw new Error(this+" has no flush method");
  return this.flush.apply(this, arguments);
}
ExecAPI.prototype.__iterator__ = function() {
  this.initialize();
  if (this.__iterator__ == ExecAPI.prototype.__iterator)
    throw new Error(this+" has no __iterator__ method");
  return this.__iterator__();
}

/* @jazzdoc ExecAPI.splice
 * Chain together any number of shell-like serial pipeline stages. This can only
 * happen to uninitialized ExecAPI instances (see ExecAPI.prototype.initialized)
 * but this might change in the future.
 */
ExecAPI.splice = function(a, b) {
  print('splicing', a);
  print('    with', b);
  if (arguments.length < 2)
    return arguments[0];
  if (arguments.length == 2) {
    /* Link a's source to b's sink */
    a.src = b;
    b.src = a;
    /* Consolidate multiple internal commands */
    return {
      '__proto__':  ExecAPI.prototype,
      'initialize': function() {
        /* Initialize both child ExecAPIs */
        a.initialize();
        b.initialize();
        /* Allow user to write to our sink */
        this.write = $P(a.write,a);
        this.flush = $P(a.flush,a);
        /* Allow user to read from our source */
        this.__iterator__ = $P(b.__iterator__,b);
      },
      'src': b.src,
      'snk': a.snk,
    };
  }
  if (arguments.length > 2) {
    return Array.prototype.reduce.call(arguments, function(a,b)ExecAPI.splice(a,b));
  }
}
ExecAPI.prototype.pipeline = function() {
  var top = this;
  var rval = [];
  while (top.src) {
    print(top.src);
    top = top.src;
  }
  do rval.push(top);
  while (top = top.snk);
  return rval;
}

ExecAPI.extend = function() {
}
ExecAPI.prototype.cat = function() {
  return 'meow!';
}
ExecAPI.prototype.isInternal = function()!this.command;
ExecAPI.prototype.toString = function() {
  if (this.command)
    return '[ExecAPI External Process "'+this.command+'"]';
  else if (this.generator)
    return '[ExecAPI Generator Process "'+this.generator.name+'"]';
  else if (this.functor) {
    return '[ExecAPI Simple Function "'+this.functor.name+'"]';
  }
  else if (this.__iterator__)
    return '[ExecAPI Iterator Process]';
  else
    return '[ExecAPI NULL]';
}
/* @jazzdoc ExecAPI.prototype.exec
 * Short-hand compostion and execution operations.
 *
 * @form instance.exec()
 * Run the pipeline until all stages are exhausted. This is useful for pipelines which
 * have no output, and therefore cannot be executed in the typcal course of consuming
 * the pipeline's output. It's also useful for pipelines which have neither output nor
 * input.
 *
 * @form instance.exec(commandString)
 * @form instance.exec(generatorFunction)
 * @form instance.exec(iteratorObject)
 * Same as ExecAPI.splice(instance, new ExecAPI(argument)) whatever argument may be
 * from the above mentioned forms. This, in tandem with Shellalike.exec() provides 
 * a convenient short-hand form of using the Shellalike module. Here's an example:
 *
 *   exec(generatorFunc).exec("sink-command")
 */
ExecAPI.prototype.exec = function() {
  if (arguments.length == 0) {
    /* Execute an enclosed pipeline */
  }
  print(this+ '.exec('+Array.prototype.join.call(arguments,',')+')');

  /* Construct an argument vector of the form [this, exec(arg0), ... exec(argN)] */
  var args = [this];
  args.forEach(function(a)args.push(exec(a)));
  //args.push.apply(args, Array.prototype.map.call(arguments, function(a)exec(a)));
  /* NOTE the new/apply contention above! if ExecAPI ever accepts more args, we'll have to refactor here! */
  print('splicing', args);
  return ExecAPI.splice.apply(null, args);
}

/* @jazzdoc shellalike.exec
 * This is the most convenient and powerful API for the shellalike module.
 * See ExecAPI.prototype.exec for more information.
 */
function exec(command) {
  var rval = {
    '__proto__': ExecAPI.prototype,
  };
  if ('string' === typeof command) {
    rval.command = command;
  }
  else if ('function' === typeof command) {
    rval.generator = command;
  }
  else if (command.functor) {
    rval.functor = command.functor;
  }
  else if (command.__iterator__) {
    rval.iterator = function()command.__iterator__()
  }
  else throw new Error("Invalid exec() command");
  return rval;
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
            print('%%', m_elements[0].data);
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
            print('%%', el.data);
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
  function shape() {
    var ie = m_graph.linearize(function(x)x.internal?'i':'e').join('');
    if (!m_graph.isLinear())
      throw new Error("Currently only linear pipelines are supported!");

    for each(var [nam,pat] in [
      ['empty',         /^$/],
      ['all internal',  /^i+$]/],
      ['all external',  /^e+$]/],
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
    switch (shape()) {
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

  m_graph = new Digraph({
    'validateGraph': Pipeline_validateGraph,
    'realizeGraph':  Pipeline_realizeGraph,
  });

  return this.iface = {
    'shape':        shape,
    'add':          Pipeline_add,
  };
}




















/* exports */
exports.Digraph = Digraph;
exports.Pipeline = Pipeline;
exports.Process = Process;
exports.flines = flines;
exports.p2open = p2open;
exports.exec = exec;
exports.ExecAPI = ExecAPI;
