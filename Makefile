ifeq ($(KERNELRELEASE),)

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

.PHONY: build clean

all: build

build:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	rm -rf *.o *~ core .depend modules.order Module.symvers .tmp_versions .*.cmd *.ko *.mod.c

test: build
	# NOTE: this is a unreliable test
	-- rmmod ktext.ko &> /dev/null
	insmod ktext.ko max_elements=0
	./ktexter 4 6 3 3 --die=20
	rmmod ktext.ko

else

$(info Building with KERNELRELEASE = ${KERNELRELEASE})

obj-m := ktext.o
ktext-y := ktext_mod.o ktext_object.o fops_status.o

endif
