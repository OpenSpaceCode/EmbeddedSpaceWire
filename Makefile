CC := gcc
CXX := g++
CFLAGS := -Wall -Wextra -pedantic -std=c99 -O2 -fPIC
CFLAGS += -I./include -I./external/EmbeddedSpacePacket/include
CXXFLAGS := -Wall -Wextra -std=c++14 -O2
CXXFLAGS += -I./include -I./external/EmbeddedSpacePacket/include

# Source files
CORE_SRCS := src/spacewire_codec.c \
             src/spacewire_frame.c \
             src/spacewire_router.c \
             src/spacewire_packet.c

ESP_SRCS := external/EmbeddedSpacePacket/src/space_packet.c

EXAMPLE_SRCS := examples/main.c
TEST_SRCS := tests/unit_tests.cpp

# Object files
CORE_OBJS := $(CORE_SRCS:.c=.o)
ESP_OBJS := $(ESP_SRCS:.c=.o)
EXAMPLE_OBJS := $(EXAMPLE_SRCS:.c=.o)
TEST_OBJS := $(TEST_SRCS:.cpp=.o)

# Output files
LIB_STATIC := libspacewire.a
LIB_SHARED := libspacewire.so
EXAMPLE_BIN := spacewire_example
TEST_BIN := spacewire_tests

# Build targets
.PHONY: all clean test example lib

all: lib example

lib: $(LIB_STATIC) $(LIB_SHARED)

$(LIB_STATIC): $(CORE_OBJS) $(ESP_OBJS)
	ar rcs $@ $^

$(LIB_SHARED): $(CORE_OBJS) $(ESP_OBJS)
	$(CC) -shared -o $@ $^

example: $(EXAMPLE_BIN)

$(EXAMPLE_BIN): $(EXAMPLE_OBJS) $(LIB_STATIC)
	$(CC) $(CFLAGS) -o $@ $^

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_OBJS) $(LIB_STATIC)
	$(CXX) $(CXXFLAGS) -o $@ $^ -lgtest -lgtest_main -lpthread

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(CORE_OBJS) $(ESP_OBJS) $(EXAMPLE_OBJS) $(TEST_OBJS)
	rm -f $(LIB_STATIC) $(LIB_SHARED) $(EXAMPLE_BIN) $(TEST_BIN)

distclean: clean
	find . -name "*.o" -delete

.PHONY: help
help:
	@echo "EmbeddedSpaceWire Build System"
	@echo "=============================="
	@echo "Targets:"
	@echo "  all      - Build library and examples (default)"
	@echo "  lib      - Build static and shared libraries"
	@echo "  example  - Build example application"
	@echo "  test     - Build and run unit tests"
	@echo "  clean    - Remove build artifacts"
	@echo "  distclean - Clean including object files"
	@echo "  help     - Display this help message"
