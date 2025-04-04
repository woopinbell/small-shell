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
TEST_OBJS := $(SRCS:.c=.test.o)
TEST_TARGET := small-shell-test
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

%.test.o: %.c
	$(CC) $(CPPFLAGS) -DSMALL_SHELL_TESTING $(CFLAGS) -c -o $@ $<

$(TEST_TARGET): $(TEST_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(TEST_OBJS) $(LDLIBS)

$(PARSER_API_TARGET): tests/parser_api.c $(filter-out src/main.c,$(SRCS))
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ \
		tests/parser_api.c $(filter-out src/main.c,$(SRCS)) $(LDLIBS)

readline:
	$(MAKE) USE_READLINE=1

test: $(TARGET) $(TEST_TARGET) $(PARSER_API_TARGET)
	./tests/smoke.sh
	./tests/faults.sh
	./tests/allocation.sh
	./$(PARSER_API_TARGET)

clean:
	rm -f $(TARGET) $(TEST_TARGET) $(PARSER_API_TARGET) $(OBJS) $(TEST_OBJS)

.PHONY: all readline test clean
