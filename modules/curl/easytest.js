
const cURL = require('curl');
const easycurl = cURL.easycurl;
const easycurl_slist = cURL.easycurl_slist;
const ByteArray = require('binary').ByteArray;


var c = new easycurl;

for (var i in c) {
    print("" + i + ": " + c[i]);
}

var blob = new ByteArray();

c.header = function(data) {
    print("HEADER: " + data.trim());
}

c.write = function(data) {
    print("GOT: " + data.size);

    // can convert to a string here since data could be
    //  in middle of variable length encoding

    // convert to ByteArray and append
    blob.concat(ByteArray(data));
}

var headers = new easycurl_slist;

headers.append('foo: bar');
headers.append('ding: bat');
print("headers: " + headers.toArray());
c.setopt(c.CURLOPT_HTTPHEADER, headers);

c.setopt(c.CURLOPT_URL, 'http://www.google.com');

// change this to "1" to show everything
c.setopt(c.CURLOPT_VERBOSE, 0);

c.setopt(c.CURLOPT_HTTPGET, 1);
c.perform();

// Now glue it all together
print('-----------------------------------------')

print(blob.decodeToString('utf8'));

print("done");