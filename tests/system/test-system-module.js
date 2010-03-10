#!/usr/bin/gsr -zzdd
const system = require('system')
const {cay} = require('shellalike');
/* What way were we run? */
if (arguments && arguments.length > 1) {
  if (arguments[1] != 'test' || arguments[2] != 'args') {
    print('usage:',arguments[0]);
    throw 1;
  }
  if (system.args[1] != 'test' || system.args[2] != 'args') {
    print("system.args is broken");
    throw 1;
  }
  if (system.args[3] == 'test-stdin') {
    print('meanwhile, in cay subprocess... system.stdin.readln() yields:', system.stdin.readln());
    throw 0;
  } else {
    print("system.args test passed!");
    throw 0;
  }
}
cay('gsr -ddF '+arguments[0]+' test args').rtrim().print();
//print = function(){};
/* Basic tests */
var test = "TEST PASSED";
if (system.env.hasOwnProperty('TEST'))
  test += ', OLD VALUE: "'+system.env.TEST+'"';
system.env.TEST = test;
print('system.env.TEST =', system.env.TEST);
/* This is not specified as part of the CommonJS spec for the system module,
 * but it is a feature of GPSEE :) */
cay('[ x"'+system.env.TEST+'" = x"$TEST" ] && echo cay test PASSED || echo cay test FAILED').rtrim().print();
//system.env['errno==EINVAL'] = 'failure';
/* Enumeration test */
system.env[1] = 'FUN!';
var allvars = [];
for (let k in system.env) allvars.push(k);
print('system.env keys =', allvars.join(', '));
print('system.stdin  =', system.stdin);
print('system.stdout =', system.stdout);
print('system.stderr =', system.stderr);
var p = cay('echo cay+stdin test success!')('gsr -ddF '+arguments[0]+' test args test-stdin').rtrim().print();
