//  Implement an echo server in JS
//
//  Nearly identical to node.js's sample echo server, except
//  we currently do not support encodings in net.js, and the
//  last line of this script must kick off a reactor (for now).
//

var tcp = require("net");
var server = tcp.createServer(function (socket) {
//  socket.setEncoding("utf8");
  socket.addListener("connect", function () {
    socket.write("hello\r\n");
  });
  socket.addListener("data", function (data) {
    socket.write(data);
  });
  socket.addListener("end", function () {
    socket.write("goodbye\r\n");
    socket.close();
  });
});
server.listen(7000);


tcp.reactor([server]);

