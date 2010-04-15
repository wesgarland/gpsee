ifndef GPSEE_SRC_DIR
$(error	GPSEE_SRC_DIR unspecified)
endif

ifndef BUILD
$(error	BUILD unspecified)
endif

ifndef TR
$(error TR unspecified)
endif

ifndef SPIDERMONKEY_SRC
$(error SPIDERMONKEY_SRC unspecified: did you forget to read the build instructions?)
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
