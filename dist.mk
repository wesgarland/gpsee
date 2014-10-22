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

HG ?= hg
HAVE_TOP_RULE=1
top: build install bin-dist

include Makefile

gpsee_version: LOADLIBES= 
gpsee_version: LDLIBS=
$(GPSEE_SRC_DIR)/gpsee_release.mk: gpsee_version
	@echo GPSEE_RELEASE="$(shell ./gpsee_version)"	 > $@
include $(GPSEE_SRC_DIR)/gpsee_release.mk

gpsee-$(GPSEE_RELEASE)_src.tar.gz:: TMPFILE=gpsee_file_list.tmp
gpsee-$(GPSEE_RELEASE)_src.tar.gz:: 
	@$(RM) $(TMPFILE) || [ X = X ]
	ls version.c >> $(TMPFILE)
	$(HG) locate >> $(TMPFILE)
	[ ! -d gpsee-$(GPSEE_RELEASE) ] || rm -rf gpsee-$(GPSEE_RELEASE)
	egrep -v 'core$$|depend.mk$$|~$$|surelynx|/\.hg[a-z]*$$|(([^A-Za-z_]|^)CVS([^A-Za-z_]|$$))|,v$$|\.[ao]$$|\.$(SOLIB_EXT)$$|^[	 ]*$$' \
		$(TMPFILE) | sort -u | $(GNU_TAR) -T - -zcf $@
	@echo $(TMPFILE)

src-dist:: DATE_STAMP=$(shell date '+%b-%d-%Y')
src-dist:: COUNT=$(shell ls gpsee-$(GPSEE_RELEASE)*.tar.gz 2>/dev/null | grep -c $(DATE_STAMP))
src-dist:: gpsee-$(GPSEE_RELEASE)_src.tar.gz
	[ ! -d gpsee-$(GPSEE_RELEASE)-$(DATE_STAMP)-$(COUNT) ] || rmdir gpsee-$(GPSEE_RELEASE)
	mkdir -p gpsee-$(GPSEE_RELEASE)/spidermonkey/
	ln -s $(abspath $(SPIDERMONKEY_SRC)/../) gpsee-$(GPSEE_RELEASE)/spidermonkey/js
	$(HG) log > gpsee-$(GPSEE_RELEASE)/hg.log
	cd gpsee-$(GPSEE_RELEASE) && $(GNU_TAR) -zxf ../gpsee-$(GPSEE_RELEASE)_src.tar.gz
	$(GNU_TAR) -zvchf gpsee-$(GPSEE_RELEASE)-$(DATE_STAMP)-$(COUNT).tar.gz gpsee-$(GPSEE_RELEASE)
	rm -rf gpsee-$(GPSEE_RELEASE)-$(DATE_STAMP)-$(COUNT)

invasive-bin-dist:: INVASIVE_EXTRAS += $(shell ldd $(BUILT_EXPORTED_PROGS) $(BUILT_EXPORTED_LIBS)  $(EXPORT_PROGS) $(EXPORT_LIBEXEC_OBJS) $(EXPORT_LIBS) 2>/dev/null \
	| $(EGREP) = | $(SED) 's;.*=>[ 	]*;;' | $(EGREP) -v '^/lib|^/usr/lib|^/usr/local/lib|^/platform|^/opt/csw|^/usr/sfw|^/opt/sfw' | sort -u)
invasive-bin-dist bin-dist:: TARGET=$(UNAME_SYSTEM)-$(UNAME_RELEASE)-$(UNAME_MACHINE)
invasive-bin-dist bin-dist:: DATE_STAMP=$(shell date '+%b-%d-%Y')
invasive-bin-dist bin-dist:: COUNT=$(shell ls $(STREAM)_gpsee_$(TARGET)*.tar.gz 2>/dev/null | grep -c $(DATE_STAMP))
invasive-bin-dist bin-dist:: DIST_ARCHIVE ?= $(STREAM)_gpsee_$(TARGET)-$(DATE_STAMP)-$(COUNT).tar.gz
invasive-bin-dist bin-dist:: install
	LD_LIBRARY_PATH="$(SOLIB_DIR):$(JSAPI_LIB_DIR):/lib:/usr/lib" $(GNU_TAR) -zcvf $(DIST_ARCHIVE) \
		$(foreach FILE, $(notdir $(EXPORT_PROGS)), "$(BIN_DIR)/$(FILE)")\
		$(foreach FILE, $(notdir $(EXPORT_LIBEXEC_OBJS)), "$(LIBEXEC_DIR)/$(FILE)")\
		$(foreach FILE, $(notdir $(EXPORT_LIBS)), "$(SOLIB_DIR)/$(FILE)")\
		$(foreach MODULE,$(EXTRA_MODULES),$(wildcard $(LIBEXEC_DIR)/$(MODULE).$(SOLIB_EXT) $(LIBEXEC_DIR)/$(MODULE).js))\
		$(LIB_MOZJS) $(LIB_FFI) $(INVASIVE_EXTRAS) $(TARGET_LIBEXEC_JS)\
		$(BUILT_EXPORTED_PROGS) $(BUILT_EXPORTED_LIBS)
	@echo
	@echo Done $@: $(STREAM)_gpsee_$(TARGET)-$(DATE_STAMP)-$(COUNT).tar.gz
	@echo

