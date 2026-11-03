#include <rtthread.h>
#include <string.h>
#include "osal.h"
#include "osal_inner.h"

/* RT-Thread does not have a char-device registration framework like Linux.
 * The osal_dev is stored as-is; the media modules interact with user space
 * through their own channels, so registration is a bookkeeping no-op. */

int osal_dev_register(osal_dev *dev)
{
	if (dev == NULL) {
		osal_log("osal_dev_register: NULL dev\n");
		return OSAL_FAILURE;
	}

	return OSAL_SUCCESS;
}
// EXPORT_SYMBOL(osal_dev_register);

void osal_dev_unregister(osal_dev *dev)
{
	/* No native device framework on RT-Thread, nothing to unregister. */
	if (dev == NULL)
		return;
}
// EXPORT_SYMBOL(osal_dev_unregister);

osal_dev *osal_dev_create(const char *name)
{
	osal_dev *dev;

	dev = (osal_dev *)rt_malloc(sizeof(osal_dev));
	if (dev == NULL) {
		osal_log("rt_malloc failed!\n");
		return NULL;
	}
	rt_memset(dev, 0, sizeof(osal_dev));

	if (name != NULL) {
		rt_strncpy(dev->name, name, OSAL_DEV_NAME_LEN - 1);
		dev->name[OSAL_DEV_NAME_LEN - 1] = '\0';
	}
	dev->minor = 0;
	dev->parent_minor = 0;
	dev->fops = NULL;
	dev->pmops = NULL;
	dev->dev = NULL;
	dev->owner = NULL;
	dev->private_data = NULL;

	return dev;
}
// EXPORT_SYMBOL(osal_dev_create);

int osal_dev_destroy(osal_dev *dev)
{
	if (dev == NULL)
		return OSAL_FAILURE;

	if (dev->dev)
		osal_dev_unregister(dev);

	rt_free(dev);
	return OSAL_SUCCESS;
}
// EXPORT_SYMBOL(osal_dev_destroy);

void osal_poll_wait(osal_poll *table, osal_wait *wait)
{
	/* RT-Thread callers block through the osal_wait APIs. */
	(void)table;
	(void)wait;
}
