CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Wpedantic -MMD -MP
CPPFLAGS ?= -Iinclude
LDFLAGS ?=
LDLIBS ?=

BIN_DIR := build/bin
OBJ_DIR := build/obj
TEST_OBJ_DIR := $(OBJ_DIR)/test
TEST_DIR := build/test
SAN_DIR := build/san
TARGET := $(BIN_DIR)/small-shell
SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,$(OBJ_DIR)/%.o,$(SRCS))
TEST_OBJS := $(patsubst src/%.c,$(TEST_OBJ_DIR)/%.o,$(SRCS))
TEST_TARGET := $(TEST_DIR)/small-shell-test
PARSER_API_TARGET := $(TEST_DIR)/parser-api
TIMEOUT_TARGET := $(TEST_DIR)/timeout-runner
ASAN_TARGET := $(SAN_DIR)/small-shell-asan
ASAN_TEST_TARGET := $(SAN_DIR)/small-shell-test-asan
ASAN_PARSER_API_TARGET := $(SAN_DIR)/parser-api-asan
UBSAN_TARGET := $(SAN_DIR)/small-shell-ubsan
UBSAN_TEST_TARGET := $(SAN_DIR)/small-shell-test-ubsan
UBSAN_PARSER_API_TARGET := $(SAN_DIR)/parser-api-ubsan
SANITIZER_CFLAGS := $(CFLAGS) -O1 -g -fno-omit-frame-pointer
SANITIZER_IMAGE ?= gcc:13-bookworm

ifeq ($(USE_READLINE),1)
CPPFLAGS += -DUSE_READLINE
LDLIBS += -lreadline
endif

.PHONY: all readline test test-asan test-ubsan test-sanitizers-container clean fclean re

all: $(TARGET)

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(OBJ_DIR)/%.o: src/%.c | $(OBJ_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(TEST_OBJ_DIR)/%.o: src/%.c | $(TEST_OBJ_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -DSMALL_SHELL_TESTING $(CFLAGS) -c -o $@ $<

$(TEST_TARGET): $(TEST_OBJS) | $(TEST_DIR)
	$(CC) $(LDFLAGS) -o $@ $(TEST_OBJS) $(LDLIBS)

$(PARSER_API_TARGET): tests/parser_api.c $(filter-out src/main.c,$(SRCS)) | $(TEST_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ \
		tests/parser_api.c $(filter-out src/main.c,$(SRCS)) $(LDLIBS)

$(TIMEOUT_TARGET): tests/timeout_runner.c | $(TEST_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $<

$(ASAN_TARGET): $(SRCS) | $(SAN_DIR)
	$(CC) $(CPPFLAGS) $(SANITIZER_CFLAGS) -fsanitize=address \
		$(LDFLAGS) -o $@ $(SRCS) $(LDLIBS)

$(ASAN_TEST_TARGET): $(SRCS) | $(SAN_DIR)
	$(CC) $(CPPFLAGS) -DSMALL_SHELL_TESTING $(SANITIZER_CFLAGS) \
		-fsanitize=address $(LDFLAGS) -o $@ $(SRCS) $(LDLIBS)

$(ASAN_PARSER_API_TARGET): tests/parser_api.c $(filter-out src/main.c,$(SRCS)) | $(SAN_DIR)
	$(CC) $(CPPFLAGS) $(SANITIZER_CFLAGS) -fsanitize=address \
		$(LDFLAGS) -o $@ tests/parser_api.c \
		$(filter-out src/main.c,$(SRCS)) $(LDLIBS)

$(UBSAN_TARGET): $(SRCS) | $(SAN_DIR)
	$(CC) $(CPPFLAGS) $(SANITIZER_CFLAGS) -fsanitize=undefined \
		$(LDFLAGS) -o $@ $(SRCS) $(LDLIBS)

$(UBSAN_TEST_TARGET): $(SRCS) | $(SAN_DIR)
	$(CC) $(CPPFLAGS) -DSMALL_SHELL_TESTING $(SANITIZER_CFLAGS) \
		-fsanitize=undefined $(LDFLAGS) -o $@ $(SRCS) $(LDLIBS)

$(UBSAN_PARSER_API_TARGET): tests/parser_api.c $(filter-out src/main.c,$(SRCS)) | $(SAN_DIR)
	$(CC) $(CPPFLAGS) $(SANITIZER_CFLAGS) -fsanitize=undefined \
		$(LDFLAGS) -o $@ tests/parser_api.c \
		$(filter-out src/main.c,$(SRCS)) $(LDLIBS)

$(BIN_DIR) $(OBJ_DIR) $(TEST_OBJ_DIR) $(TEST_DIR) $(SAN_DIR):
	mkdir -p $@

readline:
	$(MAKE) USE_READLINE=1

test: $(TARGET) $(TEST_TARGET) $(PARSER_API_TARGET) $(TIMEOUT_TARGET)
	./tests/smoke.sh
	./tests/faults.sh
	./tests/allocation.sh
	./tests/lifecycle.sh
	$(PARSER_API_TARGET)
	./tests/performance.sh

test-asan: $(ASAN_TARGET) $(ASAN_TEST_TARGET) $(ASAN_PARSER_API_TARGET) $(TIMEOUT_TARGET)
	ASAN_OPTIONS=halt_on_error=1 SMALL_SHELL_BIN="$(CURDIR)/$(ASAN_TARGET)" ./tests/smoke.sh
	ASAN_OPTIONS=halt_on_error=1 SMALL_SHELL_TEST_BIN="$(CURDIR)/$(ASAN_TEST_TARGET)" ./tests/faults.sh
	ASAN_OPTIONS=halt_on_error=1 SMALL_SHELL_TEST_BIN="$(CURDIR)/$(ASAN_TEST_TARGET)" ./tests/allocation.sh
	ASAN_OPTIONS=halt_on_error=1 SMALL_SHELL_TEST_BIN="$(CURDIR)/$(ASAN_TEST_TARGET)" ./tests/lifecycle.sh
	ASAN_OPTIONS=halt_on_error=1 $(ASAN_PARSER_API_TARGET)
	ASAN_OPTIONS=halt_on_error=1 SMALL_SHELL_BIN="$(CURDIR)/$(ASAN_TARGET)" ./tests/performance.sh

test-ubsan: $(UBSAN_TARGET) $(UBSAN_TEST_TARGET) $(UBSAN_PARSER_API_TARGET) $(TIMEOUT_TARGET)
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 SMALL_SHELL_BIN="$(CURDIR)/$(UBSAN_TARGET)" ./tests/smoke.sh
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 SMALL_SHELL_TEST_BIN="$(CURDIR)/$(UBSAN_TEST_TARGET)" ./tests/faults.sh
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 SMALL_SHELL_TEST_BIN="$(CURDIR)/$(UBSAN_TEST_TARGET)" ./tests/allocation.sh
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 SMALL_SHELL_TEST_BIN="$(CURDIR)/$(UBSAN_TEST_TARGET)" ./tests/lifecycle.sh
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 $(UBSAN_PARSER_API_TARGET)
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 SMALL_SHELL_BIN="$(CURDIR)/$(UBSAN_TARGET)" ./tests/performance.sh

test-sanitizers-container:
	docker run --rm --network none --read-only \
		--tmpfs /tmp:exec,size=256m -v "$(CURDIR):/source:ro" \
		$(SANITIZER_IMAGE) sh /source/tests/container_sanitizers.sh

clean:
	rm -rf build tests/__pycache__ .pytest_cache

fclean: clean

re: fclean all

-include $(OBJS:.o=.d) $(TEST_OBJS:.o=.d)
