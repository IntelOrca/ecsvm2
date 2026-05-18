CC ?= cc
BUILD_DIR := build
OBJ_DIR := obj
TARGET := $(BUILD_DIR)/ecsvm
CORE_PROJECT := lib/core
CORE_ECSBIN := $(CORE_PROJECT)/out/core.ecsbin

ECSVM_ENABLE_SDL3 ?= 1

COMMON_SRCS := \
	src/bin.c \
	src/bin_decompile.c \
	src/bin_inspect.c \
	src/component.c \
	src/diagnostic.c \
	src/ecs_tree.c \
	src/ecs_generator.c \
	src/ecs_lexer.c \
	src/ecs_parser.c \
	src/ecsvm.c \
	src/lib_core.c \
	src/logger.c \
	src/managed_runtime.c \
	src/main.c \
	src/pipeline.c \
	src/pong.c \
	src/project.c \
	src/project_common.c \
	src/stream.c \
	src/system_time.c \
	src/xml.c

COMMON_OBJS := $(COMMON_SRCS:src/%.c=$(OBJ_DIR)/%.o)
EXAMPLE_PROJECTS := $(wildcard examples/*/project.toml)
EXAMPLE_DIRS := $(patsubst %/project.toml,%,$(EXAMPLE_PROJECTS))
EXAMPLE_TARGETS := $(foreach dir,$(EXAMPLE_DIRS),$(dir)/out/$(notdir $(dir)).ecsbin)
EXAMPLE_OUT_DIRS := $(addsuffix /out,$(EXAMPLE_DIRS))

CPPFLAGS := -DECSVM_ENABLE_SDL3=$(ECSVM_ENABLE_SDL3) -Iinclude
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -pedantic
LDLIBS := -lm

ifeq ($(MAKECMDGOALS),clean)
SDL3_SRCS :=
SDL3_CFLAGS :=
SDL3_LIBS :=
else ifeq ($(ECSVM_ENABLE_SDL3),0)
SDL3_SRCS :=
SDL3_CFLAGS :=
SDL3_LIBS :=
else ifeq ($(ECSVM_ENABLE_SDL3),1)
ifneq ($(strip $(SDL3_PREFIX)),)
export PKG_CONFIG_PATH := $(SDL3_PREFIX)/lib64/pkgconfig:$(SDL3_PREFIX)/lib/pkgconfig:$(PKG_CONFIG_PATH)
endif

SDL3_PACKAGE := $(shell pkg-config --exists sdl3 && printf '%s' sdl3 || pkg-config --exists SDL3 && printf '%s' SDL3)
ifeq ($(SDL3_PACKAGE),)
$(error SDL3 development files not found. Install SDL3 or set SDL3_PREFIX, or build with ECSVM_ENABLE_SDL3=0)
endif

SDL3_SRCS := \
	src/system_renderer.c \
	src/system_window.c
SDL3_CFLAGS := $(shell pkg-config --cflags $(SDL3_PACKAGE))
SDL3_LIBS := $(shell pkg-config --libs $(SDL3_PACKAGE))
else
$(error ECSVM_ENABLE_SDL3 must be set to 0 or 1)
endif

SDL3_OBJS := $(SDL3_SRCS:src/%.c=$(OBJ_DIR)/%.o)
OBJS := $(COMMON_OBJS) $(SDL3_OBJS)

CPPFLAGS += $(SDL3_CFLAGS)
LDLIBS += $(SDL3_LIBS)

.PHONY: all clean examples

all: $(TARGET) $(CORE_ECSBIN)

examples: $(TARGET) $(CORE_ECSBIN) $(EXAMPLE_TARGETS)

$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CC) $(OBJS) -o $@ $(LDLIBS)

$(OBJ_DIR)/%.o: src/%.c | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR) $(OBJ_DIR):
	mkdir -p $@

$(CORE_ECSBIN): $(TARGET) $(wildcard $(CORE_PROJECT)/src/*.ecs) $(CORE_PROJECT)/project.toml
	./$(TARGET) build $(CORE_PROJECT) > /dev/null

clean:
	rm -rf $(BUILD_DIR) $(OBJ_DIR) $(CORE_PROJECT)/out $(EXAMPLE_OUT_DIRS)

define example_rule
$(1)/out/$(notdir $(1)).ecsbin: $(TARGET) $(CORE_ECSBIN) $(1)/project.toml $$(shell find $(1)/src -type f -name '*.ecs' 2>/dev/null)
	./$(TARGET) build --core-lib $(CORE_ECSBIN) $(1)
endef

$(foreach dir,$(EXAMPLE_DIRS),$(eval $(call example_rule,$(dir))))
