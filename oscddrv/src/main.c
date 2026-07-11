// oscddrv/src/main.c

#include "oscddrv.h"

static dev_t oscddrv_devno;
static struct cdev oscddrv_cdev;
static struct class *oscddrv_class;
static struct device *oscddrv_device;

static struct file_operations oscddrv_fops = {
    .owner = THIS_MODULE,
};

static int __init oscddrv_init(void)
{
    int ret;

    /* 1. 动态分配字符设备号 */
    ret = alloc_chrdev_region(&oscddrv_devno, 0, 1, DEVICE_NAME);
    if (ret) {
        printk(KERN_ERR "oscddrv: failed to allocate device number\n");
        return ret;
    }
    printk(KERN_INFO "oscddrv: allocated device number (major=%d, minor=%d)\n",
           MAJOR(oscddrv_devno), MINOR(oscddrv_devno));

    /* 2. 初始化并注册 cdev */
    cdev_init(&oscddrv_cdev, &oscddrv_fops);
    ret = cdev_add(&oscddrv_cdev, oscddrv_devno, 1);
    if (ret) {
        printk(KERN_ERR "oscddrv: failed to add cdev\n");
        goto err_cdev;
    }

    /* 3. 创建类（用于自动生成设备节点） */
    oscddrv_class = class_create(CLASS_NAME);
    if (IS_ERR(oscddrv_class)) {
        ret = PTR_ERR(oscddrv_class);
        printk(KERN_ERR "oscddrv: failed to create class\n");
        goto err_class;
    }

    /* 4. 创建设备节点 /dev/oscddrv */
    oscddrv_device = device_create(oscddrv_class, NULL, oscddrv_devno, NULL, DEVICE_NAME);
    if (IS_ERR(oscddrv_device)) {
        ret = PTR_ERR(oscddrv_device);
        printk(KERN_ERR "oscddrv: failed to create device\n");
        goto err_device;
    }

    printk(KERN_INFO "oscddrv: module loaded successfully\n");
    return 0;

err_device:
    class_destroy(oscddrv_class);
err_class:
    cdev_del(&oscddrv_cdev);
err_cdev:
    unregister_chrdev_region(oscddrv_devno, 1);
    return ret;
}

static void __exit oscddrv_exit(void)
{
    device_destroy(oscddrv_class, oscddrv_devno);
    class_destroy(oscddrv_class);
    cdev_del(&oscddrv_cdev);
    unregister_chrdev_region(oscddrv_devno, 1);
    printk(KERN_INFO "oscddrv: module unloaded successfully\n");
}

module_init(oscddrv_init);
module_exit(oscddrv_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(DRIVER_DESC);