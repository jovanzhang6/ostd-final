// oscddrv/include/oscddrv.h

#ifndef OSCDDRV_H
#define OSCDDRV_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>

#define DRIVER_DESC "OSCD Virtual Character Device Driver"
#define DEVICE_NAME "oscddrv"
#define CLASS_NAME  "oscddrv_class"

#endif