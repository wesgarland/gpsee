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
##				Requires GNU Make 3.81 or better.
##
## @author	Wes Garland, PageMail, Inc., wes@page.ca
## @date	August 2007
## @version	$Id: Makefile,v 1.42 2010/12/02 21:59:42 wes Exp $

ifndef HAVE_TOP_RULE
top: 	
	@if [ -f ./local_config.mk ]; then $(MAKE) help; echo " *** Running $(MAKE) build"; echo; $(MAKE) build; else $(MAKE) help; fi
endif

GPSEE_SRC_DIR ?= $(shell pwd)

$(GPSEE_SRC_DIR)/local_config.mk:
	@echo
	@echo	" * GPSEE local configuration information missing"
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

GPSEE_BUILD = TRUE
include $(GPSEE_SRC_DIR)/local_config.mk
include $(GPSEE_SRC_DIR)/system_detect.mk
-include $(GPSEE_SRC_DIR)/$(UNAME_SYSTEM)_config.mk
-include $(GPSEE_SRC_DIR)/version.mk
-include $(GPSEE_SRC_DIR)/$(STREAM)_stream.mk

AUTOGEN_HEADERS		+= modules.h
-include $(GPSEE_SRC_DIR)/spidermonkey/vars.mk

# Rules to fix up SpiderMonkey lib path for the case when we do a staging build,
# as required by a configure/make/sudo make install build.
ifdef JSAPI_LIBS
ifeq ($(STAGED_SPIDERMONKEY_BUILD),TRUE)         
JSAPI_INSTALL_LIBS = $(subst -lmozjs,$(SOLIB_DIR)/libmozjs.$(SOLIB_EXT),$(subst $(JSAPI_LIB_DIR),$(SOLIB_DIR),$(JSAPI_LIBS)))
else
JSAPI_INSTALL_LIBS = $(JSAPI_LIBS)
endif
endif

PROGS                   ?= gpsee_version

ALL_MODULES		?= $(filter-out $(IGNORE_MODULES) ., $(shell cd modules && find . -type d -name '[a-z]*' -prune | sed 's;^./;;') $(shell cd $(STREAM)_modules 2>/dev/null && find . -type d -name '[a-z]*' -prune | sed 's;^./;;'))
IGNORE_MODULES		+= pairodice mozshell mozfile file filesystem-base
INTERNAL_MODULES 	+= vm gpsee system

include $(GPSEE_SRC_DIR)/ffi.mk

ifneq (X,X$(filter $(MAKECMDGOALS),install build all real-clean build_debug))
include $(GPSEE_SRC_DIR)/sanity.mk
endif

export GPSEE_SRC_DIR BUILD STREAM
export SPIDERMONKEY_BUILD SPIDERMONKEY_SRC

ALL_MODULES			:= $(ALL_MODULES)
AR_MODULES			:= $(filter $(INTERNAL_MODULES), $(ALL_MODULES))
LOADABLE_MODULES		:= $(filter-out $(INTERNAL_MODULES), $(ALL_MODULES))
AR_MODULE_DIRS_GLOBAL		:= $(wildcard $(foreach MODULE, $(AR_MODULES), modules/$(MODULE)))
LOADABLE_MODULE_DIRS_GLOBAL	:= $(wildcard $(foreach MODULE, $(LOADABLE_MODULES), modules/$(MODULE)))
AR_MODULE_DIRS_STREAM		:= $(wildcard $(foreach MODULE, $(AR_MODULES), $(STREAM)_modules/$(MODULE)))
LOADABLE_MODULE_DIRS_STREAM	:= $(wildcard $(foreach MODULE, $(LOADABLE_MODULES), $(STREAM)_modules/$(MODULE)))
AR_MODULE_DIRS_ALL		:= $(AR_MODULE_DIRS_GLOBAL) $(AR_MODULE_DIRS_STREAM)
LOADABLE_MODULE_DIRS_ALL	:= $(LOADABLE_MODULE_DIRS_GLOBAL) $(LOADABLE_MODULE_DIRS_STREAM)
AR_MODULE_FILES			:= $(foreach MODULE_DIR, $(AR_MODULE_DIRS_ALL), $(MODULE_DIR)/$(notdir $(MODULE_DIR)).$(LIB_EXT))
SO_MODULE_DSOS			:= $(shell $(foreach DIR, $(LOADABLE_MODULE_DIRS_ALL), [ -r "$(DIR)/$(notdir $(DIR)).c" ] || [ -r "$(DIR)/$(notdir $(DIR)).cpp" ] && echo "$(DIR)/$(notdir $(DIR)).$(SOLIB_EXT)";))
SO_MODULE_FILES			:= $(SO_MODULE_DSOS)
JS_MODULE_FILES			:= $(shell $(foreach DIR, $(LOADABLE_MODULE_DIRS_ALL), [ ! -r "$(DIR)/$(notdir $(DIR)).c" ] && [ ! -r "$(DIR)/$(notdir $(DIR)).cpp" ] && echo "$(DIR)/$(notdir $(DIR)).js";))
ALL_MODULE_DIRS			:= $(sort $(AR_MODULE_DIRS_ALL) $(LOADABLE_MODULE_DIRS_ALL) $(dir $(JS_MODULE_FILES)))

include build.mk
ifndef SUDO_USER
-include depend.mk
endif

GPSEE_SOURCES	 	= gpsee.c gpsee_$(STREAM).c gpsee_lock.c gpsee_flock.c gpsee_util.c gpsee_modules.c gpsee_compile.c gpsee_context_private.c \
			  gpsee_xdrfile.c gpsee_hookable_io.c gpsee_datastores.c gpsee_monitors.c gpsee_realms.c gpsee_gccallbacks.c \
			  gpsee_bytethings.c gpsee_p2open.c

GPSEE_OBJS	 	= $(GPSEE_SOURCES:.c=.o) $(AR_MODULE_FILES)

ifneq ($(STREAM),surelynx)
GPSEE_OBJS		+= gpsee_$(STREAM).o
endif

EXPORT_PROGS		= gpsee-config
BUILT_EXPORTED_PROGS	= $(BIN_DIR)/gsr $(BIN_DIR)/gpsee_precompiler
EXPORT_SCRIPTS		= sample_programs/jsie.js
BUILT_EXPORTED_LIBS	= $(SOLIB_DIR)/libgpsee.$(SOLIB_EXT)
EXPORT_LIBEXEC_OBJS 	= $(SO_MODULE_FILES)
EXPORT_HEADERS		= gpsee.h gpsee-jsapi.h gpsee_config.h gpsee_lock.c gpsee_flock.h gpsee_formats.h gpsee-iconv.h gpsee_version.h
EXPORT_HEADERS		+= $(wildcard gpsee_$(STREAM).h)
EXPORT_LIBEXEC_JS	:= $(wildcard $(sort $(JS_MODULE_FILES) $(wildcard $(SO_MODULE_DSOS:.$(SOLIB_EXT)=.js))))
TARGET_LIBEXEC_JS	:= $(addprefix $(LIBEXEC_DIR)/, $(notdir $(EXPORT_LIBEXEC_JS)))
TARGET_LIBEXEC_JSC 	:= $(join $(dir $(TARGET_LIBEXEC_JS)), $(addsuffix c,$(addprefix .,$(notdir $(TARGET_LIBEXEC_JS)))))
GPSEE_LOADLIBES		:= -lgpsee
LOADLIBES		+= $(GPSEE_LOADLIBES) $(JSAPI_INSTALL_LIBS)

$(BUILT_EXPORTED_PROGS): $(SOLIB_DIR)/libgpsee.$(SOLIB_EXT) $(SOLIB_DIR)/libmozjs.$(SOLIB_EXT)

DEPEND_FILES_X	 = $(addsuffix .X,$(PROGS) $(notdir $(BUILD_EXPORTED_PROGS)) $(EXPORT_PROGS)) $(GPSEE_OBJS:.o=.X)
DEPEND_FILES 	+= $(sort $(wildcard $(DEPEND_FILES_X:.X=.c) $(DEPEND_FILES_X:.X=.cpp)))

.PHONY:	all build _build _prebuild clean real-clean depend build_debug build_debug_modules \
	show_modules clean_modules src-dist bin-dist top help install-%

build: _prebuild
	$(MAKE) _build
_prebuild: $(SPIDERMONKEY_BUILD) $(LIBFFI_BUILD)
_build: $(AUTOGEN_HEADERS) $(AUTOGEN_SOURCE) $(GPSEE_OBJS) $(EXPORT_LIBS) $(PROGS) \
	$(EXPORT_PROGS) $(EXPORT_LIBEXEC_OBJS) $(EXPORT_HEADERS) $(SO_MODULE_FILES) \
	$(addsuffix .o, $(notdir $(BUILT_EXPORTED_PROGS))) $(EXPORT_EXTRA_FILES) $(VERSION_O) \
	gpsee.pc

$(SPIDERMONKEY_BUILD):
	cd spidermonkey && $(MAKE) build
$(LIBFFI_BUILD):
	cd spidermonkey && $(MAKE) build

install: install-dirs install-spidermonkey install-libffi install-built-exported $(TARGET_LIBEXEC_JSC) gsr-link 
install: EXPORT_PROGS += $(EXPORT_SCRIPTS)
install-built-exported:  $(BUILT_EXPORTED_LIBS) $(BUILT_EXPORTED_PROGS) $(SOLIB_DIR)/pkgconfig/gpsee.pc

clean: EXPORT_LIBEXEC_OBJS:=$(filter-out %.js,$(EXPORT_LIBEXEC_OBJS))
clean: EXTRA_CLEAN_RULE=clean_modules clean_makefile_depends
clean: OBJS += $(wildcard $(GPSEE_OBJS) $(PROGS:=.o) $(AR_MODULES) $(SO_MODULES) $(wildcard ./gpsee_*.o)) doxygen.log libgpsee.a
clean: OBJS += $(addsuffix .o,$(notdir $(BUILT_EXPORTED_PROGS)))

real-clean: clean
	cd spidermonkey && $(MAKE) clean
	cd libffi && $(MAKE) clean

gsr-link:
ifneq (X$(GSR_SHEBANG_LINK),X)
	[ -h "$(GSR_SHEBANG_LINK)" ] || ln -s "$(BIN_DIR)/gsr" "$(GSR_SHEBANG_LINK)"
endif

gpsee_modules.o: CPPFLAGS += -DDEFAULT_LIBEXEC_DIR=\"$(LIBEXEC_DIR)\" -DDSO_EXTENSION=\"$(SOLIB_EXT)\"

modules.h: Makefile $(STREAM)_stream.mk
	@date '+/* Generated by $(USER) on $(HOSTNAME) at %a %b %e %Y %H:%M:%S %Z */' > $@
	@echo $(INTERNAL_MODULES) \
		| $(TR) ' ' '\n'  \
		| $(SED) -e 's/.*/InternalModule(&)/' \
		>> $@

install-js_components:
		@echo " * Installing JavaScript module components"
		@$(if $(TARGET_LIBEXEC_JS), [ -d $(LIBEXEC_DIR) ] || mkdir -p $(LIBEXEC_DIR))
		$(if $(TARGET_LIBEXEC_JS), $(CP) $(EXPORT_LIBEXEC_JS) $(LIBEXEC_DIR))

install-spidermonkey: $(SOLIB_DIR)/libmozjs.$(SOLIB_EXT)
		$(CP) -rp $(JSAPI_INCLUDE_DIR) $(GPSEE_PREFIX_DIR)

install-libffi:
		$(CP) -rp $(LIBFFI_LIB_DIR) $(GPSEE_PREFIX_DIR)

install-dirs:
	@echo " * Creating target directories"
	@[ -d $(GPSEE_PREFIX_DIR) ] || $(MKDIR) $(GPSEE_PREFIX_DIR)
	@[ -d $(SOLIB_DIR) ] || $(MKDIR) $(SOLIB_DIR)
	@[ -d $(INCLUDE_DIR) ] || $(MKDIR) $(INCLUDE_DIR)
	@[ -d $(LIBEXEC_DIR) ] || $(MKDIR) $(LIBEXEC_DIR)
	@[ -d $(BIN_DIR) ] || $(MKDIR) $(BIN_DIR)

$(TARGET_LIBEXEC_JSC): $(BIN_DIR)/gpsee_precompiler
$(TARGET_LIBEXEC_JSC): $(BIN_DIR)/gpsee_precompiler install-js_components $(TARGET_LIBEXEC_JS)
	@$(BIN_DIR)/gpsee_precompiler $(dir $@)$(shell echo $(notdir $@) | sed -e 's/^\.//' -e 's/c$$//') || [ X = X ]

show_modules:
	@echo 
	@echo "*** GPSEE-$(STREAM) Module Configuration ***"
	@echo
	@echo "Internal Modules (native):"
	@echo "$(sort $(AR_MODULES))" | $(SED) -e 's/  */ /g' | tr ' ' '\n' | $(GREP) -v '^ *$$' | $(SED) 's/^/ - /'
	@echo 
	@echo "Loadable Modules:"
	@echo "$(sort $(LOADABLE_MODULES))" | $(SED) -e 's/  */ /g' | tr ' ' '\n' | $(GREP) -v '^ *$$' | $(SED) 's/^/ - /'
	@echo
	@echo "DSO Modules:"
	@echo "$(sort $(dir $(SO_MODULE_FILES)))" | $(SED) -e 's/  */ /g' | tr ' ' '\n' | $(GREP) -v '^ *$$' | $(SED) 's/^/ - /'
	@echo
	@echo "Blended Modules:"
	@echo "$(sort $(dir $(wildcard $(SO_MODULE_FILES:.$(SOLIB_EXT)=.js))))" | $(SED) -e 's/  */ /g' | tr ' ' '\n' | $(GREP) -v '^ *$$' | $(SED) 's/^/ - /'
	@echo
	@echo "Pure JS Modules:"
	@echo "$(sort $(dir $(JS_MODULE_FILES)))" | $(SED) -e 's/  */ /g' | tr ' ' '\n' | $(GREP) -v '^ *$$' | $(SED) 's/^/ - /'
	@echo
	@echo "Module Documentation:"
	@echo  $(sort $(wildcard $(foreach MODULE, $(ALL_MODULES), modules/$(MODULE)/$(MODULE).jsdoc $(STREAM)_modules/$(MODULE)/$(MODULE).jsdoc))) |\
		$(SED) -e 's/  */ /g' | tr ' ' '\n' | $(GREP) -v '^ *$$' | $(SED) 's/^/ - /'

clean_makefile_depends:
	$(RM) gpsee_release.mk

clean_modules:
	@$(foreach MODULE, $(AR_MODULE_FILES) $(SO_MODULE_DSOS), \
	  echo && echo " * Cleaning $(dir $(MODULE))" && cd "$(GPSEE_SRC_DIR)/$(dir $(MODULE))" && \
	  ls && \
	  $(MAKE) -f "$(GPSEE_SRC_DIR)/modules.mk" STREAM=$(STREAM) GPSEE_SRC_DIR=$(GPSEE_SRC_DIR) clean;\
	)
	@echo ""
	@echo " * Done cleaning modules"

ifndef SUDO_USER
build_modules:: $(AR_MODULE_FILES) $(SO_MODULE_DSOS)
modules/%/depend.mk: modules.mk
	cd $(dir $@) && $(MAKE) -f $(GPSEE_SRC_DIR)/modules.mk depend
$(SO_MODULE_DSOS) $(AR_MODULE_FILES)::
	cd $(dir $@) && $(MAKE) -f $(GPSEE_SRC_DIR)/modules.mk $(notdir $@)
modules/% $(STREAM)_modules/%:: gpsee_config.h
	cd $(dir $@) && $(MAKE) -f $(GPSEE_SRC_DIR)/modules.mk $(notdir $@)
endif

build_debug_modules:
	@echo
	@echo " * Module debug info: "
	@$(foreach DIR, $(ALL_MODULE_DIRS), cd "$(GPSEE_SRC_DIR)/$(DIR)" && $(MAKE) --quiet -f "$(GPSEE_SRC_DIR)/modules.mk" build_debug_module;)
	@echo

build_debug_sudo:
ifdef SUDO_USER
	@echo " * Running under sudo"
endif

build_debug: build_debug_sudo build_debug_modules

$(BIN_DIR)/%: %.o
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@

$(SOLIB_DIR)/libmozjs.$(SOLIB_EXT): $(JSAPI_LIB_DIR)/$(notdir $@)
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	$(CP) $(JSAPI_LIB_DIR)/$(notdir $@) $@
ifeq ($(UNAME_SYSTEM),Darwin)
	install_name_tool -id $@ $@
endif

$(SOLIB_DIR)/libgpsee.$(SOLIB_EXT): $(GPSEE_OBJS) $(AR_MODULE_FILES)
$(SOLIB_DIR)/libgpsee.$(SOLIB_EXT): LOADLIBES := $(filter-out $(LOADLIBES),$(GPSEE_LOADLIBES)) $(SOLIB_DIR)/libmozjs.$(SOLIB_EXT)
libgpsee.$(LIB_EXT): $(filter %.o,$(GPSEE_OBJS)) $(foreach MODULE_DIR, $(AR_MODULE_DIRS_ALL), $(wildcard $(MODULE_DIR)/*.o))
libgpsee.$(SOLIB_EXT):
	@echo " * Cannot build this target; please sudo make install instead"
	@false
ifneq (X$(GSR_SHEBANG_LINK),X)
gsr.o: CPPFLAGS += -DSYSTEM_GSR="\"${GSR_SHEBANG_LINK}\""
endif
gsr.o: WARNINGS := $(filter-out -Wcast-align, $(WARNINGS))
gsr.o: gpsee_version.h
$(BIN_DIR)/gsr: gsr.o $(VERSION_O)

$(SPIDERMONKEY_BUILD)/libjs_static.a:
	$(warning $@ should have already been built!)
	cd $(SPIDERMONKEY_BUILD)
	make libjs_static.a

gpsee_config.h depend.mk: STREAM_UCASE=$(shell echo $(STREAM) | $(TR) '[a-z]' '[A-Z]')
ifeq (X,X$(filter $(MAKECMDGOALS),install depend real-clean clean clean_modules))
gpsee_config.h: Makefile $(filter-out depend.mk,$(wildcard *.mk))
endif
gpsee_config.h:
	@echo " * Generating $@"
	@echo "/* Generated `date` by $(USER) on $(HOSTNAME) */ " > $@
	@echo "$(foreach DEFINE, $(GPSEE_C_DEFINES),@#if !defined($(DEFINE))@# define $(DEFINE)@#endif@)" \
		| $(TR) @ '\n' | $(SED) -e "s/=/ /" >> $@
	@echo "#if defined(HAVE_IDENTITY_TRANSCODING_ICONV) && !defined(HAVE_ICONV)" >> $@
	@echo "# define HAVE_ICONV" >> $@
	@echo "#endif" >> $@
	@echo "#define GPSEE_$(BUILD)_BUILD" >> $@
	@echo "#define GPSEE_$(STREAM_UCASE)_STREAM" >> $@
	@echo "#define GPSEE_$(shell echo $(UNAME_SYSTEM) | $(TR) '[a-z]' '[A-Z]')_SYSTEM" >> $@
clean build install: AUTOGEN_HEADERS := $(AUTOGEN_HEADERS) gpsee_config.h
depend.mk: MDFLAGS+=-DGPSEE_$(STREAM_UCASE)_STREAM

TEMPLATE_MARKUP =\
		-e 's;@@CC@@;$(CC);g'\
		-e 's;@@CXX@@;$(CXX);g'\
		-e 's;@@CFLAGS@@;$(CFLAGS);g'\
		-e 's;@@LDFLAGS@@;$(LDFLAGS) -lgpsee;g'\
		-e 's;@@CPPFLAGS@@;$(filter-out -I $(GPSEE_SRC_DIR) -I$(GPSEE_SRC_DIR),$(CPPFLAGS));g'\
		-e 's;@@CXXFLAGS@@;$(CXXFLAGS);g'\
		-e 's;@@LOADLIBES@@;$(LOADLIBES) $(JSAPI_INSTALL_LIBS);g'\
		-e 's;@@GPSEE_PREFIX_DIR@@;$(GPSEE_PREFIX_DIR);g'\
		-e 's;@@LIBEXEC_DIR@@;$(LIBEXEC_DIR);g'\
		-e 's;@@BIN_DIR@@;$(BIN_DIR);g'\
		-e 's;@@SOLIB_DIR@@;$(SOLIB_DIR);g'\
		-e 's;@@GPSEE_RELEASE@@;$(GPSEE_RELEASE);g'\
		-e 's;@@STREAM@@;$(STREAM);g'\
		-e 's;@@GCC_PREFIX@@;$(GCC_PREFIX);g'\
		-e 's;@@BUILD@@;$(BUILD);g'\
		-e 's;@@INCLUDE_DIR@@;$(INCLUDE_DIR);g'\
		-e 's;@@EXPORT_LIBS@@;$(EXPORT_LIBS);g'\
		-e 's;@@GPSEE_SRC@@;$(GPSEE_SRC_DIR);g'\
		-e 's;@@SPIDERMONKEY_SRC@@;$(SPIDERMONKEY_SRC);g'\
		-e 's;@@SPIDERMONKEY_BUILD@@;$(SPIDERMONKEY_BUILD);g'\
		-e 's;@@OUTSIDE_MK@@;$(GPSEE_SRC_DIR)/outside.mk;g'\

gpsee-config gpsee.pc: gpsee_version gpsee_version.h Makefile local_config.mk spidermonkey/local_config.mk spidermonkey/vars.mk $(LIBFFI_CONFIG_DEPS)
gpsee-config gpsee.pc: GPSEE_RELEASE=$(shell ./gpsee_version)

gpsee-config: gpsee-config.template 
	@echo " * Generating $@"
	@$(SED)	$(TEMPLATE_MARKUP) < $@.template > $@
	chmod 755 $@

gpsee.pc: gpsee.pc.template 
	@echo " * Generating $@"
	@$(SED)	$(TEMPLATE_MARKUP) < $@.template > $@

$(SOLIB_DIR)/pkgconfig/gpsee.pc: gpsee.pc
	@[ -d $(dir $@) ] || $(MKDIR) $(dir $@)
	$(CP) $^ $@

gpsee_version: gpsee_version.c gpsee_version.h
	$(CC) gpsee_version.c -o $@

help:
	@echo
	@echo   "Make Targets:"
	@echo   " build         Build GPSEE"
	@echo   " install       Install GPSEE under $(GPSEE_PREFIX_DIR)/"
	@echo   " build_debug   Show build system debug info"
	@echo	" show_modules  Show module configuration"
	@echo   " clean         Clean up this dir"
	@echo   " dist-clean    Clean up this dir and installed files"
	@echo
	@echo   "To customize your build, edit ./local_config.mk"
	@echo
