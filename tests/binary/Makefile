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

## @file	Makefile	Helpers for running tests. `make` diffs test
##				results against "committed" test results.
##				`make commit` commits test results.
## @author	Donny Viszneki, PageMail, Inc., donny.viszneki@gmail.com
## @date	June 2009
## @version	$Id: Makefile,v 1.2 2011/12/05 19:13:38 wes Exp $

GPSEE_SRC_DIR ?= ../..

all ::
	gsr -ddzzF ./ByteString.js -- -q > ByteString.test.temp && touch ByteString ByteString.test && diff ByteString.test ByteString.test.temp
	gsr -ddzzF ./ByteArray.js -- -q > ByteArray.test.temp && touch ByteArray.test && diff ByteArray.test ByteArray.test.temp

commit ::
	-mv ByteString.test.temp ByteString.test
	-mv ByteArray.test.temp ByteArray.test

