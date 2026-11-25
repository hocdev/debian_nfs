CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -fPIC $(shell pkg-config --cflags gtk+-3.0)
LDFLAGS ?= -shared -Wl,-soname,libfound-gtk3.so.0
LIBS ?= $(shell pkg-config --libs gtk+-3.0)

TARGET = libfound-gtk3.so.0.0.0
SRCS = found-gtk3.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
