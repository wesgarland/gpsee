const ffi = require("gffi");
const _readline = new ffi.CFunction(ffi.pointer, "readline", ffi.pointer);
const _add_history = new ffi.CFunction(ffi.void, "add_history", ffi.pointer);
 
const commands = 
[
    { label: "quit",            fn: quit,                               help: "Quit the debugger" },
    { label: "break",           fn: require("./breakpoints").add,       help: "Set a breakpoint" },
    { label: "continue",        fn: resume,                             help: "Resume execution" },
    { label: "c",               fn: resume },                
    { label: "step",            fn: step,                               help: "Simple instruction step" },
    { label: "s",               fn: step },                
    { label: "backtrace",       fn: where,                              help: "Show stack dump" },
    { label: "bt",              fn: where },
    { label: "up",              fn: up,                                 help: "Set caller as current frame" },
    { label: "down",            fn: down,                               help: "Set callee as current frame" }, 
    { label: "why",             fn: why,                                help: "Explain reason for stopping program" },
    { label: "display",         fn: require("./display").add,           help: "Evaluate an expression whenever we are in the current frame" },
    { label: "print",           fn: printExpr,                          help: "Evaluate an expression in the current frame and print the result" },
    { label: "eval",            fn: printExpr },
    { label: "call",            fn: callExpr,                           help: "Evaluate an expression in the current frame" },
    { label: "jsval",           fn: jsvalExpr,                          help: "Evaluate an expression in the current frame and print the jsval of the result" },
    { label: "jsdb",            fn: jsdbExpr,                           help: "Run a jsdb command from the gsr UI" },
    { label: "sources",         fn: sources,                            help: "List source files in the debuggee's realm" },
    { label: "args",            fn: args,                               help: "Show function arguments" },
    { label: "locals",          fn: locals,                             help: "Show local variables" },
    { label: "return",          fn: returnVal,                          help: "Force the current frame to return a value, then resume" },
    { label: "throw",           fn: throwVal,                           help: "Force the current frame to throw a value, then resume" },
    { label: "list",            fn: list,                               help: "List source code" },
    { label: "help",            fn: gsrHelp,                            help: "Help summary or details on a specific command" }
// ptype -> JS_GET_CLASS
];

function gsrHelp(args)
{
  if (args.length)
  {
    print("Sorry, no detailed help available for " + args[0]);
    return;
  }

  for (i=0; i < commands.length; i++)
    if (commands[i].help)
      print(commands[i].label + "                  ".slice(commands[i].label.length) + commands[i].help);
}

function argsToExpr(args)
{
  var expr = args.join(" ");

  if (!require("vm").isCompilableUnit(expr))
  {
    print("Error: '" + expr + "' is not a complete expression");
    return null;
  }

  if (!require("./jsapi").checkStatement(expr))
  {
    print("Error: '" + expr + "' is not a compilable expression");
    return null;
  }

  return expr;
}

function printExpr(args)
{
  var expr = argsToExpr(args);
  var result;

  if (expr)
    show(expr);
}

function callExpr(args)
{
  var expr = argsToExpr(args);

  if (expr)
    print("result: " + reval(expr).toSource());
}

function jsdbExpr(args)
{
  var expr = argsToExpr(args);

  if (expr)
    safeEval(expr);
}

function jsvalExpr(args)
{
  var expr = argsToExpr(args);

  if (expr)
    print("result: " + require("vm").jsval(safeEval(expr)));
}

function parseArgv(str)
{
  str = str.replace(/^ /,'');
  str = str.replace(/  *$/,'');
  str = str.replace(/;$/,'');
  return str.split(" ");
}

function findCommand(word)
{
  var i;
  var candidate = null;

  if (!word || !word.length)
    return null;

  for (i=0; i < commands.length; i++)
  {
    if (word == commands[i].label)
      return commands[i];

    if (word == commands[i].label.substr(0, word.length))
    {
      if (candidate !== null)
        return { fn: function() { print(word + " matches more than one command (" + commands[candidate].label + ", " + commands[i].label +")") }};
      candidate = i;
    }
  }

  if (candidate === null)
    return { fn: function() { print("Command not found") }};

  return commands[candidate];
}

var lastInput = "";
exports.userInput = function()
{
  var str;
  var strp;
  var cmd;

  require("./display").showForThisFrame();
  strp = _readline("(gsrdb) ");

  if (!strp)
    str = "";
  else
    str = strp.asString();

  if (!str.length && !lastInput)
    return;
  
  if (!str.length)
    str = lastInput;
  else
    lastInput = str;

  _add_history(strp);

  args = parseArgv(str);

  cmd = findCommand(args[0]);
  cmd.fn(args.slice(1));
};

