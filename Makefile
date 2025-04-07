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
	src/string_builder.c \
	src/runtime.c \
	src/exec.c \
	src/heredoc.c \
	src/redirection.c \
	src/builtin.c
OBJS := $(SRCS:.c=.o)
TEST_OBJS := $(SRCS:.c=.test.o)
TEST_TARGET := small-shell-test
PARSER_API_TARGET := tests/parser-api
TIMEOUT_TARGET := tests/timeout-runner
ASAN_TARGET := small-shell-asan
ASAN_TEST_TARGET := small-shell-test-asan
ASAN_PARSER_API_TARGET := tests/parser-api-asan
UBSAN_TARGET := small-shell-ubsan
UBSAN_TEST_TARGET := small-shell-test-ubsan
UBSAN_PARSER_API_TARGET := tests/parser-api-ubsan
SANITIZER_CFLAGS := $(CFLAGS) -O1 -g -fno-omit-frame-pointer
SANITIZER_IMAGE ?= gcc:13-bookworm

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

$(TIMEOUT_TARGET): tests/timeout_runner.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $<

$(ASAN_TARGET): $(SRCS)
	$(CC) $(CPPFLAGS) $(SANITIZER_CFLAGS) -fsanitize=address \
		$(LDFLAGS) -o $@ $(SRCS) $(LDLIBS)

$(ASAN_TEST_TARGET): $(SRCS)
	$(CC) $(CPPFLAGS) -DSMALL_SHELL_TESTING $(SANITIZER_CFLAGS) \
		-fsanitize=address $(LDFLAGS) -o $@ $(SRCS) $(LDLIBS)

$(ASAN_PARSER_API_TARGET): tests/parser_api.c $(filter-out src/main.c,$(SRCS))
	$(CC) $(CPPFLAGS) $(SANITIZER_CFLAGS) -fsanitize=address \
		$(LDFLAGS) -o $@ tests/parser_api.c \
		$(filter-out src/main.c,$(SRCS)) $(LDLIBS)

$(UBSAN_TARGET): $(SRCS)
	$(CC) $(CPPFLAGS) $(SANITIZER_CFLAGS) -fsanitize=undefined \
		$(LDFLAGS) -o $@ $(SRCS) $(LDLIBS)

$(UBSAN_TEST_TARGET): $(SRCS)
	$(CC) $(CPPFLAGS) -DSMALL_SHELL_TESTING $(SANITIZER_CFLAGS) \
		-fsanitize=undefined $(LDFLAGS) -o $@ $(SRCS) $(LDLIBS)

$(UBSAN_PARSER_API_TARGET): tests/parser_api.c $(filter-out src/main.c,$(SRCS))
	$(CC) $(CPPFLAGS) $(SANITIZER_CFLAGS) -fsanitize=undefined \
		$(LDFLAGS) -o $@ tests/parser_api.c \
		$(filter-out src/main.c,$(SRCS)) $(LDLIBS)

readline:
	$(MAKE) USE_READLINE=1

test: $(TARGET) $(TEST_TARGET) $(PARSER_API_TARGET) $(TIMEOUT_TARGET)
	./tests/smoke.sh
	./tests/faults.sh
	./tests/allocation.sh
	./tests/lifecycle.sh
	./$(PARSER_API_TARGET)
	./tests/performance.sh

test-asan: $(ASAN_TARGET) $(ASAN_TEST_TARGET) \
		$(ASAN_PARSER_API_TARGET) $(TIMEOUT_TARGET)
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
		SMALL_SHELL_BIN="$(CURDIR)/$(ASAN_TARGET)" ./tests/smoke.sh
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
		SMALL_SHELL_TEST_BIN="$(CURDIR)/$(ASAN_TEST_TARGET)" \
		./tests/faults.sh
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
		SMALL_SHELL_TEST_BIN="$(CURDIR)/$(ASAN_TEST_TARGET)" \
		./tests/allocation.sh
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
		SMALL_SHELL_TEST_BIN="$(CURDIR)/$(ASAN_TEST_TARGET)" \
		./tests/lifecycle.sh
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
		./$(ASAN_PARSER_API_TARGET)
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
		SMALL_SHELL_BIN="$(CURDIR)/$(ASAN_TARGET)" \
		./tests/performance.sh

test-ubsan: $(UBSAN_TARGET) $(UBSAN_TEST_TARGET) \
		$(UBSAN_PARSER_API_TARGET) $(TIMEOUT_TARGET)
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
		SMALL_SHELL_BIN="$(CURDIR)/$(UBSAN_TARGET)" ./tests/smoke.sh
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
		SMALL_SHELL_TEST_BIN="$(CURDIR)/$(UBSAN_TEST_TARGET)" \
		./tests/faults.sh
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
		SMALL_SHELL_TEST_BIN="$(CURDIR)/$(UBSAN_TEST_TARGET)" \
		./tests/allocation.sh
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
		SMALL_SHELL_TEST_BIN="$(CURDIR)/$(UBSAN_TEST_TARGET)" \
		./tests/lifecycle.sh
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
		./$(UBSAN_PARSER_API_TARGET)
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
		SMALL_SHELL_BIN="$(CURDIR)/$(UBSAN_TARGET)" \
		./tests/performance.sh

test-sanitizers-container:
	docker run --rm --network none --read-only \
		--tmpfs /tmp:exec,size=256m -v "$(CURDIR):/source:ro" \
		$(SANITIZER_IMAGE) \
		sh /source/tests/container_sanitizers.sh

clean:
	rm -f $(TARGET) $(TEST_TARGET) $(PARSER_API_TARGET) $(TIMEOUT_TARGET) \
		$(ASAN_TARGET) $(ASAN_TEST_TARGET) $(ASAN_PARSER_API_TARGET) \
		$(UBSAN_TARGET) $(UBSAN_TEST_TARGET) $(UBSAN_PARSER_API_TARGET) \
		$(OBJS) $(TEST_OBJS)
	rm -rf $(ASAN_TARGET).dSYM $(ASAN_TEST_TARGET).dSYM \
		$(ASAN_PARSER_API_TARGET).dSYM $(UBSAN_TARGET).dSYM \
		$(UBSAN_TEST_TARGET).dSYM $(UBSAN_PARSER_API_TARGET).dSYM

.PHONY: all readline test test-asan test-ubsan \
	test-sanitizers-container clean
