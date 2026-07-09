SRC_DIR := src
obj-m := $(MODULE_NAME).o
$(MODULE_NAME)-objs := $(SRC_DIR)/main.o
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
ccflags-y := -I$(src)/include

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(SRC_DIR)/*.o $(SRC_DIR)/*.mod $(SRC_DIR)/*.mod.c $(SRC_DIR)/*.mod.o

install:
	sudo insmod $(MODULE_NAME).ko

remove:
	sudo rmmod $(MODULE_NAME)

info:
	@echo "查看驱动日志："
	sudo dmesg | tail -20

.PHONY: all clean install remove info