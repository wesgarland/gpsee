# Introduction #

SpiderMonkey is the JavaScript interpreter from Mozilla which ships in its browser products. It is also the original JavaScript interpreter.  Written in C (now C++), SpiderMonkey is fast, flexible, portable, and small.

# Details #

The current incarnation of SpiderMonkey is code-named TraceMonkey.  TraceMonkey adds a tracing JIT on top of SpiderMonkey, compiling JavaScript Regular Expressions and "Hot Loops" down to blisteringly-fast native assembly language.

Two important source repositories exist for TraceMonkey.  The mozilla-central repository is the code that the current bleeding-edge Firefox is built with. The tracemonkey repository is where the active work on TraceMonkey happens, and is the absolute bleeding edge.  They are related Mercurial trees, so you can clone from and pull from the other to select which "view" of the source code you'd like.  **These are the only version of SpiderMonkey supported by GPSEE**.

More information at Mozilla: https://developer.mozilla.org/en/SpiderMonkey