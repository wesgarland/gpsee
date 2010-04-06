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
## @version	$Id: Makefile,v 1.34 2010/03/06 03:25:24 wes Exp $

top: 	
	@if [ -f ./local_config.mk ]; then $(MAKE) help; echo " *** Running $(MAKE) build"; echo; $(MAKE) build; else $(MAKE) help; fi

PWD = $(shell pwd)
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
-include $(GPSEE_SRC_DIR)/spidermonkey/vars.mk

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

# PROGS must appear before build.mk until darwin-ld.sh is obsolete.
PROGS		 	?= gsr minimal

include build.mk
-include depend.mk

GPSEE_SOURCES	 	= gpsee.c gpsee_$(STREAM).c gpsee_lock.c gpsee_flock.c gpsee_util.c gpsee_modules.c gpsee_compile.c gpsee_context_private.c gpsee_xdrfile.c
GPSEE_OBJS	 	= $(GPSEE_SOURCES:.c=.o) $(AR_MODULE_FILES)

ifneq ($(STREAM),surelynx)
GPSEE_OBJS		+= gpsee_$(STREAM).o
endif

AUTOGEN_HEADERS		+= modules.h gpsee_config.h
EXPORT_PROGS	 	= gsr gpsee-config
EXPORT_SCRIPTS		= sample_programs/jsie.js
EXPORT_LIBS	 	= libgpsee.$(SOLIB_EXT)
EXPORT_LIBEXEC_OBJS 	= $(SO_MODULE_FILES)
EXPORT_HEADERS		= gpsee.h gpsee-jsapi.h gpsee_config.h gpsee_lock.c gpsee_flock.h gpsee_formats.h gpsee-iconv.h
EXPORT_HEADERS		+= $(wildcard gpsee_$(STREAM).h)
EXPORT_LIBEXEC_JS	:= $(wildcard $(sort $(JS_MODULE_FILES) $(wildcard $(SO_MODULE_DSOS:.$(SOLIB_EXT)=.js))))
TARGET_LIBEXEC_JS	:= $(addprefix $(LIBEXEC_DIR)/, $(notdir $(EXPORT_LIBEXEC_JS)))
TARGET_LIBEXEC_JSC 	:= $(join $(dir $(TARGET_LIBEXEC_JS)), $(addsuffix c,$(addprefix .,$(notdir $(TARGET_LIBEXEC_JS)))))

LOADLIBES		+= -lgpsee
$(PROGS): LDFLAGS	:= -L. $(LDFLAGS) $(JSAPI_LIBS)

DEPEND_FILES_X	 = $(addsuffix .X,$(PROGS)) $(GPSEE_OBJS:.o=.X)
DEPEND_FILES 	+= $(sort $(wildcard $(DEPEND_FILES_X:.X=.c) $(DEPEND_FILES_X:.X=.cpp)))

.PHONY:	all clean real-clean depend build_debug build_debug_modules show_modules clean_modules src-dist bin-dist top help install_js_components
build install: $(GPSEE_OBJS) $(EXPORT_LIBS) $(PROGS) $(EXPORT_PROGS) $(EXPORT_LIBEXEC_OBJS) $(EXPORT_HEADERS) $(SO_MODULE_FILES)
install: $(TARGET_LIBEXEC_JSC) gsr-link
install: EXPORT_PROGS += $(EXPORT_SCRIPTS)

clean: EXPORT_LIBEXEC_OBJS:=$(filter-out %.js,$(EXPORT_LIBEXEC_OBJS))
clean: EXTRA_CLEAN_RULE=clean_modules
clean: OBJS += $(wildcard $(GPSEE_OBJS) $(PROGS:=.o) $(AR_MODULES) $(SO_MODULES) $(wildcard ./gpsee_*.o)) doxygen.log
real-clean: clean
	cd spidermonkey && $(MAKE) clean
	cd libffi && $(MAKE) clean

gsr-link:
	[ -h "$(GSR_SHEBANG_LINK)" ] || ln -s "$(BIN_DIR)/gsr" "$(GSR_SHEBANG_LINK)"

gpsee_modules.o: CPPFLAGS += -DDEFAULT_LIBEXEC_DIR=\"$(LIBEXEC_DIR)\" -DDSO_EXTENSION=\"$(SOLIB_EXT)\"

modules.h: Makefile $(STREAM)_stream.mk
	@date '+/* Generated by $(USER) on $(HOSTNAME) at %a %b %e %Y %H:%M:%S %Z */' > $@
	@echo $(INTERNAL_MODULES) \
		| $(TR) ' ' '\n'  \
		| $(SED) -e 's/.*/InternalModule(&)/' \
		>> $@

install_js_components:
		@echo " * Installing JavaScript module components"
		@$(if $(TARGET_LIBEXEC_JS), [ -d $(LIBEXEC_DIR) ] || mkdir -p $(LIBEXEC_DIR))
		$(if $(TARGET_LIBEXEC_JS), $(CP) $(EXPORT_LIBEXEC_JS) $(LIBEXEC_DIR))

$(TARGET_LIBEXEC_JSC):	install_js_components gsr $(TARGET_LIBEXEC_JS)
	./gsr -ndf $(dir $@)$(shell echo $(notdir $@) | sed -e 's/^\.//' -e 's/c$$//') || /bin/true

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

clean_modules:
	@$(foreach MODULE, $(AR_MODULE_FILES) $(SO_MODULE_DSOS), \
	echo && echo " * Cleaning $(dir $(MODULE))" && cd "$(GPSEE_SRC_DIR)/$(dir $(MODULE))" && \
	ls && \
	$(MAKE) -f "$(GPSEE_SRC_DIR)/modules.mk" STREAM=$(STREAM) GPSEE_SRC_DIR=$(GPSEE_SRC_DIR) clean;\
	)
	@echo ""
	@echo " * Done cleaning modules"

build_modules:: $(AR_MODULE_FILES) $(SO_MODULE_DSOS)
modules/%/depend.mk: modules.mk
	cd $(dir $@) && $(MAKE) -f $(GPSEE_SRC_DIR)/modules.mk depend
$(SO_MODULE_DSOS) $(AR_MODULE_FILES)::
	cd $(dir $@) && $(MAKE) -f $(GPSEE_SRC_DIR)/modules.mk $(notdir $@)
modules/% $(STREAM)_modules/%:: gpsee_config.h
	cd $(dir $@) && $(MAKE) -f $(GPSEE_SRC_DIR)/modules.mk $(notdir $@)

build_debug_modules:
	@echo
	@echo " * Module debug info: "
	@$(foreach DIR, $(ALL_MODULE_DIRS), cd "$(GPSEE_SRC_DIR)/$(DIR)" && $(MAKE) --quiet -f "$(GPSEE_SRC_DIR)/modules.mk" build_debug_module;)
	@echo

build_debug: build_debug_modules

GPSEE_RELEASE=0.2-pre2
gpsee-$(GPSEE_RELEASE)_src.tar.gz:: TMPFILE=gpsee_file_list.tmp
gpsee-$(GPSEE_RELEASE)_src.tar.gz:: 
	@$(RM) $(TMPFILE) || [ X = X ]
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

invasive-bin-dist:: INVASIVE_EXTRAS += $(shell ldd $(EXPORT_PROGS) $(EXPORT_LIBEXEC_OBJS) $(EXPORT_LIBS) 2>/dev/null \
	| $(EGREP) = | $(SED) 's;.*=>[ 	]*;;' | $(EGREP) -v '^/lib|^/usr/lib|^/usr/local/lib|^/platform|^/opt/csw|^/usr/sfw|^/opt/sfw' | sort -u)
invasive-bin-dist bin-dist:: TARGET=$(UNAME_SYSTEM)-$(UNAME_RELEASE)-$(UNAME_MACHINE)
invasive-bin-dist bin-dist:: DATE_STAMP=$(shell date '+%b-%d-%Y')
invasive-bin-dist bin-dist:: COUNT=$(shell ls $(STREAM)_gpsee_$(TARGET)*.tar.gz 2>/dev/null | grep -c $(DATE_STAMP))
invasive-bin-dist bin-dist:: DIST_ARCHIVE ?= $(STREAM)_gpsee_$(TARGET)-$(DATE_STAMP)-$(COUNT).tar.gz
invasive-bin-dist bin-dist:: install
	LD_LIBRARY_PATH="$(SOLIB_DIR):$(JSAPI_LIB_DIR):/lib:/usr/lib" gtar -zcvf $(DIST_ARCHIVE) \
		$(foreach FILE, $(notdir $(EXPORT_PROGS)), "$(BIN_DIR)/$(FILE)")\
		$(foreach FILE, $(notdir $(EXPORT_LIBEXEC_OBJS)), "$(LIBEXEC_DIR)/$(FILE)")\
		$(foreach FILE, $(notdir $(EXPORT_LIBS)), "$(SOLIB_DIR)/$(FILE)")\
		$(LIB_MOZJS) $(LIB_FFI) $(INVASIVE_EXTRAS)
	@echo
	@echo Done $@: $(STREAM)_gpsee_$(TARGET)-$(DATE_STAMP)-$(COUNT).tar.gz
	@echo

libgpsee.$(SOLIB_EXT): LDFLAGS += $(JSAPI_LIBS)
libgpsee.$(SOLIB_EXT): $(GPSEE_OBJS) $(AR_MODULE_FILES)

gsr.o: EXTRA_CPPFLAGS += -DSYSTEM_GSR="\"${GSR_SHEBANG_LINK}\""
gsr.o: WARNINGS := $(filter-out -Wcast-align, $(WARNINGS))
gsr: gsr.o $(VERSION_O)

JSDOC_TEMPLATE=$(GPSEE_SRC_DIR)/docgen/jsdoc/templates/pmi
JSDOC_TARGET_DIR=$(GPSEE_SRC_DIR)/docs/modules
JSDOC=java -jar "$(JSDOC_DIR)/jsrun.jar" "$(JSDOC_DIR)/app/run.js" -x=jsdoc -a -t=$(JSDOC_TEMPLATE) --directory=$(JSDOC_TARGET_DIR) 

JAZZDOC_TEMPLATE=$(GPSEE_SRC_DIR)/docgen/jazzdoc/template.html
JAZZDOC_TARGET_DIR=$(GPSEE_SRC_DIR)/docs/modules

docs-dir::
	@[ -d docs/source/gpsee ] || mkdir -p docs/source/gpsee

docs-doxygen::
	@rm -f doxygen.log
	doxygen

#docs-test:: JSDOC_TEMPLATE=/export/home/wes/svn/jsdoc-toolkit-read-only/jsdoc-toolkit/templates/jsdoc
docs-test::
	gsr -c "global=this" -Sddf $(HOME)/svn/jsdoc-toolkit-read-only/jsdoc-toolkit/app/run.js -- -x=jsdoc -a -t=$(JSDOC_TEMPLATE) --directory=$(JSDOC_TARGET_DIR) $(addprefix $(GPSEE_SRC_DIR)/,$(wildcard $(foreach MODULE, $(ALL_MODULES), modules/$(MODULE)/$(MODULE).jsdoc $(STREAM)_modules/$(MODULE)/$(MODULE).jsdoc)))

docs-jsdocs::
	$(JSDOC) $(addprefix $(GPSEE_SRC_DIR)/,$(wildcard $(foreach MODULE, $(ALL_MODULES), modules/$(MODULE)/$(MODULE).jsdoc $(STREAM)_modules/$(MODULE)/$(MODULE).jsdoc)))

docs-jazz:: DOCFILES = $(wildcard $(addsuffix /*.c, $(ALL_MODULE_DIRS)) $(addsuffix /*.decl, $(ALL_MODULE_DIRS)) $(addsuffix /*.js, $(ALL_MODULE_DIRS)))
docs-jazz::
	$(JAZZDOC) -O 'template: "$(JAZZDOC_TEMPLATE)", output: "$(JAZZDOC_TARGET_DIR)/jazzdocs.html", title: "GPSEE Module Documentation"' -- $(DOCFILES)

docs:: docs-dir docs-doxygen docs-jsdocs docs-jazz
	@echo " * Documentation generation complete"

publish-docs::
	@tar -cf - docs | ssh wes@www.page.ca 'cd public_html/opensource/gpsee && tar -xvf -'

gpsee_config.h depend.mk: STREAM_UCASE=$(shell echo $(STREAM) | $(TR) '[a-z]' '[A-Z]')
gpsee_config.h: Makefile $(wildcard *.mk)
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
depend.mk: MDFLAGS+=-DGPSEE_$(STREAM_UCASE)_STREAM
gpsee-config: gpsee-config.template Makefile local_config.mk spidermonkey/local_config.mk spidermonkey/vars.mk $(LIBFFI_CONFIG_DEPS)
	@echo " * Generating $@"
	@$(SED) \
		-e 's;@@CFLAGS@@;$(CFLAGS);g'\
		-e 's;@@LDFLAGS@@;$(LDFLAGS) -lgpsee $(JSAPI_LIBS);g'\
		-e 's;@@CPPFLAGS@@;$(filter-out -I $(GPSEE_SRC_DIR) -I$(GPSEE_SRC_DIR),$(CPPFLAGS));g'\
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

