ccflags-y:=-std=gnu99 -Wno-declaration-after-statement
ifneq ($(KERNELRELEASE),)
	obj-m:=wpantap.o
else
	KERNELDIR?=/lib/modules/$(shell uname -r)/build
	PWD:=$(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif
