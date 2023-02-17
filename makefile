UNAME = $(shell uname -o)

CC = gcc
CFLAGS = -Wextra -Wall -std=gnu99 -Iinclude -Wno-unused-parameter -Wno-unused-variable -Wno-duplicate-decl-specifier

ifeq ($(UNAME), Msys)
MSFLAGS = -lws2_32
endif

MQTT_C_SOURCES = src/mqtt.c src/mqtt_pal.c
MQTT_C_EXAMPLES = bin/simple_publisher bin/simple_subscriber bin/reconnect_subscriber 
#MQTT_C_UNITTESTS = bin/tests
BINDIR = bin

all: $(BINDIR) $(MQTT_C_EXAMPLES)

bin/simple_%: examples/simple_%.c $(MQTT_C_SOURCES)
	$(CC) $(CFLAGS) $^ -lpthread -lcurl $(MSFLAGS) -o $@

bin/reconnect_%: examples/reconnect_%.c $(MQTT_C_SOURCES)
	$(CC) $(CFLAGS) $^ -lpthread $(MSFLAGS) -o $@


$(BINDIR):
	mkdir -p $(BINDIR)

clean:
	rm -rf $(BINDIR)

check: all
	./$(MQTT_C_UNITTESTS)
