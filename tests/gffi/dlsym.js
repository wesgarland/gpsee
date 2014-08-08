#! /usr/bin/gsr

const gffi = require("gffi");
const lib  = new gffi.Library("libgpsee.so");

try
{
  if (lib.dlsym("gpsee_throw"))
    print("success");
  else
    print("fail");
}
catch(e)
{
  print("fail: " + e);
}
