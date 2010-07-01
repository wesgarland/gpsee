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

# Compilation tools

ifdef COMPILER_IS_NON_GNU
$(error Sorry, your compiler is not yet supported)
else
ifdef GCC_PREFIX
GCC_BIN_DIR_SLASH ?= $(GCC_PREFIX)/bin/
endif
GCC		?= $(GCC_BIN_DIR_SLASH)gcc
GXX		?= $(GCC_BIN_DIR_SLASH)g++
CC		?= $(GCC)

CXX		?= $(GXX)
LINKER		?= $(GCC) -shared
CC		:= $(GCC_BIN_DIR_SLASH)$(CC)
CXX		:= $(GCC_BIN_DIR_SLASH)$(CXX)

CPP		?= $(GCC) -E
MAKEDEPEND	?= gcc -E -MM -MG
endif

LEX		?= lex
YACC		?= yacc
AR		?= ar
AR_RU		?= $(AR) -ru
SO_AR		?= $(LINKER) $(LDFLAGS) -o
RANLIB		?= ranlib

# Shell tools
CP		?= cp -f
MV		?= mv -f
RM		?= rm -f
RM_R		?= rm -rf
MKDIR		?= mkdir -p
GREP		?= grep
EGREP		?= egrep
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

ICONV_LDFLAGS	?= -liconv
GPSEE_C_DEFINES	+= HAVE_ICONV GPSEE_STD_SUSV3 GPSEE_STD_XSI
DEFAULT_GPSEE_PREFIX_DIR ?= /usr/local/gpsee

# Build a timestamp object, requires $(VERSION_H) [version.h]
ifndef NO_VERSION_O_RULES
ifdef VERSION_O
VERSION_C 	?= $(VERSION_O:.$(OBJ_EXT)=.c)
VERSION_H 	?= $(VERSION_O:.$(OBJ_EXT)=.h)

ifdef SUDO_USER
.PRECIOUS:		$(VERSION_O) $(VERSION_C)
else
.PHONY:			$(VERSION_O)
endif
$(VERSION_O):           $(VERSION_C)
	$(CC) $(CFLAGS) -c $(VERSION_C) -o $@
$(VERSION_C):           $(VERSION_C_IN) $(VERSION_H)
	$(MK_VERSION_C)
else
VERSION_C=
VERSION_H=
endif # VERSION_O
endif # NO_VERSION_O_RULES
