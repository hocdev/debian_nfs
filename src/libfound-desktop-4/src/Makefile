CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -fPIC $(shell pkg-config --cflags glib-2.0 gobject-2.0 gio-2.0)
LDFLAGS ?= -shared -Wl,-soname,libfound-desktop-4-reconstructed.so.0
LIBS ?= $(shell pkg-config --libs glib-2.0 gobject-2.0 gio-2.0)

TARGET = libfound-desktop-4-reconstructed.so
SRCS = found-desktop-utils.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
