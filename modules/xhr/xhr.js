/**
 * xhr.js -- XMLHttpRequest in javascript
 *
 *  Basic XHR for
 *   - gpsee  http://code.google.com/p/gpsee/
 *   - libcurl http://curl.haxx.se/
 *
 * SPECS:
 *   - XMLHttpRequest V1
 *     http://www.w3.org/TR/XMLHttpRequest/
 *
 *   - XMLHttpRequest V2
 *     http://www.w3.org/TR/XMLHttpRequest2/
 *
 * IMPORTANT:
 *  - At the moment, this mostly follows the v1 spec.
 *  - Does not support XML/HTML parsing
 *  - Does not support Events (from V2 spec)
 *  - This is SYNCHRONOUS, as async requires a reactor
 *
 * Change Log:
 * 04-APR-2010:
 *  - Support username/password in http
 *  - Correctly handle headers when redirects, 100s, or auth occurs
 *  - Change reponseBody to responseRaw as per XHRv2
 *
 * Copyright 2010 Nick Galbreath
 * MIT LICENCE
 * http://www.opensource.org/licenses/mit-license.php
 *
 */
const curlmod = require('curl');

const ByteArray = require('binary').ByteArray;

// Only two items exported.
const easycurl = curlmod.easycurl;
const easycurl_slist = curlmod.easycurl_slist;

/**
 * @class
 */
var XMLHttpRequest = function() {
    this._readyState = this.UNSENT;
    this._send_flag = false;
    this._error_flag = false;
    this._status_line = null;
    this._method = null;
    this.headers_in = [];

    var z = this.curl = new easycurl;
    // CURL SETUP


    // COOKIES TBD
    // set where cookies should be stored
    // http://curl.haxx.se/libcurl/c/curl_easy_setopt.html#CURLOPTCOOKIE
    z.setopt(z.CURLOPT_COOKIEFILE, '');

    // 0 or 1 for debuggig output
    z.setopt(z.CURLOPT_VERBOSE, 1);

    // on redirect, follow it
    z.setopt(z.CURLOPT_FOLLOWLOCATION, 1);

    // but don't follow too may times.
    z.setopt(z.CURLOPT_MAXREDIRS, 10);

    // and update the referrer http header automatically
    z.setopt(z.CURLOPT_AUTOREFERER, 1);

    // SSL: don't bother with peer verification
    z.setopt(z.CURLOPT_SSL_VERIFYPEER, 0);

    // CALLBACKS FOR READS AND HEADERS
    z.blob = [];
    z.header_list = [];
    z.write  = function(s) { z.blob.concat(ByteArray(s)); };
    z.header = function(s) { z.header_list.push(s); }

};

XMLHttpRequest.prototype = {

    // Status codes
    UNSENT            : 0,
    OPENED            : 1,
    HEADERS_RECEIVED  : 2,
    LOADING           : 3,
    DONE              : 4,

    _status_re : /^HTTP.[0-9.]+ +([0-9]+) +(.*)/ ,
    _charset_re : /charset=([-\w]*)/,

    // You'll want to define something like this to be called
    // at the start of each request.
    _reset: function(s) {
        this._readyState = this.UNSENT;
        this._send_flag = false;
        this._error_flag = false;
        this._status_line = null;
        this.curl.blob = new ByteArray();
        this.curl.header_list = [];
        this.extraheaders = new easycurl_slist();
        this.headers_in.length = 0;
    },

    /**
     * Hook for doing content decoding (eg zip, gzip)
     */
    _contentDecoder: function(header, raw) {
        // this implementation does nothing
        return raw;
    },

    /**
     * Hook for doing charset decoding (utf8, latin1, etc)
     *
     * The API here isn't quite right
     */
    _charsetSniffer: function(header, raw) {

        if (header !== null) {
            var parts = this._charset_re.exec(header);
            if (parts && parts[1]) {
                return parts[1].toUpperCase();
            }
        }

        // if mimetype is html-list
        // take a slice of the first 1024 bytes
        // look for charset=
        // the exact spec is much more complicates but this
        // will do
        /*
          var idx = indexOf('charset=');
          if (idx != 0) {


          }
        */
        // Still nothing?  Look at byte order marks

        // http://www.w3.org/TR/xml/#sec-guessing
        if (raw.size >= 2 && raw[0] == 0xfe && raw[1] == 0xff) {
            return "UTF-16BE";
        }
        if (raw.size >= 2 && raw[0] == 0xff && raw[1] == 0xfe) {
            return "UTF-16LE";
        }
        if (raw.size >=3 && raw[0] == 0xEF && raw[1] == 0xBB && raw[2] == 0xbf) {
            return "UTF-8";
        }
        // when in doubt try this
        return 'UTF-8';
    },

    /**
     * Getter
     */
    get readyState() {
        return this._readyState;
    },

    /**
     */
    abort: function() {
        this._error_flag = true;

        if ( ! ((this._readyState === this.UNSENT ||
                 this._readyState === this.OPENED ) && (! this._send_flag))) {
            this._readyState = this.DONE;
            this._send_flag = false;
            this.onreadystatechange();
        }
        this._readyState = this.UNSENT;
    },

    /**
     */
    getAllResponseHeaders: function() {
        if (this._readyState < this.LOADING) {
            throw new Error("INVALID_STATE_ERR");
        }

        if (this._error_flag) {
            return '';
        }

        // do it lazy and not save result.  Unlike to be called
        // twice (or even once!)
	return this.headers_in.join('\r\n');
    },

    /**
     */
    getResponseHeader: function(header) {
        // step 1
        if (this._readyState < this.LOADING) {
            throw new Error("INVALID_STATE_ERR");
        }

        // step 2
        // validate syntax

        // step 3
        if (this._error_flag) {
            return null;
        }

        // steps 4,5
        var vals = []
        var name = header.toLowerCase();
        for (var i = 0; i < this.headers_in.length; ++i) {
            var s = this.headers_in[i];
            var pos = s.indexOf(':');
            if (pos > 0) {
                if (s.slice(0,pos).toLowerCase() == name) {
                    vals.push(s.substr(pos+1).trim());
                }
            }
        }
        if (vals.length > 0) {
            return vals.join(', ');
        }

        // step 6
        return null;
    },

    /**
     * Method is "GET" / "POST"
     * URL is fully qualified URL
     * ASYNC is currently ignored
     *
     * user and password are optional (and currently ignored)
     */
    open: function(method, url, async, user, password) {
        // step 1
        this._method = method;

        // step 2
        // some validation of format

        // step 3 -- normalize upper case
        var tmp = method.toUpperCase();
        if (tmp in ['CONNECT', 'DELETE', 'GET', 'HEAD', 'OPTIONS', 'POST',
                    'PUT', 'TRACE', 'TRACK']) {
            this._method = tmp;
        }

        // step 4
        if (this._method in ['CONNECT', 'TRACE', 'TRACK']) {
            throw new Error("SECURITY_ERR");
        }

        // step 5 -- remove fragment

        // step 6 -- resolve relative url
        //  Does not apply for server-side XHR

        print("URL = " + url);
        this.curl.setopt(this.curl.CURLOPT_URL, url);

        if (false) {
            throw new Error("DOMException.SYNTAX_ERR");
        }

        // step 7 -- unsupported scheme
        if (false) {
            throw new Error("DOMException.NOT_SUPPORTED_ERR");
        }

        // steps 8,9,10
        // dealing with user passwords
        //  Should set libcurl stuff
        if (user) {
            this.curl.opt(this.curl.CURLOPT_USERNAME, user);
        }
        if (password) {
            this.curl.opt(this.curl.CURLOPT_PASSWORD, password);
        }


        // step 11
        // same origin policy
        //  Does not apply for server XHR

        // step 12
        //  In this implementation async is ignored
        if (async === undefined) {
            this.async = false;
        }

        // step 13
        // user stuff

        // step 14
        // user stuff

        // step 15
        // user stuff

        // step 16
        // password

        // step 17
        // password

        // step 18
        // password

        // step 19
        this._reset()

        // step 20
        // cancel any existing network activity

        // step 21
        this._readyState = this.OPENED;
        this._send_flag = false;
        this.onreadystatechange();
    },

    /**
     */
    send: function(data) {
        // step 1
        if (this.readyState != this.OPENED) {
            throw new Error("DOMException.INVALID_STATE_ERR");
        }

        // step 2
        if (this._send_flag) {
            throw new Error("(DOMException.INVALID_STATE_ERR");
        }

        if (this.async) {
            this._send_flag = true;
        }

        // Step 4
        var postdata = '';
        if (this._method !== 'GET') {
            if (typeof data !== 'undefined' && data !== null ) {
                postdata = '' + data;
            }
        }
        this.curl.setopt(this.curl.CURLOPT_HTTPHEADER, this.extraheaders);
        // reset header and body buffers
        this._reset();

        if (this._method === 'GET') {
            this.curl.setopt(this.curl.CURLOPT_HTTPGET, 1);
            this.curl.setopt(this.curl.CURLOPT_POST, 0);
        } else if (this._method === 'POST') {
            this.curl.setopt(this.curl.CURLOPT_HTTPGET, 0);
            this.curl.setopt(this.curl.CURLOPT_POST, 1);
            this.curl.setopt(this.curl.CURLOPT_COPYPOSTFIELDS, postdata);
        } else {
            throw new Error("DOMException.NOT_SUPPORTED_ERR");
        }
        // DO IT
        this.curl.perform();

        // When curl is configured to follow redirects and to perform
        // auth, and follow htp code 100 the final headers list
        // contains the headers for ALL intermediate steps.
        //
        var imax =  this.curl.header_list.length;
        for (var i = 0; i < imax; ++i) {
            var ahead = this.curl.header_list[i];
            if (this._status_re.exec(ahead)) {
                // header is a status line "HTTP/1.1 302..."
                // delete all previously seen headers

                this._status_line = ahead;
                this.headers_in.length = 0;
            } else {
                // else copy
                this.headers_in.push(ahead);
            }
        }

        // step 6 -- no actual state change
        this.onreadystatechange();

        // step 7
        if (this.async) {
            // Not implemented here
        }

        // lots of steps here skipped

        this._readyState = this.HEADERS_RECEIVED;
        this.onreadystatechange();

        this._readyState = this.LOADING;
        this.onreadystatechange();

        // Step 9
        this._readyState = this.DONE;
        this.onreadystatechange();
    },

    /**
     */
    setRequestHeader: function(key, value) {

        // step 1
        if (this._readyState != this.OPENED) {
            throw new Error("DomException.INVALID_STATE_ERR");
        }

        // step 2
        if (this._send_flag) {
            throw new Error("DomException.INVALID_STATE_ERR");
        }

        // step 3
        // check for validity of header name

        // step 4
        if (value === null || value === undefined) {
            return;
        }

        // step 5
        // check for validity of value

        // steps 6,7
        //  setting HTTP headers
        //  all are allowed for server side XHR

        this.extraheaders.append(key + ': ' + value);
    },

    /**
     * XHRv2
     * returns a ByteString
     */
    get responseBody() {
        if (this._readyState != this.DONE) {
            return null;
        }

        // Hook for content decoding to make it easier instead
        // over-riding 'getters'.  This is for hooking in gzip
        // or deflate/uncompress
        var header = this.getResponseHeader('content-encoding');
        return  this._contentDecoder(header, this.curl.blob);
    },

    /**
     * Return Javascript UTF8, properly decoded
     *
     */
    get responseText() {
        var raw; // raw bytes from network
        var header; // header storage
        var charset; // guess of charset encoding

        if (this._readyState != this.DONE) {
            return null;
        }
        raw = this.responseBody;
        if (raw === null) {
            return '';
        }

        // HACK ALERT, this whole thing should be an API

        header = this.getResponseHeader('content-type');
        charset = this._charsetSniffer(header, raw);

        try {
            return raw.decodeToString(charset);
        } catch(e) {
            if (charset ==  'UTF-8' || charset == 'UTF8') {
                // LOTS of files have content type, meta tag, bom or
                //  anything to indicate charset the default is to try
                //  utf8, but frequenty that will fail. In N. America,
                //  try latin1.  There are algorithm to detect asian
                //  charset too.  See the firefox source code.

                return raw.decodeToString('iso-8859-1');
            }

            // if we already tried latin1, then just explode
            throw e;
        }
    },

    get responseXML() {
        if (this._readyState != this.DONE) {
            return null;
        }

        var raw = this.responseBody;
        if (raw === null) {
            return '';
        }
        var header = this.getResponseHeader('content-type');
        var charset = this._charsetSniffer(header, raw);
        return raw.decodeToString(charset);

        // ACTUALLY DO NOT return a DOCUMENT
        // find content-type
        // if exsists and not text/xml, application/xml or ends in +xml
        // return null
    },

    /**
     * @type long
     */
    get status() {
        if (this._status_line === null) {
            throw new Error("DomException.INVALID_STATE_ERR");
        }

        var parts = this._status_re.exec(this._status_line);
        return parseInt(parts[1]);
    },

    get statusText() {
        if (this._status_line === null) {
            throw new Error("DOMException.INVALID_STATE_ERR");
        }
        var parts = this._status_re.exec(this._status_line);
        return parts[2];
    },

    /**
     */
    onreadystatechange: function() {
        // NOP
        // user defines and overwrite with own function
    },
};

exports.XMLHttpRequest = XMLHttpRequest;
