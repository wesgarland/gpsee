#! /bin/sh

[ "$GSR" ] || GSR=/usr/bin/gsr
export GSR
path="`dirname $0`"

$GSR -F "$path/stack_dump.js" >/dev/null 2>&1
exitCode="$?"
if [ "$exitCode" = 1 ]; then
  echo "OKAY: Exit code for an uncaught exception passes"
else
  echo "FAIL: Exit code for an uncaught exception was $exitCode"
fi

$GSR -F "$path/warning.js" >/dev/null 2>&1
exitCode="$?"
if [ "$exitCode" = 0 ]; then
  echo "OKAY: Exit code for no error passes"
else
  echo "FAIL: Exit code for no error was $exitCode"
fi

$GSR -F "$path/syntax_error.js" >/dev/null 2>&1
exitCode="$?"
if [ "$exitCode" = 1 ]; then
  echo "OKAY: Exit code for a syntax error passes"
else
  echo "FAIL: Exit code for a syntax error was $exitCode"
fi

$GSR -F "$path/throw_123.js" >/dev/null 2>&1
exitCode="$?"
if [ "$exitCode" = 123 ]; then
  echo "OKAY: Exit code for an integer throw passes"
else
  echo "FAIL: Exit code for throw(123) was $exitCode"
fi

$GSR -c 'blah(' -F "$path/throw_123.js" >/dev/null 2>&1
exitCode="$?"
if [ "$exitCode" = 1 ]; then
  echo "OKAY: Exit code for a syntax error in -c passes"
else
  echo "FAIL: Exit code for a syntax error in -c was $exitCode"
fi

