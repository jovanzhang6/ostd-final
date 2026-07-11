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
#define BUFFER_SIZE  4096

/* 每个打开文件描述符的私有数据 */
struct oscddrv_private {
    loff_t offset;
    int writes;
    int reads;
};

/* ioctl 命令定义 */
#define OSCDDRV_IOC_MAGIC 'k'

#define CMD_GET_STATS   _IOR(OSCDDRV_IOC_MAGIC, 1, struct oscddrv_stats)
#define CMD_RESET       _IO(OSCDDRV_IOC_MAGIC, 2)
#define CMD_SET_BUFSIZE _IOW(OSCDDRV_IOC_MAGIC, 3, size_t)
#define CMD_SET_MODE    _IOW(OSCDDRV_IOC_MAGIC, 4, int)

/* 驱动状态结构 */
struct oscddrv_stats {
    size_t bufsize;      /* 当前缓冲区大小 */
    int total_writes;    /* 全局累计写入次数 */
    int total_reads;     /* 全局累计读取次数 */
    int mode;            /* 当前工作模式 */
};

/* 工作模式 */
#define MODE_NORMAL      0
#define MODE_PERFORMANCE 1

#endif