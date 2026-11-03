#ifndef __OSAL_PINMUX_H__
#define __OSAL_PINMUX_H__

#include "osal.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @ingroup osal_pinmux
 * @brief Configure pinmux for a specific pin-function pair.
 *
 * @par Description:
 * This API configures the pinmux register to set a pin to a specific function.
 * It abstracts the platform-specific PINMUX_CONFIG macro.
 *
 * @param pin_name; [in] Pin name identifier (platform-specific enum/define).
 * @param func [in] Function name identifier (platform-specific enum/define).
 *
 * @par Support System:
 * linux alios.
 */
#define osal_pinmux_config(pin, func) PINMUX_CONFIG(pin, func)

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* __OSAL_PINMUX_H__ */
