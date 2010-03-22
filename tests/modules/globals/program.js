var i = 0;
var k = 2;

const mod = require("./module");

mod.incr(10);

if (i != mod.get())
  print("Error: i != module::get()");

if (i != 11)
  print("Error: i = " + i + "; expected 11");
else
  print("i test passed");

if (typeof j !== "undefined")
  print("Error: var j leaked from module");
else
  print("j test passed");

if (k !== 2)
  print("Error: var k leaked from module");
else
  print("k test passed");
  
print("Note: it is normal to see strict mode errors about j in module.js");
