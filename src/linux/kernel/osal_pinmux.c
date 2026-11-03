/*
 * Linux kernel pinmux implementation.
 *
 * In Linux kernel mode, PINMUX_CONFIG is provided by the kernel's
 * pinctrl subsystem header (e.g., pinctrl-cv190x.h) which is
 * included via the driver's Makefile ccflags-y.
 * This file exists only to satisfy the build system; all pinmux
 * configuration is handled by the PINMUX_CONFIG macro at compile time.
 */
#include "osal.h"
