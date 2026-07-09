#ifndef OSCDDRV_H
#define OSCDDRV_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#define DRIVER_DESC "OSCD 虚拟字符设备驱动"

static int __init oscddrv_init(void);
static void __exit oscddrv_exit(void);

#endif