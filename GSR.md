# Introduction #

<tt>gsr</tt> is the standard GPSEE Script Runner ("scripting host"). This is a program, built with GPSEE-core, which allows users to run CommonJS programs in the GPSEE environment.

If you are using GPSEE simply to run CommonJS programs as shell scripts, then this the executable you are looking for.  If you want a fancy interactive [REPL](REPL.md), you might want to look at [JSIE](JSIE.md), or [gpsee-js].

If you are embedding GPSEE into your own software, <tt>gsr</tt> may provide an alternative to minimal.c as a starting point, and can serve as a test or evaluation platform.

# Recommended Flags #
  * -dd when debugging your script
  * -ddzz when debugging a native module (will cause crashes closer to GC hazards by forcing frequent garbage collection)

# Reserved Options and Flags #
  * -r option is reserved for [PageMail's](http://www.page.ca/) SureLynx RC file library. It specifies an alternate RC file.
  * -D option is reserved for PageMail's output redirector. It specifies a file to receive all terminal output.

# Command Line Help #

<blockquote><pre>
gsr 1.0-pre1 - GPSEE Script Runner for GPSEE 0.2-pre1<br>
Copyright (c) 2007-2009 PageMail, Inc. All Rights Reserved.<br>
<br>
As an interpreter: #! gsr {-/*flags*/}<br>
As a command:      gsr [-z #] [-n] <[-c code]|[-f filename]><br>
{-/*flags*/} {[--] [arg...]}<br>
Command Options:<br>
-c code     Specifies literal JavaScript code to execute<br>
-f filename Specifies the filename containing code to run<br>
-F filename Like -f, but skip shebang if present.<br>
-h          Display this help<br>
-n          Engine will load and parse, but not run, the script<br>
flags       A series of one-character flags which can be used<br>
in either file interpreter or command mode<br>
--          Arguments after -- are passed to the script<br>
<br>
Valid Flags:<br>
a - Allow (read-only) access to caller's environment<br>
C - Disables compiler caching via JSScript XDR serialization<br>
d - Increase verbosity<br>
e - Do not limit regexps to n^3 levels of backtracking<br>
J - Disable nanojit<br>
S - Disable Strict mode<br>
R - Load RC file for interpreter (gsr) based on<br>
script filename.<br>
U - Disable UTF-8 C string processing<br>
W - Do not report warnings<br>
x - Parse <!-- comments --> as E4X tokens<br>
</pre></blockquote>

# Sample gsr program #
```
#! /usr/bin/gsr -dd

print("Hello, world!");
```

Don't forget to give it execute permissions: <tt>chmod 755 myprogram.js</tt>

# Shebang Path & /usr/bin/env #

Historically, we have expected a link to your <tt>gsr</tt> installation in <tt>/usr/bin</tt>. A fixed location is required to do /usr/bin/env's inability to pass along shebang arguments (i.e. -dd above). An alternate solution, which will allow gsr to be found anywhere along your PATH, allows comment-embedded options. This is outlined in [Issue 12](http://code.google.com/p/gpsee/issues/detail?id=12).

# Pre-compiled Code #

By default, all GPSEE programs support pre-compiled JavaScript. What this means is that JavaScript byte code is recorded to disk when it is tokenized for the first time, and loaded on subsequent invocations rather than re-tokenizing.

When GSR is run with the -n option, the pre-compiled JavaScript will be generated even though the script is not executed.  You can avoid this side effect with -nC if desired.

Pre-compilation requires that the user running the script have write access to the directory where the script is located. Inability to update to the pre-compiled tokens, due to permission errors or other reasons, will not affect the correct execution of the program.