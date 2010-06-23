#!/usr/bin/env python

#text = None

#name = '/opt/local/include/curl/curl.h'
#name = '/usr/include/curl/curl.h'
#f = open(name)
#text = f.read()
#f.close();

import sys
text = sys.stdin.read()
import re

longargs = [];
objargs = [];
fnargs = [];
print "static JSConstDoubleSpec easycurl_options[] = {"

for mo in re.finditer(r'CINIT.([A-Z_]+), *([A-Z]+), *([0-9]+)', text):
    val = int(mo.group(3))
    t   = mo.group(2)

    if t == 'LONG':
        longargs.append(mo.group(1))
    if t == 'OBJECTPOINT':
        val += 10000
        objargs.append(mo.group(1))
    if t == 'FUNCTIONPOINT':
        val += 20000
        fnargs.append(mo.group(1))

#for mo in re.finditer(r'CINIT.', text):
    print '{%s, "CURLOPT_%s", JSPROP_READONLY | JSPROP_PERMANENT | JSPROP_ENUMERATE, {0,0,0}},' % (val, mo.group(1))


print "{0,0,0,{0,0,0}}";
print "};";
