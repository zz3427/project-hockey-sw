# ============================================================================
# Root Makefile for project-hockey-sw
#
# Repo layout should match:
#   project-hockey-sw/
#     Makefile
#     driver/
#       Makefile
#       air_hockey.c
#       air_hockey.h
#     test/
#       test_positions.c
#     app/
#       ...
#
# This file builds:
#   1. the kernel module (through driver/Makefile)
#   2. the userspace test program(s)
#
# Spliting into two Makefiles allows kernel module builds which uses
#	 linux kernel build system to be separate. makes it cleaner
# ============================================================================

KERNEL_SOURCE := /usr/src/linux-headers-$(shell uname -r)

# Compiler for normal userspace programs.
CC := cc

# Common C flags for userspace code.
#
# -I driver
#   lets userspace test programs include "air_hockey.h" from driver/
#
# -Wall -Wextra
#   enables useful warnings
#
# -g
#   includes debug info 
CFLAGS := -I driver -Wall -Wextra -g

# Future-proof variable for test binaries.
#
# Right now we only have one test program, but keeping a variable makes it easy
# to extend later.
TEST_BINS := tests/test_positions


.PHONY: default all module tests clean help

default: all

all: module tests

# Build the kernel module
#
# We delegate this to driver/Makefile.
module:
	$(MAKE) -C driver KERNEL_SOURCE=$(KERNEL_SOURCE)

# Build all test programs
tests: $(TEST_BINS)

# Userspace test program: test_positions
#
# This is a normal C userspace program, not kernel code.
# It talks to the kernel driver through:
#   - open("/dev/air_hockey")
#   - ioctl(...)
test/test_positions: tests/test_positions.c driver/air_hockey.h
	$(CC) $(CFLAGS) tests/test_positions.c -o test/test_positions

# Clean everything produced by this software repo
#
# This calls the driver clean rule for kernel-module artifacts, and also removes
# normal userspace test executables.
# ----------------------------------------------------------------------------
clean:
	$(MAKE) -C driver KERNEL_SOURCE=$(KERNEL_SOURCE) clean
	rm -f $(TEST_BINS)

# ----------------------------------------------------------------------------
# Optional helper target: print usage
# ----------------------------------------------------------------------------
help:
	@echo "Available targets:"
	@echo "  make / make all   - build kernel module and test programs"
	@echo "  make module       - build only the kernel module"
	@echo "  make tests        - build only userspace tests"
	@echo "  make clean        - remove generated files"