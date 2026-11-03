#include "io.h"
#include <aos/kernel.h>
#include <k_api.h>

static kspinlock_t mask_lock;


unsigned int osal_io_read(unsigned long addr)
{
	return readl((void *)addr);
}

void osal_io_write(unsigned long addr, unsigned int data)
{
	writel(data, (void *)addr);
}

void osal_io_write_mask(unsigned long addr, unsigned int mask, unsigned int data)
{
	unsigned long flags;
	u32 value;

	krhino_spin_lock_irq_save(&mask_lock, flags);
	value = readl_relaxed((void *)addr) & ~mask;
	value |= (data & mask);
	writel_relaxed(value, (void *)addr);
	krhino_spin_unlock_irq_restore(&mask_lock, flags);
}
