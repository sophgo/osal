#ifndef __OSAL_GPIO_H__
#define __OSAL_GPIO_H__

#include "osal.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

typedef void *osal_gpio_t;

/**
 * @ingroup osal_gpio
 * @brief Initialize a GPIO port.
 *
 * @param gpio [in/out] GPIO handle to initialize.
 * @param port_idx [in] GPIO port index.
 *
 * @return OSAL_SUCCESS on success, OSAL_FAILURE on failure.
 *
 * @par Support System:
 * linux alios.
 */
int osal_gpio_init(osal_gpio_t *gpio, unsigned int port_idx);

/**
 * @ingroup osal_gpio
 * @brief Set GPIO pin direction.
 *
 * @param gpio [in] GPIO handle.
 * @param mask [in] Bit mask of pins to configure.
 * @param direction [in] 0 for input, 1 for output.
 *
 * @return OSAL_SUCCESS on success, OSAL_FAILURE on failure.
 *
 * @par Support System:
 * linux alios.
 */
int osal_gpio_dir(osal_gpio_t *gpio, unsigned int mask, int direction);

/**
 * @ingroup osal_gpio
 * @brief Write value to GPIO pin.
 *
 * @param gpio [in] GPIO handle.
 * @param mask [in] Bit mask of pins to write.
 * @param value [in] Value to write (0 or 1).
 *
 * @return OSAL_SUCCESS on success, OSAL_FAILURE on failure.
 *
 * @par Support System:
 * linux alios.
 */
int osal_gpio_write(osal_gpio_t *gpio, unsigned int mask, int value);

/**
 * @ingroup osal_gpio
 * @brief De-initialize / release a GPIO handle.
 *
 * @param gpio [in] GPIO handle.
 *
 * @par Support System:
 * linux alios.
 */
void osal_gpio_deinit(osal_gpio_t *gpio);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* __OSAL_GPIO_H__ */
