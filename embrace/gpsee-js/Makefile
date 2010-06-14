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

## @file	Makefile	Makefile to build the Mozilla JS Shell with GPSEE. 
##				Requires that GPSEE already be built and installed.
## @author	Wes Garland, PageMail, Inc., wes@page.ca
## @date	May 2009
## @version	$Id: Makefile,v 1.7 2010/06/14 22:11:59 wes Exp $

top:	build

PRODUCT ?= gpsee-js

GPSEE_CONFIG	?= ../../gpsee-config
include $(shell $(GPSEE_CONFIG) --outside.mk)

GPSEE_INCLUDE_DIR	?= $(shell $(GPSEE_CONFIG) --includedir)
SPIDERMONKEY_BUILD	?= $(shell $(GPSEE_CONFIG) --spidermonkey-build)
SHELL_BUILD		= $(SPIDERMONKEY_BUILD)/shell
EXTRA_CLEAN_RULE	= clean_shell_build
GPSEE_JS_CXXFLAGS	+= -I$(GPSEE_INCLUDE_DIR) -I$(SPIDERMONKEY_SRC) $(shell $(GPSEE_CONFIG) --cppflags)
GPSEE_JS_LDFLAGS	+= $(filter-out -lmozjs,$(filter-out -lgpsee,$(shell $(GPSEE_CONFIG) --ldflags))) $(GPSEE_SRC_DIR)/libgpsee.a
EXPORT_PROGS		+= $(PRODUCT)

build: $(EXPORT_LIBS) $(EXPORT_PROGS) $(EXPORT_EXTRA_FILES)

$(SHELL_BUILD)/$(PRODUCT).mk: Makefile $(SHELL_BUILD)/Makefile $(SHELL_BUILD)/$(PRODUCT).cpp
	@echo " * Building $(SHELL_BUILD)/$(PRODUCT).mk"
	@$(SED) -e 's/ js.cpp/$(PRODUCT).cpp/' -e 's/= js/= $(PRODUCT)/' <$(SHELL_BUILD)/Makefile >$@
	@echo "GARBAGE += $(PRODUCT).mk $(PRODUCT).cpp" >> $@
	@echo 'CXXFLAGS := $$(GPSEE_JS_CXXFLAGS) $$(CXXFLAGS)' >> $@
	@echo 'LDFLAGS := $$(GPSEE_JS_LDFLAGS) $$(LDFLAGS)' >> $@

$(SHELL_BUILD)/$(PRODUCT).cpp: $(GPSEE_SRC_DIR)/embrace/gpsee-js/gpsee-js.cpp
	$(CP) $^ $@

$(GPSEE_SRC_DIR)/libgpsee.a:
	cd $(GPSEE_SRC_DIR) && $(MAKE) libgpsee.a

$(PRODUCT): $(GPSEE_SRC_DIR)/libgpsee.a $(SHELL_BUILD)/$(PRODUCT).mk $(SHELL_BUILD)/$(PRODUCT).cpp
ifeq "$(STREAM)" "surelynx"
	cd $(SHELL_BUILD) && $(MAKE) -f $(SURELYNX_SRC)/vars.mk surelynx_build_platform.h
endif
	[ -d "$(SHELL_BUILD)/.deps" ] || mkdir "$(SHELL_BUILD)/.deps"
	cd $(SHELL_BUILD) && $(MAKE) -f $(SHELL_BUILD)/$(PRODUCT).mk "GPSEE_JS_CXXFLAGS=$(GPSEE_JS_CXXFLAGS)" "GPSEE_JS_LDFLAGS=$(GPSEE_JS_LDFLAGS)" $@
	[ -f $(SHELL_BUILD)/$(PRODUCT) ] && $(CP) $(SHELL_BUILD)/$(PRODUCT) .

clean_shell_build: $(SHELL_BUILD)/$(PRODUCT).mk
	cd $(SHELL_BUILD) && make -f $(SHELL_BUILD)/$(PRODUCT).mk clean






