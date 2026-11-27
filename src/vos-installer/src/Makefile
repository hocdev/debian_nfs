CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Werror $(shell pkg-config --cflags gtk+-3.0 glib-2.0)
LDFLAGS ?= $(shell pkg-config --libs gtk+-3.0 glib-2.0)

OBJS = main.o installer_backend.o
TARGET = vos-installer

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c installer.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
