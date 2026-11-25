CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -fPIC -DGETTEXT_PACKAGE=\"nemo-share\" $(shell pkg-config --cflags gtk+-3.0 glib-2.0)
LDFLAGS ?= -shared -Wl,-soname,libnemo-share.so -Wl,--allow-shlib-undefined $(shell pkg-config --libs gtk+-3.0 glib-2.0)

OBJS = shares.o nemo-share.o
TARGET = libnemo-share.so

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
