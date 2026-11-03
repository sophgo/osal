#ifndef __OSAL_IO_H__
#define __OSAL_IO_H__

#include "osal.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @ingroup osal_io
 * @brief Read a 32-bit value from a memory-mapped I/O register.
 *
 * @par Description:
 * This API is used to read a 32-bit value from a memory-mapped I/O register at the specified address.
 *
 * @param addr [in] The address of the register to read.
 *
 * @return The 32-bit value read from the register.
 *
 * @par Support System:
 * linux alios.
 */
unsigned int osal_io_read(unsigned long addr);

/**
 * @ingroup osal_io
 * @brief Write a 32-bit value to a memory-mapped I/O register.
 *
 * @par Description:
 * This API is used to write a 32-bit value to a memory-mapped I/O register at the specified address.
 *
 * @param addr [in] The address of the register to write.
 * @param data [in] The 32-bit value to write to the register.
 *
 * @par Support System:
 * linux alios.
 */
void osal_io_write(unsigned long addr, unsigned int data);

/**
 * @ingroup osal_io
 * @brief Write a 32-bit value to a memory-mapped I/O register with bitmask.
 *
 * @par Description:
 * This API is used to perform an atomic read-modify-write operation on a memory-mapped I/O register.
 * It clears the bits specified by mask, then sets the bits from data that are enabled in mask.
 * The operation formula: new_value = (old_value & ~mask) | (data & mask).
 *
 * @attention
 * This function is atomic and protected by spinlock on linux system.
 *
 * @param addr [in] The address of the register to write.
 * @param mask [in] Bitmask indicating which bits to modify.
 * @param data [in] The 32-bit value to write for the masked bits.
 *
 * @par Support System:
 * linux alios.
 */
void osal_io_write_mask(unsigned long addr, unsigned int mask, unsigned int data);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif /* __OSAL_IO_H__ */