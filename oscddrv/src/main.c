// oscddrv/src/main.c

#include "oscddrv.h"

static dev_t oscddrv_devno;
static struct cdev oscddrv_cdev;
static struct class *oscddrv_class;
static struct device *oscddrv_device;
static char *oscddrv_buffer;
static size_t oscddrv_bufsize;
static int oscddrv_total_writes;
static int oscddrv_total_reads;
static int oscddrv_mode;

static int oscddrv_open(struct inode *inode, struct file *file)
{
    struct oscddrv_private *priv;

    priv = kzalloc(sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    priv->offset = 0;
    priv->writes = 0;
    priv->reads = 0;

    file->private_data = priv;

    pr_info("oscddrv: device opened\n");
    return 0;
}

static int oscddrv_release(struct inode *inode, struct file *file)
{
    struct oscddrv_private *priv = file->private_data;

    if (priv) {
        kfree(priv);
        file->private_data = NULL;
    }

    pr_info("oscddrv: device closed\n");
    return 0;
}

static ssize_t oscddrv_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    struct oscddrv_private *priv = file->private_data;
    size_t to_read;

    if (priv->offset >= oscddrv_bufsize)
        return 0;   /* EOF */

    to_read = min(count, oscddrv_bufsize - priv->offset);
    if (to_read == 0)
        return 0;

    if (copy_to_user(buf, oscddrv_buffer + priv->offset, to_read))
        return -EFAULT;

    priv->offset += to_read;
    priv->reads++;
    oscddrv_total_reads++;

    return to_read;
}

static ssize_t oscddrv_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    struct oscddrv_private *priv = file->private_data;
    size_t to_write;

    if (priv->offset >= oscddrv_bufsize)
        return -ENOSPC;

    to_write = min(count, oscddrv_bufsize - priv->offset);
    if (to_write == 0)
        return 0;

    if (copy_from_user(oscddrv_buffer + priv->offset, buf, to_write))
        return -EFAULT;

    priv->offset += to_write;
    priv->writes++;
    oscddrv_total_writes++;

    return to_write;
}

static long oscddrv_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct oscddrv_stats stats;
    size_t new_size;
    int mode;
    char *new_buf;

    switch (cmd) {
    case CMD_GET_STATS:
        stats.bufsize = oscddrv_bufsize;
        stats.total_writes = oscddrv_total_writes;
        stats.total_reads = oscddrv_total_reads;
        stats.mode = oscddrv_mode;
        if (copy_to_user((struct oscddrv_stats __user *)arg, &stats, sizeof(stats)))
            return -EFAULT;
        return 0;

    case CMD_RESET:
        memset(oscddrv_buffer, 0, oscddrv_bufsize);
        oscddrv_total_writes = 0;
        oscddrv_total_reads = 0;
        return 0;

    case CMD_SET_BUFSIZE:
        if (get_user(new_size, (size_t __user *)arg))
            return -EFAULT;
        if (new_size == 0 || new_size > 65536)
            return -EINVAL;
        if (new_size == oscddrv_bufsize)
            return 0;

        new_buf = krealloc(oscddrv_buffer, new_size, GFP_KERNEL);
        if (!new_buf)
            return -ENOMEM;

        if (new_size > oscddrv_bufsize)
            memset(new_buf + oscddrv_bufsize, 0, new_size - oscddrv_bufsize);

        oscddrv_buffer = new_buf;
        oscddrv_bufsize = new_size;
        return 0;

    case CMD_SET_MODE:
        if (get_user(mode, (int __user *)arg))
            return -EFAULT;
        if (mode != MODE_NORMAL && mode != MODE_PERFORMANCE)
            return -EINVAL;
        oscddrv_mode = mode;
        return 0;

    default:
        return -ENOTTY;
    }
}

static struct file_operations oscddrv_fops = {
    .owner = THIS_MODULE,
    .open = oscddrv_open,
    .release = oscddrv_release,
    .read = oscddrv_read,
    .write = oscddrv_write,
    .unlocked_ioctl = oscddrv_ioctl,
};

static int __init oscddrv_init(void)
{
    int ret;

    /* 1. 初始化全局变量并分配缓冲区 */
    oscddrv_bufsize = BUFFER_SIZE;
    oscddrv_total_writes = 0;
    oscddrv_total_reads = 0;
    oscddrv_mode = MODE_NORMAL;

    oscddrv_buffer = kzalloc(oscddrv_bufsize, GFP_KERNEL);
    if (!oscddrv_buffer) {
        printk(KERN_ERR "oscddrv: failed to allocate global buffer\n");
        return -ENOMEM;
    }

    /* 2. 动态分配字符设备号 */
    ret = alloc_chrdev_region(&oscddrv_devno, 0, 1, DEVICE_NAME);
    if (ret) {
        printk(KERN_ERR "oscddrv: failed to allocate device number\n");
        goto err_buffer;
    }
    printk(KERN_INFO "oscddrv: allocated device number (major=%d, minor=%d)\n",
           MAJOR(oscddrv_devno), MINOR(oscddrv_devno));

    /* 3. 初始化并注册 cdev */
    cdev_init(&oscddrv_cdev, &oscddrv_fops);
    ret = cdev_add(&oscddrv_cdev, oscddrv_devno, 1);
    if (ret) {
        printk(KERN_ERR "oscddrv: failed to add cdev\n");
        goto err_cdev;
    }

    /* 4. 创建类 */
    oscddrv_class = class_create(CLASS_NAME);
    if (IS_ERR(oscddrv_class)) {
        ret = PTR_ERR(oscddrv_class);
        printk(KERN_ERR "oscddrv: failed to create class\n");
        goto err_class;
    }

    /* 5. 创建设备节点 */
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
err_buffer:
    kfree(oscddrv_buffer);
    return ret;
}

static void __exit oscddrv_exit(void)
{
    device_destroy(oscddrv_class, oscddrv_devno);
    class_destroy(oscddrv_class);
    cdev_del(&oscddrv_cdev);
    unregister_chrdev_region(oscddrv_devno, 1);
    kfree(oscddrv_buffer);
    printk(KERN_INFO "oscddrv: module unloaded successfully\n");
}

module_init(oscddrv_init);
module_exit(oscddrv_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(DRIVER_DESC);