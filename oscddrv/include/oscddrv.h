// oscddrv/include/oscddrv.h

#ifndef OSCDDRV_H
#define OSCDDRV_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define DRIVER_DESC "OSCD Virtual Character Device Driver"
#define DEVICE_NAME "oscddrv"
#define CLASS_NAME  "oscddrv_class"

/* 每个打开文件描述符的私有数据 */
struct oscddrv_private {
    char *buffer;
    size_t bufsize;
    loff_t offset;
    int writes;
    int reads;
};

#endif