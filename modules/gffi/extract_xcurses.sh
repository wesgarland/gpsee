#! /bin/bash
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
#
# @file extract_xcurses.sh	This file is not part of the build.
# 				Its job is to create the xcurses_api.incl file from xcurses_api.tdf.
# 				xcurses_api.tdf cannot be redistributed with the FOSS distro.
# 				It is checked into CVS, in case we ever need to regenerated 
#				xcurses_api.incl.
#
# @author	Wes Garland, wes@page.ca
# @date		Sep 2009
#

echo "/* Generated on `date` by $USER@`hostname` -- please do not hand-edit this file */"
echo
cat ../../licenses/license_block.c

dos2unix < "$1" 2>/dev/null \
	| egrep -v '^[	]*$' \
	| grep -v '^[^	]' \
	| grep '()' \
	| sed -e 's/^	//' -e 's/()	/	/' \
	| grep '^[a-z]' \
	| while read function SUSV3 U98 U95 P92 C99 SVID3 BSD
	  do
	    filter="0"
	    [ $SUSV3 = m ] && filter="$filter || defined(GPSEE_STD_SUSV3)"
	    [ $U98 = m ] && filter="$filter || defined(GPSEE_STD_U98)"
	    [ $U95 = m ] && filter="$filter || defined(GPSEE_STD_U95)"
	    [ $P92 = m ] && filter="$filter || defined(GPSEE_STD_P92)"
	    [ $C99 = m ] && filter="$filter || defined(GPSEE_STD_C99)"
	    [ $SVID3 = m ] && filter="$filter || defined(GPSEE_STD_SVID3)"
	    [ "$BSD" = "m" ] && filter="$filter || defined(GPSEE_STD_BSD)"
	    echo "#if $filter"
	    echo "defineFunction($function)"
	    echo "#endif"
	  done
