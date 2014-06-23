ifndef GPSEE_SRC_DIR
$(error	GPSEE_SRC_DIR unspecified)
endif

ifneq (X,X$(filter $(MAKECMDGOALS),install build all real-clean))
ifndef BUILD
$(error	BUILD unspecified)
endif

ifndef STREAM
$(error	STREAM unspecified)
endif

ifndef TR
$(error TR unspecified)
endif

ifndef SED
$(error SED unspecified)
endif

ifndef EGREP
$(error EGREP unspecified)
endif

ifndef SPIDERMONKEY_SRC
$(error SPIDERMONKEY_SRC unspecified: did you forget to read the build instructions?)
endif

ifndef SPIDERMONKEY_BUILD
$(error SPIDERMONKEY_BUILD unspecified: did you forget to read the build instructions?)
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

ifeq (X$(JSAPI_INCLUDE_DIR),X)
$(error JSAPI_INCLUDE_DIR is not specified; make install in $(abspath ./spidermonkey)/ to fix)
endif

ifndef LIB_EXT
$(error LIB_EXT is not specified)
endif

ifndef SOLIB_EXT
$(error SOLIB_EXT is not specified)
endif

ifndef AR_RU
$(error AR_RU is not specified)
endif

ifndef LINK_SOLIB
$(error LINK_SOLIB is not specified)
endif

ifndef RANLIB
$(error RANLIB is not specified)
endif

endif #done building-target-conditional part
