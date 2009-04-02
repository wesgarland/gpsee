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
HOSTNAME		?= $(shell hostname)
GPSEE_LIBNAME		= gpsee

CPPFLAGS 		+= $(EXTRA_CPPFLAGS)
LDFLAGS 		+= $(EXTRA_LDFLAGS)  
LDFLAGS_SOLIB_DIRS	+= $(SOLIB_DIR)
LOADLIBES 		+= $(EXTRA_LOADLIBES)
CFLAGS			+= $(EXTRA_CFLAGS)

# Compilation tools
CC		= gcc
CXX		= g++
LD		= gcc -shared
LEX		?= lex
YACC		?= yacc
CPP		?= gcc -E
AR		?= ar
AR_RU		?= $(AR) -ru
SO_AR		?= $(LD) $(LDFLAGS) -o
RANLIB		?= ranlib
MAKEDEPEND	?= gcc -E -MM -MG

# Shell tools
CP		?= cp -f
MV		?= mv -f
RM		?= rm -f
MKDIR		?= mkdir -p
GREP		?= grep
CHMOD		?= chmod
CHOWN		?= chown
AWK		?= awk
TR		?= tr
SED		?= sed

# File extensions; EXE_EXT includes dot if required
OBJ_EXT		?=o
EXE_EXT		?=
LIB_EXT		?=a
SOLIB_EXT	?=so

%.$(SOLIB_EXT):	
		$(if $(VERSION_O), $(MAKE) $(VERSION_O))
		$(SO_AR) $@ $^ $(VERSION_O)

%.$(LIB_EXT):	
		$(if $(VERSION_O), $(MAKE) $(VERSION_O))
		$(AR_RU) $@ $(VERSION_O) $^
		$(RANLIB) $@

ifneq ($(MAKECMDGOALS),install-nodeps)
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),dist-clean)
ifneq ($(MAKECMDGOALS),all-clean)
ifneq ($(MAKECMDGOALS),build_debug)
depend depend.mk: 	XFILES=$(sort $(filter %.c %.cc %.cpp %.cxx,$(DEPEND_FILES)))
depend depend.mk: 	Makefile $(DEPEND_FILES)
			@echo " * Building dependencies for: $(XFILES)"
			$(if $(XFILES), $(MAKEDEPEND) $(MDFLAGS) $(CPPFLAGS) -DMAKEDEPEND $(XFILES) > $@)
			@touch depend.mk
endif # goal = build_debug
endif # goal = all-clean
endif # goal = dist-clean
endif # goal = clean
endif # goal = install-nodeps

clean:
	-$(if $(strip $(OBJS)), $(RM) $(OBJS))
	-$(if $(strip $(EXPORT_LIBS)), $(RM) $(EXPORT_LIBS))
	-$(if $(strip $(EXPORT_LIBEXEC_OBJS)), $(RM) $(EXPORT_LIBEXEC_OBJS)) 
	-$(if $(strip $(PROGS)), $(RM) $(PROGS))
	-$(if $(strip $(EXPORT_PROGS)), $(RM) $(EXPORT_PROGS))
	-$(if $(strip $(AUTOGEN_HEADERS)), $(RM) $(AUTOGEN_HEADERS))
	-$(if $(strip $(EXTRA_CLEAN_RULE)), $(MAKE) $(EXTRA_CLEAN_RULE))
	$(RM) depend.mk

build_debug:
	@echo "Depend Files: $(DEPEND_FILES)"

# Install shared libraries
install-nodeps install install-solibs: XLIBS =$(strip $(filter %.$(SOLIB_EXT),$(EXPORT_LIBS)))
install-solibs:	$(EXPORT_LIBS) $(EXPORT_LIBEXEC_OBJS)
		@$(if $(XLIBS), [ -d $(SOLIB_DIR) ] || mkdir -p $(SOLIB_DIR))
		$(if $(XLIBS), $(CP) $(XLIBS) $(SOLIB_DIR))
		@$(if $(EXPORT_LIBEXEC_OBJS), [ -d $(LIBEXEC_DIR) ] || mkdir -p $(LIBEXEC_DIR))
		$(if $(EXPORT_LIBEXEC_OBJS), $(CP) $(EXPORT_LIBEXEC_OBJS) $(LIBEXEC_DIR))

# Install binaries and shared libraries
install-nodeps install:	XPROGS =$(strip $(EXPORT_PROGS))
install-nodeps install:	XCGIS =$(strip $(CGI_PROGS))
install:	$(EXPORT_LIBS) $(EXPORT_LIBEXEC_OBJS) $(EXPORT_PROGS) $(CGI_PROGS)
ifneq (X$(EXPORT_LIBS)$(EXPORT_LIBEXEC_OBJS),X)
		@make install-solibs
endif
		@make install-nodeps

install-nodeps:
		@$(if $(XPROGS),[ -d $(BIN_DIR) ] || mkdir -p $(BIN_DIR))
		$(if $(XPROGS), $(CP) $(EXPORT_PROGS) $(BIN_DIR))
		@$(if $(XLIBS), [ -d $(SOLIB_DIR) ] || mkdir -p $(SOLIB_DIR))
		$(if $(XLIBS), $(CP) $(XLIBS) $(SOLIB_DIR))
		@$(if $(EXPORT_LIBEXEC_OBJS), [ -d $(LIBEXEC_DIR) ] || mkdir -p $(LIBEXEC_DIR))
		$(if $(EXPORT_LIBEXEC_OBJS), $(CP) $(EXPORT_LIBEXEC_OBJS) $(LIBEXEC_DIR))

# Propagate changes to headers and static libraries
srcmaint:	XLIBS =$(strip $(filter %.$(LIB_EXT),$(EXPORT_LIBS)))
srcmaint:	XHEADERS =$(strip $(EXPORT_HEADERS))
srcmaint:	$(strip $(filter %.$(LIB_EXT),$(EXPORT_LIBS))) $(EXPORT_HEADERS)
		@$(if $(XLIBS), [ -d $(STATICLIB_DIR) ] || mkdir -p $(STATICLIB_DIR))
		$(if $(XLIBS), $(CP) $(XLIBS) $(STATICLIB_DIR))
		@$(if $(XHEADERS), [ -d $(INCLUDE_DIR) ] || mkdir -p $(INCLUDE_DIR))
		$(if $(XHEADERS), $(CP) $(XHEADERS) $(INCLUDE_DIR))
