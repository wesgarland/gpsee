# GPSEE has moved #
So much for Google Code being more stable than our original home at Sun (Project Kenai)...Google Code is closing Q2 2015.   The new master repository is at http://github.org/wesgarland/gpsee

# What is GPSEE? #

  * a platform for developing and running ServerSide JavaScript [CommonJS](CommonJS.md) programs
  * a general-purpose C API for embedding SpiderMonkey + CommonJS
  * a general-purpose C API for adding interoperability between JSAPI projects
  * licensed under the exact same terms as SpiderMonkey (MPL 1.1, GPLv2, LGPL 2.1)
  * pronounced "_gypsy_"

GPSEE is written and maintained by PageMail, Inc. The primary developers are Wes Garland and Donny Viszneki, however, we are actively looking for contributions from the community! GPSEE is an offshoot of earlier, proprietary research and development at PageMail. That project's primary goal was to create an easy-to-embed JavaScript language interpreter which can be used as glue and configuration logic in large software systems, with interest in the ability to write batch jobs and serve web content.

GPSEE is delivered as one unit, but compromises three main  facets:

  1. **gsr**, the GPSEE Script Runner, which executes CommonJS programs, including via shell-script "shebang" facilities. GPSEE also ships with some sample [REPL](REPL.md)s (shells), including a CommonJS-enabled version of Mozilla's JS Shell.
  1. **GPSEE-core** is a group of facilities which allow embedders to load modules, and to use SpiderMonkey's limited-use facilities (e.g. context private storage) without worrying about stepping on other modules' toes. GPSEE-core is designed to be call-compatible with SpiderMonkey, so that it may be turned on and off with a simple header file change.
  1. **modules** is a directory with "stock" modules, adding functionality to the JavaScript language such as ByteString, ByteArray, POSIX signals, CGI, and FFI (foreign-function interface). The FFI module allows painless wrapping of C functions and datatypes for use in creating new JavaScript modules.

# POSIX #

GPSEE's FFI module is specifically targeted at reflecting POSIX into JavaScript -- in a way which does not require runtime detection of the underlying architecture or other portability hacks.  GPSEE targets the SUSv3 and SVID interface groups.

GPSEE FFI code which uses POSIX functions will run the same on every operating system and CPU, regardless of the underlying implementation. The JavaScript programmer does not need to worry about details like struct layout, pointer sizes, or even whether the interface is defined as a C function or a C pre-processor macro.

# Support #

GPSEE is provided as-is, without warranty of any kind, express or implied, including but not limited to the warranties of merchantability, or fitness for any purpose. We do not guarantee that GPSEE does not infringe upon any patents or copyrights, although we try very hard to insure that it does not.

There is no support for GPSEE. However, you can often find the GPSEE developers on [irc://irc.freenode.org/#gpsee](irc://irc.freenode.org/#gpsee), and we're friendly.

# Status #

GPSEE-0.2 is currently stable for production use on a relatively old version of SpiderMonkey (), yielding approximately JavaScript 1.8.2, which has 99% of ES5 features.

While we have not committed any significant feature upgrades in a while, we are using GPSEE for internal development and are pushing bugfixes to Google Code as the bugs are discovered and fixed.

# Road Map #
We have attempted an upgrade to JS 1.8.5 in the past with GPSEE-0.3, however that particular fork is semi-abandoned now. The Mozilla folks have made significant API changes which made our goal of maintaining source-code compatibility for GPSEE C add-ons very very difficult, and made our other goal of SpiderMonkey-version agnosticism virtually impossible.

A newer version of GPSEE is expected at some point in the future that will leap-frog all the interm changes Mozilla have made to SpiderMonkey, catching up to some version of Enterprise (LTS) Firefox.  This will be GPSEE-0.4, and it will be break source-code compatibility of GPSEE C modules.   We hope to keep JavaScript programs modules 100% backwards compatible, including any use of gffi (the FFI module).

We also plan to break out many optional GPSEE modules (e.g. curses, curl) from the base distribution, and introduce a formal way to build/install/distribute modules for GPSEE.  This may land in either GPSEE-0.2 or GPSEE-0.4.

Once external modules are available, we will also develop technology to allow external modules to inject knowledge about new structs into the gffi module. This will make it possible to add support for shared libraries whose API use struct pointers without also modifying modules/gffi/structs.decl and re-compiling GPSEE.

# Hot Links #

  * Building GPSEE from Sources: [Building](Building.md)
  * Running/Writing CommonJS programs: [GSR](GSR.md), [CommonJS](CommonJS.md), [Modules](Modules.md)
  * Embedding GPSEE into other applications: [EmbeddingGPSEE](EmbeddingGPSEE.md)
  * Porting from other SpiderMonkey embeddings, see [PortingToGPSEE](PortingToGPSEE.md).