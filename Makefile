CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Wpedantic
CPPFLAGS ?= -Iinclude
LDFLAGS ?=
LDLIBS ?=

TARGET := small-shell
SRCS := \
	src/main.c \
	src/input.c \
	src/token.c \
	src/parser.c \
	src/expand.c \
	src/env.c \
	src/utils.c \
	src/runtime.c \
	src/exec.c \
	src/heredoc.c \
	src/redirection.c \
	src/builtin.c
OBJS := $(SRCS:.c=.o)
PARSER_API_TARGET := tests/parser-api

ifeq ($(USE_READLINE),1)
CPPFLAGS += -DUSE_READLINE
LDLIBS += -lreadline
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(PARSER_API_TARGET): tests/parser_api.c $(filter-out src/main.c,$(SRCS))
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ \
		tests/parser_api.c $(filter-out src/main.c,$(SRCS)) $(LDLIBS)

readline:
	$(MAKE) USE_READLINE=1

test: $(TARGET) $(PARSER_API_TARGET)
	./tests/smoke.sh
	./$(PARSER_API_TARGET)

clean:
	rm -f $(TARGET) $(PARSER_API_TARGET) $(OBJS)

.PHONY: all readline test clean
