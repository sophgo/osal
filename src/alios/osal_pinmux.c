/*
 * AliOS pinmux implementation.
 *
 * In AliOS mode, PINMUX_CONFIG is provided by pinctrl-mars.h which
 * is included via the chip component include path.
 * This file exists only to satisfy the build system; all pinmux
 * configuration is handled by the PINMUX_CONFIG macro at compile time.
 */
#include "osal.h"
