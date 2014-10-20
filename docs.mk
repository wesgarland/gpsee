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

HAVE_TOP_RULE=1
top: build docs publish-docs

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
