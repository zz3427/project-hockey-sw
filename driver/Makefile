# ============================================================================
# driver/Makefile
#
# Purpose:
#   Build the Linux kernel module for the FPGA air hockey driver.
#

# This Makefile supports two modes:
#
#   Mode A: kernel build system mode
#     When the Linux kernel recursively invokes this Makefile, it defines
#     KERNELRELEASE. In that case, do NOT run normal targets.
#     Simply tell the kernel which module object to build.
#
#   Mode B: normal user-invoked mode
#     When you or the root Makefile call:
#         make -C driver
#     KERNELRELEASE is not defined yet, so this Makefile invokes the kernel
#     build system
# ============================================================================

# ----------------------------------------------------------------------------
# Mode A: being called from inside the Linux kernel build system
ifneq ($(KERNELRELEASE),)

# Tell Kbuild which module object to build.
#
# air_hockey.o is compiled from air_hockey.c and then linked into air_hockey.ko
#
# Result:
#   air_hockey.ko   <- loadable kernel module
obj-m := air_hockey.o

# ----------------------------------------------------------------------------
# Mode B: normal user-invoked mode
else

# Allow the parent/root Makefile to pass KERNEL_SOURCE in.
# If not passed, fall back to the currently running kernel version.
KERNEL_SOURCE ?= /usr/src/linux-headers-$(shell uname -r)

# Absolute path to this driver directory.
#
# The kernel build system needs this so it knows which external module
# directory to compile.
PWD := $(shell pwd)

# PHONY targets are logical commands, not real files.
.PHONY: default all clean help

# Default target:
# Build the kernel module.
default: all

all:
	$(MAKE) -C $(KERNEL_SOURCE) SUBDIRS=$(PWD) modules

# Clean target:
# Ask the kernel build system to remove generated module-build outputs.
#
# This removes files such as:
#   *.o
#   *.ko
#   *.mod.c
#   Module.symvers
#   modules.order
# and other generated Kbuild artifacts in this directory.
clean:
	$(MAKE) -C $(KERNEL_SOURCE) SUBDIRS=$(PWD) clean

# Optional helper target.
help:
	@echo "driver/Makefile targets:"
	@echo "  make        - build air_hockey.ko"
	@echo "  make clean  - remove generated module files"

endif