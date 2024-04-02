obj-m += engine.o

kernel_version := $(shell uname -r)

export BUILDROOT_DIR=/home/choi/buildroot
export ARCH=arm64
export CROSS_COMPILE=$(BUILDROOT_DIR)/output/host/bin/aarch64-buildroot-linux-gnu-

KERNEL_DIR ?= $(BUILDROOT_DIR)/output/build/linux-custom

all default: modules
install: modules_install

modules modules_install help clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(shell pwd) $@
	$(MAKE) cp

.PHONY: nfs cp
nfs:
	cp engine.ko ~/Shared

cp:
	cp engine.ko ../../