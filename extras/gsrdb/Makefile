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
## @file        Makefile        Makefile to build an updated jsd/jsdb and gsrdb.
##                              Requires that GPSEE already be built and installed.
## @author      Wes Garland, PageMail, Inc., wes@page.ca
## @date        May 2010
## @version     $Id: Makefile,v 1.1 2010/06/14 22:23:49 wes Exp $

top: build

GPSEE_CONFIG    	?= ../../gpsee-config
EXPORT_PROGS		= gsrdb
EXPORT_HEADERS		= jsd/jsdebug.h jsd/jsdb/jsdb.h
EXPORT_LIBS		= $(LIBRARY)
AUTOGEN_SRC		= gsrdb.c
EXPORT_EXTRA_FILES	= gsrdb.js ui.js breakpoints.js display.js jsapi.js
EXTRA_FILE_DIR		= $(SOLIB_DIR)/gsrdb

include $(shell $(GPSEE_CONFIG) --outside.mk)

HERE		:= $(PWD)
JSD		:= $(HERE)/jsd
LIBRARY		= libgpsee-jsdb.$(SOLIB_EXT)

build: $(EXPORT_LIBS) $(EXPORT_PROGS)

LIBOBJS 	+= jsdebug.o jsd_atom.o jsd_high.o jsd_hook.o jsd_lock.o jsd_obj.o jsd_scpt.o jsd_stak.o jsd_step.o \
		   jsd_text.o jsd_val.o jsdb.o jsdrefl.o gpsee_debugger.o editline.o sysunix.o

JS		:= $(dir $(SPIDERMONKEY_SRC))
VPATH		+= $(JSD) $(JSD)/jsdb $(JS)/src/editline
CPPFLAGS	+= -I$(JSD) -I$(JSD)/jsdb -DGPSEE_DEBUGGER

$(LIBRARY):	$(LIBOBJS)
OBJS		+= $(LIBOBJS) gsrdb.o gsr.o

jsdb.o: 	CPPFLAGS += "-DJSDB_LOAD_DEBUGGER=\"load('debugger.js')\""
$(LIBOBJS):	EXTRA_CPPFLAGS	+= -DEXPORT_JSD_API -DMOZILLA_CLIENT

gsrdb: 		EXTRA_LOADLIBES += -L. -lgpsee-jsdb $(shell $(GPSEE_CONFIG) --ldflags)
gsrdb.o:	EXTRA_CPPFLAGS += -DSYSTEM_GSR="\"${GSR_SHEBANG_LINK}db\"" -DDEBUGGER_JS=\""$(SOLIB_DIR)/gsrdb/gsrdb.js\""
gsrdb.c:	
		ln -s $(GPSEE_SRC_DIR)/gsr.c $@

sysunix.o editline.o:	CPPFLAGS += -DANSI_ARROWS -DHAVE_TCGETATTR -DHIDE -DUSE_DIRENT -DSYS_UNIX -DHAVE_STDLIB -DUNIQUE_HISTORY  

install:	gsrdb-link

gsrdb-link:
	        [ -h "$(GSR_SHEBANG_LINK)db" ] || ln -s "$(BIN_DIR)/gsrdb" "$(GSR_SHEBANG_LINK)db"
