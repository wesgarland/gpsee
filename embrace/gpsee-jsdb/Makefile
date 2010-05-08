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

PRODUCT 	= gpsee-jsdb
HERE		:= $(PWD)
JSD		:= $(HERE)/jsd
EXPORT_LIBS	+= $(LIBRARY)
EXPORT_PROGS	+= gpsee-jsdb.js

include ../gpsee-js/Makefile

LIBRARY			= libgpsee-jsdb.$(SOLIB_EXT)

top build: $(EXPORT_LIBS) $(EXPORT_PROGS)

LIBSRCS         = \
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

OBJS 		+= $(LIBSRCS:.cpp=.o)

JS		:= $(dir $(SPIDERMONKEY_SRC))
EXTRA_CPPFLAGS	+= -DEXPORT_JSD_API -DMOZILLA_CLIENT
VPATH		+= $(JSD) $(JSD)/jsdb $(JS)/src/editline
CPPFLAGS	+= -I$(JSD)

GPSEE_JS_CXXFLAGS       += -DGPSEE_NO_WORKERS -DJSDEBUGGER -I$(JSD) -I$(JSD)/jsdb
GPSEE_JS_LDFLAGS        += $(addprefix $(HERE)/,$(OBJS))

$(LIBRARY) $(PRODUCT):	$(OBJS)

jsdb.o: CPPFLAGS += "-DJSDB_LOAD_DEBUGGER=\"load('$(BIN_DIR)/gpsee-jsdb.js')\""
gpsee-jsdb.js: $(JSD)/jsdb/debugger.js
	$(CP) $^ $@