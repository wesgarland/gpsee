#! /bin/sh -f
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Initial Developer of the Original Code is PageMail, Inc.
#
# Portions created by the Initial Developer are 
# Copyright (c) 2007-2010, PageMail, Inc. All Rights Reserved.
#
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK ***** 
#

# This program tests the capabilities of the JSAPI build environment on 
# your platform. It uses the js-config which is created when your compile
# any modern (post 1.8.0) SpiderMonkey.  It depends on having a working
# build environment (capable of building an embedding) and a bash shell
# available somewhere on your PATH (or linked to /bin/sh). There are no
# other dependencies.
#
# Usage: If $1 is cleanup, we remove all temporary files created by this
#        program. They are normally left behind for debugging purposes.
#
#        If $1 and $2 are specified, they are used as the name of the output
#        header and location of js-config respectively. If $2 is not specified,
#        we look for js-config on your path.
#
# If you need to use a make program other than 'make', specify it in the MAKE
# environment variable when invoking this script.
#
# This program can currently detect
#  - That your compilation environment works

if [ "$1" = "cleanup" ]; then
  rm -f probe-jsapi probe-jsapi.o probe-jsapi.cpp
  exit 0
fi

if [ ! "$BASH_VERSION" ]; then
  if [ ! -x "/bin/bash" ]; then
    . ${gpsee_dir}/configure.incl
    bash="`locate bash`"
  else
    bash=/bin/bash
  fi

  if [ "$bash" ]; then
    BASH_VERSION=dontrecurse $bash $0 $*
    exit $?
  fi
fi

if [ "$BASH_VERSION" = "dontrecurse" ] || [ ! "$BASH_VERSION" ]; then
  echo "Sorry, need a bash shell to run this program"
  exit 1
fi

argv_zero="$0"
output_header="$1"
[ "$1" ] || output_header="probe-jsapi.h"
JS_CONFIG="js-config"
[ "$2" ] && JS_CONFIG="$2"
listFuncs="typeset -F"
[ "$MAKE" ] || MAKE=make
[ ! "$USER" ] && [ "$LOGNAME" ] && USER="$LOGNAME"

cat <<EOF

probe-jsapi.h -- Figure out what features your version of JSAPI supports
Copyright (c) 2010, PageMail, Inc. All Rights Reserved.
Licensed under the same terms as SpiderMonkey: MPL 1.1, GPL 2, LGPL 2.1

EOF

if ! ${JS_CONFIG} --prefix 2>/dev/null >&2; then
  echo "*** Could not locate your copy of js-config. Please specify it as the second argument"
  echo "    to $argv_zero or arrange for it to be on your PATH"
  echo "Stop."
  echo
  exit 1
fi

if [ -x /usr/ucb/echo ]; then
  print="/usr/ucb/echo -n"
else
  print="printf %s"
fi

print()
{
  $print "$*"
}

say()
{
  echo "$*" >&5
}

headers="#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <jsapi.h>"
main="int main(int argc, const char *argv[])"
prologue="$headers
$main {"
needCx="JSContext *cx = JS_NewContext(JS_NewRuntime(0x4000), 0x4000);"
epilogue="return 0;}"

build()
{
  rm -f probe-jsapi probe-jsapi.o
  cat > probe-jsapi.cpp
  $MAKE JS_CONFIG="$JS_CONFIG" -f probe-jsapi.mk probe-jsapi >&6
  return $?
}

run_build()
{
  [ "$1" = "0" ] || return $1

  ./probe-jsapi
  return $?
}

emit_define()
{
  echo "#define $*" >&7
}

emit_comment()
{
  [ "$1" ] &&  echo "/* $* */" >&7
  unset seen
  while read line
  do
    [ "$seen" ] || echo "/**"
    echo " * $line"
    seen=1
  done >&7

  [ "$seen" ] && echo " */" >&7
}

emit_code()
{
  cat >&7
}

. `dirname ${argv_zero}`/probe-jsapi.incl

date > probe-jsapi.out
date > probe-jsapi.err

# fd 5 = output from say
# fd 6 = output from build process
# fd 7 = the output header

echo "Generating ${output_header}"
echo "/* Generated `date` by $USER on `hostname` */" > "${output_header}"
$MAKE JS_CONFIG="$JS_CONFIG" -f probe-jsapi.mk build_debug | emit_comment 7>>"${output_header}"

$listFuncs | sed 's/^.* //' | grep '^test_' | sort | while read func
do
  print "Checking "
  unset dots
  $func 5>&1 6>>probe-jsapi.out 2>>probe-jsapi.err 7>>"${output_header}" | while read line
  do 
    [ "$dots" ] && $print ".. "
    print "$line"
    dots=yes
  done 
  echo
  [ "${PIPESTATUS[0]}" = "0" ] || break
done 

rm -f probe-jsapi.o probe-jsapi
echo "Done."
echo