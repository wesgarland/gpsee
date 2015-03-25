#! /usr/bin/gsr -zzdd
/**
 *  @file	string.js		Test ByteString against String to look for differences in common methods.
 *  @author	Wes Garland, wes@page.ca
 *  @date	Sep 2014
 */

const ByteString = require("binary").ByteString;
ByteString.prototype.toString = function() { return this.decodeToString("US-ASCII") };

const testData = 
[
  { method: 'ssTemplate',	data: "hello", 	argv: [ 0 ] },
  { method: 'ssTemplate',	data: "hello", 	argv: [ 0,1 ] },
  { method: 'ssTemplate',	data: "hello", 	argv: [ 1 ] },
  { method: 'ssTemplate',	data: "hello", 	argv: [ -2 ] },
  { method: 'ssTemplate',	data: "hello", 	argv: [ 1, -2 ] },
  { method: 'ssTemplate',	data: "hello", 	argv: [ 5 ] },
  { method: 'ssTemplate',	data: "hello", 	argv: [ 0,5 ] },
  { method: 'ssTemplate',	data: "hello", 	argv: [ 1,4 ] },
  { method: 'ssTemplate',	data: "hello", 	argv: [ "world" ] },
  { method: 'ssTemplate',	data: "hello", 	argv: [ 2, "world" ] },
  { method: 'ssTemplate',	data: "hello", 	argv: [ 20, "world" ] },
  { method: 'ssTemplate',	data: "hello", 	argv: [ 20 ] },
  { method: 'ssTemplate',	data: "hello", 	argv: [ -20 ] },
  { method: 'ssTemplate',	data: "hello", 	argv: [ 1, -20 ] },
  { method: 'ssTemplate',	data: "hello", 	argv: [ -20, 1 ] },
];

/* Describe a test object */
function descr(test)
{
  return '"' + test.data + '".' + test.method + "(" + test.argv.join() + ")";
}

/* Duplicate a test object, changing the method name */
function dupl(o, method)
{
  var p, n = {};
  for (p in o)
    n[p] = o[p];
  n.method = method;
  return n;
}

var allGood = true;
var test, i, len;
var s1, s2, res1, res2;

/* Rewrite ssTemplate tests for substr, substring, slice */
len = testData.length;
for (i=0; i < len; i++)
{
  test = testData[i];
  if (test.method == 'ssTemplate')
  {
    test.method = 'substr';
    testData[len + i] = dupl(test, "substring");
    testData[2*len + i] = dupl(test, "slice");
  }
}

print("Comparing String against ByteString:");
/* Perform the tests */
for (i=0; i < testData.length; i++)
{
  test = testData[i];

  s1 = new String(test.data);
  res1 = s1[test.method].apply(s1, test.argv);
  s2 = new ByteString(test.data);
  res2 = s2[test.method].apply(s2, test.argv);
  
  if (res1 != res2)
  {
    allGood = false;
    print("FAIL " + descr(test) + " -\t"  + res1 + " != " + res2);
  }
  else
    print("pass " + descr(test) + " -\t"  + res1 + " == " + res2);
}

if (!allGood)
  throw 1;
