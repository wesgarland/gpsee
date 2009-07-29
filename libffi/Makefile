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
# Copyright (c) 2007, PageMail, Inc. All Rights Reserved.
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
## @date	May 2009
## @version	$Id: Makefile,v 1.2 2009/07/23 18:42:48 wes Exp $ 
## @file	Makefile 
## 
## GNU make Makefile to allow us to build libffi in a way that
## will be predictably useable by GPSEE embeddings.
##
## This Makefile will build libffi, found in LIBFFI_SRC
## and install it under GPSEE_PREFIX.
##
## @note We do not support LibFFI releases prior to 3.0.8, although
##       older releases are expected to work.
##

BUILDING_LIBFFI=TRUE
include ./local_config.mk

top: build_debug help

LIBFFI_LIB_DIR		?= $(SOLIB_DIR)
LIBFFI_INCLUDE_DIR	?= $(GPSEE_PREFIX_DIR)/include/libffi

CONFIGURE_OPTIONS	+= --prefix=$(LIBFFI_PREFIX)

ifneq (X$(LIBFFI_LIB_DIR),X)
CONFIGURE_OPTIONS	+= --libdir=$(LIBFFI_LIB_DIR)
endif

ifneq (X$(LIBFFI_INCLUDE_DIR),X)
CONFIGURE_OPTIONS	+= --includedir=$(LIBFFI_INCLUDE_DIR)
endif

JS_CONFIG ?= $(JS_CONFIG_DIR)/js-config 

# Debug build?
ifneq (X,X$(filter $(BUILD),DEBUG))
CONFIGURE_OPTIONS       += --enable-debug
endif

# Debug-Release or Profile build? Add debug symbols
ifneq (X,X$(filter $(BUILD),PROFILE DRELEASE ))
LIBFFI_CFLAGS += -g
endif

# Build the GNU make command line for libffi, based
# on details in local-config.mk. All LIBFFI_* variables must
# be set by the time this code is executed.
LIBFFI_MAKE = $(MAKE) $(LIBFFI_MAKE_OPTIONS) 

ifneq (X$(LIBFFI_CFLAGS),X)
LIBFFI_MAKE += "CFLAGS=$(LIBFFI_CFLAGS)"
endif

ifneq (X$(LIBFFI_CXXFLAGS),X)
LIBFFI_MAKE += "CXXFLAGS=$(LIBFFI_CXXFLAGS)"
endif

ifneq (X$(LIBFFI_CPPFLAGS),X)
LIBFFI_MAKE += "CPPFLAGS=$(LIBFFI_CPPFLAGS)"
endif

LIBFFI_BUILD     ?= $(abspath builds/$(shell echo $(BUILD) | $(TR) '[A-Z]' '[a-z]'))

test:
	echo "$(LIBFFI_BUILD)"

.PHONY: top help all build install clean dist-clean distclean build_debug

help:	
	@echo	"Make Targets:"
	@echo	" build         Build libffi"
	@echo	" install       Install libffi and create vars.mk"
	@echo	" build_debug   Show build system debug info"
	@echo	" clean         Clean up this dir"
	@echo	" dist-clean    Clean up libffi install"
	@echo	
	@echo	"To customize your build, edit ./local_config.mk"
	@echo
	@echo	"Note: You can override the BUILD variable on the make command line, e.g."
	@echo	"      make BUILD=RELEASE install"

all:	build

$(LIBFFI_SRC)/configure: 
	@echo	"*** Error: Could not locate $@"
	@[ X = Y ]

$(LIBFFI_BUILD)/Makefile: $(LIBFFI_SRC)/configure #Makefile local_config.mk 
	[ -d $(LIBFFI_BUILD) ] || mkdir -p $(LIBFFI_BUILD)
	cd $(LIBFFI_BUILD) && $(LIBFFI_SRC)/configure $(CONFIGURE_OPTIONS)
	@[ -s $@ ] && touch $@

build: $(LIBFFI_BUILD)/Makefile
ifneq (X,X$(LIBFFI_SRC))
	cd $(LIBFFI_BUILD) && $(LIBFFI_MAKE)
ifneq ($(QUIET),True)
	@echo ""
	@echo " * LibFFI is now built, but needs to be installed. Before installation,"
	@echo "   please insure that the target directory, $(LIBFFI_PREFIX),"
	@echo "   exists and that you have permission to write in it. Then run make install."
	@echo ""
endif
else
	@echo " * Using system-supplied libffi"
endif

JSAPI_LDFLAGS=$(shell $(JS_CONFIG) --libs)
ifeq ($(shell uname -s),Darwin)
JSAPI_LDFLAGS:=$(filter -l%,$(JSAPI_LDFLAGS)) $(filter -L%,$(JSAPI_LDFLAGS))\
	$(filter -%_namespace,$(JSAPI_LDFLAGS))\
	$(filter -Wl%,$(JSAPI_LDFLAGS))
endif

install: build
	@echo "*  Installing LibFFI in $(LIBFFI_LIB_DIR), "
	@echo "  headers in $(LIBFFI_INCLUDE_DIR)."
	cd $(LIBFFI_BUILD) && $(LIBFFI_MAKE) install
	@echo " * Building vars.mk"
	@echo "LIBFFI_CONFIG_DEPS=libffi/vars.mk libffi/local_config.mk" > vars.mk
	@echo "LIBFFI_LDFLAGS=$(shell PKG_CONFIG_PATH=$(LIBFFI_LIB_DIR)/pkgconfig $(PKG_CONFIG) libffi --libs)" >> vars.mk
	@echo "LIBFFI_CFLAGS=$(shell PKG_CONFIG_PATH=$(LIBFFI_LIB_DIR)/pkgconfig $(PKG_CONFIG) libffi --cflags)" >> vars.mk

ifneq ($(QUIET),True)
	@echo ""
	@echo " * LibFFI Build & Installation Complete: you can build GPSEE FFI modules now."
endif

clean:
	cd $(LIBFFI_BUILD) && $(LIBFFI_MAKE) clean || /bin/true
	$(RM) $(LIBFFI_BUILD)/Makefile || /bin/true
	[ ! -f vars.mk ] || $(RM) vars.mk || /bin/true
	$(RM) -r .libs || /bin/true
	rmdir $(shell find $(LIBFFI_BUILD) -type d) 2>/dev/null || /bin/true
	@echo " * Done clean - ignore errors" 

distclean dist-clean: clean
	cd $(LIBFFI_BUILD) && $(LIBFFI_MAKE) distclean || /bin/true
	$(RM) -r $(LIBFFI_BUILD)
	@echo " * Done dist-clean - ignore errors" 

ifeq (X$(LIBFFI_LIB_DIR),X)
build_debug:	LIBFFI_LIB_DIR = "UNDEFINED LOCATION"
endif

ifeq (X$(LIBFFI_INCLUDE_DIR),X)
build_debug:	LIBFFI_INCLUDE_DIR = "UNDEFINED LOCATION"
endif

build_debug: DRELEASE="Debuggable-Release"
build_debug: RELEASE="Release"
build_debug: DEBUG="Debugging"
build_debug: PROFILE="Profiling"
build_debug:
	@echo
	@echo "This is a $($(BUILD)) build."
	@echo "It will be installed under $(LIBFFI_PREFIX)"
	@echo " - libffi will be installed in $(LIBFFI_LIB_DIR)"
	@echo " - headers will be installed in $(LIBFFI_INCLUDE_DIR)"
	@echo 
	@echo "Make will be invoked in $(LIBFFI_BUILD) as:"
	@echo "  $(LIBFFI_MAKE)"
	@echo
	@echo "Configure will be invoked as:"
	@echo "$(LIBFFI_SRC)/configure $(CONFIGURE_OPTIONS)"
	@echo
	@echo "CC=$(CC)"
	@echo "PATH=$(PATH)"


