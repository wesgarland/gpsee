# Shipping Manifest #
  * [gsr](gsr.md), the GPSEE Script Runner ("scripting host")
  * [gffi module](Module_gffi.md), an FFI module for calling C functions from JavaScript
  * [cgi module](Module_cgi.md), a module for developing web applications via [CGI](http://hoohoo.ncsa.uiuc.edu/cgi/intro.html)
  * [PHPSession class](Module_cgi.md), a class for reading PHP Session files and reflecting them as JavaScript objects
  * [Query class](Module_cgi.md), a class reflecting CGI GET and POST queries into JavaScript objects and handling file uploads
  * [curses module](Module_curses.md), a module for interacting with the UNIX terminal (screen and keyboard) via ncurses.
  * [gpsee-js], the Mozilla JS shell on top of GPSEE
  * [pairodice module](Module_pairodice.md), a module designed to show how to write modules. Implements a class which can roll two dice.
  * [signal module](Module_signal.md), a module which lets you trigger JavaScript functions with POSIX signals
  * [module](Module_system.md), a module which provides access to native operating system details
  * [thread module](Module_thread.md), a module which lets you spawn OS threads running JavaScript code from JavaScript
  * [vm module](Module_vm.md), a module for manipulating or querying SpiderMonkey itself
  * [jsie](jsie.md).js, a full screen curses REPL written in JavaScript hosted by [gsr](gsr.md)

# Directory Organization #
  * <tt>modules</tt>: Standard modules
  * <tt>apr_modules</tt>: Modules compiled only in APR stream (not yet implemented)
  * <tt>unix_modules</tt>: Modules compiled only in UNIX stream (default stream)
  * <tt>spidermonkey</tt>: SpiderMonkey build automation, configuration, and object files
  * <tt>sample_programs</tt>: Programs written to run under [[gsr](gsr.md)], like [[jsie](jsie.md)].js
  * <tt>tests</tt>: Programs written to test GPSEE, roughly consistent in style with the [[InteroperableJS](InteroperableJS.md)] tests
  * <tt>licenses</tt>: Full text of licenses you can choose to use GPSEE under
  * <tt>docgen</tt>: Templates for automatic documentation generation

## Build System Files ##
  * <tt>modules/<i>myModule</i>/module.mk</tt>: Extra build rules when building ''myModule''.  Needed when modules need more than just a plain C (or c++) file to build, or when modules need to pull in extra libraries.
  * <tt><i>osIdentifier</i><code>_</code>config.mk</tt>: Special rules for a particular operating system. Local _osIdentifier_ is determined with <tt>uname -s</tt>.
  * <tt><i>stream</i><code>_</code>stream.mk</tt>: Special rules for a particular [[stream](GPSEE.md)]
  * <tt>local_config.mk</tt>: File to edit for local build system changes. Specifies  GPSEE installation directory.
  * <tt>spidermonkey/local_config.mk</tt>: File to edit to specify SpiderMonkey build details and source location. Supplied as <tt>spidermonkey/local_config.mk.sample</tt>.