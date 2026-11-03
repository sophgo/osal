#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "osal.h"
#include "osal_inner.h"

/* Internal context for translating osal_platform_driver to Linux struct platform_driver */
struct osal_platform_driver_linux_ctx {
    struct platform_driver linux_drv;
    struct dev_pm_ops linux_pm_ops;    /* only populated if osal_drv->pmops != NULL */
    osal_platform_driver *osal_drv;
    struct of_device_id dt_match[2];
};

/* Shim remove: retrieve osal_platform_device, call driver remove, free it.
 * We need to recover the osal_platform_device that was allocated in probe.
 * Strategy: store the osal_platform_device pointer in the native platform_device's
 * drvdata as a secondary pointer, alongside the driver's own drvdata which is
 * set via osal_platform_set_drvdata.
 *
 * Actually, the simplest approach is to NOT free the osal_platform_device in remove.
 * Instead, we keep it alive and let the driver's drvdata pointer serve as the
 * reference. The osal_platform_device is freed in osal_platform_driver_unregister
 * after the module is unloaded. This avoids the lifetime conflict between
 * osal_platform_set_drvdata (which stores the driver's private data, not the
 * osal_platform_device itself) and our need to recover the osal_platform_device.
 *
 * However, for probe/remove to work correctly, we need a way to map from
 * struct platform_device * back to osal_platform_device *.
 * We use devm_kzalloc for the osal_platform_device so it is automatically freed
 * when the device is unbound. And we store a pointer to it in a dedicated field.
 */

/* Map from struct platform_device * to osal_platform_device *.
 * We store this mapping in platform_data, which is separate from drvdata. */
static inline void osal_linux_set_osal_pdev(struct platform_device *pdev, osal_platform_device *osal_pdev)
{
    pdev->dev.platform_data = osal_pdev;
}

static inline osal_platform_device *osal_linux_get_osal_pdev(struct platform_device *pdev)
{
    return (osal_platform_device *)pdev->dev.platform_data;
}

static inline osal_platform_device *osal_linux_dev_get_osal_pdev(struct device *dev)
{
    return dev ? (osal_platform_device *)dev->platform_data : NULL;
}

/* Shim probe: Linux platform_device -> osal_platform_device */
static int osal_linux_probe_shim(struct platform_device *pdev)
{
    struct osal_platform_driver_linux_ctx *ctx;
    osal_platform_device *osal_pdev;
    int ret;

    /* Retrieve our context from the platform_driver that matched this device.
     * pdev->dev.driver points to the device_driver embedded in our linux_drv,
     * so we can use container_of to get back to the linux_ctx. */
    ctx = container_of(pdev->dev.driver, struct osal_platform_driver_linux_ctx,
                       linux_drv.driver);

    osal_pdev = devm_kzalloc(&pdev->dev, sizeof(*osal_pdev), GFP_KERNEL);
    if (!osal_pdev)
        return -ENOMEM;

    strncpy(osal_pdev->name, pdev->name, OSAL_PLATFORM_DEV_NAME_LEN - 1);
    osal_pdev->name[OSAL_PLATFORM_DEV_NAME_LEN - 1] = '\0';
    osal_pdev->dev = pdev;          /* store native pdev for resource access */
    osal_pdev->drvdata = NULL;
    osal_pdev->resource = NULL;

    /* Store the osal_platform_device pointer in platform_data for recovery in remove */
    osal_linux_set_osal_pdev(pdev, osal_pdev);

    ret = ctx->osal_drv->probe(osal_pdev);
    if (ret) {
        /* devm_kzalloc will be automatically freed on probe failure,
         * but we need to clear platform_data to avoid stale pointer */
        osal_linux_set_osal_pdev(pdev, NULL);
    }
    return ret;
}

/* Shim remove: retrieve osal_platform_device, call driver remove.
 * osal_platform_device is devm-allocated, so it's auto-freed after remove returns. */
static int osal_linux_remove_shim(struct platform_device *pdev)
{
    struct osal_platform_driver_linux_ctx *ctx;
    osal_platform_device *osal_pdev;

    ctx = container_of(pdev->dev.driver, struct osal_platform_driver_linux_ctx,
                       linux_drv.driver);

    osal_pdev = osal_linux_get_osal_pdev(pdev);

    if (osal_pdev && ctx->osal_drv->remove)
        ctx->osal_drv->remove(osal_pdev);

    osal_linux_set_osal_pdev(pdev, NULL);

    return 0;
}

/* PM ops shim functions */
static int osal_linux_pm_suspend(struct device *dev)
{
    struct osal_platform_driver_linux_ctx *ctx;
    osal_platform_device *osal_pdev;

    ctx = container_of(dev->driver, struct osal_platform_driver_linux_ctx,
                       linux_drv.driver);
    osal_pdev = osal_linux_dev_get_osal_pdev(dev);

    if (ctx->osal_drv->pmops && ctx->osal_drv->pmops->pm_suspend)
        return ctx->osal_drv->pmops->pm_suspend(
            osal_pdev ? osal_pdev->drvdata : ctx->osal_drv->pmops->private_data);
    return 0;
}

static int osal_linux_pm_resume_early(struct device *dev)
{
    struct osal_platform_driver_linux_ctx *ctx;
    osal_platform_device *osal_pdev;

    ctx = container_of(dev->driver, struct osal_platform_driver_linux_ctx,
                       linux_drv.driver);
    osal_pdev = osal_linux_dev_get_osal_pdev(dev);

    if (ctx->osal_drv->pmops && ctx->osal_drv->pmops->pm_resume_early)
        return ctx->osal_drv->pmops->pm_resume_early(
            osal_pdev ? osal_pdev->drvdata : ctx->osal_drv->pmops->private_data);
    return 0;
}

static int osal_linux_pm_resume(struct device *dev)
{
    struct osal_platform_driver_linux_ctx *ctx;
    osal_platform_device *osal_pdev;

    ctx = container_of(dev->driver, struct osal_platform_driver_linux_ctx,
                       linux_drv.driver);
    osal_pdev = osal_linux_dev_get_osal_pdev(dev);

    if (ctx->osal_drv->pmops && ctx->osal_drv->pmops->pm_resume)
        return ctx->osal_drv->pmops->pm_resume(
            osal_pdev ? osal_pdev->drvdata : ctx->osal_drv->pmops->private_data);
    return 0;
}

static int osal_linux_pm_freeze(struct device *dev)
{
    struct osal_platform_driver_linux_ctx *ctx;
    osal_platform_device *osal_pdev;

    ctx = container_of(dev->driver, struct osal_platform_driver_linux_ctx,
                       linux_drv.driver);
    osal_pdev = osal_linux_dev_get_osal_pdev(dev);

    if (ctx->osal_drv->pmops && ctx->osal_drv->pmops->pm_lowpower_enter)
        return ctx->osal_drv->pmops->pm_lowpower_enter(
            osal_pdev ? osal_pdev->drvdata : ctx->osal_drv->pmops->private_data);
    return 0;
}

static int osal_linux_pm_restore(struct device *dev)
{
    struct osal_platform_driver_linux_ctx *ctx;
    osal_platform_device *osal_pdev;

    ctx = container_of(dev->driver, struct osal_platform_driver_linux_ctx,
                       linux_drv.driver);
    osal_pdev = osal_linux_dev_get_osal_pdev(dev);

    if (ctx->osal_drv->pmops && ctx->osal_drv->pmops->pm_lowpower_exit)
        return ctx->osal_drv->pmops->pm_lowpower_exit(
            osal_pdev ? osal_pdev->drvdata : ctx->osal_drv->pmops->private_data);
    return 0;
}

static int osal_linux_pm_poweroff(struct device *dev)
{
    struct osal_platform_driver_linux_ctx *ctx;
    osal_platform_device *osal_pdev;

    ctx = container_of(dev->driver, struct osal_platform_driver_linux_ctx,
                       linux_drv.driver);
    osal_pdev = osal_linux_dev_get_osal_pdev(dev);

    if (ctx->osal_drv->pmops && ctx->osal_drv->pmops->pm_poweroff)
        return ctx->osal_drv->pmops->pm_poweroff(
            osal_pdev ? osal_pdev->drvdata : ctx->osal_drv->pmops->private_data);
    return 0;
}

int osal_platform_driver_register(osal_platform_driver *drv)
{
    struct osal_platform_driver_linux_ctx *ctx;
    int ret;

    if (!drv || !drv->probe) {
        osal_log("osal: invalid driver or probe function\n");
        return -EINVAL;
    }
    if (!drv->of_match_table) {
        osal_log("osal: of_match_table is NULL\n");
        return -EINVAL;
    }

    ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
        return -ENOMEM;

    ctx->osal_drv = drv;

    /* Initialize Linux platform_driver shim */
    ctx->linux_drv.probe = osal_linux_probe_shim;
    ctx->linux_drv.remove = osal_linux_remove_shim;
    ctx->linux_drv.driver.name = drv->name;
    ctx->linux_drv.driver.owner = THIS_MODULE;
    ctx->linux_drv.driver.of_match_table = ctx->dt_match;

    osal_strncpy(ctx->dt_match[0].compatible, drv->of_match_table,
         osal_strlen(drv->of_match_table));

    /* Translate PM ops if provided */
    if (drv->pmops) {
        memset(&ctx->linux_pm_ops, 0, sizeof(ctx->linux_pm_ops));
        if (drv->pmops->pm_suspend)
            ctx->linux_pm_ops.suspend = osal_linux_pm_suspend;
        if (drv->pmops->pm_resume_early)
            ctx->linux_pm_ops.resume_early = osal_linux_pm_resume_early;
        if (drv->pmops->pm_resume)
            ctx->linux_pm_ops.resume = osal_linux_pm_resume;
        if (drv->pmops->pm_lowpower_enter)
            ctx->linux_pm_ops.freeze = osal_linux_pm_freeze;
        if (drv->pmops->pm_lowpower_exit)
            ctx->linux_pm_ops.restore = osal_linux_pm_restore;
        if (drv->pmops->pm_poweroff)
            ctx->linux_pm_ops.poweroff = osal_linux_pm_poweroff;
        ctx->linux_drv.driver.pm = &ctx->linux_pm_ops;
    }

    drv->private_data = ctx;

    ret = __platform_driver_register(&ctx->linux_drv, THIS_MODULE);
    if (ret) {
        drv->private_data = NULL;
        kfree(ctx);
    }
    return ret;
}
EXPORT_SYMBOL(osal_platform_driver_register);

void osal_platform_driver_unregister(osal_platform_driver *drv)
{
    struct osal_platform_driver_linux_ctx *ctx;

    if (!drv)
        return;

    ctx = drv->private_data;
    if (ctx) {
        platform_driver_unregister(&ctx->linux_drv);
        drv->private_data = NULL;
        kfree(ctx);
    }
}
EXPORT_SYMBOL(osal_platform_driver_unregister);

void osal_platform_set_drvdata(osal_platform_device *pdev, void *data)
{
    if (!pdev)
        return;

    pdev->drvdata = data;
    /* Also set in the native Linux platform_device so dev_get_drvdata works */
    if (pdev->dev)
        platform_set_drvdata((struct platform_device *)pdev->dev, data);
}
EXPORT_SYMBOL(osal_platform_set_drvdata);

void *osal_platform_get_drvdata(osal_platform_device *pdev)
{
    if (!pdev)
        return NULL;
    return pdev->drvdata;
}
EXPORT_SYMBOL(osal_platform_get_drvdata);

void *osal_platform_get_resource(osal_platform_device *pdev, unsigned int type, unsigned int num)
{
    if (!pdev || !pdev->dev)
        return NULL;
    return (void *)platform_get_resource((struct platform_device *)pdev->dev, type, num);
}
EXPORT_SYMBOL(osal_platform_get_resource);

void *osal_platform_get_resource_byname(osal_platform_device *pdev, unsigned int type, const char *name)
{
    if (!pdev || !pdev->dev)
        return NULL;
    return (void *)platform_get_resource_byname((struct platform_device *)pdev->dev, type, name);
}
EXPORT_SYMBOL(osal_platform_get_resource_byname);

int osal_platform_get_irq(osal_platform_device *pdev, unsigned int num)
{
    if (!pdev || !pdev->dev)
        return pdev ? pdev->irq : -1;
    return platform_get_irq((struct platform_device *)pdev->dev, num);
}
EXPORT_SYMBOL(osal_platform_get_irq);

int osal_platform_get_irq_byname(osal_platform_device *pdev, const char *name)
{
    if (!pdev || !pdev->dev)
        return pdev ? pdev->irq : -1;
    return platform_get_irq_byname((struct platform_device *)pdev->dev, name);
}
EXPORT_SYMBOL(osal_platform_get_irq_byname);

osal_clk *osal_platform_get_clk_byname(osal_platform_device *pdev, const char *name)
{
    struct device *dev;

    if (!pdev || !pdev->dev)
        return NULL;

    dev = &((struct platform_device *)pdev->dev)->dev;
    return osal_clk_get(dev, name);
}
EXPORT_SYMBOL(osal_platform_get_clk_byname);

void osal_platform_put_clk(osal_platform_device *pdev, osal_clk *clk)
{
    struct device *dev;

    if (!pdev || !pdev->dev || !clk)
        return;

    dev = &((struct platform_device *)pdev->dev)->dev;
    osal_clk_put(dev, clk);
}
EXPORT_SYMBOL(osal_platform_put_clk);

void *osal_devm_ioremap(osal_platform_device *pdev, unsigned long phys_addr, unsigned long size)
{
    struct device *dev;

    if (!pdev || !pdev->dev)
        return NULL;

    dev = &((struct platform_device *)pdev->dev)->dev;
    return devm_ioremap(dev, phys_addr, size);
}
EXPORT_SYMBOL(osal_devm_ioremap);

void *osal_devm_ioremap_resource_byname(osal_platform_device *pdev, const char *name)
{
    struct resource *res;
    struct device *dev;

    if (!pdev || !pdev->dev || !name)
        return NULL;

    dev = &((struct platform_device *)pdev->dev)->dev;
    res = platform_get_resource_byname((struct platform_device *)pdev->dev, IORESOURCE_MEM, name);
    if (!res)
        return NULL;

    return devm_ioremap(dev, res->start, resource_size(res));
}
EXPORT_SYMBOL(osal_devm_ioremap_resource_byname);
