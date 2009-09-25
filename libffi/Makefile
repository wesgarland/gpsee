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
## @version	$Id: Makefile,v 1.5 2009/09/14 20:26:49 wes Exp $ 
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

GPSEE_SRC_DIR ?= ..
GPSEE_SRC_DIR := $(abspath $(GPSEE_SRC_DIR))

include $(GPSEE_SRC_DIR)/local_config.mk
include $(GPSEE_SRC_DIR)/system_detect.mk
-include $(GPSEE_SRC_DIR)/$(UNAME_SYSTEM)_config.mk   
-include $(GPSEE_SRC_DIR)/$(STREAM)_stream.mk

NO_BUILD_RULES=True
include $(GPSEE_SRC_DIR)/build.mk

include local_config.mk

ifneq ($(MAKECMDGOALS)X, X)
ifneq ($(MAKECMDGOALS), help)
LIBFFI_LIB_DIR		?= $(SOLIB_DIR)
LIBFFI_INCLUDE_DIR	?= $(GPSEE_PREFIX_DIR)/include/libffi
include sanity.mk
LIBFFI_BUILD     ?= $(abspath builds/$(shell echo $(BUILD) | $(TR) '[A-Z]' '[a-z]'))
endif
endif

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

top:
	$(MAKE) help

.PHONY: top help all build install clean dist-clean distclean build_debug

help:	
	@echo
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
	@echo

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
	@if [ "$(QUIET)" != "True" ]; then \
	  echo ""; \
	  echo " * LibFFI is now built, but needs to be installed. Before installation," 	;\
	  echo "   please insure that the target directory, $(LIBFFI_PREFIX),"			;\
	  echo "   exists and that you have permission to write in it. Then run make install."	;\
	  echo "" ;\
	fi
else
	@echo " * Using system-supplied libffi UNSUPPORTED"
endif

install:
	@$(MAKE) $(MAKEFLAGS) QUIET=True build
	@$(MAKE) $(MAKEFLAGS) _install
	@$(MAKE) $(MAKEFLAGS) vars.mk
 
#ifneq (X$(findstring $(MAKECMDGOALS),_install vars.mk),X)
#JSAPI_LDFLAGS=$(shell $(JS_CONFIG) --libs)
#ifeq ($(shell uname -s),Darwin)
#JSAPI_LDFLAGS:=$(filter -l%,$(JSAPI_LDFLAGS)) $(filter -L%,$(JSAPI_LDFLAGS))\
#	$(filter -%_namespace,$(JSAPI_LDFLAGS))\
#	$(filter -Wl%,$(JSAPI_LDFLAGS))
#endif
#endif

_install: QUIET=True
_install:
	@echo " * Installing LibFFI"
	@rm vars.mk >/dev/null || [ X = X ]
	@echo "*  Installing LibFFI in $(LIBFFI_LIB_DIR), "
	@echo "  headers in $(LIBFFI_INCLUDE_DIR)."
	[ -d $(LIBFFI_INCLUDE_DIR) ] || mkdir -p $(LIBFFI_INCLUDE_DIR)
	[ -d $(LIBFFI_LIB_DIR) ] || mkdir -p $(LIBFFI_LIB_DIR)
	[ -w $(LIBFFI_INCLUDE_DIR) ] || echo "**** cannot write to $(LIBFFI_INCLUDE_DIR): did you forget to read the build instructions?"
	[ -w $(LIBFFI_LIB_DIR) ] || echo "**** cannot write to $(LIBFFI_LIB_DIR): did you forget to read the build instructions?"
	[ -w $(LIBFFI_INCLUDE_DIR) ] && [ -w $(LIBFFI_LIB_DIR) ]
	[ "$(LIBFFI_LIB_DIR)" != "" ]
	cd $(LIBFFI_BUILD) && $(LIBFFI_MAKE) install
	@echo ""
ifneq ($(QUIET),True)
	@echo " * LibFFI Build & Installation Complete"
endif

vars.mk:
	@echo " * Building vars.mk"
	@echo "LIBFFI_CONFIG_DEPS=$(GPSEE_SRC_DIR)/libffi/vars.mk $(GPSEE_SRC_DIR)/libffi/local_config.mk" >> vars.mk
	@echo "LIBFFI_LDFLAGS=$(shell PKG_CONFIG_PATH=$(LIBFFI_LIB_DIR)/pkgconfig $(PKG_CONFIG) libffi --libs)" >> vars.mk
	@echo "LIBFFI_CFLAGS=$(shell PKG_CONFIG_PATH=$(LIBFFI_LIB_DIR)/pkgconfig $(PKG_CONFIG) libffi --cflags)" >> vars.mk
	@echo "LIBFFI_LIB_DIR=$(LIBFFI_LIB_DIR)" >> vars.mk

clean:
	cd $(LIBFFI_BUILD) && $(LIBFFI_MAKE) clean || [ X = X ]
	$(RM) $(LIBFFI_BUILD)/Makefile || [ X = X ]
	[ ! -f vars.mk ] || $(RM) vars.mk || [ X = X ]
	$(RM) -r .libs || [ X = X ]
	rmdir $(shell find $(LIBFFI_BUILD) -type d) 2>/dev/null || [ X = X ]
	@echo " * Done clean - ignore errors" 

distclean dist-clean: clean
	cd $(LIBFFI_BUILD) && $(LIBFFI_MAKE) distclean || [ X = X ]
	$(RM) -r $(LIBFFI_BUILD)
	@echo " * Done dist-clean - ignore errors" 

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

test:
	@echo "LIBFFI_CFLAGS=$(shell PKG_CONFIG_PATH=$(LIBFFI_LIB_DIR)/pkgconfig $(PKG_CONFIG) libffi --libs)"

