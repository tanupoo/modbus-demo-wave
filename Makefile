TARGETS = modbus_server
TARGETS += modbus_client

include Makefile.common

CFLAGS += -g -Wall -Werror
CPPFLAGS += -I../apps/include
LDFLAGS += -L../apps/lib
LDLIBS += -lmodbus
