# ============================================================================
# Root Makefile for project-hockey-sw
#
# Purpose:
#   Build userspace app/test programs and optionally the kernel module.
#
# Design choice:
#   We do NOT build the kernel module by default anymore.
#   This avoids rebuilding driver/air_hockey.ko when only userspace files
#   changed.
#
# Main targets:
#   make                  -> build common userspace programs only
#   make module           -> build kernel module only
#   make tests            -> build all tests
#   make app/main         -> build main app only
#   make tests/test_mouse -> build one specific test only
#   make clean            -> clean module + userspace outputs
# ============================================================================

KERNEL_SOURCE := /usr/src/linux-headers-$(shell uname -r)

CC := cc
CFLAGS := -I driver -I app -Wall -Wextra -g

# ----------------------------------------------------------------------------
# Userspace programs
# ----------------------------------------------------------------------------

TEST_BINS := tests/test_positions tests/test_mouse tests/test_starting_pos
APP_BINS := app/main

.PHONY: default all module tests apps clean help

# Default target:
# Build userspace only, not the kernel module.
default: all

all: tests apps

# ----------------------------------------------------------------------------
# Kernel module target
# ----------------------------------------------------------------------------

module:
	$(MAKE) -C driver KERNEL_SOURCE=$(KERNEL_SOURCE)

# ----------------------------------------------------------------------------
# Build all userspace tests
# ----------------------------------------------------------------------------

tests: $(TEST_BINS)

# ----------------------------------------------------------------------------
# Build all userspace apps
# ----------------------------------------------------------------------------

apps: $(APP_BINS)

# ----------------------------------------------------------------------------
# Individual userspace build rules
#
# Any userspace file that uses game_io_* must link with app/game_io.c.
# ----------------------------------------------------------------------------

tests/test_positions: tests/test_positions.c app/game_io.c app/game_io.h driver/air_hockey.h
	$(CC) $(CFLAGS) tests/test_positions.c app/game_io.c -o tests/test_positions

tests/test_mouse: tests/test_mouse.c app/game_io.c app/game_io.h driver/air_hockey.h
	$(CC) $(CFLAGS) tests/test_mouse.c app/game_io.c -o tests/test_mouse

tests/test_starting_pos: tests/test_starting_pos.c app/game_io.c app/game_io.h driver/air_hockey.h
	$(CC) $(CFLAGS) tests/test_starting_pos.c app/game_io.c -o tests/test_starting_pos

app/main: app/main.c app/game_io.c app/physics_engine.c app/physics_engine.h app/game_io.h driver/air_hockey.h
	$(CC) $(CFLAGS) app/main.c app/game_io.c app/physics_engine.c -o app/main -lm

# ----------------------------------------------------------------------------
# Clean everything
# ----------------------------------------------------------------------------

clean:
	$(MAKE) -C driver KERNEL_SOURCE=$(KERNEL_SOURCE) clean
	rm -f $(TEST_BINS) $(APP_BINS)

help:
	@echo "Targets:"
	@echo "  make                  - build userspace programs only"
	@echo "  make module           - build kernel module only"
	@echo "  make tests            - build all tests"
	@echo "  make apps             - build all apps"
	@echo "  make app/main         - build main app only"
	@echo "  make tests/test_mouse - build only test_mouse"
	@echo "  make tests/test_starting_pos - build only test_starting_pos"
	@echo "  make help             - show this help message"
	@echo "  make clean            - remove generated files"