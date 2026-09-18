CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99 -g -I$(INCLUDE_DIR) -Ilib/sphragis

TARGET  = program

# Directories
SRC_DIR     = src
TESTS_DIR   = tests
BUILD_DIR   = build
INCLUDE_DIR = include

# Recursively find all .c files in src/ and its subdirectories
SOURCES = $(shell find $(SRC_DIR) -name '*.c')

# Object files and Dependency files mapped to build/
OBJS    = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SOURCES))
DEPS    = $(OBJS:.o=.d)

# External object files
EXT_OBJS = lib/sphragis/sphragis.o

# Default target
all: $(TARGET)

# Link
$(TARGET): $(OBJS) $(EXT_OBJS)
	$(CC) $(OBJS) $(EXT_OBJS) -o $@ $(LDFLAGS)

# Compile rule with automatic dependency and dynamic directory generation
# -MMD -MP are used to generate .d files, and -include $(DEPS) to track exactly which .c files depend on which headers.
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Create build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $@

# === PHONY TARGETS ===
clean:
	rm -rf $(BUILD_DIR) $(TARGET)

rebuild: clean all

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean rebuild run

# Include compiler-generated dependency files
-include $(DEPS)
