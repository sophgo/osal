#include <stdlib.h>
#include <string.h>

#include "osal.h"
#include "osal_inner.h"

int osal_platform_driver_register(osal_platform_driver *drv)
{
    osal_platform_device *pdev;
    int ret;

    if (!drv || !drv->probe) {
        osal_log("osal_platform_driver_register: invalid driver or probe function\n");
        return OSAL_FAILURE;
    }

    /* Allocate a minimal osal platform device */
    pdev = (osal_platform_device *)osal_zalloc(sizeof(osal_platform_device));
    if (!pdev)
        return OSAL_FAILURE;

    strncpy(pdev->name, drv->name, OSAL_PLATFORM_DEV_NAME_LEN - 1);
    pdev->name[OSAL_PLATFORM_DEV_NAME_LEN - 1] = '\0';
    pdev->drvdata = NULL;
    pdev->dev = NULL;          /* No native device on AliOS */
    pdev->resource = (void *)drv->resource_table;   /* point to driver's resource table */
    pdev->resource_count = drv->resource_count;     /* store table entry count */
    pdev->irq = -1;

    drv->private_data = pdev;

    /* Directly call probe — no platform bus matching on AliOS */
    ret = drv->probe(pdev);
    if (ret) {
        osal_log("osal_platform_driver_register: driver %s probe failed: %d\n", drv->name, ret);
        osal_free(pdev);
        drv->private_data = NULL;
        return ret;
    }

    return OSAL_SUCCESS;
}

void osal_platform_driver_unregister(osal_platform_driver *drv)
{
    if (!drv)
        return;

    if (drv->remove && drv->private_data) {
        drv->remove((osal_platform_device *)drv->private_data);
    }

    if (drv->private_data) {
        osal_free(drv->private_data);
        drv->private_data = NULL;
    }
}

void osal_platform_set_drvdata(osal_platform_device *pdev, void *data)
{
    if (pdev)
        pdev->drvdata = data;
}

void *osal_platform_get_drvdata(osal_platform_device *pdev)
{
    if (!pdev)
        return NULL;
    return pdev->drvdata;
}

void *osal_platform_get_resource(osal_platform_device *pdev, unsigned int type, unsigned int num)
{
    /* AliOS: search resource table by type and index */
    const osal_resource *table;
    unsigned int i, count = 0;

    if (!pdev)
        return NULL;

    table = (const osal_resource *)pdev->resource;
    if (!table)
        return NULL;

    for (i = 0; i < pdev->resource_count; i++) {
        if (table[i].flags == type) {
            if (count == num)
                return (void *)&table[i];
            count++;
        }
    }
    return NULL;
}

void *osal_platform_get_resource_byname(osal_platform_device *pdev, unsigned int type, const char *name)
{
    /* AliOS: search resource table by type and name */
    const osal_resource *table;
    unsigned int i;

    if (!pdev || !name)
        return NULL;

    table = (const osal_resource *)pdev->resource;
    if (!table)
        return NULL;

    for (i = 0; i < pdev->resource_count; i++) {
        if (table[i].flags == type &&
            table[i].name &&
            osal_strcmp(table[i].name, name) == 0) {
            return (void *)&table[i];
        }
    }
    return NULL;
}

int osal_platform_get_irq(osal_platform_device *pdev, unsigned int num)
{
    osal_resource *res;

    if (!pdev)
        return -1;

    /* First try to find IRQ in resource table */
    res = (osal_resource *)osal_platform_get_resource(pdev, OSAL_IORESOURCE_IRQ, num);
    if (res)
        return (int)res->start;

    /* Fall back to cached irq field */
    return pdev->irq;
}

int osal_platform_get_irq_byname(osal_platform_device *pdev, const char *name)
{
    osal_resource *res;

    if (!pdev || !name)
        return -1;

    /* Search resource table for IRQ by name */
    res = (osal_resource *)osal_platform_get_resource_byname(pdev, OSAL_IORESOURCE_IRQ, name);
    if (res)
        return (int)res->start;

    /* Fall back to cached irq field */
    return pdev->irq;
}

osal_clk *osal_platform_get_clk_byname(osal_platform_device *pdev, const char *name)
{
    osal_resource *res;
    osal_clk_ctrl_info clk_info;

    if (!pdev || !name)
        return NULL;

    res = (osal_resource *)osal_platform_get_resource_byname(pdev, OSAL_IORESOURCE_CLK, name);
    if (!res) {
        osal_log("No CLK resource for %s\n", name);
        return NULL;
    }

    osal_strcpy(clk_info.name, res->name);
    clk_info.reg_addr = res->start;
    clk_info.bit = res->end;
    if (osal_clk_register(&clk_info)) {
        osal_log("osal_clk_register fail for %s\n", clk_info.name);
        return NULL;
    }
    return osal_clk_get(pdev->dev, name);
}

void osal_platform_put_clk(osal_platform_device *pdev, osal_clk *clk)
{
    if (!clk)
        return;

    osal_clk_put_unregister(NULL, clk);
}

/* AliOS devm_ioremap: no MMU, so ioremap is just a cast.
 * We record the mapping in a simple linked list attached to pdev->drvdata
 * so that osal_platform_driver_unregister can clean them up on driver removal.
 *
 * Since AliOS has no devm framework, we use a per-device tracking list.
 * The list is walked and all recorded ioremap entries are "freed" (no-op on AliOS)
 * when the driver is unregistered.
 */

struct osal_devm_ioremap_entry {
    unsigned long phys_addr;
    unsigned long size;
    void *virt_addr;
    struct osal_devm_ioremap_entry *next;
};

/* Retrieve the devm tracking list from the osal_platform_device.
 * We store it as a secondary pointer in a dedicated field.
 * Since pdev->drvdata is the driver's own private data (sys_device etc.),
 * we cannot use it. Instead, we piggyback on the resource_count field
 * alignment padding or use a separate mechanism.
 *
 * Simplest approach: store the list head pointer in pdev->resource
 * after the driver has finished probe, but that conflicts with resource lookup.
 *
 * Better approach: embed the list head in a wrapper struct around drvdata.
 * But that would change the drvdata contract.
 *
 * Most practical approach: use a global hash or a per-driver list.
 * Since AliOS typically has very few devices, a simple per-driver list
 * stored in drv->private_data (which is the osal_platform_device on AliOS)
 * works well. We add a devm_list field to track ioremap entries.
 *
 * Actually, the cleanest approach for AliOS: since ioremap is a no-op
 * (just returns phys_addr cast), and iounmap is also a no-op, the "devm"
 * aspect is trivially satisfied — there is nothing to clean up.
 * We still provide the function for API consistency, but it's effectively
 * identical to osal_ioremap on AliOS.
 */

void *osal_devm_ioremap(osal_platform_device *pdev, unsigned long phys_addr, unsigned long size)
{
    /* AliOS: no MMU, ioremap is identity mapping. No cleanup needed. */
    if (!pdev)
        return NULL;

    return (void *)phys_addr;
}

void *osal_devm_ioremap_resource_byname(osal_platform_device *pdev, const char *name)
{
    osal_resource *res;

    if (!pdev || !name)
        return NULL;

    res = (osal_resource *)osal_platform_get_resource_byname(pdev, OSAL_IORESOURCE_MEM, name);
    if (!res)
        return NULL;

    /* AliOS: no MMU, ioremap is identity mapping */
    return (void *)res->start;
}