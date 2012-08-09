var a = require("./reqVM");
var b = require("vm");

print(a.vm===b ? 'PASS' : 'FAIL');
