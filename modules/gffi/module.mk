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
# Copyright (c) 2009-2010, PageMail, Inc. All Rights Reserved.
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
# Notes
#
# 1. Cannot use standard AUTOGEN_HEADERS / AUTOGEN_SOURCE rules for some 
#    of these files, as producing them during the depend.mk phase could
#    result in compilation errors (and does on Snow Leopard).  This is 
#    because the depend.mk phase precenes the dynamic generation of 
#    std_cppflags.mk, which in turn selects the correct standards symbol
#    set for for the target being built.
#
# 2. There are "standards" symbol sets and "regular" symbols sets. The
#    "standards" symbols sets are built with an explicitly-selected
#    standard, usually Single UNIX Specification v3 (SUSv3) and the
#    System V Interface Definition (SVID). Whatever standard is selected,
#    (via GPSEE_STD_ macros) GFFI will theoretically expose the standard
#    symbol name, with a dynamically generated trampoline if necessary,
#    or die trying (during the build process).  We force C99 for the
#    standard symbol sets, although this is not strictly necessary if
#    we're not exposing SUSv3.
#
# 3. The "gpsee" symbol set is special, and refers to "however GPSEE was
#    was built" - which is usually /not/ strict SUSv3
#
# 4. Additional structs can be added in structs.decl
#
# 5. Additional symbols sets (header groups) can by added by specifying
#    MYGROUP_defs.%: 	HEADERS  = list_of_headers_with_defines
#    and then adding MYGROUP to do the DEFS variable.

GFFI_DIR := $(shell pwd)

include $(GPSEE_SRC_DIR)/ffi.mk
include $(GPSEE_SRC_DIR)/iconv.mk
ifneq ($(MAKECMDGOALS),clean)
include std_cppflags.mk
endif
include sanity.mk

std_cppflags.mk: mk_std_cppflags
	@echo " * Building $@"
	$(CC) $(CFLAGS) $(CPPFLAGS) mk_std_cppflags.c -o mk_std_cppflags
	./mk_std_cppflags "STD_CPPFLAGS=-std=gnu99" > $@
mk_std_cppflags: CPPFLAGS := -I$(GPSEE_SRC_DIR) $(GFFI_CPPFLAGS) -std=gnu99

DEFS	 	 	= gpsee std
ND_AUTOGEN_HEADERS	+= compiler_dmp.re $(foreach DEF,$(DEFS),$(DEF)_defs.dmp) defines.incl structs.incl std_gpsee_no.h std_macro_consts.h
ND_AUTOGEN_SOURCE	+= $(foreach DEF,$(DEFS),$(DEF)_defs.c) aux_types.incl std_cppflags.mk
EXTRA_MODULE_OBJS	+= util.o structs.o defines.o std_functions.o MutableStruct.o CFunction.o Memory.o Library.o WillFinalize.o CType.o
PROGS			+= $(foreach DEF,$(DEFS),$(DEF)_defs) defines-test aux_types mk_std_cppflags std_macro_consts
OBJS			+= $(EXTRA_MODULE_OBJS)
CFLAGS			+= $(LIBFFI_CFLAGS)
LDFLAGS			+= $(LIBFFI_LDFLAGS) $(GFFI_LDFLAGS)
MDFLAGS 		+= $(LIBFFI_CFLAGS)

.PRECIOUS:		$(ND_AUTOGEN_SOURCE) $(ND_AUTOGEN_HEADERS)

build:

build_debug_module:
	@echo " - In gffi"
	@echo "   - CFLAGS   		= $(CFLAGS)"
	@echo "   - LDFLAGS  		= $(LDFLAGS)"
	@echo "   - CPPFLAGS 		= $(CPPFLAGS)"
	@echo "   - STD_CPPFLAGS	= $(STD_CPPFLAGS)"
	@echo "   - GFFI_CPPFLAGS	= $(GFFI_CPPFLAGS)"

clean:	OBJS += aux_types.o
clean:	AUTOGEN_HEADERS = $(ND_AUTOGEN_HEADERS)
clean:  AUTOGEN_SOURCE = $(ND_AUTOGEN_SOURCE)

gffi.$(SOLIB_EXT):   LDFLAGS += -lffi
gffi.o: aux_types.incl jsv_constants.decl
structs.o: structs.incl
defines.o: defines.incl
std_functions.o std_gpsee_no.h std_defs.dmp std_defs std_defs.o: CPPFLAGS += $(GFFI_CPPFLAGS) $(STD_CPPFLAGS)
std_macro_consts.o std_functions.o: CPPFLAGS := -I$(GPSEE_SRC_DIR) $(CPPFLAGS) 
std_functions.o: std_gpsee_no.h

std_gpsee_no.h: std_functions.h std_macro_consts.h
	@echo " * Building $@"
	@echo "/* `date` */" > $@
	$(EGREP) '^function[(]' function_aliases.incl | $(SED) -e 's/^[^,]*, *//' -e 's/,.*//' -e 's/.*/#define GPSEE_NO_&/' >> $@
	$(EGREP) '^voidfunction[(]' function_aliases.incl | $(SED) -e 's/^[^(]*(//' -e 's/,.*//' -e 's/.*/#define GPSEE_NO_&/' >> $@
	$(EGREP) '^function[(]' unsupported_functions.incl | $(SED) -e 's/.*[(]//' -e 's/[)].*//' -e 's/.*/#define GPSEE_NO_&/' >> $@
	$(CPP) $(CPPFLAGS) -dM - < std_functions.h | $(SED) 's/[ 	][ 	]*/ /g' | $(EGREP) '[ 	]__builtin_..*$$' | \
		$(SED) -e 's/^#define //' -e 's/[ (].*//' -e 's/^_*//' -e 's/.*/#define GPSEE_NO_&/' >> $@

std_macro_consts.h: std_macro_consts
	./std_macro_consts > $@

compiler_dmp.re %.dmp defines.incl: sort=LC_COLLATE=C sort

compiler_dmp.re:
	$(CPP) $(CPPFLAGS) -dM - < /dev/null | $(SED) 's/[ 	][ 	]*/ /g' | $(sort) -u \
	| $(SED) \
		-e 's/[[]/[[]/g' -e 's/[]]/[]]/g' \
		-e 's/[*]/[*]/g' -e 's/[?]/[?]/g' \
		-e 's/[(]/[(]/g' -e 's/[)]/[)]/g' \
		-e 's/[.]/[.]/g' \
		-e 's/[|]/[|]/g' \
	> $@
INCLUDE_DIRS=$(GFFI_DIR) /usr/local/include /usr/include /
std_defs.dmp gpsee_defs.dmp: std_macro_consts.h
%.dmp: FP_HEADERS=$(sort $(wildcard $(foreach DIR,$(INCLUDE_DIRS),$(addprefix $(DIR)/,$(filter-out $^,$(HEADERS))))) $(addprefix $(GFFI_DIR)/,$(filter $^,$(HEADERS))))
%.dmp: compiler_dmp.re #Makefile
	@echo ""
	@echo " * Generating $@ from $(HEADERS), found at:"
	@ls $(FP_HEADERS)
	@echo ""
	$(CPP) $(CPPFLAGS) -dM $(FP_HEADERS)\
		| $(SED) 's/[ 	][ 	]*/ /g' \
		| $(sort) -u \
		| $(EGREP) -vf compiler_dmp.re \
		| $(EGREP) -v '^#define *NULL '\
		> $@ || [ X = X ]
	[ -s $@ ] || rm $@
	[ -f $@ ]

std_defs:	LEADING_CPPFLAGS += -I$(GFFI_DIR)
std_defs.%:	HEADERS  = std_functions.h stdint.h std_macro_consts.h
gpsee_defs.%: 	HEADERS  = $(GPSEE_SRC_DIR)/gpsee.h $(GPSEE_SRC_DIR)/gpsee-iconv.h std_macro_consts.h

############ BEWARE - Dragons Below ###############

# for sed
DEFINE=^ *\# *define
NAME=[^ ][^ ]*
START=$(DEFINE) $(NAME)[ ][ ]*
ARGMACRO_START=$(DEFINE) $(NAME)[ ]*[(][^)]*[)]

# for egrep; each expression must be parenthesized
OWS=([ ]*)
WS=([ ][ ]*)
SQUOTE='"'"'
LPAR=$(OWS)[(]$(OWS)
RPAR=$(OWS)[)]$(OWS)
CHARLIT=$(SQUOTE)[^$(SQUOTE)]$(SQUOTE)
INT_TYPE=([UL]|LL)
INT=((-?[0-9]+$(INT_TYPE)?)|(-?0x[A-Fa-f0-9]+$(INT_TYPE)?))
FLOAT_TYPE=([LF])
FLOAT=(-?[0-9]+\.[0-9]+(e[+-][0-9]+)$(FLOAT_TYPE)?)
OPER=(<<|>>|\||\^|%|\-|\+|\*|/|~|&&|\|\||!)
OPER:=($(OWS)$(OPER)$(OWS))
CAST_T=(int|long|char)
CAST_T:=(($(CAST_T))|(unsigned|signed|long)$(WS)$(CAST_T))
CAST_T:=($(CAST_T)|const $(CAST_T))
CAST=($(LPAR)$(OWS)$(CAST_T)$(OWS)$(RPAR))

# This is not exhaustive (it cannot recurse)
INT_EXPR=($(INT)|($(CHARLIT)))
INT_EXPR:=($(INT_EXPR)|$(CAST)$(OWS)$(INT_EXPR))
INT_EXPR:=($(INT_EXPR)|($(LPAR)$(INT_EXPR)$(RPAR)))
INT_EXPR:=($(INT_EXPR)|$(CAST)$(OWS)$(INT_EXPR))
INT_EXPR:=($(INT_EXPR)|($(LPAR)$(INT_EXPR)$(RPAR)))
INT_EXPR:=($(INT_EXPR)|$(INT_EXPR)($(OPER)$(INT_EXPR)))+
INT_EXPR:=($(INT_EXPR)|($(LPAR)$(INT_EXPR)$(RPAR))+)

# Neither is this
FLOAT_EXPR:=$(FLOAT)
FLOAT_EXPR:=($(FLOAT_EXPR)|$(CAST)$(OWS)$(FLOAT_EXPR))
FLOAT_EXPR:=($(FLOAT_EXPR)|($(LPAR)$(FLOAT_EXPR)$(RPAR)))
FLOAT_EXPR:=($(FLOAT_EXPR)|$(CAST)$(OWS)$(FLOAT_EXPR))
FLOAT_EXPR:=($(FLOAT_EXPR)|($(LPAR)$(FLOAT_EXPR)$(RPAR)))
FLOAT_EXPR:=($(FLOAT_EXPR)|$(FLOAT_EXPR)($(OPER)$(FLOAT_EXPR)))+
FLOAT_EXPR:=($(FLOAT_EXPR)|($(LPAR)$(FLOAT_EXPR)$(RPAR))+)

# This isn't bad
STRING_EXPR="([^\\"]|\\\\|\\")*"

# But this is
TMS_EXPR=([A-Za-z0-9_()~!+-][\" A-Za-z0-9_()~!^&|<>,+-]*)

%_defs.c: %_defs.dmp
	@echo " * Building $@"
	@echo "/* `date` */" > $@
		@echo " - Integer Expression"
	@$(foreach HEADER, $(HEADERS), echo "#include \"$(HEADER)\"" >> $@;)
	@echo "#include <stdio.h>" >> $@
	@echo "#include \"../../gpsee_formats.h\"" >> $@
	@echo "#undef main" >> $@
	@echo "int main(int argc, char **argv) {" >> $@
	@$(EGREP) '$(START)$(INT_EXPR)$$' $*_defs.dmp \
	| $(SED) \
		-e 's/  */ /g' \
		-e 's/^\(#define \)\([^ ][^ ]*\)\(.*\)/\
		printf("haveInt(\2,"); \
	        printf(((\2 < 0) ? "%lld,1," GPSEE_SIZET_FMT ")\\n":"%llu,0," GPSEE_SIZET_FMT ")\\n"),(long long)(\2),sizeof(\2));/' \
	>> $@
	@echo " - Floating-point Expression"
	@$(EGREP) '$(START)$(FLOAT_EXPR)$$' $*_defs.dmp \
		| $(SED) \
		-e 's/  */ /g' \
		-e 's/^\(#define \)\([^ ][^ ]*\)\(.*\)/\
		printf("haveFloat(\2,%100e," GPSEE_SIZET_FMT ")\\n",(\2),sizeof(\2));/' \
	>> $@
	@echo " - Strings"
	@$(EGREP) '$(START)$(STRING_EXPR)$$' $*_defs.dmp \
	| $(SED) -e 's/^\(#define \)\([^ ][^ ]*\)\(.*\)/\
		printf("haveString(\2,\\\"%s\\\")\\n",(\2));/' \
	>> $@

	@echo " - Transitive Macros & Simple Expressions"
	@$(EGREP) '$(START)$(TMS_EXPR)$$' $*_defs.dmp\
	| $(EGREP) -v '($(ARGMACRO_START))|($(START)$(STRING_EXPR))' \
	| $(EGREP) -v '($(START)$(STRING_EXPR))' \
	| $(EGREP) -v '($(START)$(INT_EXPR))' \
	| $(EGREP) -v '($(START)$(FLOAT_EXPR))' \
	| $(SED) -f tmse_parse.sed \
		>>$@
#	@echo " - Argument Macro Expressions"
#	$(EGREP) '$(ARGMACRO_START) *..*$$' $*_defs.dmp\
#	| $(SED) \
#		-e 's/^#define  *//' \
#		-e 's/\"/\\\\\\\"/g'\
#		-e 's/^\([^(]*\)\( *\)\([(][^)]*[)]\)\( *\)\(.*\)$$/puts("haveArgMacro(\1, \\"\3\\",\\"\5\\")");/' \
#		>>$@
	@echo "return 0; }" >> $@

check:
	@if echo "$(strip $(VALUE))" | $(EGREP) '^$(STRING_EXPR)$$' >/dev/null; then \
	  echo "$(VALUE) is a STRING_EXPR";\
	else \
	  echo "$(VALUE) is not a STRING_EXPR";\
	fi
	@if echo "$(strip $(VALUE))" | $(EGREP) '^$(FLOAT_EXPR)$$' >/dev/null; then \
	  echo "$(VALUE) is a FLOAT_EXPR";\
	else \
	  echo "$(VALUE) is not a FLOAT_EXPR";\
	fi
	@if echo "$(strip $(VALUE))" | $(EGREP) '^$(INT_EXPR)$$' >/dev/null; then \
	  echo "$(VALUE) is an INT_EXPR";\
	else \
	  echo "$(VALUE) is not an INT_EXPR";\
	fi

defines.incl: $(foreach DEF,$(DEFS),$(DEF)_defs)
	@echo " * Building $@"
	@echo "/* `date` */" > $@
	@echo  "#pragma GCC system_header" >> $@
	@for DEF in $(DEFS); do echo "startDefines($${DEF})"; ./$${DEF}_defs | $(sort) -k2 -t '('; echo "endDefines($${DEF})"; done >> $@
	@echo "haveDefs($(foreach DEF,$(DEFS),$(DEF),))" >> $@
	@for DEF in $(DEFS); do echo "haveDef($${DEF})"; done >> $@

aux_types.incl: aux_types aux_types.decl 
	./aux_types > $@

structs.incl: structs.decl module.mk
	@echo " * Building $@"
	@echo "/* `date` */" > $@
	$(SED) \
		-e '/beginStruct/h' \
		-e 's/\(beginStruct[(]\)\([^)]*\)\([)].*\)/#define member_offset(X) offsetOf(\2,X)/' \
		-e '/^#define member_offset/p' \
		-e 's/member_offset/member_size/' \
		-e 's/offsetOf/fieldSize/' \
		-e '/member_size/p' \
		-e '/member_size/x' \
		-e 's/\(beginStruct[(]\)\([^)]*\)\([)].*\)/\1\2,\2\3/' \
		-e ':loop' -e 's/\(beginStruct[^,]*,[^ ]*\) /\1_/' -e '/beginStruct/t loop' \
		-e '/endStruct/p' \
		-e 's/\(endStruct[(]\)\([^)]*\)\([)].*\)/#undef member_offset/' \
		-e '/^#undef member_offset/p' \
		-e 's/member_offset/member_size/' \
	< structs.decl >> $@


defines-test:	LDFLAGS = $(shell $(GPSEE_SRC_DIR)/gpsee-config --ldflags)
