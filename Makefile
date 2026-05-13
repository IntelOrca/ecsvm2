CC ?= cc
BUILD_DIR := build
OBJ_DIR := obj
TARGET := $(BUILD_DIR)/ecsvm

ECSVM_ENABLE_SDL3 ?= 1

COMMON_SRCS := \
	src/component.c \
	src/ecsbin.c \
	src/ecsvm.c \
	src/main.c \
	src/pong.c \
	src/project.c

COMMON_OBJS := $(COMMON_SRCS:src/%.c=$(OBJ_DIR)/%.o)

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

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CC) $(OBJS) -o $@ $(LDLIBS)

$(OBJ_DIR)/%.o: src/%.c | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR) $(OBJ_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR) $(OBJ_DIR)
