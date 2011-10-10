const m = require("./module");
var cls;
var ctor;
var instance;

function UserClass()
{
  this.that = 123;
}

for (cls in m)
{
  print("Testing " + cls);

  ctor = m[cls].shift();
  if (ctor !== this[cls])
    print(" * Error: constructors do not match");

  for each (instance in m[cls])
  {
    if (!instance instanceof this[cls])
      print(" * Error: instanceof " + instance.toSource());
  }
}
