//  Implement an echo server in JS
//
//  Nearly identical to node.js's sample echo server, except
//  we currently do not support encodings in net.js, and the
//  last line of this script must kick off a reactor (for now).
//

require("reactor").activate(function main() {

var net = require('net');
var server = net.createServer(function echo_serverConnectionHandler(c) { //'connection' listener
  print('server connected');
  c.on('end', function echo_clientEndHandler() {
    print('server disconnected');
  });
  c.write('hello\r\n');
  c.pipe(c);
});
server.listen(8124, function echo_serverCreateHandler() { //'listening' listener
  print('server bound');
});

});
