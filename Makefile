CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Wpedantic
CPPFLAGS ?= -Iinclude
LDFLAGS ?=
LDLIBS ?=

TARGET := small-shell
SRCS := src/main.c
OBJS := $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean
