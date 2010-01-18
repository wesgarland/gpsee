
10-Jan-2010

This is a thin wrapper around the libcurl "easycurl" interface:

     http://curl.haxx.se/libcurl/c/

Comments Welcome!


TODO:
 * I've add "// TBD:" near code that needs help
 * add GPSEE's error reporting if there is a problem in module load
 * Figure out minimum libcurl supported, may it's just a matter of adding
   around everything
    #ifdef OPTION_NAME
       code using OPTION_NAME
    #endif

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

There are a few callbacks in libcurl.  Instead of using "setopt" you
define "header" and "write" functions IN YOUR EASY CURL OBJECT.

For whatever reason I did this instead:

* create a easycurl_callback object
* add a 'write', and 'header' callback methods to this object

For example:

    //   callbacks must live as long as the request is in use.
    var z = mycurlistance...
    z.blob = "";
    z.header_list = [];
    z.write  = function(s) { this.blob += s; }
    z.header = function(s) { this.header_list.push(s); }

    // this isn't a call back but you'll want to define something like 
    // this to be called at the start of each request.
    z.clear_callbacks  = function(s) { this.blob = "";this.header_list =[];}

enjoy!


