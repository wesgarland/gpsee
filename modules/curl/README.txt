
10-Jan-2010

This is a thin wrapper around the libcurl "easycurl" interface:

     http://curl.haxx.se/libcurl/c/

Comments Welcome!


TODO:
 * I've add "// TBD:" near code that needs help
 * add GPSEE's error reporting if there is a problem in module load
 * Use GPSEE ByteArray/String/ByteThing for the body of the request
   WARNING: Right now, all data is converted to ASCII in order not
     to blow up the UTF8 conversion in the js engine.
 * Figure out minimum libcurl supported, may it's just a matter of adding
   around everything
    #ifdef OPTION_NAME
       code using OPTION_NAME
    #endif
 * Anyway to prevent this type of bug?
   var cb = new easycurl_cb; /* YES */
   var cb = easycurl_cb();   /* NO */

EXAMPLE CODE:

  See modules/curl/http.js  for a sample HTTP get/poster.

DESIGN NOTES:

SETOPT
http://curl.haxx.se/libcurl/c/curl_easy_setopt.html

CURLcode curl_easy_setopt(CURL *handle, CURLoption option, parameter);

"CURLoption option" is an integer, to select which option is used. The
valid values are defied by preprocessor #defines, and the value are
not consquetive and create by bit-or-ing other #defines: it's not easy
by inspect to get the list of valid options.

The third parameter is the value for the option -- it's just a blind
void* pointer.  It could be a integer, a function, a char* string, or
a double value depending on the option.

That's great for a C programmer but a bit tricky for making a
javascript API.  To aid in property generation a small python program
looks at a <curl/curl.h> file and pulls out all the appropriate
defines, make a JSClass and a switch statement to determine the type
(e.g. int, double, string).

GETINFO:

the getinfo function is simpler version of setopt.  The constants here
are defined in libcurl_curlinfo.h

SLISTS:

slists are simple linked lists used for passing multiple values into
setopt.  This is reflected in javascript.  There is only one function
'append'. There is no support for iteration or reading individual
items.  Given the use cases this should not be much of an issue.

    var extraheaders = new easycurl_slist();
    extraheaders.append('Accept: text/html,....');
    extraheaders.append('Accept-Language: en-us,en;q=0.5');
    extraheaders.append('Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7');
    mycurl.setopt(mycurl.CURLOPT_HTTPHEADER, this.extraheaders);

Not that glamourous, but any high level code could convert an array to
 slist in one line.

CALLBACKS

There are a few callbacks in libcurl.   Ideally you would just pass a
JS function to setopt and it would Just Work.

For whatever reason I did this instead:

* create a easycurl_callback object
* add a 'write', and 'header' callback methods to this object

For example:

    //   callbacks must live as long as the request is in use.
    var z = mycurlistance...

    // important: make sure to use 'new'
    this.callbacks = new easycurl_callback();
    this.callbacks.blob = "";
    this.callbacks.header_list = [];
    this.callbacks.write  = function(s) { this.blob += s; }
    this.callbacks.header = function(s) { this.header_list.push(s); }
    z.setopt(z.CURLOPT_WRITEFUNCTION, this.callbacks);
    z.setopt(z.CURLOPT_HEADERFUNCTION, this.callbacks);

    // this isn't a call back but you'll want to define something like 
    // this to be called at the start of each request.
    this.callbacks.reset  = function(s) { this.blob = "";this.header_list =[];}

While a bit screwy, the C code is now very simple since the GC and scope
issues are handled by the calling code, not the C code.

enjoy!


