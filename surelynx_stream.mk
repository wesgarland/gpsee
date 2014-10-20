include $(GPSEE_SRC_DIR)/system_detect.mk
APR_PROJECT 	= TRUE

SURELYNX_PREFIX_DIR	?= /var/surelynx
SURELYNX_PLATFORM 	?= $(UNAME_SYSTEM)-$(UNAME_RELEASE)-$(UNAME_MACHINE)-$(UNAME_CPU)
SURELYNX_PLATFORM_DIR	?= $(SURELYNX_PREFIX_DIR)/platform/$(SURELYNX_PLATFORM)

DEFAULT_GPSEE_PREFIX_DIR = $(SURELYNX_PREFIX_DIR)/gpsee
GPSEE_PREFIX_DIR	 = $(SURELYNX_PREFIX_DIR)/gpsee

GPSEE_LIBNAME	= gpsee
GPSEE_C_DEFINES	+= NO_GPSEE_FLOCK HAVE_ICONV HAVE_APR
GPSEE_LIBRARY	?= lib$(GPSEE_LIBNAME).$(SOLIB_EXIT)
LDFLAGS		+= -lapr_surelynx -lapr


LDFLAGS	 	  := -L$(SURELYNX_PLATFORM_DIR)/lib/apr_surelynx -L$(SURELYNX_PLATFORM_DIR)/lib -R$(SURELYNX_PREFIX_DIR)/apr/lib -R$(SURELYNX_PLATFORM_DIR)/lib/apr_surelynx -R$(SURELYNX_PLATFORM_DIR)/lib -R$(SURELYNX_PREFIX_DIR)/apr/lib
LDLIBS		  := $(SURELYNX_PLATFORM_DIR)/lib/apr_surelynx/libapr_surelynx.so $(LDLIBS)
CPPFLAGS 	  := -D__SURELYNX__ -I$(SURELYNX_PREFIX_DIR)/include/apr -I/var/surelynx/include/apr_surelynx -I/var/surelynx/include $(CPPFLAGS)


NO_VERSION_O_RULES = True
include $(GPSEE_SRC_DIR)/unix_stream.mk

