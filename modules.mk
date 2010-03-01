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
#
# Generic Makefile for building a module.
# Can be augmented with module.mk in the module's directory.
#

# MODULE holds the name of the module, eg. "binary", based on the
# name of the current directory unless overrideen
MODULE ?= $(notdir $(abspath .))
MODULE := $(MODULE)

# MODULE_OBJ is the object file containing the module. Additional
# objects can be specified with EXTRA_MODULE_OBJS
MODULE_OBJ ?= $(MODULE).o

include $(GPSEE_SRC_DIR)/local_config.mk
include $(GPSEE_SRC_DIR)/system_detect.mk
include $(GPSEE_SRC_DIR)/version.mk
-include $(GPSEE_SRC_DIR)/$(UNAME_SYSTEM)_config.mk
include $(GPSEE_SRC_DIR)/$(STREAM)_stream.mk
ifneq ($(MAKECMDGOALS),clean)
include $(GPSEE_SRC_DIR)/spidermonkey/vars.mk
endif
-include module.mk
include $(GPSEE_SRC_DIR)/build.mk
ifneq ($(MAKECMDGOALS),depend)
-include depend.mk
endif

DEPEND_FILES += $(GPSEE_SRC_DIR)/modules.mk module.mk 
DEPEND_FILES += $(wildcard $(MODULE_OBJ:.o=.c) $(MODULE_OBJ:.o=.cpp))
DEPEND_FILES += $(wildcard $(EXTRA_MODULE_OBJS:.o=.c) $(EXTRA_MODULE_OBJS:.o=.cpp))

ifdef VERSION_O
ifneq (X$(wildcard $(VERSION_H)),X)
EXTRA_MODULE_OBJS += $(VERSION_O)
else
VERSION_O := 
endif
endif

$(MODULE).$(LIB_EXT) $(MODULE).$(SOLIB_EXT): $(MODULE_OBJ) $(EXTRA_MODULE_OBJS)

.PHONY: build_debug_module

clean: OBJS+=$(MODULE_OBJ) $(EXTRA_MODULE_OBJS) $(MODULE)


