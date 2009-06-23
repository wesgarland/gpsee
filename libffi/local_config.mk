## Please edit this file to suit your local environment and requirements.
## This file controls most of aspects of auto-building libffi 3 or better,
## to maintain configuration synchronization with GPSEE.
##
## Using a system-supplied libffi is possible/acceptable, but currently
## untested/unsupported. To enable system-supplied libffi, omit the
## LIBFFI_SRC_DIR variable.

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

lastword ?= $(if $1,$(word $(words $1),$1))

GPSEE_SRC_DIR 		?= ..
#LIBFFI_SRC		?= $(HOME)/libffi-3.0.8
LIBFFI_SRC		?= $(lastword $(sort $(wildcard $(HOME)/libffi-3*[0-9])))
PKG_CONFIG		?= pkg-config
LIBFFI_PREFIX		?= $(GPSEE_PREFIX_DIR)/libffi
LIBFFI_MAKE_OPTIONS	?= -j3
TR			?= tr

ifeq ($(LIBFFI_SRC),)
$(info *** Please untar libffi-3*.tar.gz in your home directory, or)
$(info *** edit $(PWD)/local_config.mk)
$(info *** to reflect an alternate location.)
$(error Could not locate libffi source code)
endif

## - RELEASE    Optimized build, no symbols
## - DRELEASE   Optimized build, symbols
## - PROFILE    Optimized build, symbols, profiling enabled
## - DEBUG      Debug build (assertions), symbols
BUILD			?= RELEASE

NO_BUILD_RULES=TRUE
include $(GPSEE_SRC_DIR)/outside.mk
