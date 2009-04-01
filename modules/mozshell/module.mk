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
# Automated release build of the shell yielded:
#
#	c++ -o js.o -c  -DEXPORT_JS_API -DOSTYPE=\"SunOS5\" -DOSARCH=SunOS 
#	-I/export/home/wes/mozilla-hg/tracemonkey/js/src -I.. -I/export/home/wes/mozilla-hg/tracemonkey/js/src/shell 
#	-I.  -I../dist/include   -I../dist/include -I/usr/include/mps -I/sdk/include    -fPIC  -I/usr/openwin/include 
#	-fno-rtti -fno-exceptions -Wno-long-long -pedantic -fno-strict-aliasing -fshort-wchar -pthreads  
#	-DNDEBUG -DTRIMMED -O  -I/usr/openwin/include -DMOZILLA_CLIENT 
#	-include ../mozilla-config.h -Wp,-MD,.deps/js.pp /export/home/wes/mozilla-hg/tracemonkey/js/src/shell/js.cpp
#
#	c++ -o js -I/usr/openwin/include 
#	-fno-rtti -fno-exceptions -Wno-long-long -pedantic -fno-strict-aliasing -fshort-wchar -pthreads  
#	-DNDEBUG -DTRIMMED -O  js.o    -lpthread  -z ignore -R '$ORIGIN:$ORIGIN/..'  -Wl,-rpath-link,/bin -Wl,-rpath-link,/lib  
#	-L../dist/bin -L../dist/lib -L/usr/lib/mps -R/usr/lib/mps -lnspr4 ../editline/libeditline.a ../libjs_static.a -lsocket -ldl -lm      
WARNINGS := $(filter-out -Wstrict-prototypes -Wshadow -Wcast-align, $(WARNINGS))

CXXFLAGS += -DEXPORT_JS_API -DMOZILLA_CLIENT -include $(SPIDERMONKEY_BUILD)/mozilla-config.h
CXXFLAGS := $(CXXFLAGS) -I$(SPIDERMONKEY_BUILD)/dist/include -I$(SPIDERMONKEY_BUILD) -I$(SPIDERMONKEY_SRC)
#include $(SPIDERMONKEY_BUILD)/shell/Makefile
#include $(SPIDERMONKEY_BUILD)/config/autoconf.mk
