//
// Example code for curl module -- a simple highlevel http client
//   see end for how to use
//

const curlmod = require('curl');
const ByteArray = require('binary').ByteArray;

// Only two items exported.
const easycurl = curlmod.easycurl;
const easycurl_slist = curlmod.easycurl_slist;


var http = function()
{
    this.curl = new easycurl();

    this.body = null;
    this.headers = null;

    var z = this.curl;

    /* Basic options */

    // set where cookies should be stored
    // http://curl.haxx.se/libcurl/c/curl_easy_setopt.html#CURLOPTCOOKIE
    this.curl.setopt(z.CURLOPT_COOKIEFILE, '');

    // on redirect, follow it
    z.setopt(z.CURLOPT_FOLLOWLOCATION, 1);
    // but don't follow too may times.
    z.setopt(z.CURLOPT_MAXREDIRS, 10);
    // and update the referrer http header automatically
    z.setopt(z.CURLOPT_AUTOREFERER, 1);

    // SSL: don't bother with peer verification
    z.setopt(z.CURLOPT_SSL_VERIFYPEER, 0);
    // CALLBACKS FOR READS AND HEADERS
    z._blobs = [];
    z._header_list = [];
    z.write  = function(s) {
        //print("GOT CHUNK: ");
        z.blobs.push(ByteArray(s));
    }
    z.header = function(s) {
        //print("GOT : " + s);
        z.header_list.push(s);
    }

    // 0 or 1 for debuggig output
    z.setopt(z.CURLOPT_VERBOSE, 1);
    z.debug = function(itype, data) {
        switch (itype) {
        case 0:
            // informational messages
            print(ByteArray(data).decodeToString('ascii').trim());
            break;
        case 1:
            // response headers
            break;
        case 2:
            // request headers
            //print(ByteArray(data).decodeToString('ascii').trim());
            break;
        case 3:
            // data in?
            break;
        case 4:
            // data out?
            break
        default:
            print("UNKNOWN");
        }
    }

    // You'll want to define
    // something like this to be called at the start of
    // each request.
    this.reset  = function(s) {
        this.curl.blobs = [];
        this.curl.header_list =[];
    }

    /*
     * Optional extra HTTP headers to simulate a "real browser"
     *
     */

    // these are built-in curl options
    z.setopt(z.CURLOPT_USERAGENT, 'Mozilla/5.0 (Macintosh; U; Intel Mac OS X 10.5; en-US; rv:1.9.0.3) Gecko/2008092414 Firefox/3.0.3');
    z.setopt(z.CURLOPT_ENCODING, 'gzip,deflate');

    // these are "extras"...
    this.extraheaders = new easycurl_slist();
    this.extraheaders.append('Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8');
    this.extraheaders.append('Accept-Language: en-us,en;q=0.5');
    this.extraheaders.append('Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7');
    z.setopt(z.CURLOPT_HTTPHEADER, this.extraheaders);

};


http.prototype = {
    get: function(url, referrer)  {
        var z = this.curl;
        z.setopt(z.CURLOPT_HTTPGET, 1);
        z.setopt(z.CURLOPT_POST, 0);
        return this._setup(url, referrer);
    },

    /**
     * post data must be formatted appropriated into one string
     */
    post: function(url, referrer, post) {
        var z = this.curl;
        z.setopt(z.CURLOPT_HTTPGET, 0);
        z.setopt(z.CURLOPT_POST, 1);
        z.setopt(z.CURLOPT_COPYPOSTFIELDS, post);
        return this._setup(url, referrer);
    },

    _setup: function(url, referrer) {
        var z = this.curl;
        z.setopt(z.CURLOPT_URL, url);
        z.setopt(z.CURLOPT_REFERER, referrer || "");
        this.reset();
        z.perform();

        // code http 100 is a bit funny
        // http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html

        // maybe use  z.getinfo(z.CURLINFO_RESPONSE_CODE)
        // instead of the regexp
        var code = 100;
        var i = 0;
        while (code === 100) {
            var status = this.curl.header_list[i].trim();
            if (status.length > 0) {
                var parts = /^HTTP\/[0-9.]+ ([0-9]+)/(status);
                code = parseInt(parts[1]);
            }
            ++i; // future: break if i > XXX?
        }

        var parts = this.curl.blobs.length;
        if (parts == 0) {
            this.body = null;
        } else {
            this.body =this.curl.blobs[0];
            for (var i=1; i< parts; ++i) {
                this.body.concat(this.curl.blobs[i]);
            }
        }
        this.headers = this.curl.header_list;

        return {status:code, headers:this.headers,body:this.body}
    },

    /*
     * Sample to show off the easycurl.getinfo method
     */
    info: function() {
        var z= this.curl;

        print("" +  z.getinfo(z.CURLINFO_SIZE_DOWNLOAD) +
              " bytes downloaded in " +
              + z.getinfo(z.CURLINFO_TOTAL_TIME) + " seconds");
    }
};

try {
    var h = new http;
    var code = h.get('http://www.google.com/');

    print("DONE");
    if (0) {
        for (var i = 0; i < h.headers.length; ++i) {
            print(h.headers[i].trim());
        }

        // charset algorithm for html is quite complicated
        var s = h.body.decodeToString('utf8');

        print(s);
        print("LENGTH: " + s.length);
        h.info();
    }
} catch (e) {
    for (var i in e) {
        print(i + ': ' + e[i]);
    }
}

