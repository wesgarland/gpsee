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

## @file	Makefile	GPSEE Makefile. Build instructions for GPSEE and its modules.
## @author	Wes Garland, PageMail, Inc., wes@page.ca
## @date	August 2007
## @version	$Id: Makefile,v 1.5 2009/04/01 22:30:55 wes Exp $

# BUILD		DEBUG | DRELEASE | PROFILE | RELEASE
# STREAM	unix | surelynx | apr

export BUILD	= DEBUG
export STREAM	= unix

ALL_MODULES		?= $(filter-out $(IGNORE_MODULES) ., $(shell cd modules && find . -type d -name '[a-z]*' -prune | sed 's;^./;;') $(shell cd $(STREAM)_modules && find . -type d -name '[a-z]*' -prune | sed 's;^./;;'))
IGNORE_MODULES		= pairodice
INTERNAL_MODULES 	= vm system

top: all

GPSEE_SRC_DIR ?= $(shell pwd)
export GPSEE_SRC_DIR
PWD = $(shell pwd)

include $(GPSEE_SRC_DIR)/$(STREAM)_stream.mk
include $(GPSEE_SRC_DIR)/system_detect.mk
-include $(GPSEE_SRC_DIR)/local_config.mk

AR_MODULES		= $(filter $(INTERNAL_MODULES), $(ALL_MODULES))
SO_MODULES		= $(filter-out $(INTERNAL_MODULES), $(ALL_MODULES))

AR_MODULE_DIRS_GLOBAL	= $(wildcard $(foreach MODULE, $(AR_MODULES), modules/$(MODULE)))
SO_MODULE_DIRS_GLOBAL	= $(wildcard $(foreach MODULE, $(SO_MODULES), modules/$(MODULE)))
AR_MODULE_DIRS_STREAM	= $(wildcard $(foreach MODULE, $(AR_MODULES), $(STREAM)_modules/$(MODULE)))
SO_MODULE_DIRS_STREAM	= $(wildcard $(foreach MODULE, $(SO_MODULES), $(STREAM)_modules/$(MODULE)))
AR_MODULE_DIRS_ALL	= $(AR_MODULE_DIRS_GLOBAL) $(AR_MODULE_DIRS_STREAM)
SO_MODULE_DIRS_ALL	= $(SO_MODULE_DIRS_GLOBAL) $(SO_MODULE_DIRS_STREAM)
AR_MODULE_FILES		= $(foreach MODULE_DIR, $(AR_MODULE_DIRS_ALL), $(MODULE_DIR)/$(notdir $(MODULE_DIR))_module.$(LIB_EXT))
SO_MODULE_FILES		= $(foreach MODULE_DIR, $(SO_MODULE_DIRS_ALL), $(MODULE_DIR)/$(notdir $(MODULE_DIR))_module.$(SOLIB_EXT))

spidermonkey/vars.mk:
	cd spidermonkey && $(MAKE) BUILD=$(BUILD) install
include spidermonkey/vars.mk
ifdef SPIDERMONKEY_SRC
export SPIDERMONKEY_SRC
endif
ifdef SPIDERMONKEY_BUILD
export SPIDERMONKEY_BUILD
endif
include build.mk
-include depend.mk

GPSEE_SOURCES	 	= gpsee.c gpsee_$(STREAM).c gpsee_lock.c gpsee_flock.c gpsee_util.c gpsee_modules.c gpsee_context_private.c
GPSEE_OBJS	 	= $(GPSEE_SOURCES:.c=.o) $(AR_MODULE_FILES)
GPSEE_LIBRARY		= lib$(GPSEE_LIBNAME).$(SOLIB_EXT)

ifneq ($(STREAM),surelynx)
GPSEE_OBJS		+= gpsee_$(STREAM).o
endif

PROGS		 	= gsr
AUTOGEN_HEADERS		+= modules.h
EXPORT_PROGS	 	= $(PROGS) 
EXPORT_SCRIPTS		= sample_programs/jsie.js
EXPORT_LIBS	 	= $(GPSEE_LIBRARY)
EXPORT_LIBEXEC_OBJS 	= $(SO_MODULE_FILES)
EXPORT_HEADERS		= gpsee.h gpsee_lock.c gpsee_flock.h

LOADLIBES		+= -l$(GPSEE_LIBNAME) 
EXTRA_LDFLAGS		+= $(JSAPI_LIBS)
LIB_MOZJS		= $(JSAPI_LIB_DIR)/libmozjs.$(SOLIB_EXT)

.PHONY:	all clean real-clean depend build_debug build_debug_modules show_modules clean_modules src-dist bin-dist
all install: $(GPSEE_OBJS) $(PROGS) $(EXPORT_PROGS) $(EXPORT_LIBS) $(EXPORT_LIBEXEC_OBJS) $(EXPORT_HEADERS) $(SO_MODULE_FILES)

install: sm-install gsr-link
install: EXPORT_PROGS += $(EXPORT_SCRIPTS)

clean: EXTRA_CLEAN_RULE=clean_modules
clean: OBJS += $(wildcard $(GPSEE_OBJS) $(PROGS:=.o) $(AR_MODULES) $(SO_MODULES) $(wildcard ./gpsee_*.o))
real-clean: clean
	cd spidermonkey && $(MAKE) clean

sm-install:
	cd spidermonkey && $(MAKE) BUILD=$(BUILD) install

gsr-link:
	[ -h "$(GSR_SHEBANG_LINK)" ] || ln -s "$(BIN_DIR)/gsr" "$(GSR_SHEBANG_LINK)"

gpsee_modules.o: CPPFLAGS += -DDEFAULT_LIBEXEC_DIR=\"$(LIBEXEC_DIR)\" -DDSO_EXTENSION=\"$(SOLIB_EXT)\"

$(PROGS): $(addsuffix .o,$(PROGS))

DEPEND_FILES_X	 = $(addsuffix .X,$(PROGS)) $(GPSEE_OBJS:.o=.X) 
DEPEND_FILES_X	+= $(SO_MODULE_FILES:.$(SOLIB_EXT)=.X)) $(wildcard $(AR_MODULE_FILES:.$(LIB_EXT)=.X))
DEPEND_FILES 	:= $(sort $(wildcard $(DEPEND_FILES_X:.X=.c) $(DEPEND_FILES_X:.X=.cc) $(DEPEND_FILES_X:.X=.cpp) $(DEPEND_FILES_X:.X=.cpp)))

modules.h: Makefile $(STREAM)_stream.mk
	@date '+/* Generated by $(USER) on $(HOSTNAME) at %a %b %e %Y %H:%M:%S %Z */' > $@
	@echo $(INTERNAL_MODULES) \
		| $(TR) ' ' '\n'  \
		| $(SED) -e 's/.*/InternalModule(&)/' \
		>> $@

show_modules:
	@echo 
	@echo "*** GPSEE-$(STREAM) Module Configuration ***"
	@echo
	@echo "Internal Modules:"
	@echo "$(AR_MODULES)" | $(SED) -e 's/  */ /g' | tr ' ' '\n' | $(GREP) -v '^ *$$' | $(SED) 's/^/ - /'
	@echo 
	@echo "DSO Modules:"
	@echo "$(SO_MODULES)" | $(SED) -e 's/  */ /g' | tr ' ' '\n' | $(GREP) -v '^ *$$' | $(SED) 's/^/ - /'
	@echo
	@echo "Documented Modules:"
	@echo  $(wildcard $(foreach MODULE, $(ALL_MODULES), modules/$(MODULE)/$(MODULE).jsdoc $(STREAM)_modules/$(MODULE)/$(MODULE).jsdoc)) |\
		$(SED) -e 's/  */ /g' | tr ' ' '\n' | $(GREP) -v '^ *$$' | $(SED) 's/^/ - /'

clean_modules:
#	@$(foreach DIR, $(dir $(AR_MODULE_FILES) $(SO_MODULE_FILES)), echo && echo " * Cleaning $(DIR)" && cd "$(DIR)" && make -f "$(GPSEE_SRC_DIR)/modules.mk" clean && cd "$(GPSEE_SRC_DIR)";)
	@$(foreach MODULE, $(AR_MODULE_FILES) $(SO_MODULE_FILES), echo && echo " * Cleaning $(dir $(MODULE))" && cd "$(dir $(MODULE))" && make -f "$(GPSEE_SRC_DIR)/modules.mk" MODULE=$(notdir $(MODULE)) clean && cd "$(GPSEE_SRC_DIR)";)

build_modules:: $(AR_MODULE_FILES) $(SO_MODULE_FILES)
build_modules $(AR_MODULE_FILES) $(SO_MODULE_FILES)::
	cd $(dir $@) && $(MAKE) -f $(GPSEE_SRC_DIR)/modules.mk $(notdir $@) MODULE=$(notdir $@)

build_debug_modules:
	@$(foreach DIR, $(dir $(AR_MODULE_FILES) $(SO_MODULE_FILES)), cd "$(DIR)" && make -f "$(GPSEE_SRC_DIR)/modules.mk" build_debug_module && cd "$(GPSEE_SRC_DIR)";)

build_debug: build_debug_modules

GPSEE_RELEASE=0.2-pre1
gpsee-$(GPSEE_RELEASE)_src.tar.gz:: TMPFILE=gpsee_file_list.tmp
gpsee-$(GPSEE_RELEASE)_src.tar.gz:: 
	@$(RM) $(TMPFILE) || true
	ls $(PROGS:=.c) $(GPSEE_SOURCES) Doxyfile Makefile *.mk [A-Z][A-Z][A-Z][A-Z]* \
		gpsee.jsdoc gpsee_*.[ch] gpsee.h \
		| sort -u >> $(TMPFILE)
	find licenses -type f >> $(TMPFILE)
	find modules -type f >> $(TMPFILE)
	find sample_programs -type f >> $(TMPFILE)
	find tests -type f >> $(TMPFILE)
	find docgen -type f >> $(TMPFILE)
	ls spidermonkey/Makefile spidermonkey/*sample >> $(TMPFILE)
	find unix_modules -type f >> $(TMPFILE) 
	find apr_modules -type f >> $(TMPFILE) || [ ! -d apr_modules ]
	[ ! -d gpsee-$(GPSEE_RELEASE) ] || rm -rf gpsee-$(GPSEE_RELEASE)
	egrep -v 'depend.mk$$|~$$|surelynx|^.hg$$|(([^A-Za-z_]|^)CVS([^A-Za-z_]|$$))|,v$$|\.[ao]$$|\.$(SOLIB_EXT)$$|^[	 ]*$$' \
		$(TMPFILE) | gtar -T - -zcf $@
	@echo $(TMPFILE)

src-dist:: DATE_STAMP=$(shell date '+%b-%d-%Y')
src-dist:: COUNT=$(shell ls gpsee-$(GPSEE_RELEASE)*.tar.gz 2>/dev/null | grep -c $(DATE_STAMP))
src-dist:: gpsee-$(GPSEE_RELEASE)_src.tar.gz
	[ ! -d gpsee-$(GPSEE_RELEASE)-$(DATE_STAMP)-$(COUNT) ] || rmdir gpsee-$(GPSEE_RELEASE)
	mkdir gpsee-$(GPSEE_RELEASE)
	cd gpsee-$(GPSEE_RELEASE) && gtar -zxf ../gpsee-$(GPSEE_RELEASE)_src.tar.gz
	gtar -zcvf gpsee-$(GPSEE_RELEASE)-$(DATE_STAMP)-$(COUNT).tar.gz gpsee-$(GPSEE_RELEASE)
	rm -rf gpsee-$(GPSEE_RELEASE)-$(DATE_STAMP)-$(COUNT)

bin-dist:: TARGET=$(UNAME_SYSTEM)-$(UNAME_RELEASE)-$(UNAME_MACHINE)
bin-dist:: DATE_STAMP=$(shell date '+%b-%d-%Y')
bin-dist:: COUNT=$(shell ls $(STREAM)_gpsee_$(TARGET)*.tar.gz 2>/dev/null | grep -c $(DATE_STAMP))
bin-dist:: install
	gtar -zcvf $(STREAM)_gpsee_$(TARGET)-$(DATE_STAMP)-$(COUNT).tar.gz \
		$(foreach FILE, $(notdir $(EXPORT_PROGS)), "$(BIN_DIR)/$(FILE)")\
		$(foreach FILE, $(notdir $(EXPORT_LIBEXEC_OBJS)), "$(LIBEXEC_DIR)/$(FILE)")\
		$(foreach FILE, $(notdir $(EXPORT_LIBS)), "$(SOLIB_DIR)/$(FILE)")\
		$(LIB_MOZJS)

$(GPSEE_LIBRARY): $(GPSEE_OBJS)

gsr: $(GPSEE_LIBRARY)
gsr: gsr.o

JSDOC_DIR=/opt/jsdoc-toolkit
JSDOC_TEMPLATE=$(GPSEE_SRC_DIR)/docgen/jsdoc/templates/pmi
JSDOC_TARGET_DIR=$(GPSEE_SRC_DIR)/docs/modules
JSDOC=cd "$(JSDOC_DIR)" && java -jar jsrun.jar app/run.js -x=jsdoc -a -t=$(JSDOC_TEMPLATE) --directory=$(JSDOC_TARGET_DIR) 

docs::
	@[ -d docs/source/gpsee ] || mkdir -p docs/source/gpsee
	doxygen
	$(JSDOC) $(addprefix $(GPSEE_SRC_DIR)/,$(wildcard $(foreach MODULE, $(ALL_MODULES), modules/$(MODULE)/$(MODULE).jsdoc $(STREAM)_modules/$(MODULE)/$(MODULE).jsdoc)))
	rm doxygen.log
