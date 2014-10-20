#! /usr/bin/gsr -C

const BINARY = require('binary');

function roundTrip(testString)
{
  var bsHello = new BINARY.ByteString(testString, "ascii");
  var b64Hello = BINARY.toBase64(bsHello);
  var sHello = BINARY.fromBase64(b64Hello);
  var rtString = require("gffi").Memory(sHello).asString();

  print("Testing string: " + testString + " - " + (rtString == testString ? "pass" : "FAIL"));
  if (rtString != testString)
    print("in: " + testString + "\tout: " + rtString + "\tb64: " + b64Hello);
}

roundTrip("");
roundTrip("h");
roundTrip("he");
roundTrip("hel");
roundTrip("hell");
roundTrip("hello");
roundTrip("hello, world");
roundTrip("hello, world1");
roundTrip("hello, world12");
roundTrip("hello, world123");
roundTrip("hello, world1234");




