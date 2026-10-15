CC ?= cc
# Stronger warnings for code quality; any warning fails the build
CFLAGS ?= -O2 -Iinclude -Werror -Wall -Wextra -Wpedantic -Wconversion -Wshadow \
		  -Wcast-align -Wcast-qual -Wpointer-arith -Wformat=2 \
		  -Wmissing-prototypes -Wstrict-prototypes -Wredundant-decls -Wundef \
		  -std=c99
AR ?= ar
CFLAGS += -fPIC

BUILD_DIR ?= build
OBJ_DIR := $(BUILD_DIR)/obj
LIB_DIR := $(BUILD_DIR)/lib
BIN_DIR := $(BUILD_DIR)/bin

# Source files
CORE_SRCS := src/spacewire_spw_packet.c \
             src/spacewire_router.c \
             src/spacewire_packet.c
# Weak default payload hooks, kept apart because an application may replace them.
WEAK_SRCS := src/weak/spacewire_payload.c

EXAMPLE_SRCS := examples/main.c
TEST_SRCS := tests/unit_tests.c \
             tests/test_spw_packet.c \
             tests/test_router.c \
             tests/test_packet.c \
             tests/test_payload.c
# Linked as an object file, so its strong sw_payload_*() definitions replace the
# library's weak defaults (an override inside a static archive would be ignored).
OVERRIDE_TEST_SRCS := tests/test_payload_override.c

# Object files
CORE_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(CORE_SRCS))
WEAK_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(WEAK_SRCS))
EXAMPLE_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(EXAMPLE_SRCS))
TEST_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(TEST_SRCS))
OVERRIDE_TEST_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(OVERRIDE_TEST_SRCS))

# Output files
LIB_STATIC := $(LIB_DIR)/libspacewire.a
LIB_SHARED := $(LIB_DIR)/libspacewire.so
EXAMPLE_BIN := $(BIN_DIR)/spacewire_example
TEST_BIN := $(BIN_DIR)/spacewire_tests
OVERRIDE_TEST_BIN := $(BIN_DIR)/spacewire_override_tests

# Build targets
.PHONY: all clean test example lib coverage-html help distclean

all: lib test

lib: $(LIB_STATIC) $(LIB_SHARED)

$(LIB_STATIC): $(CORE_OBJS) $(WEAK_OBJS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(LIB_SHARED): $(CORE_OBJS) $(WEAK_OBJS)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^

example: $(EXAMPLE_BIN)
	./$(EXAMPLE_BIN)

$(EXAMPLE_BIN): $(EXAMPLE_OBJS) $(LIB_STATIC)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^

test: $(TEST_BIN) $(OVERRIDE_TEST_BIN)
	./$(TEST_BIN)
	./$(OVERRIDE_TEST_BIN)

$(TEST_BIN): $(TEST_OBJS) $(LIB_STATIC)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^

$(OVERRIDE_TEST_BIN): $(OVERRIDE_TEST_OBJS) $(LIB_STATIC)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILD_DIR)
	rm -f libspacewire.a libspacewire.so spacewire_example spacewire_tests

coverage-html:
	bash tools/coverage_html.sh

distclean: clean
	find . -name "*.o" -delete

help:
	@echo "EmbeddedSpaceWire Build System"
	@echo "=============================="
	@echo "Targets:"
	@echo "  all      - Build library and examples (default)"
	@echo "  lib      - Build static and shared libraries"
	@echo "  example  - Build example application"
	@echo "  test     - Build and run unit tests (default and overridden payload hooks)"
	@echo "  coverage-html - Generate HTML coverage report"
	@echo "  output   - Build artifacts are written to ./build"
	@echo "  clean    - Remove build artifacts"
	@echo "  distclean - Clean including object files"
	@echo "  help     - Display this help message"
