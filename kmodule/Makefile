ccflags-y:=-std=gnu99 -Wno-declaration-after-statement
ifneq ($(KERNELRELEASE),)
	obj-m:=wpantap.o
else
	KERNELDIR?=/lib/modules/$(shell uname -r)/build
	PWD:=$(shell pwd)
build default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
clean:
	$(RM) *.o *.cmd *.order *.mod.c *.symvers *.ko *.mod
uninstall:
	$(RM) /lib/modules/$(shell uname -r)/wpantap.ko
install: build
	ln -s $(shell realpath ./wpantap.ko) /lib/modules/$(shell uname -r)
	depmod -a
endif

	
