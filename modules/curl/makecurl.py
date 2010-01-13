#!/usr/bin/env python

text = None

name = '/opt/local/include/curl/curl.h'
f = open(name)
#f = open('/Users/nickg/root/include/curl/curl.h')
text = f.read()
f.close();

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


print "int option_expected_type(int opt) {";
print "switch (opt) {"
for i in longargs:
    print "case CURLOPT_%s: " % i
print "return 0;";
for i in objargs:
    print "case CURLOPT_%s: " % i
print "return 1;";
for i in fnargs:
    print "case CURLOPT_%s: " % i
print "return 2;";
print "default: return -1;"
print "}"
print "}"


