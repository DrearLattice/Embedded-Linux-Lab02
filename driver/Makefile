KDIR := $(HOME)/embedded_lab1/kernel/linux-5.15

ARCH := arm
CROSS_COMPILE := arm-linux-gnueabihf-
CC := arm-linux-gnueabihf-gcc-11

obj-m += lab2_driver.o

ccflags-y += -march=armv7-a

all:
	make -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) CC=$(CC) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
