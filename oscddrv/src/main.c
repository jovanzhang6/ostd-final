// oscddrv/src/main.c

#include "oscddrv.h"

static int __init oscddrv_init(void)
{
    printk(KERN_INFO "oscddrv: module loaded successfully\n");
    return 0;
}

static void __exit oscddrv_exit(void)
{
    printk(KERN_INFO "oscddrv: module unloaded successfully\n");
}

module_init(oscddrv_init);
module_exit(oscddrv_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(DRIVER_DESC);