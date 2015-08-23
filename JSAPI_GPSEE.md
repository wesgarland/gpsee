# Introduction #

GPSEE is designed to interfere as little as possible with JSAPI as possible. 99% of code which runs on unadultered SpiderMonkey should run on GPSEE with few, if any, changes.

# Headers #

  * replace **jsapi.h** with **gpsee-jsapi.h**
  * replace **iconv.h** with **gpsee-iconv.h** (include after gpsee-jsapi.h)

**Note**: If you are wrapping code which you intend to make into a GPSEE module (i.e. you want to augment GPSEE), you should defined GPSEE\_JSAPI\_MODULE before including <tt>gpsee-jsapi.h</tt>. If, on the other hand, you are wrapping an entire program or system (i.e. you are trying to add GPSEE functionality to an existing JS embedding), you should define GPSEE\_JSAPI\_PROGRAM.

# Libraries #

  * replace -lmozjs with -lgpsee

# CFLAGS, CPPFLAGS, LDFLAGS, etc #

  * replace js\_config script with gpsee\_config script

If you are not using js\_config, we suggest removing your ad-hoc Makefile additions and collected the correct flags form gpsee\_config. This insure that your program remains portable to future versions of GPSEE.  For example, to add the correct flags to link in libgpsee.so (or .dylib if you are on Darwin), you might add the following statements to your Makefile:

```
GPSEE_CONFIG ?= /path/to/gpsee_config
myProgram LDFLAGS += $(shell $(GPSEE_CONFIG) --ldflags)
```

# What exactly does GPSEE mess with? #

GPSEE attempts to take certain SpiderMonkey use-once facilities (such as context private storage), and multiplexes it so that multiple modules can make use of these facilities without being aware of one another.

GPSEE accomplishes this by redefining, via the C pre-processor, certain symbols. GPSEE also tries to enforce some coding conventions, including how to start/stop the JS environment, what module interfaces look like, and so forth.

| **facility** | **status** |
|:-------------|:-----------|
| iconv        | iconv.h headers should be replaced with gpsee-iconv.h. GPSEE modules will automatically link with correct iconv library as needed. If your modules use iconv, we suggest following the link pattern in modules/binary/module.mk.|
| Class Declaration | Surround class names with GPSEE\_CLASS\_NAME() in GPSEE modules |
| Script Compilation | JS\_CompileFile() is automatically intercepted so we can cache compiled byte code on disk, in the same directory as the source script. |
| Error handler | GPSEE installs its own error handler by default, but does not intercept calls to replace it |
| JS\_SetContextPrivate<br>JS_GetContextPrivate<table><thead><th> Virtualized with macro; program-wide or module-wide pointer </th></thead><tbody>
<tr><td> JS_SetContextCallback </td><td> Virtualized with macro </td></tr>
<tr><td> JS_GetGlobalObject </td><td> Virtualized with macro, returns either program-wide or module-wide pointer </td></tr></tbody></table>

<h1>Stubs</h1>

Certain things are left as "platform stubs" in GPSEE, to allow easy integration with whatever platform you are embedding JavaScript in.  For example, if you were embedding GPSEE in Apache, you would probably want to use Apache's configuration and logging libraries, rather than something proprietary and orthogonal to Apache.<br>
<br>
<ul><li><b>Configuration</b> - GPSEE uses the <tt>rc</tt> routines in gpsee_unix.h to configure internal parameters (e.g. size of JS stack). The current stubs simply allow GPSEE to use all default values.<br>
</li><li><b>Logging</b> - GPSEE uses gpsee_log and friends in gpsee_unix.h to log low-level errors, as well as the output of the default error reporter. The current stubs wrap syslog.</li></ul>

<h1>Detecting GPSEE from Source Code</h1>

The following C pre-compiler macros are defined if you have include <tt>gpsee.h</tt> or <tt>gpsee-jsapi.h</tt>:<br>
<ul><li>GPSEE_GLOBAL_NAMESPACE_NAME<br>
</li><li>GPSEE_CURRENT_VERSION_STRING<br>
</li><li>GPSEE_MAJOR_VERSION_NUMBER<br>
</li><li>GPSEE_MINOR_VERSION_NUMBER<br>
</li><li>GPSEE_MICRO_VERSION_NUMBER</li></ul>

Additionally, programs or modules which can build with or without GPSEE are encouraged to switch behaviour based on the prescence of -DGPSEE in the CPPFLAGS.<br>
<br>
<h1>FFI Binary Compatibility</h1>

If you have types that you would like to use a pointers with FFI, they need to be ByteThings. Any reasonable JSObject in which the private slot contains a C or C++ type which maintains an array of bytes and a length word can probably be adjusted to work, provided your JSObject does not make use of the JSMarkOp.