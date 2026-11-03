#include <linux/io.h>

static DEFINE_RAW_SPINLOCK(__io_lock);

unsigned int osal_io_read(unsigned long addr)
{
	return readl((void __iomem *)addr);
}
EXPORT_SYMBOL_GPL(osal_io_read);

void osal_io_write(unsigned long addr, unsigned int data)
{
	writel(data, (void __iomem *)addr);
}
EXPORT_SYMBOL_GPL(osal_io_write);

void osal_io_write_mask(unsigned long addr, unsigned int mask, unsigned int data)
{
	unsigned long flags;
	u32 value;

	raw_spin_lock_irqsave(&__io_lock, flags);
	value = readl_relaxed((void __iomem *)addr) & ~mask;
	value |= (data & mask);
	writel_relaxed(value, (void __iomem *)addr);
	raw_spin_unlock_irqrestore(&__io_lock, flags);
}
EXPORT_SYMBOL_GPL(osal_io_write_mask);
