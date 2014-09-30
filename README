Thanks for downloading GPSEE!  This is version 0.2.1.

Please visit us on the web at http://code.google.com/p/gpsee/
for the most recent documentation and build instructions.

The most recent version of SpiderMonkey which has been tested and is 
known to work with GPSEE is revision 4498af260a06 from the Mozilla-1.9.2 
(FireFox 3.6) tree.

The GPSEE-0.2 will not work with post-fatvals JSAPI. GPSEE-0.3 is in
development now and will support JS-1.8.5.

To select a specific version of SpiderMonkey, enter the repository directory
and use the following command: hg revert --rev=XXX --all

What is GPSEE?
=========================================================================

     * a platform for developing and running CommonJS programs
     * a general-purpose C API for embedding SpiderMonkey + CommonJS
     * a general-purpose C API for adding interoperability between JSAPI
       projects
     * licensed under the exact same terms as SpiderMonkey
     * pronounced "gypsy"

   GPSEE is written and maintained by PageMail, Inc. The primary
   developers are Wes Garland and Donny Viszneki, however, we are actively
   looking for contributions from the community! GPSEE is an offshoot of
   earlier, proprietary research and development at PageMail. That
   project's primary goal was to create an easy-to-embed JavaScript
   language interpreter which can be used as glue and configuration logic
   in large software systems, with interest in the ability to write batch
   jobs and serve web content.

   GPSEE is delivered as one unit, but compromises three main facets:
    1. gsr, the GPSEE Script Runner, which executes CommonJS programs,
       including via shell-script "shebang" facilities. GPSEE also ships
       with some sample REPLs (shells), including a CommonJS-enabled
       version of Mozilla's JS Shell.
    2. GPSEE-core is a group of facilities which allow embedders to load
       modules, and to use SpiderMonkey's limited-use facilities (e.g.
       context private storage) without worrying about stepping on other
       modules' toes. GPSEE-core is designed to be call-compatible with
       SpiderMonkey, so that it may be turned on and off with a simple
       header file change.
    3. modules is a directory with "stock" modules, adding functionality
       to the JavaScript language such as ByteString, ByteArray, POSIX
       signals, CGI, and FFI (foreign-function interface). The FFI module
       allows painless wrapping of C functions and datatypes for use in
       creating new JavaScript modules.



Support
=========================================================================

GPSEE is provided as-is, without warranty of any kind, express or implied,
including but not limited to the warranties of merchantability, or fitness
for any purpose. We do not guarantee that GPSEE does not infringe upon any
patents or copyrights, although we try very hard to insure that it does not.

There is no support for GPSEE. However, you can often find the GPSEE
developers on irc://irc.freenode.org/#gpsee, and we're friendly.



How to build GPSEE
=========================================================================

*************************************************************************
****
**** The most recent version of these build instructions is 
**** available at http://code.google.com/p/gpsee/wiki/Building
****
*************************************************************************

   Building GPSEE is very straightforward. GPSEE's build system knows how
   to build SpiderMonkey, LibFFI, GPSEE, the included modules, and how to
   install it all. There is no autoconf magic, you just need to edit three
   files named local_config.mk (one for GPSEE, one for SpiderMonkey, one
   for LibFFI) to let GPSEE know where you keep things and you're off to
   the races.

General Prerequisites
---------------------
     * Solaris 10+, Recent Linux, or MacOS/X 10.5+
     * Any UNIX(tm) conforming to SUSv3 should work
     * BSD UNIX is also expected to work
     * GCC 3.4
     * GNU Make 3.81

     * SpiderMonkey prerequisites for multi-threaded build (we've listed the
       current ones below, but Mozilla could change them on us)
     * autoconf-2.13 (exactly 2.13)
     * NSPR 4.7
     * https://developer.mozilla.org/en/Mozilla_Build_FAQ for more info

Suggested Distro Packages
-------------------------
Solaris 10
     * ftp://ftp.sunfreeware.com/pub/freeware/sparc/8/autoconf-2.13-sol8-sparc-local.gz
     * NSPR in SUNWpr, SUNWprd packages which ship with Solaris 10u5

Mac Ports
     * sudo port install autoconf213
     * sudo port install nspr
     * Note: Using Mac Ports GCC is not supported; use Apple's compiler
       for a pain-free build

Fedora Core 10
     * sudo yum install -y autoconf213

Ubuntu 9.10
     * sudo apt-get install mercurial autoconf2.13 libnspr4-dev
       libncurses5-dev libdb-dev

Basic Steps
-----------

What follows is a point-form list of typical steps required to build and
install GPSEE. If you're unable to build it for some reason, you can look for
help in #gpsee on irc.freenode.org. We're often online at wierd hours of the
day, and almost always available during EST (GMT-0500) business hours.

Get the sources into your homedir
     * cd
     * wget ftp://sources.redhat.com/pub/libffi/libffi-3.0.10.tar.gz
     * tar -zxvf libffi-3.0.10.tar.gz
     * mkdir hg
     * cd hg
     * hg clone https://gpsee.googlecode.com/hg/ gpsee
     * hg clone http://hg.mozilla.org/releases/mozilla-1.9.2/

Configure GPSEE, LibFFI, and SpiderMonkey
     * cd gpsee
     * cp local_config.mk.sample local_config.mk
     * cp spidermonkey/local_config.mk.sample spidermonkey/local_config.mk
     * cp libffi/local_config.mk.sample libffi/local_config.mk
     * vi local_config.mk #edit suitably, then quit
     * vi spidermonkey/local_config.mk #edit suitably, then quit
     * vi libffi/local_config.mk #edit suitably, then quit

Build SpiderMonkey
     * cd spidermonkey
     * make build
     * sudo make install
     * cd ..

Build Libffi
     * cd libffi
     * make build
     * sudo make install
     * cd ..

Build GPSEE
     * make build
     * sudo make install

Test
     * /usr/bin/gsr -c 'print("Hello, World");'

