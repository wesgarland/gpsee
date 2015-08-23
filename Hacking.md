# Introduction #

GPSEE-core was created to make it easy to add custom JS classes and objects to otherwise
typical-looking SpiderMonkey embeddings through a simple module system. Additionally, GPSEE multiplexes certain SpiderMonkey facilities (e.g. context-private storage) so that multiple modules may make use of the same SpiderMonkey facilities without having to know about each other.

# Modules #

There are three types modules which can provide objects (and classes) to GPSEE: native, script, and blended (native+script). Modules are loaded with the require() JavaScript function.

In most cases, JavaScript programs are treated as a "top-level" module, however this is not strictly necessary if you are embedding GPSEE to augment another environment, rather than using it to build a scripting host.  Loading a program module is done with the <tt>gpsee_runProgramModule()</tt> C function. The GPSEE script runner (gsr) treats both shebang (#!) scripts and program files loaded with -f as program modules.

## Module Loading ##
Modules are loaded with the JavaScript statement ```
require("moduleName");```.  This evaluates to a JavaScript object containing the module's exports. Modules are singletons: a module which has already been loaded will return the same exports object. Keeping a reference to this object is useful in some situations, as it is possible for GPSEE to unload a module under certain circumstances if none of its exports (or their children) are reachable during garbage collection. It is also faster to use a reference to a module's exports than it is to repeatedly call require(), particularlly when using a relative module name.

### Disk Modules ###
Module names beginning with ./ or ../ are sourced relative to the containing module; otherwise they are considered absolute paths, and located by searching module path.  The default module path contains the program module's directory and the libexec directory. By default, the libexec directory is provided by the Makefile variable <tt>GPSEE_LIBEXEC_DIR</tt>, however it can be overridden by an environment variable of the same same, or the RC variable <tt>gpsee_libexec_dir</tt>.

### Internal Modules ###
A special group of absolute-named modules can override any file in the module path. These are called "internal modules" and are compiled right into the GPSEE library. By default, only "system" and "vm" are built as internal modules; this can be changed by editting the <tt>INTERNAL_MODULES</tt> variable in the Makefile.

## Writing Native Modules ##

A native module is a collection of objects, classes, etc., written in C, C++ or other complied language in typical [SpiderMonkey idiom](http://developer.mozilla.org/En/SpiderMonkey/JSAPI_Phrasebook), which are attached as properties to the module's exports object.

#### Module Filename ####
To build within the GPSEE tree, native modules must be named _moduleName_`_`module.c or _moduleName_`_`module.cpp, in a directory underneath either <tt>modules/</tt> or <tt>$(STREAM)<code>_</code>modules/</tt>. The latter should not be used unless your module cannot be built without $(STREAM)'s support -- for example, building a module to reflect an Apache request\_req would go into the APR stream because it would depend on facilities provided by APR.  You do not need to edit the GPSEE Makefile to add modules; they will be detected and built automatically.

To build outside of GPSEE, native modules must be named _moduleName_`_`module.so  (or .dylib on Mac OS/X) and be installed in one of the module path directories.  If you are building modules without GPSEE, it is imperative you insure that both packages (i.e. GPSEE and whatever you are doing) use the same SpiderMonkey header files, js-config, and library, as the SpiderMonkey ABI is not stable and varies depending on C preprocessor flags. Using the <tt>gpsee-config</tt> program to configure your embedding, and letting GPSEE build SpiderMonkey is the easiest way to make sure you do not have any conflicts.

#### InitModule function ####
This function, named either <tt><i>moduleName</i><code>_</code>InitModule()</tt> or simply <tt>InitModule()</tt> must be provided by any native module.  It is called the first time the module is used (via the JavaScript require() function), and receives a JS context and exports object.  The InitModule function should do whatever startup initialization it needs to do, and "decorate" this exports object to expose its functionality; this object becomes the return value of the JavaScript require() method.

```
const char *Thread_InitModule(JSContext *cx, JSObject *moduleExports)```

Upon successful initialization, the Init function must return a module identifier.  This moduleID does not need to be unique, however it is recommended that unique names indicating the module name and origin are chosen. This string is used primarily in diagnostic messages.

If initialization fails, the InitModule function return NULL. If your module specifies a FiniModule function, it will be called to clean up dangling resources.

If your module needs private storage shared between the InitModule and FiniModule functions, use the JS\_GetPrivate() and JS\_SetPrivate() functions on the exports object to set and get a void pointer.

#### FiniModule function ####
This function, named either ''moduleName''_FiniModule() or simply FiniModule() must be provided by any native module which allocates resources that cannot be managed by JSAPI garbage collection (including [JSClass.finalize](https://developer.mozilla.org/En/SpiderMonkey/JSAPI_Reference/JSClass.finalize))._

This function receives a JavaScript context, and the same exports object that was received during InitModule.  Returning JS\_TRUE from this function indicates to GPSEE that you have successfully released ALL resources dependent on this module -- including its address space.  Returning JS\_FALSE will prevent a DSO module from unloading via dlclose(), except when shutting down the interpreter in its entirety.

#### MODULE\_ID ####

MODULE\_ID is a C-precompiler macro to an array of bytes (C string literal).  It is returned from ModuleInit and should start the exception name with every call to gpsee\_throw() your module makes.  It must also be prepended to your class name in any calls to JS\_InitClass().  The moduleID will not be required to instanciate the class, however it will be shown when calling the toString() method on your module's classes. Keeping moduleIDs consistent and meaningful will help when debugging large programs written with GPSEE.

MODULE\_ID is normally defined as GPSEE\_GLOBAL\_NAMESPACE\_NAME ".module." module\_provider "." module\_name

So if your module's name is "database" and you work for PageMail  (with a domain of <tt>page.ca</tt>), your moduleID would be <tt>gpsee.ca.page.module.database</tt>.

### The Global Object ###

Please do not reference the global object in your module.  If you find yourself needing to do so, you are almost certainly commiting a design error. If you are porting a class which uses the global object, consider using the module's scope object instead. You can access the module's scope by calling ```
gpsee_getModuleScope()``` on any JSObject **which is a descendant of your module's exports object.**

### Blended Modules ###

Blended modules (native + script) may be created by having a JavaScript module with the same canonical name as the native module. For example, if you have native module code in <tt>/opt/gpsee/libexec/myModule_module.so</tt>, you can augment it with JavaScript code in <tt>/opt/gpsee/libexec/myModule_module.js</tt>.

The DSO module will be loaded first, if it succeeds, the JavaScript module will then be loaded. The JavaScript module will receive the same exports object as the DSO module.  Failure to load the script module (i.e. uncaught exception, syntax error) will cause the module in its entirety to fail, and the shared object will be unloaded from memory after calling its ModuleFini function.

### DSO Modules vs. Internal Modulse ###

Both these module types use the same Native Module API, however there are a few differences:
  * DSO Modules are loaded on demand, and may be unloaded when their exports are no longer reachable from JavaScript.
  * Internal Modules are never unloaded
  * Internal Modules may not declare InitModule and FiniModule API entry points; only moduleName\_InitModule and and moduleName\_FiniModule
  * Internal Modules ''must'' provide a FiniModule function; this is optional with DSO modules.
  * Internal Modules may not be used as part of a blended module

### DSO Module Unloading ###
  * Do not unload when FiniModule returns JS\_FALSE
  * Do not unload when exports or exports' chidlren are referenced

## Writing JavaScript Modules ##

JavaScript modules are executed within their own scope, which is a child of the global object (and any parent modules' scopes). The variable <tt>exports</tt> will already be defined for the module, and serves as the return value of the JavaScript require() function. The module's scope is private, and will be persisted as long as the module is loaded. The symbol ```
this``` evaluates to the module's scope outside of any function in the module.

There are some interesting side-effect of the module hierachy implementation which may be unexpected. These should not be taken advantage of, as they are likely to change in future implementations:
  * The program module has its own scope, just as any other module. This scope is not the global object.
  * Each module receives a different require function, which all appear to be the same. However, each unique require function carries with it a different parent scope; it is this parent information that is used by GPSEE to determine relative module paths.

An alias to the global scope's require function exists as <tt>loadModule</tt> and may be used by persons interesting in experimenting with other types of modules; <tt>'''system'''.include</tt> is probably of similar interest.

A JavaScript module may specify its moduleID by setting the moduleID property in its scope object. If this property is not set by the time the module initalizes, GPSEE will calculate a moduleID based on the module's canonicalized filename.  This moduleID can be used to throw GPSEE exceptions, and certainly ''should'' be used when throwing exceptions from a blended module, or a module which is part of GPSEE's standard library.

## Writing 'friendly' classes ##
  * Must be part of same module
  * Do not forget to check class pointer before calling JS\_GetPrivate() (or use JS\_GetClassPrivate())

# Writing GPSEE Programs in C #
  * main() should be in programname.c
  * Add programname to EXPORT\_PROGS variable in Makefile

# Writing programs with gsr #
  * Just like writing a shell script
  * Start the file with ```
#! /path/to/gsr -flags```
  * Give the file execute permissions
  * See [[gsr](gsr.md)] documentation for more information

# Extensions to SpiderMonkey #

## Global Symbols ##
| gpseeNamespace | Value of C macro GPSEE\_GLOBAL\_NAMESPACE\_NAME |
|:---------------|:------------------------------------------------|
| print          | Function to send strings to stdout. More than one parameter will be passed. They will be coerced toString and collated before output. |
| arguments      | Array of arguments passed to script.            |
| environ        | Array matching layout of the C environ pointer (environment variables). Only available if flag a is selected. |
| loadModule     | Alias for the top-level require method.         |

## Reserved Facilities ##

  * runtime private is used to store gpsee\_interpreter\_t handle

## Multiplexed Services ##
  * Context private storage
  * GC Callback (soon)
  * Operation Callback (soon)

## C functions ##
  * Error reporter
  * gpsee\_printf
  * gpsee\_throw
  * Sample minimal embedding link

# Runtime Configuration #
Tying in with a given application infrastructure's runtime configuration varies too much for GPSEE to ship with anything allow meaningful runtime configuraiton. There are stubs for an RC file library in gpsee\_unix.c, and in the future, any other stream should contain a full implementation. For example, a stream which embeds GPSEE into an Apache web server might have the gpsee\_apr.c populated with an RC library that knows how to interface with Apache configuration directives.

## GPSEE RC Variables ##
| **Variable**                             |**Type** | **Default**        |**Description**|
|:-----------------------------------------|:--------|:-------------------|:--------------|
| gpsee\_report\_warnings                  | bool    |  true              |               |
| gpsee\_heap\_maxbytes                    | long    |  0x40000           |               |
| gpsee\_gc\_maxbytes                      | long    |  0                 |               |
| gpsee\_stack\_chunk\_size                 | int     |  8192              |               |
| gpsee\_javascript\_version               | float   |  -                 | e.g. 1.5 downgrades to JavaScript 1.5 features |

# Interesting Makefile Variables #
## gpsee/Makefile ##
|BUILD|Defines build type. DEBUG is a normal debugging/maintainer build. It allows the most flexibility and feedback, with "tools" such as gcZeal and dynamic/static assertions available.|
|:----|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
|INTERNAL\_MODULES|Which native modules get statically linked right into the embedding.                                                                                                               |
|IGNORE\_MODULES|Modules which should not be built. (Usually to mark unfinished work etc.)                                                                                                          |

## gpsee/local\_config.mk ##
GPSEE\_PREFIX\_DIR - Where GPSEE will install its binaries, libraries, headers, etc.

## In spidermonkey/local\_config.mk ##

|BUILD|Type of SpiderMonkey build. May be different than GPSEE build type, but we suggest either a full DEBUG build or keeping it in sync with your GPSEE  build. Mixing and matching build types should work but isn't tested.|
|:----|:-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
|JSAPI\_PREFIX|Where SpiderMonkey should install its headers and libs                                                                                                                                                                  |
|SPIDERMONKEY\_SRC|Where you keep the version of SpiderMonkey you want to build/use with GPSEE.                                                                                                                                            |

## Build Types ##
|RELEASE | intended for final-release products; full optimization, no debugging. |
|:-------|:----------------------------------------------------------------------|
|DRELEASE| intended for a release build with debugging symbols.                  |
|PROFILE |(not yet available) is a DRELEASE build with profiling compiler options.|

# Porting #
## Native Modules from other SpiderMonkey platforms ##
  * Modules must have a preprocessor-defined MODULE\_ID, available to every to file in the module
  * Modules implementing classes must have a preprocessor-defined CLASS\_ID, defined in terms of MODULE\_ID, and should implement one class per file
  * Modules should not access the global object; the module-wide object available via gpsee\_getModuleObject() should be used instead.
  * Modules must have a ModuleInit and a ModuleFini fucntion
**ModuleFini should return JS\_FALSE if the module stores private information not tied to JavaScript objects (i.e. is stateful). Note that this will prevent dynamic module unloading.
  * JSClass structs should have the first member modified to be wrapped by the GPSEE\_CLASS\_NAME macro.  The following snippet shows how to declare JSClass instances which can compile with and without GPSEE:**

Old:
```
 static JSClass byteString_class = {
     "ByteString",
     ...
 }
```

New:
```
 #if !defined GPSEE_MAJOR_VERSION_NUMBER
 # define GPSEE_CLASS_NAME(cls) #cls
 #endif
 static JSClass byteString_class = {
     GPSEE_CLASS_NAME(ByteString),
     ...
 }
```

## JavaScript Modules from other JavaScript platforms ##

# Submitting Patches #

Patches should be submitted as bugs in the GPSEE Issue Tracker at http://code.google.com/p/gpsee/issues/list, with explanations of the problems they solve and what platforms they've been tested on.  Suitable patches will be reviewed and pushed to the GPSEE [Mercurial](Mercurial.md) repository by a GPSEE developer.

## Configuring Mercurial ##
GPSEE uses a [Mercurial](Mercurial.md) configuration which is compatible with Mozilla's; it is expected that most people producing GPSEE patches will also be producing SpiderMonkey patches from time to time.

Here is a sample [Mercurial](Mercurial.md) configuration for producing suitable patches, and resolving conflicts with the GNU diff3 utility, on a Solaris 10 system:
```
[ui]
username = Wes Garland <wes@page.ca>
merge = /opt/sfw/bin/gdiff3

[diff]
git=1

[defaults]
diff=-p -U 8

[extensions]
mq=
```

## Creating Patches ##
Once you have configured [Mercurial](Mercurial.md) appropriately, you can create patches with a command like

> hg diff . > patch.txt

If you need to create patches only describing a few files, specify a filespec instead of the dot in the command above.  If you are maintaining multiple sets of patches for different fixes that affect common files, you should look at using Mercurial Queues.

## C Coding Style ##
  * Brace-alone-on-a-line
  * 2-space indents
  * No tabs in source code
  * Appropriate use of goto is not discouraged
  * GCC-specific constructs allowable, provided they are supported gcc 3.x and gcc 4.x
  * Lines should not exceed ~ 120 chars, and be folded to have nice vertical alignment. (If you have a 10-year-old version of xemacs, feel free to ask for wes-c-style)
  * local variables should be declared one per line, unless they are of the same type and tightly bound (like a loop iterator and invariant).
  * Functions should have descriptive names; lowercaseLeadingCamelCase.
```
flushOutputBuffer();
isInvalidUserID();
```
  * Functions which vary in very minor detail should have similar names with an underscore-delimited modifier:
```
getString_fromTerminal();
getString_fromSocket();
multiply_byRepetiveAddition();
```
  * Int-type function return values are normally 0 for success and non-zero for error, unless the function starts with "is" or "can"
<blockquote>
<ul><li>isTerminal() would return 0 if the argument is not a terminal<br>
</li><li>canWrite() would return 1 if the argument file descriptor were writeable<br>
</li><li>copyBuffer() would return 0 if the copy succeeded<br>
</blockquote>
</li><li>for() statements with non-trivial arguments should have the initializer, loop invariant, and propulsion statement on separate lines<br>
</li><li>do....while statements have the closing brace on the same line as the while<br>
</li><li>if..then..else if..then..else..if... statements have the braces in the same column rather than marching to the right of the screen<br>
</li><li>Function pointer typedefs end in <i>fn<br>
</li><li>Enumeration and struct typedefs end in</i>t<br>
</li><li>Enums are preferred over macros<br>
</li><li>Small, inlineable static functions are preferred over macros<br>
</li><li>Explicit casts are to be avoided unless unavoidable<br>
</li><li>All errors are to be checked<br>
</li><li>None of these rules are fast-and-hard; common sense overrides</li></ul>

## C Documentation Style ##
  * C-style comments are preferred
  * JavaDoc-compatible Doxygen
  * Whole-function documentation above function in / .... **/ comments
  * Where inner documentation is required, document in /** ... 