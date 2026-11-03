#ifndef __OSAL_ION_H__
#define __OSAL_ION_H__

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @ingroup osal_ion
 * @brief Allocate ION memory buffer.
 *
 * @par Description:
 * This API is used to allocate a contiguous memory buffer from the ION memory allocator,
 * and return both its physical address and virtual address.
 *
 * @param p_paddr   [out] Pointer to the physical address of the allocated buffer.
 * @param pp_vaddr  [out] Pointer to the virtual address of the allocated buffer.
 * @param buf_name  [in]  Name of the buffer, used for identification and debugging.
 * @param buf_len   [in]  Size of the buffer to allocate (unit: byte).
 * @param is_cached [in]  Whether the allocated buffer is cacheable. true for cached, false for non-cached.
 *
 * @return OSAL_SUCCESS/OSAL_FAILURE
 *
 * @par Support System:
 * linux alios.
 */
int osal_ion_alloc(unsigned long *p_paddr, void **pp_vaddr, unsigned char *buf_name, unsigned int buf_len, bool is_cached);

/**
 * @ingroup osal_ion
 * @brief Free ION memory buffer.
 *
 * @par Description:
 * This API is used to free an ION memory buffer that was previously allocated by osal_ion_alloc.
 *
 * @attention
 * The phy_addr must be a valid physical address returned by osal_ion_alloc.
 * Do not free the same buffer more than once.
 *
 * @param phy_addr [in] The physical address of the buffer to be freed.
 *
 * @return OSAL_SUCCESS/OSAL_FAILURE
 *
 * @par Support System:
 * linux alios.
 */
int osal_ion_free(unsigned long phy_addr);

/**
 * @ingroup osal_ion
 * @brief Invalidate ION buffer cache.
 *
 * @par Description:
 * This API is used to invalidate the data cache for a specified ION buffer region.
 * Invalidating the cache discards cached data, ensuring that subsequent reads
 * fetch fresh data from the actual memory.
 *
 * @attention
 * The base address will be aligned to CACHE_LINE_SIZE if it's not aligned.
 * This function is typically used before reading data written by a DMA device
 * or another CPU core.
 *
 * @param addr_p [in] Physical address of the buffer region to invalidate.
 * @param addr_v [in] Virtual address of the buffer region to invalidate.
 * @param len    [in] Size of the buffer region to invalidate (unit: byte).
 *
 * @return OSAL_SUCCESS/OSAL_FAILURE
 *
 * @par Support System:
 * linux alios.
 */
int osal_ion_cache_invalidate(unsigned long addr_p, void *addr_v, unsigned int len);

/**
 * @ingroup osal_ion
 * @brief Flush ION buffer cache.
 *
 * @par Description:
 * This API is used to flush (write back) the data cache for a specified ION buffer region.
 * Flushing the cache ensures that any modified cached data is written back
 * to the actual memory, so that DMA devices or other CPU cores can see the updates.
 *
 * @attention
 * The base address will be aligned to CACHE_LINE_SIZE if it's not aligned.
 * This function is typically used after writing data that will be read by a DMA device
 * or another CPU core.
 *
 * @param addr_p [in] Physical address of the buffer region to flush.
 * @param addr_v [in] Virtual address of the buffer region to flush.
 * @param len    [in] Size of the buffer region to flush (unit: byte).
 *
 * @return OSAL_SUCCESS/OSAL_FAILURE
 *
 * @par Support System:
 * linux alios.
 */
int osal_ion_cache_flush(unsigned long addr_p, void *addr_v, unsigned int len);

/**
 * @ingroup osal_ion
 * @brief Dump ION memory allocation information.
 *
 * @par Description:
 * This API is used to dump the current ION memory allocation status for debugging purposes.
 * It prints information about all allocated ION buffers.
 *
 * @return OSAL_SUCCESS/OSAL_FAILURE
 *
 * @par Support System:
 * linux alios.
 */
int osal_ion_dump(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif /* __OSAL_ION_H__ */