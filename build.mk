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
BIN_DIR			?= $(GPSEE_PREFIX_DIR)/bin
SOLIB_DIR		?= $(GPSEE_PREFIX_DIR)/lib
INCLUDE_DIR		?= $(GPSEE_PREFIX_DIR)/include
LIBEXEC_DIR		?= $(GPSEE_PREFIX_DIR)/libexec
LIB_MOZJS		?= $(SOLIB_DIR)/libmozjs.$(SOLIB_EXT)

PIC_CFLAG 		?= -fPIC
LDFLAGS		+= $(LEADING_LDFLAGS)
CPPFLAGS	+= $(LEADING_CPPFLAGS) -DGPSEE -I$(GPSEE_SRC_DIR) $(INCLUDES) $(JSAPI_CFLAGS)

WARNINGS	?= -Wall -Wwrite-strings -Wcast-align -Wstrict-aliasing=2 -Wwrite-strings -Winline

ifneq (X,X$(filter $(BUILD),DEBUG PROFILE DRELEASE))
CFLAGS += -g
CXXFLAGS += -g
endif

ifneq (X,X$(filter $(BUILD),PROFILE RELEASE))
CFLAGS += -O3
CXXFLAGS += -O3
endif

ifneq (X,X$(filter $(BUILD),DEBUG))
CFLAGS += -O0
CXXFLAGS += -O0
endif

ifneq (X,X$(filter $(BUILD),DRELEASE))
CFLAGS += -O1
CXXFLAGS += -O1
endif

ifneq (X,X$(filter $(BUILD),PROFILE))
CFLAGS += -p -pg
CXXFLAGS += -p -pg
endif

CFLAGS		+= $(PIC_CFLAG)
CXXFLAGS	+= $(PIC_CFLAG)
SOLIB_DIRS      += $(SOLIB_DIR)

# Implicit Rules for building libraries
%.$(SOLIB_EXT):	$(if $(wildcard $(VERSION_H)),$(VERSION_O))
		$(LINK_SOLIB) 

ifndef SUDO_USER
%.$(LIB_EXT):	$(if $(wildcard $(VERSION_H)),$(VERSION_O))
		$(AR_RU) $@ $(filter-out $(VERSION_O),$^) $(VERSION_O)
		$(RANLIB) $@
endif

export GPSEE_SRC_DIR BUILD STREAM
export SPIDERMONKEY_BUILD SPIDERMONKEY_SRC

ifneq ($(NO_BUILD_RULES),TRUE)
-include $(GPSEE_SRC_DIR)/$(STREAM)_rules.mk
-include $(GPSEE_SRC_DIR)/$(UNAME_SYSTEM)_rules.mk
endif
