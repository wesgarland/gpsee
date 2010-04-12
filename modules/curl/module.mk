
EXTRA_MODULE_OBJS = Memory.o
OBJS += $(EXTRA_MODULE_OBJS)
# Where are curl includes?
#   /opt/local/include for macports
#   /usr/local/include for others
EXTRA_CFLAGS += -I/opt/local/include
LDFLAGS += -L/opt/local/lib -lcurl
