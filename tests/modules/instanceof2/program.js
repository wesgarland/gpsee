const a = require("./module").a;
const A = [];
const b = require("./module").b;
const B = eval("[]");

print("inside program");
print(b instanceof Array);
print(B instanceof Array);
print(b instanceof Array);
print(B instanceof Array);

