CC ?= cc
# Stronger warnings for code quality
CFLAGS ?= -O2 -Iinclude -Wall -Wextra -Wpedantic -Wconversion -Wshadow \
		  -Wcast-align -Wcast-qual -Wpointer-arith -Wformat=2 \
		  -Wmissing-prototypes -Wstrict-prototypes -Wredundant-decls -Wundef \
		  -std=c11
AR ?= ar
CFLAGS += -I./external/EmbeddedSpacePacket/include -fPIC

# Source files
CORE_SRCS := src/spacewire_codec.c \
             src/spacewire_frame.c \
             src/spacewire_router.c \
             src/spacewire_packet.c

ESP_SRCS := external/EmbeddedSpacePacket/src/space_packet.c

EXAMPLE_SRCS := examples/main.c
TEST_SRCS := tests/unit_tests.c

# Object files
CORE_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(CORE_SRCS))
ESP_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(ESP_SRCS))
EXAMPLE_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(EXAMPLE_SRCS))
TEST_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(TEST_SRCS))

# Output files
LIB_STATIC := $(LIB_DIR)/libspacewire.a
LIB_SHARED := $(LIB_DIR)/libspacewire.so
EXAMPLE_BIN := $(BIN_DIR)/spacewire_example
TEST_BIN := $(BIN_DIR)/spacewire_tests

# Build targets
.PHONY: all clean test example lib coverage-html help distclean

all: lib example

lib: $(LIB_STATIC) $(LIB_SHARED)

$(LIB_STATIC): $(CORE_OBJS) $(ESP_OBJS)
	$(AR) rcs $@ $^

$(LIB_SHARED): $(CORE_OBJS) $(ESP_OBJS)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^

example: $(EXAMPLE_BIN)

$(EXAMPLE_BIN): $(EXAMPLE_OBJS) $(LIB_STATIC)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_OBJS) $(LIB_STATIC)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILD_DIR)
	rm -f libspacewire.a libspacewire.so spacewire_example spacewire_tests

coverage-html:
	bash scripts/coverage_html.sh

distclean: clean
	find . -name "*.o" -delete

help:
	@echo "EmbeddedSpaceWire Build System"
	@echo "=============================="
	@echo "Targets:"
	@echo "  all      - Build library and examples (default)"
	@echo "  lib      - Build static and shared libraries"
	@echo "  example  - Build example application"
	@echo "  test     - Build and run unit tests"
	@echo "  coverage-html - Generate HTML coverage report"
	@echo "  output   - Build artifacts are written to ./build"
	@echo "  clean    - Remove build artifacts"
	@echo "  distclean - Clean including object files"
	@echo "  help     - Display this help message"
