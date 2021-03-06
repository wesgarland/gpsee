ifneq ($(NO_BUILD_RULES),TRUE)
# Standard targets
ifndef SUDO_USER
ifneq ($(MAKECMDGOALS),)
ifneq ($(MAKECMDGOALS),top)
ifneq ($(MAKECMDGOALS),help)
ifneq ($(MAKECMDGOALS),install-nodeps)
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),dist-clean)
ifneq ($(MAKECMDGOALS),all-clean)
ifneq ($(MAKECMDGOALS),build_debug)
depend depend.mk: 	XFILES=$(sort $(filter %.c %.cpp,$(DEPEND_FILES)))
depend depend.mk: 	$(AUTOGEN_HEADERS) $(AUTOGEN_SOURCE) $(wildcard Makefile) $(DEPEND_FILES)
			$(if $(XFILES), @echo " * Building dependencies for: $(XFILES)")
			$(if $(XFILES), $(MAKEDEPEND) $(MDFLAGS) $(CPPFLAGS) -DMAKEDEPEND $(XFILES) > depend.mk)
			@touch depend.mk
endif # goal = build_debug
endif # goal = all-clean
endif # goal = dist-clean
endif # goal = clean
endif # goal = install-nodeps
endif # goal = help
endif # goal = top
endif # goal = none
endif # ndef SUDO_USER

clean: _EXPORT_LIBEXEC_OBJS=$(filter-out %.js,$(EXPORT_LIBEXEC_OBJS))
clean:
	-$(if $(strip $(OBJS)), $(RM) $(OBJS))
	-$(if $(strip $(EXPORT_LIBS)), $(RM) $(EXPORT_LIBS))
	-$(if $(strip $(_EXPORT_LIBEXEC_OBJS)), $(RM) $(_EXPORT_LIBEXEC_OBJS)) 
	-$(if $(strip $(PROGS)), $(RM) $(PROGS))
	-$(if $(strip $(EXPORT_PROGS)), $(RM) $(EXPORT_PROGS))
	-$(if $(strip $(AUTOGEN_HEADERS)), $(RM) $(AUTOGEN_HEADERS))
	-$(if $(strip $(AUTOGEN_SOURCE)), $(RM) $(AUTOGEN_SOURCE))
	-$(if $(strip $(DEBUG_DUMP_DIRS)), $(RM_R) $(DEBUG_DUMP_DIRS))
	-$(if $(strip $(EXTRA_CLEAN_RULE)), $(MAKE) $(EXTRA_CLEAN_RULE))
	$(RM) depend.mk

build_debug:
	@echo " * GPSEE debug info: "
	@echo
	@echo "Depend Files:    $(DEPEND_FILES)"
	@echo "GPSEE Source:    $(GPSEE_SRC_DIR)"
	@echo "GPSEE Prefix:    $(GPSEE_PREFIX_DIR)"
	@echo "JSAPI Build:     $(SPIDERMONKEY_BUILD)"
	@echo "CC:              $(CC)"
	@echo "CFLAGS:          $(CFLAGS)"
	@echo "CPPFLAGS:        $(CPPFLAGS)"
	@echo "CXX:             $(CXX)"
	@echo "CXXFLAGS:        $(CXXFLAGS)"      
	@echo "LINKER:          $(LINKER)"
	@echo "LDFLAGS:         $(LDFLAGS)"
	@echo "LDLIBS:          $(LDLIBS)"
	@echo "ICONV_LDFLAGS:   $(ICONV_LDFLAGS)"
	@echo "ICONV_CPPFLAGS:  $(ICONV_CPPFLAGS)"
	@echo "ICONV_HEADER:    $(ICONV_HEADER)"
	@echo
	@echo "PROGS:           $(PROGS)"
	@echo "OBJS:		$(OBJS)"
	@echo "EXPORT_LIBS:	$(EXPORT_LIBS)"

# Install shared libraries
install-nodeps install install-solibs: XLIBS =$(strip $(filter %.$(SOLIB_EXT),$(EXPORT_LIBS)))
ifdef SUDO_USER
install-solibs:
else
install-solibs:	$(EXPORT_LIBS) $(EXPORT_LIBEXEC_OBJS)
endif
		@$(if $(XLIBS), [ -d $(SOLIB_DIR) ] || mkdir -p $(SOLIB_DIR))
		$(if $(XLIBS), $(CP) $(XLIBS) $(SOLIB_DIR))
		@$(if $(EXPORT_LIBEXEC_OBJS), [ -d $(LIBEXEC_DIR) ] || mkdir -p $(LIBEXEC_DIR))
		$(if $(EXPORT_LIBEXEC_OBJS), $(CP) $(EXPORT_LIBEXEC_OBJS) $(LIBEXEC_DIR))
		@$(if $(EXPORT_EXTRA_FILES), [ -d $(EXTRA_FILE_DIR) ] || mkdir -p $(EXTRA_FILE_DIR))
		$(if $(EXPORT_EXTRA_FILES), $(CP) $(EXPORT_EXTRA_FILES) $(EXTRA_FILE_DIR))

# Install binaries and shared libraries
install-nodeps install:	XPROGS =$(strip $(EXPORT_PROGS))
install-nodeps install:	XCGIS =$(strip $(CGI_PROGS))
ifdef SUDO_USER
install:
else
install:	$(EXPORT_LIBS) $(EXPORT_LIBEXEC_OBJS) $(EXPORT_PROGS) $(CGI_PROGS)
endif
ifneq (X$(EXPORT_LIBS)$(EXPORT_LIBEXEC_OBJS),X)
		@$(MAKE) install-solibs
endif
		@$(MAKE) install-nodeps
		@$(MAKE) srcmaint

install-nodeps:
		@$(if $(XPROGS),[ -d $(BIN_DIR) ] || mkdir -p $(BIN_DIR))
		$(if $(XPROGS), $(CP) $(EXPORT_PROGS) $(BIN_DIR))
		@$(if $(XLIBS), [ -d $(SOLIB_DIR) ] || mkdir -p $(SOLIB_DIR))
		$(if $(XLIBS), $(CP) $(XLIBS) $(SOLIB_DIR))
		@$(if $(EXPORT_LIBEXEC_OBJS), [ -d $(LIBEXEC_DIR) ] || mkdir -p $(LIBEXEC_DIR))
		$(if $(EXPORT_LIBEXEC_OBJS), $(CP) $(EXPORT_LIBEXEC_OBJS) $(LIBEXEC_DIR))
		@$(if $(EXPORT_EXTRA_FILES), [ -d $(EXTRA_FILE_DIR) ] || mkdir -p $(EXTRA_FILE_DIR))
		$(if $(EXPORT_EXTRA_FILES), $(CP) $(EXPORT_EXTRA_FILES) $(EXTRA_FILE_DIR))

# Propagate changes to headers and static libraries
srcmaint:	XLIBS =$(strip $(filter %.$(LIB_EXT),$(EXPORT_LIBS)))
srcmaint:	XHEADERS =$(strip $(EXPORT_HEADERS))
ifdef SUDO_USER
srcmaint:
else
srcmaint:	$(strip $(filter %.$(LIB_EXT),$(EXPORT_LIBS))) $(EXPORT_HEADERS)
endif
ifneq (X,X$(STATICLIB_DIR))
		@$(if $(XLIBS), [ -d $(STATICLIB_DIR) ] || mkdir -p $(STATICLIB_DIR))
		$(if $(XLIBS), $(CP) $(XLIBS) $(STATICLIB_DIR))
endif
		@$(if $(XHEADERS), [ -d $(INCLUDE_DIR) ] || mkdir -p $(INCLUDE_DIR))
		$(if $(XHEADERS), $(CP) $(XHEADERS) $(INCLUDE_DIR))
endif
