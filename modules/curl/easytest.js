
const cURL = require('curl');
const ByteArray = require('binary').ByteArray;

var c = new cURL.Easy;

var blob = new ByteArray();

c.options.writefunction = function(data) {
    print("GOT: " + data.size);

    // can convert to a string here since data could be
    //  in middle of variable length encoding

    // convert to ByteArray and append
    blob.concat(ByteArray(data));
}


c.options.url = 'http://www.google.com';
c.options.verbose = 1;
c.perform();

// Now glue it all together

print(blob.decodeToString('utf8'));
