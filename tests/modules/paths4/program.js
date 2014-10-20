var modname = 'test-module-2f2809gh8209hg2';
var mod;
try {
  mod = require(modname)
} catch (e) {
  mod = false;
}
if (mod)
  throw new Error("require() should have failed! have you a module somewhere named \""+modname+"?\"");
require.paths.push(void 0);
require.paths.push('directory');
try
{
  mod = require(modname);
}
catch(e)
{
  print("FAIL: could not load module directory/" + modname);
  print("      (did you forget to invoke from within the paths4 directory?");
}
mod.finishTest();
