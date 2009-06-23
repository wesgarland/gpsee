ifneq ($(NO_BUILD_RULES),TRUE)
# Standard targets
ifneq ($(MAKECMDGOALS),install-nodeps)
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),dist-clean)
ifneq ($(MAKECMDGOALS),all-clean)
ifneq ($(MAKECMDGOALS),build_debug)
depend depend.mk: 	XFILES=$(sort $(filter %.c %.cc %.cpp %.cxx,$(DEPEND_FILES)))
depend depend.mk: 	Makefile $(DEPEND_FILES)
ifeq (X$(JSAPI_INCLUDE_DIR),X)
			$(error Your JSAPI include directory is not specified. This is \
			normally set in spidermonkey/vars.mk, which is in turn\
			created by the install rule in spidermonkey/Makefile.)
endif
			$(if $(XFILES), @echo " * Building dependencies for: $(XFILES)")
			$(if $(XFILES), $(MAKEDEPEND) $(MDFLAGS) $(CPPFLAGS) -DMAKEDEPEND $(XFILES) > depend.mk)
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
	@echo "GPSEE Prefix: $(GPSEE_PREFIX_DIR)"
	@echo "JSAPI Build:  $(SPIDERMONKEY_BUILD)"
	@echo "CC:           $(CC)"
	@echo "CFLAGS:       $(CFLAGS)"
	@echo "CPPFLAGS:     $(CPPFLAGS)"
	@echo "LDFLAGS:      $(LDFLAGS)"
	@echo "LOADLIBES:    $(LOADLIBES)"
	@echo "CXX:          $(CXX)"
	@echo "CXXFLAGS:     $(CXXFLAGS)"      

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
		@make srcmaint

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
ifneq (X,X$(STATICLIB_DIR))
		@$(if $(XLIBS), [ -d $(STATICLIB_DIR) ] || mkdir -p $(STATICLIB_DIR))
		$(if $(XLIBS), $(CP) $(XLIBS) $(STATICLIB_DIR))
endif
		@$(if $(XHEADERS), [ -d $(INCLUDE_DIR) ] || mkdir -p $(INCLUDE_DIR))
		$(if $(XHEADERS), $(CP) $(XHEADERS) $(INCLUDE_DIR))
endif
