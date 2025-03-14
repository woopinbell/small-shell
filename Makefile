CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Wpedantic
CPPFLAGS ?= -Iinclude
LDFLAGS ?=
LDLIBS ?=

TARGET := small-shell
SRCS := \
	src/main.c \
	src/input.c
OBJS := $(SRCS:.c=.o)

ifeq ($(USE_READLINE),1)
CPPFLAGS += -DUSE_READLINE
LDLIBS += -lreadline
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

readline:
	$(MAKE) USE_READLINE=1

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all readline clean
