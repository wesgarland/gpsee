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
  this.__iterator__ = function()flines(src).__iterator__();

  /* @jazzdoc shellalike.Process.write
   * @form (instance of Process).write(string)
   * Writes the contents of string to the process's stdin stream.
   */
  this.write = function(s) {
    if (!(s instanceof ByteString)) {
      if ('string' == typeof s)
        s = new ByteString(String(s));
    }
    print(s.toSource());
    if (s instanceof ByteString)
      return _fwrite.call(s, 1, s.length, snk);
    throw new Error('argument must be ByteString or String');
  }
  /* @jazzdoc shellalike.Process.flush
   * @form (instance of Process).flush()
   * Flushes our output into the Process's stdin.
   */
  this.flush = function() {
    return _fflush.call(snk);
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
ExecAPI.splice = function(snk, src) {
  print('splicing', snk);
  print('    with', src);
  if (arguments.length < 2)
    return arguments[0];
  if (arguments.length == 2) {
    /* Consolidate multiple external commands */
    if (snk.command && src.command) {
      var rval = exec(snk.command + "|" + src.command);
      return rval;
    }

    /* Consolidate multiple internal commands */
    return {
      '__proto__':  ExecAPI.prototype,
      'initialize': function() {
        snk.initialize();
        src.initialize();
        this.write = $P(snk.write,snk);
        this.__iterator__ = $P(src.__iterator__,src);
      }
    };
  }
  if (arguments.length > 2) {
    return Array.prototype.reduce.call(arguments, function(a,b)ExecAPI.splice(a,b));
  }
}
ExecAPI.extend = function() {
}
ExecAPI.prototype.cat = function() {
  return 'meow!';
}
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
  if (arguments.length == 0)
    throw new Error("TODO: Implement .exec() pipeline executor!");
  print(this+ '.exec('+Array.prototype.join.call(arguments,',')+')');

  /* Construct an argument vector of the form [this, exec(arg0), ... exec(argN)] */
  var args = [this];
  args.push.apply(args, Array.prototype.map.call(arguments, function(a)exec(a)));
  /* NOTE the new/apply contention above! if ExecAPI ever accepts more args, we'll have to refactor here! */
  print('@@splicing', args);
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

/* exports */
exports.Process = Process;
exports.flines = flines;
exports.p2open = p2open;
exports.exec = exec;
exports.ExecAPI = ExecAPI;
