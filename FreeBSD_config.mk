# for iconv
CPPFLAGS += -I/usr/local/include
LDFLAGS += -L/usr/local/lib
GFFI_LDFLAGS            += -ldb
GPSEE_C_DEFINES         += HAVE_NDBM
