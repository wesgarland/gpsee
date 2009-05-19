#! /usr/bin/gsr -zzdd

const ByteString = require("binary").ByteString;
const Binary     = require("binary").Binary;

print("This should print hello world: " + new ByteString("hello").decodeToString("utf-8"));
print("This should print an h: " + new ByteString("hello")[0].decodeToString("ascii"));
print("This should print an e: " + new ByteString("hello").charAt(1).decodeToString("US-ASCII"));
print("This should print 111:  " + new ByteString("hello").byteAt(4));
print("This should print 5:    " + new ByteString("hello").length);
print("This should print hello:" + new ByteString("hello").toByteString().decodeToString("ascii"));
print("This should print 12:   " + new ByteString("hello", "utf-8").toByteString("utf-8", "utf-16").length);
print("This should say true:   " + (new ByteString("hello") instanceof Binary));
