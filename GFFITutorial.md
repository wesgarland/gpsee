# Introduction #

GPSEE's gffi module allows JavaScript programmers to access C data types easily and portable. This allows rapid development of new JavaScript modules which wrap or import C library code.

# Basic Use #

Calling C functions from the gffi module is very straightforward.  GPSEE's FFI tries to do "the right thing" wherever possible -- automatically turning JavaScript Strings into C-style char **strings, using ByteThings as C pointers where necessary, casting JavaScript numbers to the proper C number when needed, turning JavaScript's null into a NULL pointer, and so on.**

We first start by declaring the C function we want to use, then we can invoke it with the declarator's call method as often as we like. During the declaration, we use type indicators to describe the parameters neededd to invoke the C function.

In our first example, we will write a simple program to print the current date and time, using the UNIX ctime and time functions.   These functions have the following prototypse:
```
char *ctime(const time_t *clock);
time_t time(time_t *tloc);
```

ctime uses thread-local storage for its return buffer, so we don't need to worry about freeing the memory from JavaScript when we are done with it.

The ffi.CFunction constructor is variadic; the first parameter is the return type indicator, the second paramter is the name of the function, subsequent parameters are argument type indicators.

```
const ffi    = require("gffi");
const _ctime = new ffi.CFunction(ffi.pointer, "ctime", ffi.pointer);
const _time  = new ffi.CFunction(ffi.time_t, "time", ffi.pointer);

function getTime()
{
  var now = new Memory(ffi.sizeOf[ffi.time_t]);  // allocate heap for time_t pointer
  var ret;

  ret = _time.call(now);                         // Populate now with the current time
  if (ret == -1)
    throw("failed to get current time from OS");

  ret = _ctime.call(now);                       // Get a char * with the formatted time, as an instance of Memory (or null)
  if (ret == null)
    throw("failed to format current time as a string");

  return ret.asString();                        // Convert the returned Memory instance to a JS String with the CString conversion rules
}
```

# Type Indicators #

# External Libraries #

# Casting #

# CStrings vs. JavaScript Strings #

# Built-In Types #
## Memory ##
## MutableStruct ##
## ImmutableStruct ##

# Finalization #

# Special Properties and Methods #
ffi.pointerSize: indicates the pointer size in bytes on the current platform
ffi.sizeOf(): An array, indexed by type indicator, with the size of those types in bytes

# Calling into JSAPI #