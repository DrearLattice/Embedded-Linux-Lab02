#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#define DEVICE_NAME "lab2"
#define CLASS_NAME  "lab2_class"
#define BUFFER_SIZE 1024
#define DRIVER_MAJOR 240

static int major_number;
static char *device_buffer;
static size_t buffer_len;
static int open_count;

static struct class *lab2_class;
static struct device *lab2_device;

static DEFINE_MUTEX(lab2_mutex);


/* =========================================================
 * PROCFS IMPLEMENTATION
 * ========================================================= */

static struct proc_dir_entry *proc_entry;

static int lab2_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "LAB-02 Driver Statistics\n");
    seq_printf(m, "======================\n");
    seq_printf(m, "Driver name : %s\n", DEVICE_NAME);
    seq_printf(m, "Major number : %d\n", major_number);
    seq_printf(m, "Buffer size : %d bytes\n", BUFFER_SIZE);
    seq_printf(m, "Data length : %zu bytes\n", buffer_len);
    seq_printf(m, "Open count : %d\n", open_count);

    if (buffer_len > 0)
        seq_printf(m, "Last data : [%s]\n", device_buffer);

    return 0;
}

static int lab2_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, lab2_proc_show, NULL);
}

static const struct proc_ops lab2_proc_fops = {
    .proc_open    = lab2_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};


/* =========================================================
 * SYSFS IMPLEMENTATION
 * ========================================================= */

static ssize_t buffer_len_show(struct device *dev,
                               struct device_attribute *attr,
                               char *buf)
{
    return sprintf(buf, "%zu\n", buffer_len);
}

static ssize_t open_count_show(struct device *dev,
                               struct device_attribute *attr,
                               char *buf)
{
    return sprintf(buf, "%d\n", open_count);
}

static ssize_t last_data_show(struct device *dev,
                              struct device_attribute *attr,
                              char *buf)
{
    if (buffer_len > 0)
        return sprintf(buf, "%s\n", device_buffer);

    return sprintf(buf, "(empty)\n");
}

static DEVICE_ATTR_RO(buffer_len);
static DEVICE_ATTR_RO(open_count);
static DEVICE_ATTR_RO(last_data);

static struct attribute *lab2_attrs[] = {
    &dev_attr_buffer_len.attr,
    &dev_attr_open_count.attr,
    &dev_attr_last_data.attr,
    NULL,
};

static struct attribute_group lab2_attr_group = {
    .attrs = lab2_attrs,
};


/* =========================================================
 * CHARACTER DEVICE FUNCTIONS
 * ========================================================= */

static int lab2_open(struct inode *inode, struct file *file)
{
    mutex_lock(&lab2_mutex);

    open_count++;

    printk(KERN_INFO
           "lab2: device opened (count=%d)\n",
           open_count);

    mutex_unlock(&lab2_mutex);

    return 0;
}


static int lab2_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "lab2: device released\n");

    return 0;
}


static ssize_t lab2_read(struct file *file,
                         char __user *user_buffer,
                         size_t len,
                         loff_t *offset)
{
    size_t bytes_to_read;

    if (*offset >= buffer_len)
        return 0;

    bytes_to_read =
        min(len, buffer_len - (size_t)*offset);

    if (copy_to_user(user_buffer,
                     device_buffer + *offset,
                     bytes_to_read)) {
        return -EFAULT;
    }

    *offset += bytes_to_read;

    printk(KERN_INFO
           "lab2: read %zu bytes\n",
           bytes_to_read);

    return bytes_to_read;
}


static ssize_t lab2_write(struct file *file,
                          const char __user *user_buffer,
                          size_t len,
                          loff_t *offset)
{
    size_t bytes_to_write;

    bytes_to_write =
        min(len, (size_t)(BUFFER_SIZE - 1));

    mutex_lock(&lab2_mutex);

    if (copy_from_user(device_buffer,
                       user_buffer,
                       bytes_to_write)) {
        mutex_unlock(&lab2_mutex);
        return -EFAULT;
    }

    buffer_len = bytes_to_write;

    device_buffer[buffer_len] = '\0';

    mutex_unlock(&lab2_mutex);

    printk(KERN_INFO
           "lab2: wrote %zu bytes\n",
           bytes_to_write);

    return bytes_to_write;
}


/* =========================================================
 * FILE OPERATIONS
 * ========================================================= */

static const struct file_operations lab2_fops = {
    .owner   = THIS_MODULE,
    .open    = lab2_open,
    .release = lab2_release,
    .read    = lab2_read,
    .write   = lab2_write,
};


/* =========================================================
 * MODULE INIT
 * ========================================================= */

static int __init lab2_init(void)
{
    int ret;

    device_buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);

    if (!device_buffer) {
        printk(KERN_ERR
               "lab2: failed to allocate buffer\n");
        return -ENOMEM;
    }

    buffer_len = 0;
    open_count = 0;


    /* Register character device */

    ret = register_chrdev(DRIVER_MAJOR,
                          DEVICE_NAME,
                          &lab2_fops);

    if (ret < 0) {
        printk(KERN_ERR
               "lab2: failed to register character device\n");

        kfree(device_buffer);

        return ret;
    }

    major_number = DRIVER_MAJOR;


    /* Create device class */

    lab2_class =
        class_create(THIS_MODULE, CLASS_NAME);

    if (IS_ERR(lab2_class)) {

        unregister_chrdev(DRIVER_MAJOR,
                          DEVICE_NAME);

        kfree(device_buffer);

        return PTR_ERR(lab2_class);
    }


    /* Create device */

    lab2_device =
        device_create(lab2_class,
                      NULL,
                      MKDEV(DRIVER_MAJOR, 0),
                      NULL,
                      DEVICE_NAME);

    if (IS_ERR(lab2_device)) {

        class_destroy(lab2_class);

        unregister_chrdev(DRIVER_MAJOR,
                          DEVICE_NAME);

        kfree(device_buffer);

        return PTR_ERR(lab2_device);
    }


    /* =====================================================
     * Create sysfs attributes
     * ===================================================== */

    ret = sysfs_create_group(&lab2_device->kobj,
                             &lab2_attr_group);

    if (ret) {
        printk(KERN_ERR
               "lab2: failed to create sysfs group\n");

        device_destroy(lab2_class,
                       MKDEV(DRIVER_MAJOR, 0));

        class_destroy(lab2_class);

        unregister_chrdev(DRIVER_MAJOR,
                          DEVICE_NAME);

        kfree(device_buffer);

        return ret;
    }


    /* =====================================================
     * Create /proc/lab2_info
     * ===================================================== */

    proc_entry =
        proc_create("lab2_info",
                    0444,
                    NULL,
                    &lab2_proc_fops);

    if (!proc_entry) {

        printk(KERN_WARNING
               "lab2: failed to create /proc/lab2_info\n");

        sysfs_remove_group(&lab2_device->kobj,
                           &lab2_attr_group);

        device_destroy(lab2_class,
                       MKDEV(DRIVER_MAJOR, 0));

        class_destroy(lab2_class);

        unregister_chrdev(DRIVER_MAJOR,
                          DEVICE_NAME);

        kfree(device_buffer);

        return -ENOMEM;
    }


    printk(KERN_INFO
           "lab2: /proc/lab2_info created\n");

    printk(KERN_INFO
           "lab2: driver loaded, major=%d\n",
           major_number);

    return 0;
}


/* =========================================================
 * MODULE EXIT
 * ========================================================= */

static void __exit lab2_exit(void)
{
    /* Remove procfs */

    if (proc_entry)
        proc_remove(proc_entry);


    /* Remove sysfs */

    sysfs_remove_group(&lab2_device->kobj,
                       &lab2_attr_group);


    /* Destroy device */

    device_destroy(lab2_class,
                   MKDEV(DRIVER_MAJOR, 0));

    class_destroy(lab2_class);


    /* Unregister character device */

    unregister_chrdev(DRIVER_MAJOR,
                      DEVICE_NAME);


    /* Free memory */

    kfree(device_buffer);

    printk(KERN_INFO
           "lab2: driver unloaded\n");
}


module_init(lab2_init);
module_exit(lab2_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("LAB02 Student");
MODULE_DESCRIPTION("LAB-02 Character Device Driver");
MODULE_VERSION("1.0");
