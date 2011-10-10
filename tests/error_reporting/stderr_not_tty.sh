#! /bin/sh

[ "$GSR" ] || GSR=/usr/bin/gsr
export GSR
path="`dirname $0`"

output="`$GSR -F \"$path/stack_dump.js\" 2>&1`"

if [ "$output" != "
Uncaught exception in stack_dump.js:14: An error occurred" ]; then
  echo "FAIL: Output format was incorrect; expected an exception, but no stack dump"
  echo "Got:  $output"
else
  echo "OKAY: Output format was correct: exception, but no stack dump"
fi
