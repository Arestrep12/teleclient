# Makefile for TeleClient

CC := clang
CFLAGS_BASE := -std=c11 -Wall -Wextra -Werror -Iinclude
CFLAGS_DEBUG := $(CFLAGS_BASE) -g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer -DDEBUG
CFLAGS_RELEASE := $(CFLAGS_BASE) -O2 -DNDEBUG

SRC_DIR := src
BIN_DIR := bin
BUILD_DIR := build

TEST_DIR := tests
TEST_SRCS := $(wildcard $(TEST_DIR)/test_*.c)
TEST_NAMES := $(notdir $(TEST_SRCS:.c=))
TEST_BINS := $(addprefix $(BUILD_DIR)/tests/,$(TEST_NAMES))

SRCS_ALL := $(shell find $(SRC_DIR) -name '*.c')
SRCS_CLI := $(SRC_DIR)/client/cli.c
SRCS_LIB := $(filter-out $(SRCS_CLI),$(SRCS_ALL))
OBJS_DEBUG := $(SRCS_ALL:$(SRC_DIR)/%.c=$(BUILD_DIR)/debug/%.o)
OBJS_RELEASE := $(SRCS_ALL:$(SRC_DIR)/%.c=$(BUILD_DIR)/release/%.o)

.PHONY: debug release test clean

all: debug

debug: $(BIN_DIR)/coapc_debug

$(BIN_DIR)/coapc_debug: $(OBJS_DEBUG)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS_DEBUG) -o $@ $^
	@echo "✓ Build debug complete: $@"

$(BUILD_DIR)/debug/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_DEBUG) -c -o $@ $<

release: $(BIN_DIR)/coapc

$(BIN_DIR)/coapc: $(OBJS_RELEASE)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS_RELEASE) -o $@ $^
	@echo "✓ Build release complete: $@"

$(BUILD_DIR)/release/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_RELEASE) -c -o $@ $<

test: $(TEST_BINS)
	@echo "Running TeleClient tests..."
	@for t in $(TEST_BINS); do \
		echo "→ $$t"; \
		$$t || exit 1; \
	done
	@echo "✓ All TeleClient tests passed"

$(BUILD_DIR)/tests/%: $(TEST_DIR)/%.c $(SRCS_LIB)
	@mkdir -p $(BUILD_DIR)/tests
	$(CC) $(CFLAGS_DEBUG) -o $@ $< $(SRCS_LIB)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo "✓ Cleaned"
