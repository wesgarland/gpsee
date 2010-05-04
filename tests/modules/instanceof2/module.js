var a = [];
var b = eval("[]");

print("inside module");
print(a instanceof Array);
print(b instanceof Array);

exports.a = a;
exports.b = b;
