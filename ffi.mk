-include $(GPSEE_SRC_DIR)/libffi/vars.mk

ifeq ($(LIBFFI_CONFIG_DEPS),)
IGNORE_MODULES += gffi
$(warning *** Warning: You have not built LibFFI - gffi module disabled)
endif