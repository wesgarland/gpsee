#! /bin/sh

[ "$GSR" ] || GSR=/usr/bin/gsr
export GSR
path="`dirname $0`"

output="`$GSR -F \"$path/stack_dump.js\" 2>&1`"

if expr 'Uncaught exception in /Users/wes/hg/gpsee/tests/error_reporting/stack_dump.js:15: An error occurred' : "^Uncaught exception in .*stack_dump.js:15: An error occurred$" >/dev/null; then
  echo "OKAY: Output format was correct: exception, but no stack dump"
else
  echo "FAIL: Output format was incorrect; expected an exception, but no stack dump"
  echo "Got:  $output"
fi
