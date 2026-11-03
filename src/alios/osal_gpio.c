#include <drv/gpio.h>
#include "osal.h"

int osal_gpio_init(osal_gpio_t *gpio, unsigned int port_idx)
{
	csi_gpio_t *g = (csi_gpio_t *)osal_kzalloc(sizeof(csi_gpio_t), 0);
	if (!g)
		return OSAL_FAILURE;

	if (csi_gpio_init(g, port_idx) != CSI_OK) {
		osal_kfree(g);
		return OSAL_FAILURE;
	}

	*gpio = g;
	return OSAL_SUCCESS;
}

int osal_gpio_dir(osal_gpio_t *gpio, unsigned int mask, int direction)
{
	csi_gpio_t *g = (csi_gpio_t *)(*gpio);

	if (csi_gpio_dir(g, mask, direction ? GPIO_DIRECTION_OUTPUT : GPIO_DIRECTION_INPUT) != CSI_OK)
		return OSAL_FAILURE;

	return OSAL_SUCCESS;
}

int osal_gpio_write(osal_gpio_t *gpio, unsigned int mask, int value)
{
	csi_gpio_t *g = (csi_gpio_t *)(*gpio);

	csi_gpio_write(g, mask, value);

	return OSAL_SUCCESS;
}

void osal_gpio_deinit(osal_gpio_t *gpio)
{
	csi_gpio_t *g = (csi_gpio_t *)(*gpio);

	if (g) {
		osal_kfree(g);
		*gpio = NULL;
	}
}
