ifeq ($(LIBFFI_SRC),)
$(info *** Please untar libffi-3*.tar.gz in your home directory, or)
$(info *** edit $(abspath .)/local_config.mk)
$(info *** to reflect an alternate location.)
$(error Could not locate libffi source code)
endif

ifndef GPSEE_SRC_DIR
$(error	GPSEE_SRC_DIR unspecified)
endif

ifndef BUILD
$(error	BUILD unspecified)
endif

ifndef TR
$(error TR unspecified)
endif

ifndef LIBFFI_LIB_DIR
$(error LIBFFI_LIB_DIR unspecified: did you forget to read the build instructions?)
endif

ifndef LIBFFI_INCLUDE_DIR
$(error LIBFFI_INCLUDE_DIR unspecified: did you forget to read the build instructions?)
endif

ifneq ($(BUILD),DEBUG)
ifneq ($(BUILD),PROFILE)
ifneq ($(BUILD),DRELEASE)
ifneq ($(BUILD),RELEASE)
$(error invalid BUILD specified in local_config.mk)
endif
endif
endif
endif

ifneq ($(MAKECMDGOALS),build_debug)
ifeq (X$(LIBFFI_LIB_DIR),X)
$(error LIBFFI_LIB_DIR is invalid)
endif

ifeq (X$(LIBFFI_INCLUDE_DIR),X)
$(error LIBFFI_INCLUDE_DIR is invalid)
endif
endif
