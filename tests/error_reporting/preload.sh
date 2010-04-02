#! /bin/sh

[ "$GSR" ] || GSR=/usr/bin/gsr

$GSR -c 'a=123'
$GSR -c 'lkjslkj('

echo "Should have a warning and a syntax error above"
