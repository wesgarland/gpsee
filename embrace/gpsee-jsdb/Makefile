#!gmake
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
## @file        Makefile        Makefile to build an updated jsd/jsdb, as well 
##				as the Mozilla JS Shell with jsdb and GPSEE enabled.
##                              Requires that GPSEE already be built and installed.
## @author      Wes Garland, PageMail, Inc., wes@page.ca
## @date        May 2010
## @version     $Id: Makefile,v 1.6 2010/04/28 12:45:52 wes Exp $

top: build

GPSEE_CONFIG    ?= ../../gpsee-config
include $(shell $(GPSEE_CONFIG) --gpsee-src)/outside.mk

LIBRARY			= libgpsee-jsdb.$(SOLIB_EXT)
EXPORT_LIBS		+= $(LIBRARY)
EXPORT_PROGS		+= gpsee-jsdb
PROGS			+= js gpsee-jsdb

build: $(EXPORT_LIBS) $(EXPORT_PROGS)

SPIDERMONKEY_SRC	?= $(shell $(GPSEE_CONFIG) --spidermonkey-src)

CPPSRCS         = \
                  jsdebug.cpp \
                  jsd_atom.cpp \
                  jsd_high.cpp \
                  jsd_hook.cpp \
                  jsd_lock.cpp \
                  jsd_obj.cpp \
                  jsd_scpt.cpp \
                  jsd_stak.cpp \
                  jsd_step.cpp \
                  jsd_text.cpp \
                  jsd_val.cpp \
		  jsdb.cpp \
		  jsdrefl.cpp \
                  $(NULL)

OBJS 		= $(CPPSRCS:.cpp=.o) gpsee-jsdb.o jsworkers.o
$(LIBRARY):	$(CPPSRCS:.cpp=.o) 

HERE		:= $(PWD)
JS		:= $(dir $(SPIDERMONKEY_SRC))
JSD		:= $(HERE)/jsd
EXTRA_CPPFLAGS	+= -DEXPORT_JSD_API -DMOZILLA_CLIENT
VPATH		+= $(JSD) $(JSD)/jsdb $(JS)/src/editline
CPPFLAGS	+= -I$(JSD)

GPSEE_INCLUDE_DIR       ?= $(shell $(GPSEE_CONFIG) --includedir)
SPIDERMONKEY_BUILD      ?= $(shell $(GPSEE_CONFIG) --spidermonkey-build)
SHELL_BUILD             = $(SPIDERMONKEY_BUILD)/shell
EXTRA_CLEAN_RULE        = clean_shell_build
GPSEE_JS_CXXFLAGS       += -I$(GPSEE_INCLUDE_DIR) -I$(SPIDERMONKEY_SRC) $(shell $(GPSEE_CONFIG) --cppflags) -DJSDEBUGGER -I$(JSD) -I$(JSD)/jsdb
GPSEE_JS_LDFLAGS        += $(shell $(GPSEE_CONFIG) --ldflags) -lgpsee-jsdb -L$(HERE)
EXPORT_PROGS            = gpsee-jsdb $(JSD)/jsdb/debugger.js

$(SHELL_BUILD)/gpsee-jsdb.mk: Makefile $(SHELL_BUILD)/Makefile $(SHELL_BUILD)/gpsee-jsdb.cpp
	@echo " * Building $(SHELL_BUILD)/gpsee-jsdb.mk"
	@$(SED) -e 's/ js.cpp/gpsee-jsdb.cpp/' -e 's/= js/= gpsee-jsdb/' <$(SHELL_BUILD)/Makefile >$@
	@echo "GARBAGE += gpsee-jsdb.mk gpsee-jsdb.cpp" >> $@
	@echo 'CXXFLAGS := $$(GPSEE_JS_CXXFLAGS) $$(CXXFLAGS)' >> $@
	@echo 'LDFLAGS := $$(GPSEE_JS_LDFLAGS) $$(LDFLAGS)' >> $@

$(SHELL_BUILD)/gpsee-jsdb.cpp: ../gpsee-js/gpsee-js.cpp
	$(CP) ../gpsee-js/gpsee-js.cpp $@

gpsee-jsdb: $(SHELL_BUILD)/gpsee-jsdb.mk $(SHELL_BUILD)/gpsee-jsdb.cpp
ifeq "$(STREAM)" "surelynx"
	cd $(SHELL_BUILD) && $(MAKE) -f $(SURELYNX_SRC)/vars.mk surelynx_build_platform.h
endif
	[ -d "$(SHELL_BUILD)/.deps" ] || mkdir "$(SHELL_BUILD)/.deps"
	cd $(SHELL_BUILD) && $(MAKE) -f $(SHELL_BUILD)/gpsee-jsdb.mk "GPSEE_JS_CXXFLAGS=$(GPSEE_JS_CXXFLAGS)" "GPSEE_JS_LDFLAGS=$(GPSEE_JS_LDFLAGS)" $@
	[ -f $(SHELL_BUILD)/gpsee-jsdb ] && $(CP) $(SHELL_BUILD)/gpsee-jsdb .

clean_shell_build: $(SHELL_BUILD)/gpsee-jsdb.mk
	cd $(SHELL_BUILD) && make -f $(SHELL_BUILD)/gpsee-jsdb.mk clean
