CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -fPIC $(shell pkg-config --cflags glib-2.0 gobject-2.0 gio-2.0 gio-unix-2.0)
LDFLAGS ?= -shared -Wl,-soname,libcdos-desktop-taskbar.so.0 -Wl,--allow-shlib-undefined
LIBS ?= $(shell pkg-config --libs glib-2.0 gobject-2.0 gio-2.0 gio-unix-2.0)

TARGET = libcdos-desktop-taskbar.so
SRCS = cdos-panel.c cdos-applet.c applet-window-list.c applet-panel-launchers.c cdos-systray.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
