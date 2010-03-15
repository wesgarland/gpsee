#! /bin/sh
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
# Copyright (c) 2007-2009, PageMail, Inc. All Rights Reserved.
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

##
## @file	darwin-ld.sh	Darwin linker that knows how to munge install_paths
##				This is unfortunately necessary as the Mozilla's
##				libmozjs.dylib is installed with an @execution_path-
##				relative install name, which otherwise forces all
##				GPSEE programs (including file-interpreter scripts)
##				to need a copy of libmozjs.dylib in their directory!
##
##				This program can be used in the general case for 
##				rewriting install paths. Each line of stdin is
##				an old/new path pair. Each path is expected to
##				be parseable, by shell IFS, as discrete arguments.
##				To give nice make output, call it with it with @;
##				it will echo the sub-commands to stdout.
##
## @author	Wes Garland, PageMail, Inc., <wes@page.ca>
## @date	Apr 2009
## @version	$Id: darwin-ccld.sh,v 1.3 2010/03/06 18:17:13 wes Exp $
##

argv="$*"

outfile="`
  while [ "$2" ]
  do
    [ "$1" = "-o" ] && echo "$2" && shift
    shift
  done | head -1
`"

if [ ! "$outfile" ]; then
  $argv
  exit $?
fi

echo $argv

$argv && while read old new;
do
  echo install_name_tool -change "$old" "$new" "$outfile"
  if [ "$new" ]; then
    install_name_tool -change "$old" "$new" "$outfile"
  else
    echo " * Error: new library not specified!" > /dev/stderr
  fi
done

exit $?
