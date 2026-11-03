#include <stdlib.h>
#include <string.h>
#include <aos/kernel.h>
#include <vfs.h>
#include "osal.h"
#include "osal_inner.h"

/* ======================== osal_dev_register / unregister ======================== */

int osal_dev_register(osal_dev *dev)
{
    /* AliOS does not have a char device registration framework like Linux.
     * The osal_dev is stored as-is; the actual device node is created
     * by the AliOS application layer using driver_base_ioctl etc. */
    if (!dev) {
        osal_log("osal_dev_register: NULL dev\n");
        return OSAL_FAILURE;
    }

    return OSAL_SUCCESS;
}

void osal_dev_unregister(osal_dev *dev)
{
    /* No native device framework on AliOS, nothing to unregister */
    if (!dev)
        return;
}

osal_dev *osal_dev_create(const char *name)
{
    osal_dev *dev;

    dev = (osal_dev *)osal_zalloc(sizeof(osal_dev));
    if (!dev)
        return NULL;

    strncpy(dev->name, name, OSAL_DEV_NAME_LEN - 1);
    dev->name[OSAL_DEV_NAME_LEN - 1] = '\0';
    dev->minor = 0;
    dev->parent_minor = 0;
    dev->fops = NULL;
    dev->pmops = NULL;
    dev->dev = NULL;
    dev->owner = NULL;

    return dev;
}

int osal_dev_destroy(osal_dev *dev)
{
    if (!dev)
        return OSAL_FAILURE;

    if (dev->dev)
        osal_dev_unregister(dev);

    osal_free(dev);
    return OSAL_SUCCESS;
}

void osal_poll_wait(osal_poll *table, osal_wait *wait)
{
    /* AliOS callers block through the osal_wait APIs. */
    (void)table;
    (void)wait;
}
