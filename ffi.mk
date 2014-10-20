FFI=No
ifneq ($(ALL_MODULES),X)
ifeq ($(filter $(ALL_MODULES),gffi),gffi)
FFI=Yes
endif
endif

ifeq ($(MODULE),gffi)
FFI=Yes
endif

ifeq ($(FFI),Yes)
-include $(GPSEE_SRC_DIR)/libffi/vars.mk
ifeq ($(LIBFFI_CONFIG_DEPS),)
ifeq (X,X$(filter $(MAKECMDGOALS),clean build_debug clean_modules clean_makefile_depends))
$(error You have not built LibFFi - if you do not want FFI, please add gffi to the IGNORE_MODULES list in $(GPSEE_SRC_DIR)/Makefile)
endif
$(warning *** Warning: You have not built LibFFI - gffi module disabled (target: $(MAKECMDGOALS)))
else
LIB_FFI                 ?= $(LIBFFI_LIB_DIR)/libffi.$(SOLIB_EXT)
endif
endif
