#include "aos/kernel.h"
#include "asm/cache.h"
#include "osal.h"


int osal_ion_free(unsigned long phy_addr)
{
	aos_ion_free((void *)phy_addr);

	return 0;
}

int osal_ion_init(void)
{
	return 0;
}

int osal_ion_deinit(void)
{
	return 0;
}

int osal_ion_alloc(unsigned long *p_paddr, void **pp_vaddr, unsigned char *buf_name, unsigned int buf_len, bool is_cached)
{
	void *p;

	(void)buf_name;
	(void)is_cached;

	if (p_paddr == NULL || pp_vaddr == NULL || buf_len == 0)
		return -1;

	p = aos_ion_malloc(buf_len);

	if (p) {
		*p_paddr = (unsigned long)p;
		*pp_vaddr = p;
		return 0;
	}

	return -1;
}

int osal_ion_cache_invalidate(unsigned long addr_p, void *addr_v, unsigned int len)
{
	inv_dcache_range(addr_p, len);
	return 0;
}

int osal_ion_cache_flush(unsigned long addr_p, void *addr_v, unsigned int len)
{
	flush_dcache_range(addr_p, len);
	return 0;
}

int osal_ion_dump(void)
{
	return 0;
}
