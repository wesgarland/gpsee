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
# ***** END LICENSE BLOCK ***** */

## @author	Wes Garland, PageMail, Inc. wes@page.ca
## @date	Jan 2008
## @version	$Id: Makefile,v 1.13 2011/12/05 19:13:38 wes Exp $ 
## @file	Makefile 
## 
## GNU make Makefile to allow us to build Mozilla SpiderMonkey in a
## way that will play nice with gpsee embeddings.
##
## This Makefile will build SpiderMonkey, found in SPIDERMONKEY_SRC
## and install it under GPSEE_PREFIX.
##
## @note We do not support SpiderMonkey releases prior to autoconf,
##       which arrived with version 1.8.1.
##
## @bug DRELEASE and PROFILE are not finished yet.

GPSEE_SRC_DIR ?= ..
GPSEE_SRC_DIR := $(abspath $(GPSEE_SRC_DIR))

ifneq (X,$(wildcard js/src/configure.in)X)
SPIDERMONKEY_SRC=$(shell pwd)/js/src
endif

include $(GPSEE_SRC_DIR)/local_config.mk
include $(GPSEE_SRC_DIR)/system_detect.mk
-include $(GPSEE_SRC_DIR)/$(UNAME_SYSTEM)_config.mk   
NO_BUILD_RULES = TRUE
-include $(GPSEE_SRC_DIR)/$(STREAM)_stream.mk
include local_config.mk

ifneq ($(MAKECMDGOALS),help)
ifeq ($(STAGED_SPIDERMONKEY_BUILD),TRUE)
JSAPI_PREFIX		?= $(SPIDERMONKEY_BUILD)/jsapi
else
JSAPI_PREFIX		?= $(GPSEE_PREFIX_DIR)/jsapi
endif
LIB_DIR			?= $(JSAPI_PREFIX)/lib
INCLUDE_DIR		?= $(JSAPI_PREFIX)/include
include sanity.mk
SPIDERMONKEY_BUILD	?= $(abspath builds/$(shell echo $(BUILD) | $(TR) '[A-Z]' '[a-z]'))
endif

CONFIGURE_OPTIONS	+= --enable-threadsafe --enable-ctypes

top: 
	$(MAKE) help
	$(MAKE) build

CONFIGURE_OPTIONS	+= --prefix=$(JSAPI_PREFIX)

ifneq (X$(LIB_DIR),X)
CONFIGURE_OPTIONS	+= --libdir=$(LIB_DIR)
endif

ifneq (X$(INCLUDE_DIR),X)
CONFIGURE_OPTIONS	+= --includedir=$(INCLUDE_DIR)
endif

ifneq (X$(BIN_DIR),X)
CONFIGURE_OPTIONS	+= --bindir=$(BIN_DIR)
endif

JS_CONFIG ?= $(SPIDERMONKEY_BUILD)/js-config 

# Debug build?
ifneq (X,X$(filter $(BUILD),DEBUG))
CONFIGURE_OPTIONS       += --enable-debug --disable-optimize
endif

# Debug or Debug-Release build? Enable ability for zealous GC
ifneq (X,X$(filter $(BUILD),DEBUG DRELEASE))
CONFIGURE_OPTIONS       += --enable-gczeal
endif

# Debug-Release or Profile build? Add debug symbols
ifneq (X,X$(filter $(BUILD),PROFILE DRELEASE ))
SM_CFLAGS += -g
SM_CXXFLAGS += -g
endif

# Build the GNU make command line for Spidermonkey, based
# on details in local-config.mk. All SM_* variables must
# be set by the time this code is executed.
SM_MAKE = $(MAKE) $(SM_MAKE_OPTIONS) 

ifneq (X$(SM_CFLAGS),X)
SM_MAKE += "CFLAGS=$(SM_CFLAGS)"
endif

ifneq (X$(SM_CXXFLAGS),X)
SM_MAKE += "CXXFLAGS=$(SM_CXXFLAGS)"
endif

ifneq (X$(SM_CPPFLAGS),X)
SM_MAKE += "CPPFLAGS=$(SM_CPPFLAGS)"
endif

.PHONY: top help all build install clean dist-clean distclean build_debug

help:	
	@echo
	@echo	"Make Targets:"
	@echo	" build         Build SpiderMonkey"
	@echo	" install       Install SpiderMonkey and create vars.mk"
	@echo	" build_debug   Show build system debug info"
	@echo	" clean         Clean up this dir and SpiderMonkey's"
	@echo	" dist-clean    Clean up SpiderMonkey install"
	@echo	
	@echo	"To customize your build, edit ./local_config.mk"
	@echo
	@echo	"Note: You can override the BUILD variable on the make command line, e.g."
	@echo	"      make BUILD=RELEASE install"
	@echo

all:	build

$(SPIDERMONKEY_SRC)/configure: $(SPIDERMONKEY_SRC)/configure.in
	@echo " * Generating $@"
	@[ -x $(AUTOCONF) ] || echo " *** $(AUTOCONF): command not found"
	@[ -x $(AUTOCONF) ]
	cd $(SPIDERMONKEY_SRC) && $(AUTOCONF)

$(JS_CONFIG) $(SPIDERMONKEY_BUILD)/Makefile: $(SPIDERMONKEY_SRC)/configure #Makefile local_config.mk 
	[ -d $(SPIDERMONKEY_BUILD) ] || mkdir -p $(SPIDERMONKEY_BUILD)
	cd $(SPIDERMONKEY_BUILD) && CC="$(CC)" CXX="$(CXX)" $(CONFIGURE_ENV_VARS) $(SPIDERMONKEY_SRC)/configure $(CONFIGURE_OPTIONS)
	@[ -s $@ ] && touch $@

configure: $(SPIDERMONKEY_BUILD)/Makefile vars.mk
force_configure:
	$(RM) vars.mk $(SPIDERMONKEY_BUILD)/Makefile $(SPIDERMONKEY_BUILD)/config.cache $(SPIDERMONKEY_BUILD)/ctypes/libffi/config.cache
	$(MAKE) configure

ifeq ($(UNAME_SYSTEM),Darwin)
MOZJS_INSTALL_NAME_OPT=-Wl,-install_name,$(LIB_DIR)/libmozjs.dylib
export EXTRA_LIBS
build: EXTRA_LIBS += $(MOZJS_INSTALL_NAME_OPT)
endif
build: $(SPIDERMONKEY_BUILD)/Makefile
	cd $(SPIDERMONKEY_BUILD) && $(SM_MAKE)
ifneq ($(QUIET),True)
	@echo ""
	@echo " * SpiderMonkey is now built, but needs to be installed. Before installation,"
	@echo "   please insure that the target directory, $(JSAPI_PREFIX),"
	@echo "   exists and that you have permission to write in it. Then run make install."
	@echo ""
endif

install:
	@$(MAKE) $(MAKEFLAGS) QUIET=True build
	@$(MAKE) $(MAKEFLAGS) _install
	@$(MAKE) $(MAKEFLAGS) vars.mk
	@[ ! "$(SUDO_USER)" ] || $(CHOWN) $(SUDO_USER):$(SUDO_GROUP) vars.mk
ifneq ($(QUIET),True)
	@echo " * SpiderMonkey Build & Installation Complete"
endif

ifeq (X,X$(filter $(MAKECMDGOALS),configure force_configure))
vars.mk: JSAPI_LDFLAGS=$(shell $(JS_CONFIG) --libs)
ifeq ($(UNAME_SYSTEM),Darwin)
# Work around a long-standing bug in js-config for Darwin
vars.mk: JSAPI_LDFLAGS:=$(filter -l%,$(JSAPI_LDFLAGS)) $(filter -L%,$(JSAPI_LDFLAGS))\
	$(filter -%_namespace,$(JSAPI_LDFLAGS))\
	$(filter -Wl%,$(JSAPI_LDFLAGS))
vars.mk: JSAPI_LDFLAGS:=$(filter-out $(MOZJS_INSTALL_NAME_OPT),$(JSAPI_LDFLAGS))
endif
# Work around CTypes bug in js-config
vars.mk: JSAPI_LDFLAGS:=$(filter-out %libffi.a,$(JSAPI_LDFLAGS))
endif

_install:
	@echo " * Installing Spidermonkey"	
	@$(RM) vars.mk 2>/dev/null || [ X = X ]
	@[ -d $(INCLUDE_DIR) ] || mkdir -p $(INCLUDE_DIR)
	@[ -d $(LIB_DIR) ] || mkdir -p $(LIB_DIR)
	@[ -w $(INCLUDE_DIR) ] || echo "**** cannot write to $(INCLUDE_DIR): did you forget to read the build instructions?"
	@[ -w $(LIB_DIR) ] || echo "**** cannot write to $(LIB_DIR): did you forget to read the build instructions?"
	@[ -w $(INCLUDE_DIR) ] && [ -w $(LIB_DIR) ]

	cd $(SPIDERMONKEY_BUILD) && $(SM_MAKE) install
	@[ -f $(JS_CONFIG) ] || echo "**** js-config was not installed. Build incomplete." 
	@[ -f $(JS_CONFIG) ] || echo "**** js-config should have been installed as $(JS_CONFIG)." 
	@[ -f $(JS_CONFIG) ] || echo "**** Abort."
	@[ -f $(JS_CONFIG) ]
	@echo ""

vars.mk: $(JS_CONFIG)
	@echo " * Building vars.mk"
	@echo "# Generated on `date` by $(USER)" > vars.mk
	@echo "JSAPI_INCLUDE_DIR=$(shell $(JS_CONFIG) --includedir)" >> vars.mk
	@echo "JSAPI_LIB_DIR=$(shell $(JS_CONFIG) --libdir)" >> vars.mk
	@echo "JSAPI_LIBS=$(JSAPI_LDFLAGS)" >> vars.mk
	@echo "JSAPI_CFLAGS=$(shell $(JS_CONFIG) --cflags)" >> vars.mk
	@echo "JSAPI_BUILD_TYPE=$(BUILD)" >> vars.mk
	@echo "SPIDERMONKEY_SRC=$(SPIDERMONKEY_SRC)" >> vars.mk
	@echo "SPIDERMONKEY_BUILD=$(SPIDERMONKEY_BUILD)" >> vars.mk
	@echo "STAGED_SPIDERMONKEY_BUILD = $(STAGED_SPIDERMONKEY_BUILD)" >> vars.mk

	@[ "$(SUDO_USER)" = "" ] || chown $(SUDO_USER) vars.mk

clean:
	cd $(SPIDERMONKEY_BUILD) && $(SM_MAKE) clean
	[ ! -f vars.mk ] || $(RM) vars.mk

conf-clean: clean
	cd $(SPIDERMONKEY_BUILD) && $(SM_MAKE) clean
	$(RM) $(SPIDERMONKEY_BUILD)/config.cache $(SPIDERMONKEY_BUILD)/ctypes/libffi/config.cache

distclean dist-clean: clean
	cd $(SPIDERMONKEY_BUILD) && $(SM_MAKE) distclean

build_debug: DRELEASE="Debuggable-Release"
build_debug: RELEASE="Release"
build_debug: DEBUG="Debugging"
build_debug: PROFILE="Profiling"
build_debug:
	@echo
	@echo "This is a $($(BUILD)) build."
	@echo "It will be installed under $(JSAPI_PREFIX)"
	@echo " - libmozjs will be installed in $(LIB_DIR)"
	@echo " - headers will be installed in $(INCLUDE_DIR)"
	@echo 
	@echo "Make will be invoked in $(SPIDERMONKEY_BUILD) as:"
	@echo "  $(SM_MAKE)"
	@echo
	@echo "Configure will be invoked as:"
	@echo "cd $(SPIDERMONKEY_BUILD) && CC="$(CC)" CXX="$(CXX)" $(CONFIGURE_ENV_VARS) $(SPIDERMONKEY_SRC)/configure $(CONFIGURE_OPTIONS)"
	@echo
	@echo "JS_CONFIG = $(JS_CONFIG)"
	@echo

$(SPIDERMONKEY_SRC)/configure.in:
	@echo > /dev/stderr
	@echo " *** Please install SpiderMonkey in $(SPIDERMONKEY_SRC) (could not locate configure.in)" >/dev/stderr
	@echo > /dev/stderr
	@[ X = Y ]

local_config.mk:
	@echo
	@echo
	@echo	" ***************************************************************************"
	@echo	" * Please copy:"
	@echo   " *      $(realpath .)/local_config.mk.sample"
	@echo	" * to "
	@echo   " *      $(realpath .)/local_config.mk,"
	@echo   " * then edit it to suit your environment and needs."
	@echo	" ***************************************************************************"
	@echo
	@echo
	@echo
	@[ X = Y ]
