#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "mychardev"
#define CLASS_NAME  "mychar"
#define BUFFER_SIZE 1024

MODULE_LICENSE("GPL");
MODULE_AUTHOR("OpenAI");
MODULE_DESCRIPTION("Simple Character Device Driver");
MODULE_VERSION("1.0");

static dev_t dev_num;
static struct cdev my_cdev;
static struct class *my_class;

static char kernel_buffer[BUFFER_SIZE];
static size_t data_size = 0;

/* Open */
static int mychardev_open(struct inode *inode, struct file *file)
{
    pr_info("mychardev: device opened\n");
    return 0;
}

/* Close */
static int mychardev_release(struct inode *inode, struct file *file)
{
    pr_info("mychardev: device closed\n");
    return 0;
}

/* Read */
static ssize_t mychardev_read(struct file *file,
                              char __user *user_buffer,
                              size_t len,
                              loff_t *offset)
{
    size_t bytes_to_read;

    if (*offset >= data_size)
        return 0;

    bytes_to_read = min(len, data_size - (size_t)*offset);

    if (copy_to_user(user_buffer,
                     kernel_buffer + *offset,
                     bytes_to_read))
    {
        return -EFAULT;
    }

    *offset += bytes_to_read;

    pr_info("mychardev: read %zu bytes\n", bytes_to_read);

    return bytes_to_read;
}

/* Write */
static ssize_t mychardev_write(struct file *file,
                               const char __user *user_buffer,
                               size_t len,
                               loff_t *offset)
{
    size_t bytes_to_write;

    bytes_to_write = min(len, (size_t)(BUFFER_SIZE - 1));

    if (copy_from_user(kernel_buffer,
                       user_buffer,
                       bytes_to_write))
    {
        return -EFAULT;
    }

    kernel_buffer[bytes_to_write] = '\0';
    data_size = bytes_to_write;

    pr_info("mychardev: wrote %zu bytes\n", bytes_to_write);

    return bytes_to_write;
}

/* File operations */
static struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = mychardev_open,
    .release = mychardev_release,
    .read    = mychardev_read,
    .write   = mychardev_write,
};

/* Module Init */
static int __init mychardev_init(void)
{
    int ret;

    /* Allocate major/minor numbers */
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("Failed to allocate device number\n");
        return ret;
    }

    pr_info("Allocated Major=%d Minor=%d\n",
            MAJOR(dev_num),
            MINOR(dev_num));

    /* Initialize cdev */
    cdev_init(&my_cdev, &fops);

    ret = cdev_add(&my_cdev, dev_num, 1);
    if (ret < 0) {
        pr_err("Failed to add cdev\n");
        unregister_chrdev_region(dev_num, 1);
        return ret;
    }

    /* Create device class */
    my_class = class_create(CLASS_NAME);
    if (IS_ERR(my_class)) {
        pr_err("Failed to create class\n");
        cdev_del(&my_cdev);
        unregister_chrdev_region(dev_num, 1);
        return PTR_ERR(my_class);
    }

    /* Create device node in /dev */
    if (IS_ERR(device_create(my_class,
                             NULL,
                             dev_num,
                             NULL,
                             DEVICE_NAME)))
    {
        pr_err("Failed to create device\n");
        class_destroy(my_class);
        cdev_del(&my_cdev);
        unregister_chrdev_region(dev_num, 1);
        return -1;
    }

    pr_info("mychardev: driver loaded\n");

    return 0;
}

/* Module Exit */
static void __exit mychardev_exit(void)
{
    device_destroy(my_class, dev_num);
    class_destroy(my_class);

    cdev_del(&my_cdev);

    unregister_chrdev_region(dev_num, 1);

    pr_info("mychardev: driver unloaded\n");
}

module_init(mychardev_init);
module_exit(mychardev_exit);
