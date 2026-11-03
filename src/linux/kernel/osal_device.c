#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/poll.h>
#include "osal.h"
#include "osal_inner.h"

/* Internal context: translates osal_dev + osal_fileops into Linux miscdevice + file_operations */
struct osal_dev_linux_ctx {
    struct miscdevice miscdev;
    struct file_operations fops;
    osal_dev *osal_dev;
    osal_atomic open_count;
};

/* ======================== file_operations shim functions ======================== */

static int osal_linux_dev_open(struct inode *inode, struct file *filp)
{
    struct osal_dev_linux_ctx *ctx;
    osal_dev *odev;
    int i;

    /* Retrieve context: miscdevice is embedded in ctx, filp->private_data points to miscdev
     * after misc_register sets it. Use container_of to get back to ctx. */
    ctx = container_of(filp->private_data, struct osal_dev_linux_ctx, miscdev);
    odev = ctx->osal_dev;

    i = osal_atomic_inc_return(&ctx->open_count);
	if (i > 1) {
		osal_log("%s: open %d times\n", odev->name, i);
		return 0;
	}

    if (odev->fops && odev->fops->open)
        return odev->fops->open(odev->private_data);

    //filp->private_data = odev;
    return 0;
}

static int osal_linux_dev_release(struct inode *inode, struct file *filp)
{
    struct osal_dev_linux_ctx *ctx;
    osal_dev *odev;
    int i;

    ctx = container_of(filp->private_data, struct osal_dev_linux_ctx, miscdev);
    odev = ctx->osal_dev;

	i = osal_atomic_dec_return(&ctx->open_count);
	if (i) {
		osal_log("%s: open %d times\n", odev->name, i);
		return 0;
	}

    if (odev->fops && odev->fops->release)
        return odev->fops->release(odev->private_data);

    return 0;
}

static ssize_t osal_linux_dev_read(struct file *filp, char __user *buf, size_t size, loff_t *offset)
{
    struct osal_dev_linux_ctx *ctx;
    osal_dev *odev;

    ctx = container_of(filp->private_data, struct osal_dev_linux_ctx, miscdev);
    odev = ctx->osal_dev;

    if (odev->fops && odev->fops->read)
        return odev->fops->read(buf, size, (long *)offset, odev->private_data);

    return -EINVAL;
}

static ssize_t osal_linux_dev_write(struct file *filp, const char __user *buf, size_t size, loff_t *offset)
{
    struct osal_dev_linux_ctx *ctx;
    osal_dev *odev;

    ctx = container_of(filp->private_data, struct osal_dev_linux_ctx, miscdev);
    odev = ctx->osal_dev;

    if (odev->fops && odev->fops->write)
        return odev->fops->write(buf, size, (long *)offset, odev->private_data);

    return -EINVAL;
}

static long osal_linux_dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct osal_dev_linux_ctx *ctx;
    osal_dev *odev;
    unsigned int i;

    ctx = container_of(filp->private_data, struct osal_dev_linux_ctx, miscdev);
    odev = ctx->osal_dev;

    if (odev->fops && odev->fops->cmd_list) {
        for (i = 0; i < odev->fops->cmd_cnt; i++) {
            if (odev->fops->cmd_list[i].cmd == cmd) {
                return odev->fops->cmd_list[i].handler(cmd, (void *)arg, odev->private_data);
            }
        }
    }

    return -EINVAL;
}

static long osal_linux_dev_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    return osal_linux_dev_ioctl(filp, cmd, arg);
}

#ifdef CONFIG_COMPAT
static long osal_linux_dev_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    return osal_linux_dev_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static unsigned int osal_linux_dev_poll(struct file *filp, poll_table *wait)
{
    struct osal_dev_linux_ctx *ctx;
    osal_dev *odev;
    osal_poll opoll;

    ctx = container_of(filp->private_data, struct osal_dev_linux_ctx, miscdev);
    odev = ctx->osal_dev;

    if (odev->fops && odev->fops->poll) {
        opoll.poll_table = wait;
        opoll.file = filp;
        opoll.data = odev;
        return odev->fops->poll(&opoll, odev->private_data);
    }

    return POLLIN | POLLRDNORM;
}

void osal_poll_wait(osal_poll *table, osal_wait *wait)
{
    if (!table || !table->file || !wait || !wait->wait)
        return;

    poll_wait((struct file *)table->file,
              (wait_queue_head_t *)wait->wait,
              (struct poll_table_struct *)table->poll_table);
}
EXPORT_SYMBOL(osal_poll_wait);

static int osal_linux_dev_mmap(struct file *filp, struct vm_area_struct *vma)
{
    struct osal_dev_linux_ctx *ctx;
    osal_dev *odev;
    osal_vm ovm;

    ctx = container_of(filp->private_data, struct osal_dev_linux_ctx, miscdev);
    odev = ctx->osal_dev;

    if (odev->fops && odev->fops->mmap) {
        ovm.vm = vma;
        return odev->fops->mmap(&ovm, vma->vm_start, vma->vm_end, vma->vm_pgoff, odev->private_data);
    }

    return -EINVAL;
}

static loff_t osal_linux_dev_llseek(struct file *filp, loff_t offset, int whence)
{
    struct osal_dev_linux_ctx *ctx;
    osal_dev *odev;

    ctx = container_of(filp->private_data, struct osal_dev_linux_ctx, miscdev);
    odev = ctx->osal_dev;

    if (odev->fops && odev->fops->llseek)
        return odev->fops->llseek((long)offset, whence, odev->private_data);

    return -EINVAL;
}

/* ======================== osal_dev_register / unregister ======================== */

int osal_dev_register(osal_dev *dev)
{
    struct osal_dev_linux_ctx *ctx;
    int ret;

    if (!dev) {
        osal_log("osal: osal_dev_register: NULL dev\n");
        return -EINVAL;
    }

    ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
        return -ENOMEM;

    ctx->osal_dev = dev;

    /* Initialize file_operations shim from osal_fileops */
    memset(&ctx->fops, 0, sizeof(ctx->fops));
    ctx->fops.owner = THIS_MODULE;
    if (dev->fops) {
        if (dev->fops->open)
            ctx->fops.open = osal_linux_dev_open;
        if (dev->fops->release)
            ctx->fops.release = osal_linux_dev_release;
        if (dev->fops->read)
            ctx->fops.read = osal_linux_dev_read;
        if (dev->fops->write)
            ctx->fops.write = osal_linux_dev_write;
        if (dev->fops->llseek)
            ctx->fops.llseek = osal_linux_dev_llseek;
        if (dev->fops->poll)
            ctx->fops.poll = osal_linux_dev_poll;
        if (dev->fops->mmap)
            ctx->fops.mmap = osal_linux_dev_mmap;
        if (dev->fops->cmd_list && dev->fops->cmd_cnt > 0) {
            ctx->fops.unlocked_ioctl = osal_linux_dev_unlocked_ioctl;
#ifdef CONFIG_COMPAT
            ctx->fops.compat_ioctl = osal_linux_dev_compat_ioctl;
#endif
        }
    }

    /* Initialize miscdevice */
    ctx->miscdev.minor = dev->minor ? dev->minor : MISC_DYNAMIC_MINOR;
    ctx->miscdev.name = dev->name;
    ctx->miscdev.fops = &ctx->fops;

    ret = misc_register(&ctx->miscdev);
    if (ret) {
        osal_log("osal: misc_register failed for %s, ret=%d\n", dev->name, ret);
        kfree(ctx);
        return ret;
    }

    /* Store actual minor number back to osal_dev */
    dev->minor = ctx->miscdev.minor;
    /* Store native context pointer in osal_dev.dev */
    dev->dev = ctx;

    return 0;
}
EXPORT_SYMBOL(osal_dev_register);

void osal_dev_unregister(osal_dev *dev)
{
    struct osal_dev_linux_ctx *ctx;

    if (!dev)
        return;

    ctx = (struct osal_dev_linux_ctx *)dev->dev;
    if (ctx) {
        misc_deregister(&ctx->miscdev);
        kfree(ctx);
        dev->dev = NULL;
    }
}
EXPORT_SYMBOL(osal_dev_unregister);

osal_dev *osal_dev_create(const char *name)
{
    osal_dev *dev;

    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return NULL;

    strncpy(dev->name, name, OSAL_DEV_NAME_LEN - 1);
    dev->name[OSAL_DEV_NAME_LEN - 1] = '\0';
    dev->minor = 0;
    dev->parent_minor = 0;
    dev->fops = NULL;
    dev->pmops = NULL;
    dev->dev = NULL;
    dev->owner = THIS_MODULE;
	dev->private_data = NULL;

    return dev;
}
EXPORT_SYMBOL(osal_dev_create);

int osal_dev_destroy(osal_dev *dev)
{
    if (!dev)
        return OSAL_FAILURE;

    /* If still registered, unregister first */
    if (dev->dev)
        osal_dev_unregister(dev);

    kfree(dev);
    return OSAL_SUCCESS;
}
EXPORT_SYMBOL(osal_dev_destroy);
