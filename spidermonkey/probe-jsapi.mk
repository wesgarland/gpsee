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

ifndef JS_CONFIG
$(error Please specify JS_CONFIG on your make command line)
endif

UNAME_SYSTEM	 = $(shell uname -s)

JSAPI_LDFLAGS=$(shell $(JS_CONFIG) --libs)
ifeq ($(UNAME_SYSTEM),Darwin)
# Work around a long-standing bug in js-config for Darwin
JSAPI_LDFLAGS	:= $(filter -l%,$(JSAPI_LDFLAGS)) $(filter -L%,$(JSAPI_LDFLAGS))\
		   $(filter -%_namespace,$(JSAPI_LDFLAGS))\
		   $(filter -Wl%,$(JSAPI_LDFLAGS))
endif
# Work around CTypes bug in js-config
JSAPI_LDFLAGS:=$(filter-out %libffi.a,$(JSAPI_LDFLAGS))

CXXFLAGS 	+= -I$(shell $(JS_CONFIG) --includedir)/js
LDFLAGS 	+= $(JSAPI_LDFLAGS)
CFLAGS 		+= $(shell $(JS_CONFIG) --cflags)

build_debug:
	@echo
	@echo "CFLAGS   = $(CFLAGS)"
	@echo "CXXFLAGS = $(CXXFLAGS)"
	@echo "LDFLAGS  = $(LDFLAGS)"
	@echo

probe-jsapi:: probe-jsapi.cpp
