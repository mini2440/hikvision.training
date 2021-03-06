#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/uaccess.h>        /* copy_to_user ... */
#include <linux/ioctl.h>        /* ioctl ... */
#include <linux/slab.h>         /* kmalloc ... */
#include <linux/proc_fs.h>      /* procfs ... */

/* simple log */
#define log(fmt, arg...)\
        printk(KERN_INFO "[Paul][%s][%d] "fmt"\n", __func__, __LINE__, ##arg);

#define MEM_SIZE (4*1024*1024)

struct share {
        unsigned char mem[MEM_SIZE];  // share mem
};

struct share *sh;

static int second_drv_open(struct inode *indoe, struct file *filp)
{
        log("second_drv open ok");
        return 0;
}

static int second_drv_release(struct inode *indoe, struct file *filp)
{
        log("second_drv release ok");
        return 0;
}

static struct file_operations second_drv_fops =
{
        .open    = second_drv_open,
        .release = second_drv_release,
};

static struct proc_dir_entry* proc_dir = NULL;
static struct proc_dir_entry* proc_node = NULL;

static loff_t second_drv_proc_llseek(struct file *filp, loff_t offset, int whence)
{
        loff_t new_pos = 0;
        switch(whence)
        {
                case 0: /* SEEK_SET */
                        new_pos = offset;
                        break;
 
                case 1: /* SEEK_CUR */
                        new_pos = filp->f_pos + offset;
                        break;
 
                case 2: /* SEEK_END */
                        new_pos = MEM_SIZE -1 + offset;
                        break;
 
                default: /* can't happen */
                        return -EINVAL;
        }

        if ((new_pos < 0) || (new_pos > MEM_SIZE))
                return -EINVAL; 
        filp->f_pos = new_pos;
        return new_pos;
}

static ssize_t second_drv_proc_read(struct file *filp, char __user *buff, size_t cnt, loff_t *loff)
{
        unsigned long p = *loff;
        unsigned int count = cnt;
        int ret = 0;
        if(p >= MEM_SIZE)
                return 0;
        if(count > MEM_SIZE - p)
                count = MEM_SIZE - p;

        if(copy_to_user(buff, sh->mem + p, count))
                ret = -EFAULT;
        else
        {
                *loff += count;
                ret = count;
                log("read size = %d", count);
        }

        log("second_drv proc_read ok");
        return ret;
}

static ssize_t second_drv_proc_write(struct file *filp, const char __user *buff, size_t cnt, loff_t *loff)
{
        unsigned long p = *loff;
        unsigned int count = cnt;
        int ret = 0;

        if(p >= MEM_SIZE)
                return 0;
        if(count > MEM_SIZE - p)
                count = MEM_SIZE - p;
        
        if(copy_from_user(sh->mem + p, buff, count))
                ret = -EFAULT;
        else
        {
                *loff += count;
                ret = count;
                log("write size = %d", count);
        }

        log("second_drv proc_write ok");
        return ret;
}

static const struct file_operations second_drv_proc_fops = 
{
        .owner  = THIS_MODULE,
        .llseek = second_drv_proc_llseek,
        .read   = second_drv_proc_read,
        .write  = second_drv_proc_write,
};

struct cdev *second_drv;
static dev_t second_drv_devno;
static struct class *second_drv_class;

static int __init mod_init(void)
{
        alloc_chrdev_region(&second_drv_devno, 0, 1, "second_drv_name");
        second_drv = cdev_alloc();
        cdev_init(second_drv, &second_drv_fops); 
        second_drv->owner = THIS_MODULE;
        cdev_add(second_drv, second_drv_devno, 1);

        second_drv_class = class_create(THIS_MODULE, "second_drv_class");
        device_create(second_drv_class, NULL, second_drv_devno, "second_drv_device");

        /* 申请 4M 内存，申请的内存比较大，比较容易出错，务必要检查函数返回值 */
        sh = (unsigned char *)kzalloc(MEM_SIZE, GFP_KERNEL);
        if (sh == NULL) {
                kfree(sh);
                return -ENOBUFS;
        }

        /* 在 /proc 目录下创建 share 子目录 */
        proc_dir = proc_mkdir("share", NULL);
        if(!proc_dir)
        {
                remove_proc_entry("share", NULL);
                return -ENOMEM;
        }

        /* 在 /proc/share 目录下创建 mem 节点 */
        proc_node = create_proc_entry("mem", 0666, proc_dir);
        proc_node->proc_fops = &second_drv_proc_fops;
        if(!proc_node)
        {
                remove_proc_entry("mem", proc_dir);
                return -ENOMEM;
        }

        log("mod_init ok");
        return 0;
}

static void __exit mod_exit(void)
{
        cdev_del(second_drv);
        unregister_chrdev_region(second_drv_devno, 1);
        device_destroy(second_drv_class, second_drv_devno);
        class_destroy(second_drv_class);

        /* 移除之前创建的 proc 目录和节点 */
        remove_proc_entry("mem", proc_dir);
        remove_proc_entry("share", NULL);

        log("mod_exit ok");
}

module_init(mod_init);
module_exit(mod_exit);

MODULE_LICENSE("GPL");

