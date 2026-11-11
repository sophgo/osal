#include "osal.h"
#include "osal_inner.h"


extern int osal_ion_init(void);
extern int osal_ion_deinit(void);

static int ext_drv_osal_init(void)
{
    if (osal_ion_init()) {
        osal_log("osal_ion_init failed!\n");
        return -1;
    }
	return OSAL_SUCCESS;
}

static void ext_drv_osal_exit(void)
{
    osal_ion_deinit();
    return;
}

int osal_driver_init(void)
{
    int ret;

    ret = ext_drv_osal_init();
    if (ret != 0) {
        osal_log("common init failed! ret = %#x.\n", ret);
        goto err_osal_exit;
    }
    return 0;

err_osal_exit:
    ext_drv_osal_exit();

    return -1;
}

void osal_driver_exit(void)
{
    ext_drv_osal_exit();

    return;
}

ALIOS_MODULE_REGISTER(osal_driver_init);

