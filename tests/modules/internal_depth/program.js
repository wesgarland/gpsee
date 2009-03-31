var a = require("reqVM");
var b = require("VM");

print(a.vm===b ? 'PASS' : 'FAIL');
