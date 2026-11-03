#include <rtthread.h>
#include <string.h>
#include "osal.h"
#include "osal_inner.h"

/* RT-Thread has no platform bus. Like the AliOS port, driver registration
 * builds a minimal osal_platform_device from the driver's static resource
 * table and calls probe() directly. */

int osal_platform_driver_register(osal_platform_driver *drv)
{
	osal_platform_device *pdev;
	int ret;

	if (drv == NULL || drv->probe == NULL) {
		osal_log("osal_platform_driver_register: invalid driver or probe function\n");
		return OSAL_FAILURE;
	}

	pdev = (osal_platform_device *)rt_malloc(sizeof(osal_platform_device));
	if (pdev == NULL) {
		osal_log("rt_malloc failed!\n");
		return OSAL_FAILURE;
	}
	rt_memset(pdev, 0, sizeof(osal_platform_device));

	if (drv->name != NULL) {
		rt_strncpy(pdev->name, drv->name, OSAL_PLATFORM_DEV_NAME_LEN - 1);
		pdev->name[OSAL_PLATFORM_DEV_NAME_LEN - 1] = '\0';
	}
	pdev->drvdata = NULL;
	pdev->dev = NULL;          /* No native device on RT-Thread */
	pdev->resource = (void *)drv->resource_table;
	pdev->resource_count = drv->resource_count;
	pdev->irq = -1;

	drv->private_data = pdev;

	/* Directly call probe — no platform bus matching on RT-Thread. */
	ret = drv->probe(pdev);
	if (ret) {
		osal_log("osal_platform_driver_register: driver %s probe failed: %d\n", drv->name, ret);
		rt_free(pdev);
		drv->private_data = NULL;
		return ret;
	}

	return OSAL_SUCCESS;
}
// EXPORT_SYMBOL(osal_platform_driver_register);

void osal_platform_driver_unregister(osal_platform_driver *drv)
{
	if (drv == NULL)
		return;

	if (drv->remove && drv->private_data) {
		drv->remove((osal_platform_device *)drv->private_data);
	}

	if (drv->private_data) {
		rt_free(drv->private_data);
		drv->private_data = NULL;
	}
}
// EXPORT_SYMBOL(osal_platform_driver_unregister);

void osal_platform_set_drvdata(osal_platform_device *pdev, void *data)
{
	if (pdev)
		pdev->drvdata = data;
}
// EXPORT_SYMBOL(osal_platform_set_drvdata);

void *osal_platform_get_drvdata(osal_platform_device *pdev)
{
	if (pdev == NULL)
		return NULL;
	return pdev->drvdata;
}
// EXPORT_SYMBOL(osal_platform_get_drvdata);

void *osal_platform_get_resource(osal_platform_device *pdev, unsigned int type, unsigned int num)
{
	const osal_resource *table;
	unsigned int i, count = 0;

	if (pdev == NULL)
		return NULL;

	table = (const osal_resource *)pdev->resource;
	if (table == NULL)
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
// EXPORT_SYMBOL(osal_platform_get_resource);

void *osal_platform_get_resource_byname(osal_platform_device *pdev, unsigned int type, const char *name)
{
	const osal_resource *table;
	unsigned int i;

	if (pdev == NULL || name == NULL)
		return NULL;

	table = (const osal_resource *)pdev->resource;
	if (table == NULL)
		return NULL;

	for (i = 0; i < pdev->resource_count; i++) {
		if (table[i].flags == type &&
			table[i].name &&
			rt_strncmp(table[i].name, name, RT_NAME_MAX) == 0) {
			return (void *)&table[i];
		}
	}
	return NULL;
}
// EXPORT_SYMBOL(osal_platform_get_resource_byname);

int osal_platform_get_irq(osal_platform_device *pdev, unsigned int num)
{
	osal_resource *res;

	if (pdev == NULL)
		return -1;

	res = (osal_resource *)osal_platform_get_resource(pdev, OSAL_IORESOURCE_IRQ, num);
	if (res)
		return (int)res->start;

	return pdev->irq;
}
// EXPORT_SYMBOL(osal_platform_get_irq);

int osal_platform_get_irq_byname(osal_platform_device *pdev, const char *name)
{
	osal_resource *res;

	if (pdev == NULL || name == NULL)
		return -1;

	res = (osal_resource *)osal_platform_get_resource_byname(pdev, OSAL_IORESOURCE_IRQ, name);
	if (res)
		return (int)res->start;

	return pdev->irq;
}
// EXPORT_SYMBOL(osal_platform_get_irq_byname);

osal_clk *osal_platform_get_clk_byname(osal_platform_device *pdev, const char *name)
{
	osal_resource *res;
	osal_clk_ctrl_info clk_info;

	if (pdev == NULL || name == NULL)
		return NULL;

	res = (osal_resource *)osal_platform_get_resource_byname(pdev, OSAL_IORESOURCE_CLK, name);
	if (res == NULL) {
		osal_log("No CLK resource for %s\n", name);
		return NULL;
	}

	rt_memset(&clk_info, 0, sizeof(clk_info));
	rt_strncpy(clk_info.name, res->name, sizeof(clk_info.name) - 1);
	clk_info.reg_addr = res->start;
	clk_info.bit = res->end;
	if (osal_clk_register(&clk_info)) {
		osal_log("osal_clk_register fail for %s\n", clk_info.name);
		return NULL;
	}
	return osal_clk_get(pdev->dev, name);
}
// EXPORT_SYMBOL(osal_platform_get_clk_byname);

void osal_platform_put_clk(osal_platform_device *pdev, osal_clk *clk)
{
	RT_UNUSED(pdev);

	if (clk == NULL)
		return;

	osal_clk_put_unregister(NULL, clk);
}
// EXPORT_SYMBOL(osal_platform_put_clk);

/* RT-Thread: no MMU, ioremap is identity mapping, so there is nothing
 * to clean up for the "devm" variant. */
void *osal_devm_ioremap(osal_platform_device *pdev, unsigned long phys_addr, unsigned long size)
{
	RT_UNUSED(size);

	if (pdev == NULL)
		return NULL;

	return (void *)phys_addr;
}
// EXPORT_SYMBOL(osal_devm_ioremap);

void *osal_devm_ioremap_resource_byname(osal_platform_device *pdev, const char *name)
{
	osal_resource *res;

	if (pdev == NULL || name == NULL)
		return NULL;

	res = (osal_resource *)osal_platform_get_resource_byname(pdev, OSAL_IORESOURCE_MEM, name);
	if (res == NULL)
		return NULL;

	return (void *)res->start;
}
// EXPORT_SYMBOL(osal_devm_ioremap_resource_byname);
