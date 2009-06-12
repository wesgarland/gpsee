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
## @version	$Id: Makefile,v 1.10 2009/06/12 17:01:20 wes Exp $

# BUILD		DEBUG | DRELEASE | PROFILE | RELEASE
# STREAM	unix | surelynx | apr

export BUILD	= DEBUG
export STREAM	= unix
SUBMAKE_QUIET	= False

ALL_MODULES		?= $(filter-out $(IGNORE_MODULES) ., $(shell cd modules && find . -type d -name '[a-z]*' -prune | sed 's;^./;;') $(shell cd $(STREAM)_modules && find . -type d -name '[a-z]*' -prune | sed 's;^./;;'))
IGNORE_MODULES		= pairodice file ffi
INTERNAL_MODULES 	= vm system

top: all

PWD = $(shell pwd)
GPSEE_SRC_DIR ?= $(shell pwd)
export GPSEE_SRC_DIR

# Must define GPSEE_LIBRARY above system_detect.mk so platform
# includes can do target-based build variable overrides
GPSEE_LIBRARY		= lib$(GPSEE_LIBNAME).$(SOLIB_EXT)

include $(GPSEE_SRC_DIR)/$(STREAM)_stream.mk
include $(GPSEE_SRC_DIR)/system_detect.mk
-include $(GPSEE_SRC_DIR)/local_config.mk

export GPSEE_SRC_DIR

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

# PROGS must appear before build.mk until darwin-ld.sh is obsolete.
PROGS		 	= gsr minimal

spidermonkey/vars.mk:
	cd spidermonkey && $(MAKE) BUILD=$(BUILD) QUIET=$(SUBMAKE_QUIET) install
include spidermonkey/vars.mk
ifdef SPIDERMONKEY_SRC
export SPIDERMONKEY_SRC
endif
ifdef SPIDERMONKEY_BUILD
export SPIDERMONKEY_BUILD
endif
include build.mk
-include depend.mk

GPSEE_SOURCES	 	= gpsee.c gpsee_$(STREAM).c gpsee_lock.c gpsee_flock.c gpsee_util.c gpsee_modules.c gpsee_context_private.c gpsee_xdrfile.c
GPSEE_OBJS	 	= $(GPSEE_SOURCES:.c=.o) $(AR_MODULE_FILES)
GPSEE_LIBRARY		= lib$(GPSEE_LIBNAME).$(SOLIB_EXT)

ifneq ($(STREAM),surelynx)
GPSEE_OBJS		+= gpsee_$(STREAM).o
endif

AUTOGEN_HEADERS		+= modules.h gpsee_config.h
EXPORT_PROGS	 	= gsr gpsee-config
EXPORT_SCRIPTS		= sample_programs/jsie.js
EXPORT_LIBS	 	= $(GPSEE_LIBRARY)
EXPORT_LIBEXEC_OBJS 	= $(SO_MODULE_FILES)
EXPORT_HEADERS		= gpsee.h gpsee_config.h gpsee_lock.c gpsee_flock.h 
EXPORT_HEADERS		+= $(wildcard gpsee_$(STREAM).h)

LOADLIBES		+= -l$(GPSEE_LIBNAME)
$(PROGS): LDFLAGS	:= -L. $(LDFLAGS) $(JSAPI_LIBS)

.PHONY:	all clean real-clean depend build_debug build_debug_modules show_modules clean_modules src-dist bin-dist
build all install: $(GPSEE_OBJS) $(EXPORT_LIBS) $(PROGS) $(EXPORT_PROGS) $(EXPORT_LIBEXEC_OBJS) $(EXPORT_HEADERS) $(SO_MODULE_FILES)

install: sm-install gsr-link
install: EXPORT_PROGS += $(EXPORT_SCRIPTS)

clean: EXTRA_CLEAN_RULE=clean_modules
clean: OBJS += $(wildcard $(GPSEE_OBJS) $(PROGS:=.o) $(AR_MODULES) $(SO_MODULES) $(wildcard ./gpsee_*.o)) doxygen.log
real-clean: clean
	cd spidermonkey && $(MAKE) clean

sm-install:
	cd spidermonkey && $(MAKE) BUILD=$(BUILD) QUIET=True install

gsr-link:
	[ -h "$(GSR_SHEBANG_LINK)" ] || ln -s "$(BIN_DIR)/gsr" "$(GSR_SHEBANG_LINK)"

gpsee_modules.o: CPPFLAGS += -DDEFAULT_LIBEXEC_DIR=\"$(LIBEXEC_DIR)\" -DDSO_EXTENSION=\"$(SOLIB_EXT)\"

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
	$(foreach MODULE, $(AR_MODULE_FILES) $(SO_MODULE_FILES), \
	echo && echo " * Cleaning $(dir $(MODULE))" && cd "$(GPSEE_SRC_DIR)/$(dir $(MODULE))" && \
	ls && \
	make -f "$(GPSEE_SRC_DIR)/modules.mk" MODULE=$(notdir $(MODULE)) STREAM=$(STREAM) GPSEE_SRC_DIR=$(GPSEE_SRC_DIR) clean;\
	)

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
	ls spidermonkey/Makefile spidermonkey/*.sample >> $(TMPFILE)
	find unix_modules -type f >> $(TMPFILE) 
	find apr_modules -type f >> $(TMPFILE) || [ ! -d apr_modules ]
	[ ! -d gpsee-$(GPSEE_RELEASE) ] || rm -rf gpsee-$(GPSEE_RELEASE)
	egrep -v 'core$$|depend.mk$$|~$$|surelynx|^.hg$$|(([^A-Za-z_]|^)CVS([^A-Za-z_]|$$))|,v$$|\.[ao]$$|\.$(SOLIB_EXT)$$|^[	 ]*$$' \
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

$(GPSEE_LIBRARY): $(GPSEE_OBJS) $(AR_MODULE_FILES)

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

README2: 
	lynx -dump -display_charset=US-ASCII -force_html -image_links=off -nocolor -noexec -nolist -nolog -nonumbers -nopause -noredir -noreverse \
	     -nostatus -notitle -nounderline -pseudo_inlines=off -stderr -underline_links=off -underscore=on -width=80 \
	     http://kenai.com/projects/gpsee/pages/BuildingGPSEE > $@

gpsee_config.h: Makefile $(wildcard *.mk)
	@echo " * Generating $@"
	@echo "/* Generated `date` by $(USER) on $(HOSTNAME) */ " > $@
	echo "$(foreach DEFINE, $(GPSEE_C_DEFINES),@#if !defined($(DEFINE))@# define $(DEFINE)@#endif@)" \
		| $(TR) @ '\n' | $(SED) -e "s/=/ /" >> $@
	@echo "#if defined(HAVE_IDENTITY_TRANSCODING_ICONV) && !defined(HAVE_ICONV)" >> $@
	@echo "# define HAVE_ICONV" >> $@
	@echo "#endif" >> $@
	@echo "#define GPSEE_$(BUILD)_BUILD" >> $@
	@echo "#define GPSEE_$(shell echo $(STREAM) | $(TR) a-z A-Z)_STREAM" >> $@
	@echo "#define GPSEE_$(shell echo $(UNAME_SYSTEM) | $(TR) a-z A-Z)_SYSTEM" >> $@

gpsee-config: gpsee-config.template Makefile local_config.mk spidermonkey/local_config.mk spidermonkey/vars.mk
	@echo " * Generating $@"
	@$(SED) \
		-e 's;@@CFLAGS@@;$(CFLAGS);g'\
		-e 's;@@LDFLAGS@@;$(LDFLAGS) -l$(GPSEE_LIBNAME) $(JSAPI_LIBS);g'\
		-e 's;@@CPPFLAGS@@;$(CPPFLAGS);g'\
		-e 's;@@CXXFLAGS@@;$(CXXFLAGS);g'\
		-e 's;@@LOADLIBES@@;$(LOADLIBES);g'\
		-e 's;@@GPSEE_PREFIX_DIR@@;$(GPSEE_PREFIX_DIR);g'\
		-e 's;@@LIBEXEC_DIR@@;$(LIBEXEC_DIR);g'\
		-e 's;@@BIN_DIR@@;$(BIN_DIR);g'\
		-e 's;@@SOLIB_DIR@@;$(SOLIB_DIR);g'\
		-e 's;@@GPSEE_RELEASE@@;$(GPSEE_RELEASE);g'\
		-e 's;@@STREAM@@;$(STREAM);g'\
		-e 's;@@BUILD@@;$(BUILD);g'\
		-e 's;@@INCLUDE_DIR@@;$(INCLUDE_DIR);g'\
		-e 's;@@EXPORT_LIBS@@@;$(EXPORT_LIBS);g'\
		-e 's;@@GPSEE_SRC@@;$(GPSEE_SRC_DIR);g'\
		-e 's;@@SPIDERMONKEY_SRC@@;$(SPIDERMONKEY_SRC);g'\
		-e 's;@@SPIDERMONKEY_BUILD@@;$(SPIDERMONKEY_BUILD);g'\
		-e 's;@@OUTSIDE_MK@@;$(GPSEE_SRC_DIR)/outside.mk;g'\
	< gpsee-config.template > gpsee-config
	chmod 755 gpsee-config

