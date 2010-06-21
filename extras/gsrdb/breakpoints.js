var breakpoints = [];
function Breakpoint_function(arg)
{
  reval('native_break(' + arg + ')');
  print(" * Added breakpoint for native function '" + arg + "'");
}

function Breakpoint_location(arg)
{
  var a = arg.split(":");
  
  if (a.length > 2)
    throw new Error("Invalid argument");

  if (a.length == 2)
  {
    this.lineno = parseInt(a[1],10);
    
    if (a[0].length == 0)
      this.filename = stack[currentFrame].script.url;
    else
    {
      if (require("fs-base").exists(a[0]))
        this.filename = require("./fs").canonical(a[0]);
      else
      {
        print("Could not locate file '" + a[0] + "'" + syserr()); 
        return;
      }
    }
  }
  else
  {
    if (typeof(a[0] === "number"))
    {
      this.filename = stack[currentFrame].script.url;
      this.lineno = parseInt(a[0],10);
    }
    else
      this.functionName = a[0];
  }

  if ((typeof(this.lineno) !== "number") || isNaN(this.lineno))
  {
    print("Invalid line number: " + this.lineno + "(" + typeof this.lineno + ")");
    return;
  }

  addBreakpoint(this.filename, this.lineno);
  print(" * added breakpoint: " + this.toSource());
}

function Breakpoint(arg)
{
  earg = revalToValue(arg);

  if (typeof(earg) == "object")
    return Breakpoint_function(arg);
  else
    return Breakpoint_location(arg);
}

exports.add = function (args)
{
  if (!args || (!args[0] && args[0] !== 0))
  {
    print("Usage: breakpoint <file:line>, breakpoint <line>");
  }
  else
    breakpoints.push(new Breakpoint(args[0]));
}

