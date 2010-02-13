var modname = 'test-module-2f2809gh8209hg2';
var mod;
try {
  mod = require(modname)
} catch (e) {
  mod = false;
  print('require() failed as it should :)');
}
if (mod)
  throw new Error("require() should have failed! have you a module somewhere named \""+modname+"?\"");
require.paths.push('directory');
mod = require(modname);
mod.finishTest();
