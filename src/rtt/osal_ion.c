#include <rtthread.h>
#include <rthw.h>
#include "osal.h"
#include "osal_inner.h"

/* RT-Thread has no ION allocator. On this platform (no MMU, flat memory)
 * physical and virtual addresses are identical, so rt_malloc'ed memory
 * serves as the contiguous buffer. */

int osal_ion_alloc(unsigned long *p_paddr, void **pp_vaddr, unsigned char *buf_name, unsigned int buf_len, bool is_cached)
{
	void *p;

	RT_UNUSED(buf_name);
	RT_UNUSED(is_cached);

	if (p_paddr == NULL || pp_vaddr == NULL || buf_len == 0) {
		osal_log("parameter invalid!\n");
		return OSAL_FAILURE;
	}

	p = rt_malloc(buf_len);
	if (p == NULL) {
		osal_log("rt_malloc %u bytes failed!\n", buf_len);
		return OSAL_FAILURE;
	}

	*p_paddr = (unsigned long)p;
	*pp_vaddr = p;
	return OSAL_SUCCESS;
}
// EXPORT_SYMBOL(osal_ion_alloc);

int osal_ion_free(unsigned long phy_addr)
{
	if (phy_addr == 0) {
		osal_log("phy_addr is 0!\n");
		return OSAL_FAILURE;
	}

	rt_free((void *)phy_addr);
	return OSAL_SUCCESS;
}
// EXPORT_SYMBOL(osal_ion_free);

int osal_ion_cache_invalidate(unsigned long addr_p, void *addr_v, unsigned int len)
{
	RT_UNUSED(addr_p);

	if (addr_v == NULL || len == 0) {
		return OSAL_FAILURE;
	}

	rt_hw_cpu_dcache_ops(RT_HW_CACHE_INVALIDATE, addr_v, len);
	return OSAL_SUCCESS;
}
// EXPORT_SYMBOL(osal_ion_cache_invalidate);

int osal_ion_cache_flush(unsigned long addr_p, void *addr_v, unsigned int len)
{
	RT_UNUSED(addr_p);

	if (addr_v == NULL || len == 0) {
		return OSAL_FAILURE;
	}

	rt_hw_cpu_dcache_ops(RT_HW_CACHE_FLUSH, addr_v, len);
	return OSAL_SUCCESS;
}
// EXPORT_SYMBOL(osal_ion_cache_flush);

int osal_ion_dump(void)
{
	/* No ION heap to dump on RT-Thread. */
	return OSAL_SUCCESS;
}
// EXPORT_SYMBOL(osal_ion_dump);
