#include <rtthread.h>
#include <rthw.h>
#include "osal.h"
#include "osal_inner.h"

/* RT-Thread runs on bare-metal/no-MMU for the media drivers, so a
 * memory-mapped register access is a plain volatile load/store. */

unsigned int osal_io_read(unsigned long addr)
{
	return *(volatile unsigned int *)addr;
}
// EXPORT_SYMBOL(osal_io_read);

void osal_io_write(unsigned long addr, unsigned int data)
{
	*(volatile unsigned int *)addr = data;
}
// EXPORT_SYMBOL(osal_io_write);

void osal_io_write_mask(unsigned long addr, unsigned int mask, unsigned int data)
{
	rt_base_t level;
	volatile unsigned int value;

	level = rt_hw_interrupt_disable();
	value = *(volatile unsigned int *)addr;
	value &= ~mask;
	value |= (data & mask);
	*(volatile unsigned int *)addr = value;
	rt_hw_interrupt_enable(level);
}
// EXPORT_SYMBOL(osal_io_write_mask);
