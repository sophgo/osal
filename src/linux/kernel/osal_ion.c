#include <linux/dma-buf.h>
#include <linux/version.h>
#include <linux/module.h>
#include <asm/cacheflush.h>
#include <ion/ion.h>
#include <ion/cvitek/cvitek_ion_alloc.h>
#include <asm/cacheflush.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
#include <linux/dma-map-ops.h>
#endif

#include "osal.h"
#include "osal_inner.h"

struct mem_mapping {
	unsigned long phy_addr;
	int size;
	void *vir_addr;
	void *dmabuf;
	pid_t fd_tgid;
};

static int ion_debug;
module_param(ion_debug, int, 0644);

static osal_spinlock ion_lock;
static osal_hash *ion_hash;

static bool ion_range_is_valid(unsigned long addr_p, unsigned int len)
{
	if (addr_p == 0 || len == 0)
		return false;

	/* check for integer overflow */
	if ((addr_p + len) < addr_p)
		return false;

	return true;
}

static int mem_put(struct mem_mapping *mem_info)
{
	struct mem_mapping *p;

	p = osal_kmalloc(sizeof(*p), OSAL_GFP_KERNEL);
	if (!p) {
		osal_log("kmalloc failed\n");
		return -1;
	}
	memcpy(p, mem_info, sizeof(*p));

	osal_spin_lock(&ion_lock);
	osal_hash_add(ion_hash, p->phy_addr, p);
	osal_spin_unlock(&ion_lock);

	return 0;
}

static int _hash_cb_get_mem(long long unsigned int key, void *value, void *context)
{
	struct mem_mapping *obj = (struct mem_mapping *)value;
	struct mem_mapping *mem_info = (struct mem_mapping *)context;

	if (obj->phy_addr == mem_info->phy_addr) {
		memcpy(mem_info, obj, sizeof(*mem_info));
		osal_hash_del(ion_hash, mem_info->phy_addr);
		osal_kfree(obj);
		return OSAL_FAILURE;
	}

	return OSAL_SUCCESS;
}

static int mem_get(unsigned long addr_p, struct mem_mapping *mem_info)
{
	int ret = -1;

	memset(mem_info, 0, sizeof(struct mem_mapping));
	mem_info->phy_addr = addr_p;

	osal_spin_lock(&ion_lock);
	osal_hash_for_each_safe(ion_hash, _hash_cb_get_mem, mem_info);
	osal_spin_unlock(&ion_lock);

	return (mem_info->size == 0) ? ret : 0;
}

static int _hash_cb_mem_dump(long long unsigned int key, void *value, void *context)
{
	struct mem_mapping *obj = (struct mem_mapping *)value;
	int *cnt = (int *)context;

	osal_log("ion addr=0x%lx, ion size=%d\n", obj->phy_addr, obj->size);
	(*cnt)++;

	return OSAL_SUCCESS;
}

static int mem_dump(void)
{
	int cnt = 0;

	osal_spin_lock(&ion_lock);
	osal_hash_for_each(ion_hash, _hash_cb_mem_dump, &cnt);
	osal_spin_unlock(&ion_lock);

	osal_log("ion block total=%d\n", cnt);
	return cnt;
}

static int _osal_ion_alloc(unsigned long *addr_p, void **addr_v, unsigned int len,
	unsigned int is_cached, unsigned char *name)
{
	int dmabuf_fd = 0, ret = 0;
	struct dma_buf *dmabuf;
	struct ion_buffer *ionbuf;
	unsigned char *owner_name = NULL;
	void *vmap_addr = NULL;
	struct mem_mapping mem_info;

	//vpp heap
	dmabuf_fd = cvi_ion_alloc(ION_HEAP_TYPE_CARVEOUT, len, is_cached);
	if (dmabuf_fd < 0) {
		osal_log("bm_ion_alloc len=0x%x failed\n", len);
		return -ENOMEM;
	}

	dmabuf = dma_buf_get(dmabuf_fd);
	if (!dmabuf) {
		osal_log("allocated get dmabuf failed\n");
		return -ENOMEM;
	} else {
		//free fd if get dmabuf success.
		cvi_ion_free(current->tgid, dmabuf_fd);
	}

	ionbuf = (struct ion_buffer *)dmabuf->priv;
	owner_name = vmalloc(MAX_ION_BUFFER_NAME);
	if (!owner_name) {
		osal_log("vmalloc failed\n");
		dma_buf_put(dmabuf);
		return -ENOMEM;
	}
	if (name)
		strncpy(owner_name, name, MAX_ION_BUFFER_NAME);
	else
		strncpy(owner_name, "anonymous", MAX_ION_BUFFER_NAME);

	ionbuf->name = owner_name;

	ret = dma_buf_begin_cpu_access(dmabuf, DMA_TO_DEVICE);
	if (ret < 0) {
		osal_log("dma_buf_begin_cpu_access failed\n");
		dma_buf_put(dmabuf);
		return ret;
	}

	vmap_addr = ionbuf->vaddr;
	if (!vmap_addr || IS_ERR(vmap_addr)) {
		ret = -EINVAL;
		return ret;
	}

	//push into memory manager
	mem_info.dmabuf = (void *)dmabuf;
	mem_info.vir_addr = vmap_addr;
	mem_info.phy_addr = ionbuf->paddr;
	mem_info.size = len;
	mem_info.fd_tgid = current->tgid;
	if (mem_put(&mem_info)) {
		osal_log("allocate mm put failed\n");
		return -ENOMEM;
	}

	if (ion_debug) {
		osal_log("ion alloc: pid=%d name=%s phy_addr=0x%lx size=%d\n",
			mem_info.fd_tgid, ionbuf->name, mem_info.phy_addr, mem_info.size);
	}

	*addr_p = ionbuf->paddr;
	*addr_v = vmap_addr;

	return ret;
}

static int _osal_ion_free(unsigned long addr_p)
{
	struct mem_mapping mem_info;
	struct ion_buffer *ionbuf;
	struct dma_buf *dmabuf;

	//get from memory manager
	if (mem_get(addr_p, &mem_info)) {
		osal_log("mem_info get failed, addr:0x%lx\n", addr_p);
		return -ENOMEM;
	}

	dmabuf = (struct dma_buf *)(mem_info.dmabuf);
	ionbuf = (struct ion_buffer *)dmabuf->priv;

	if (ion_debug) {
		osal_log("ion free: pid=%d name=%s phy_addr=0x%lx size=%d\n",
			mem_info.fd_tgid, ionbuf->name, mem_info.phy_addr, mem_info.size);
	}

	dma_buf_end_cpu_access(dmabuf, DMA_TO_DEVICE);
	dma_buf_put(dmabuf);

	//return free size
	return mem_info.size;
}

int osal_ion_free(unsigned long phy_addr)
{
	if (!phy_addr) {
		osal_log("phy_addr = 0\n");
		return -EINVAL;
	}
	return _osal_ion_free(phy_addr);
}
osal_module_export(osal_ion_free);

int osal_ion_alloc(unsigned long *p_paddr, void **pp_vaddr, unsigned char *buf_name, unsigned int buf_len, bool is_cached)
{
	if (!p_paddr || !pp_vaddr) {
		osal_log("p_paddr or pp_vaddr is null\n");
		return -EINVAL;
	}
	if (buf_len == 0) {
		osal_log("buf_len = 0\n");
		return -EINVAL;
	}
	return _osal_ion_alloc(p_paddr, pp_vaddr, buf_len, is_cached, buf_name);
}
osal_module_export(osal_ion_alloc);

int osal_ion_cache_invalidate(unsigned long addr_p, void *addr_v, unsigned int len)
{
	if (!ion_range_is_valid(addr_p, len)) {
		osal_log("ion_cache_invalidate: invalid addr=0x%lx, len=0x%x\n", addr_p, len);
		return -EINVAL;
	}

#if (KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE) && defined(__riscv)
	arch_sync_dma_for_device(addr_p, len, DMA_FROM_DEVICE);
#else
	__dma_map_area(phys_to_virt(addr_p), len, DMA_FROM_DEVICE);
#endif

	/*	*/
	smp_mb();
	return 0;
}
osal_module_export(osal_ion_cache_invalidate);

int osal_ion_cache_flush(unsigned long addr_p, void *addr_v, unsigned int len)
{
	if (!ion_range_is_valid(addr_p, len)) {
		osal_log("ion_cache_flush: invalid addr=0x%lx, len=0x%x\n", addr_p, len);
		return -EINVAL;
	}

#if (KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE) && defined(__riscv)
	arch_sync_dma_for_device(addr_p, len, DMA_TO_DEVICE);
#else
	__dma_map_area(phys_to_virt(addr_p), len, DMA_TO_DEVICE);
#endif

	/*	*/
	smp_mb();
	return 0;
}
osal_module_export(osal_ion_cache_flush);

int osal_ion_dump(void)
{
	return mem_dump();
}
osal_module_export(osal_ion_dump);

int osal_ion_init(void)
{
	osal_spin_lock_init(&ion_lock);

	ion_hash = osal_hash_create(128);
	if (!ion_hash) {
		osal_log("osal_hash_create fail!\n");
	}

	return 0;
}

int osal_ion_deinit(void)
{
	osal_hash_destroy(ion_hash);
	ion_hash = NULL;
	osal_spin_lock_destroy(&ion_lock);

	return 0;
}
