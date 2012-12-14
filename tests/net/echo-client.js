require("reactor").activate(function main() {

var net = require('net');
var client = net.connect({port: 8124},
    function() { //'connect' listener
  print('client connected');
  client.write('world!\r\n');
});
client.on('data', function(data) {
  print(data.toString());
  client.end();
});
client.on('end', function() {
  print('client disconnected');
});

});
