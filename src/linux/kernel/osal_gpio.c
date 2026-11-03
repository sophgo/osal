#include <linux/gpio.h>
#include <linux/slab.h>
#include "osal.h"

#define GPIO_PINS_PER_BANK  32

struct osal_gpio_ctx {
	int	gpio_base;			/* port_idx * PINS_PER_BANK */
	bool	requested[GPIO_PINS_PER_BANK];	/* per-pin request tracking */
};

int osal_gpio_init(osal_gpio_t *gpio, unsigned int port_idx)
{
	struct osal_gpio_ctx *ctx;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return OSAL_FAILURE;

	ctx->gpio_base = port_idx * GPIO_PINS_PER_BANK;
	*gpio = ctx;
	return OSAL_SUCCESS;
}

int osal_gpio_dir(osal_gpio_t *gpio, unsigned int mask, int direction)
{
	struct osal_gpio_ctx *ctx = (struct osal_gpio_ctx *)*gpio;
	int pin, gpio_num, ret;

	if (!ctx)
		return OSAL_FAILURE;

	for (pin = 0; pin < GPIO_PINS_PER_BANK; ++pin) {
		if (!(mask & (1U << pin)))
			continue;

		gpio_num = ctx->gpio_base + pin;

		/* Request GPIO if not already requested */
		if (!ctx->requested[pin]) {
			ret = gpio_request(gpio_num, "osal_gpio");
			if (ret && ret != -EBUSY)
				continue;	/* skip on error; -EBUSY = already owned */
			ctx->requested[pin] = true;
		}

		if (direction)
			gpio_direction_output(gpio_num, 0);
		else
			gpio_direction_input(gpio_num);
	}
	return OSAL_SUCCESS;
}

int osal_gpio_write(osal_gpio_t *gpio, unsigned int mask, int value)
{
	struct osal_gpio_ctx *ctx = (struct osal_gpio_ctx *)*gpio;
	int pin, gpio_num;

	if (!ctx)
		return OSAL_FAILURE;

	for (pin = 0; pin < GPIO_PINS_PER_BANK; ++pin) {
		if (!(mask & (1U << pin)))
			continue;

		gpio_num = ctx->gpio_base + pin;
		gpio_direction_output(gpio_num, value);
	}
	return OSAL_SUCCESS;
}

void osal_gpio_deinit(osal_gpio_t *gpio)
{
	struct osal_gpio_ctx *ctx = (struct osal_gpio_ctx *)*gpio;
	int pin, gpio_num;

	if (!ctx)
		return;

	for (pin = 0; pin < GPIO_PINS_PER_BANK; ++pin) {
		if (!ctx->requested[pin])
			continue;
		gpio_num = ctx->gpio_base + pin;
		gpio_free(gpio_num);
	}
	kfree(ctx);
	*gpio = NULL;
}

EXPORT_SYMBOL_GPL(osal_gpio_init);
EXPORT_SYMBOL_GPL(osal_gpio_dir);
EXPORT_SYMBOL_GPL(osal_gpio_write);
EXPORT_SYMBOL_GPL(osal_gpio_deinit);
