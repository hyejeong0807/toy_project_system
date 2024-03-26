obj-m := sensor.o

export BUILDROOT=/home/choi/buildroot
export ARCH=arm64
export CROSS_COMPILE=$(BUILDROOT)/output/host/bin/aarch64-buildroot-linux-gnu-

KERNELDIR ?= $(BUILDROOT)/output/build/linux-custom

all default: modules
install: modules_install

modules modules_install help clean:
	$(MAKE) -C $(KERNELDIR) M=$(shell pwd) $@

.PHONY: nfs
nfs:
	cp sensor.ko ~/nfs
