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
# Copyright (c) 2009, PageMail, Inc. All Rights Reserved.
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

include $(GPSEE_SRC_DIR)/ffi.mk
include sanity.mk

DEFS	 	 	= gpsee std network posix
AUTOGEN_HEADERS		+= compiler.dmp $(foreach DEF,$(DEFS),$(DEF)_defs.dmp) defines.incl structs.incl 
AUTOGEN_SOURCE		+= $(foreach DEF,$(DEFS),$(DEF)_defs.c) aux_types.incl
EXTRA_MODULE_OBJS	+= util.o structs.o defines.o MutableStruct.o CFunction.o Memory.o Library.o WillFinalize.o
PROGS			+= $(foreach DEF,$(DEFS),$(DEF)_defs) defines aux_types
OBJS			+= $(EXTRA_MODULE_OBJS)
CFLAGS			+= $(LIBFFI_CFLAGS)
LDFLAGS			+= $(LIBFFI_LDFLAGS)
MDFLAGS 		+= $(LIBFFI_CFLAGS)

.PRECIOUS:		$(AUTOGEN_SOURCE) $(AUTOGEN_HEADERS)

build:	$(DEF_FILES)

build_debug_module:
	@echo " - In gffi"
	@echo "   - CFLAGS = $(CFLAGS)"
	@echo "   - LDFLAGS = $(LDFLAGS)"

gffi_module.$(SOLIB_EXT):   LDFLAGS += -lffi
gffi_module.o: aux_types.incl jsv_constants.decl
structs.o: structs.incl
defines.o: defines.incl

%.dmp defines.incl: sort=LC_COLLATE=C sort

compiler.dmp:
	$(CPP) $(CPPFLAGS) -dM - < /dev/null | sed 's/[ 	][ 	]*/ /g' | $(sort) -u > $@
INCLUDE_DIRS=. /usr/local/include /usr/include /
%.dmp: compiler.dmp #Makefile
	@echo " * Generating $@ from $(HEADERS), found at:"
	@echo $(foreach HEADER, $(HEADERS), $(foreach DIR,$(INCLUDE_DIRS),$(wildcard $(DIR)/$(HEADER))))
	$(CPP) $(CPPFLAGS) -dM \
	        $(foreach HEADER, $(HEADERS), $(foreach DIR,$(INCLUDE_DIRS),$(wildcard $(DIR)/$(HEADER)))) \
		| sed 's/[ 	][ 	]*/ /g' \
		| $(sort) -u \
		| $(EGREP) -vf compiler.dmp \
		| $(EGREP) -v '^#define *NULL '\
		> $@ || [ X = X ]
	[ -s $@ ] || rm $@
	[ -f $@ ]

gpsee_defs.%: 	HEADERS  = $(GPSEE_SRC_DIR)/gpsee.h
std_defs.%:	HEADERS  = errno.h sys/types.h sys/stat.h fcntl.h unistd.h stdlib.h stdint.h stdio.h limits.h
math_defs.%:	HEADERS  = float.h fenv.h limits.h complex.h math.h 
network_defs.%:	HEADERS  = sys/types.h sys/socket.h time.h poll.h arpa/inet.h sys/select.h netinet/in.h arpa/nameser.h

network_defs.%:	HEADERS +=  resolv.h netdb.h
posix_defs.%:	HEADERS  = locale.h unistd.h stdio.h limits.h termios.h dirent.h errno.h fcntl.h sys/select.h signal.h sys/stat.h

posix_defs.%:	HEADERS += sys/wait.h sys/socket.h aio.h libgen.h strings.h string.h nl_types.h sys/types.h
posix_defs.%:	HEADERS += sys/types.h stdlib.h grp.h netdb.h utmpx.h fmtmsg.h fnmatch.h sys/statvfs.h sys/ipc.h ftw.h
posix_defs.%:	HEADERS += ucontext.h grp.h sys/resource.h sys/socket.h glob.h search.h arpa/inet.h math.h
posix_defs.%:	HEADERS += ctype.h sys/mman.h sys/msg.h spawn.h poll.h pthread.h sys/uio.h regex.h sched.h
posix_defs.%:	HEADERS += semaphore.h sys/sem.h pwd.h sys/shm.h monetary.h wchar.h sys/time.h sys/times.h sys/utsname.h
posix_defs.%:	HEADERS += wordexp.h 

############# BEWARE - Dragons Below ###############

# for sed
DEFINE=^ *\# *define
NAME=[^ ][^ ]*
START=$(DEFINE) $(NAME)[ ][ ]*

# for egrep; each expression must be parenthesized
OWS=([ ]*)
WS=([ ][ ]*)
SQUOTE='"'"'
LPAR=$(OWS)[(]$(OWS)
RPAR=$(OWS)[)]$(OWS)
CHARLIT=$(SQUOTE)[^$(SQUOTE)]$(SQUOTE)
INT_TYPE=([UL]|LL)
INT=((-?[0-9]+$(INT_TYPE)?)|(-?0x[A-Fa-f0-9]+$(INT_TYPE)?))
FLOAG_TYPE=([LF])
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

STRING_EXPR="([^\\"]|\\\\|\\")*"

%_defs.c: %_defs.dmp
	@echo " * Building $@"
	@echo "/* `date` */" > $@
	@echo " - Integer Expression"
	@$(foreach HEADER, $(HEADERS), echo "#include <$(HEADER)>" >> $@;)
	@echo "#include <stdio.h>" >> $@
	@echo "#include \"../../gpsee_formats.h\"" >> $@
	@echo "#undef main" >> $@
	@echo "int main(int argc, char **argv) {" >> $@
	@$(EGREP) '$(START)$(INT_EXPR)$$' $*_defs.dmp \
	| sed -e 's/^\(#define \)\([^ ][^ ]*\)\(.*\)/\
		printf("haveInt(\2,");\
		printf(((\2 < 0) ? "%lld,1," GPSEE_SIZET_FMT ")\\n":"%llu,0," GPSEE_SIZET_FMT ")\\n"),(long long)(\2),sizeof(\2));/' \
	>> $@
	@echo " - Floating-point Expression"
	@$(EGREP) '$(START)$(FLOAT_EXPR)$$' $*_defs.dmp \
	| sed -e 's/^\(#define \)\([^ ][^ ]*\)\(.*\)/\
		printf("haveFloat(\2,%e100," GPSEE_SIZET_FMT ")\\n",(\2),sizeof(\2));/' \
	>> $@
	@echo " - Strings"
	@$(EGREP) '$(START)$(STRING_EXPR)$$' $*_defs.dmp \
	| sed -e 's/^\(#define \)\([^ ][^ ]*\)\(.*\)/\
		printf("haveString(\2,\\\"%s\\\")\\n",(\2));/' \
	>> $@
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
# Warning: regexps lack precision, and sed really does need literal newlines in the insert command

	sed \
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
	< structs.decl > $@

parseMan:
	man $$MANPAGE \
	| grep '^       .*\*/ *$$' | sed -e 's;*/;XXX;' -e 's/^  */member(/' -e 's/;/,    )/' -e 's/  *\*/*,/' -e 's/  */,  /' -e 's/*,/ *, /' -e 's/XXX/*\//' -e 's;, */\*;,       /*;'  -e 's; */\*;	/*;' -e 's/^/  /' -e 's/, *, *)/,	)/'
