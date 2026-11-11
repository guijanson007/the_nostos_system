CC       = gcc
CFLAGS   = -Wall -Wextra -std=c99 -g -I$(INCLUDE_DIR) -Ilib/sphragis
LDLIBS   = -lpthread

# Executables
TARGETS  = odysseus ithaca island

# Directories
SRC_DIR     = src
BUILD_DIR   = build
INCLUDE_DIR = include

# Common shared files (e.g. utils)
COMMON_SRCS = $(wildcard $(SRC_DIR)/utils/*.c)
COMMON_OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(COMMON_SRCS))

# Process-specific source & object definitions
ODYSSEUS_SRCS = $(wildcard $(SRC_DIR)/odysseus/*.c)
ODYSSEUS_OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(ODYSSEUS_SRCS))

ITHACA_SRCS   = $(wildcard $(SRC_DIR)/ithaca/*.c)
ITHACA_OBJS   = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(ITHACA_SRCS))

ISLAND_SRCS   = $(wildcard $(SRC_DIR)/island/*.c)
ISLAND_OBJS   = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(ISLAND_SRCS))

# All object files and generated dependency files
ALL_OBJS = $(COMMON_OBJS) $(ODYSSEUS_OBJS) $(ITHACA_OBJS) $(ISLAND_OBJS)
DEPS     = $(ALL_OBJS:.o=.d)

# External object files (wildcard so it only links if the file actually exists)
EXT_OBJS = $(wildcard lib/sphragis/sphragis.o)

# Default target
all: build

build: $(TARGETS)

# Link targets
odysseus: $(COMMON_OBJS) $(ODYSSEUS_OBJS) $(EXT_OBJS)
	$(CC) $^ -o $@ $(LDLIBS)

ithaca: $(COMMON_OBJS) $(ITHACA_OBJS) $(EXT_OBJS)
	$(CC) $^ -o $@ $(LDLIBS)

island: $(COMMON_OBJS) $(ISLAND_OBJS) $(EXT_OBJS)
	$(CC) $^ -o $@ $(LDLIBS)

# Generic compile rule with automatic dependency generation
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# === PHONY TARGETS ===
clean:
	rm -rf $(BUILD_DIR) $(TARGETS) program

rebuild: clean build

.PHONY: all build clean rebuild

# Include compiler-generated dependency files
-include $(DEPS)