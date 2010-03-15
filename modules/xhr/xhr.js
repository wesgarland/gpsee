// XMLHttpRequest
// http://www.w3.org/TR/XMLHttpRequest/

const curlmod = require('curl');

// Allowing it so I can change ByteArray or ByteString as needed
const Binary = require('binary').ByteArray;

// Only two items exported.
var easycurl = curlmod.easycurl;
var easycurl_slist = curlmod.easycurl_slist;

/**
 * @class
 */
var XMLHttpRequest = function() {
    this._readyState = this.UNSENT;
    this._send_flag = false;
    this._error_flag = false;
    this._status_line = null;
    this.headers = [];

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
    z.blobs = [];
    z.header_list = [];
    z.write  = function(s) { print("BLOB: " + s.size); z.blobs.push(s); };
    z.header = function(s) { z.header_list.push(s); }

};

XMLHttpRequest.prototype = {

    // Status codes
    UNSENT            : 0,
    OPENED            : 1,
    HEADERS_RECEIVED  : 2,
    LOADING           : 3,
    DONE              : 4,

    _status_re : /^HTTP.[0-9.]+ +([0-9]+) +(.*)/,
    _charset_re : /charset=(\S*)/,

    // You'll want to define something like this to be called
    // at the start of each request.
    _reset: function(s) {
	this._readyState = this.UNSENT;
	this._send_flag = false;
	this._error_flag = false;
	this._status_line = null;
        this.curl.blobs = [];
        this.curl.header_list = [];
	this.extraheaders = new easycurl_slist();
    },

    _charsetSniffer: function(header, raw) {

	if (header !== null) {
	    var parts = this._charset_re.exec(header);
	    if (parts && parts[1]) {
		return parts[1].toLowerCase();
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
	return 'utf-8';
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
	return Array.prototype.join(this.headers, '\r\n');
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
	    this.method = tmp;
	}

	// step 4
	if (this.method in ['CONNECT', 'TRACE', 'TRACK']) {
	    throw new Error("SECURITY_ERR");
	}

	// step 5 -- remove fragment

	// step 6 -- resolve relative url
	//  Does not apply for server-side XHR
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

	// step 11
	// same origin policy
	//  Does not apply for server XHR

	// step 12
	//  In this implementation async is ignored
	if (typeof async === 'undefined') {
	    this.async = true;
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
	    if (typeof data !== 'undefined' || data !== null ) {
		postdata = data.toString();
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

	// step 6 -- no actual state change
	this.onreadystatechange();

	// step 7
	if (this.async) {
	    // Not implemented here
	}

	// lots of steps here skipped
	this.headers_in = this.curl.header_list;
	this._status_line = this.headers_in.shift();
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

	if (this._send_flag) {
	    throw new Error("DomException.INVALID_STATE_ERR");
	}

	// step 3
	// check for validity of header name

	// step 4
	if (value === null || typeof value === 'undefined') {
	    return null;
	}

	// step 5
	// check for validity of value

	// steps 6,7
	//  setting HTTP headers
	//  all are allowed for server side XHR

	this.extraheaders.append(key + ': ' + value);
    },

    /**
     * not in spec
     * returns a ByteString
     */
    get responseRaw() {
	if (this._readyState != this.DONE) {
	    return null;
	}

	// take all the chunks and concat them
	// this is a bit weird since the curl binary object
	//  IS NOT a "binary/b" ByteArray but a special
	//  binary type unique to curl
	//
	// This is not so great.
	//
	var body = null;
        var parts = this.curl.blobs.length;
        if (parts > 0) {
            body = Binary(this.curl.blobs[0]);
            for (var i=1; i< parts; ++i) {
                body.concat(Binary(this.curl.blobs[i]));
            }
        }
	if (body === null) {
	    return null;
	} else {
	    return body;
	}
    },

    /**
     * Return Javascript UTF8, properly decoded
     *
     */
    get responseText() {
	if (this._readyState != this.DONE) {
	    return null;
	}
	var raw = this.responseRaw;
	if (raw === null) {
	    return '';
	}

	var header = this.getResponseHeader('content-type');
	var charset = this._charsetSniffer(header, raw);
	return raw.decodeToString(charset);
    },

    get responseXML() {
	if (this._readyState != this.DONE) {
	    return null;
	}

	var raw = this.responseRaw;
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
