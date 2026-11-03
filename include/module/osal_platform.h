#ifndef __OSAL_PLATFORM_H__
#define __OSAL_PLATFORM_H__

#include "osal_device.h"   /* for osal_pmops */
#include "osal_clk.h"   /* for osal_pmops */

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/** Memory resource type (matches Linux IORESOURCE_MEM) */
#define OSAL_IORESOURCE_MEM  0x200
/** IRQ resource type (matches Linux IORESOURCE_IRQ) */
#define OSAL_IORESOURCE_IRQ  0x400
/** CLK resource type (only alios) */
#define OSAL_IORESOURCE_CLK  0xFF

/** Compute the size of a resource: (end - start + 1) */
#define osal_resource_size(res) ((res)->end - (res)->start + 1)

#define OSAL_PLATFORM_DEV_NAME_LEN 32

/**
 * @brief OSAL resource descriptor.
 *
 * Layout is compatible with Linux struct resource (first 4 fields match),
 * so casting between osal_resource * and struct resource * is safe.
 * On Linux, osal_platform_get_resource_byname returns a native struct resource *
 * from DTS. On AliOS, it returns an osal_resource * from the driver's resource table.
 */
typedef struct osal_resource_ {
    unsigned long start;    /**< resource start address / IRQ number */
    unsigned long end;      /**< resource end address */
    const char *name;       /**< resource name (for _byname lookup) */
    unsigned int flags;     /**< resource type: OSAL_IORESOURCE_MEM / OSAL_IORESOURCE_IRQ */
} osal_resource;

typedef struct osal_platform_device_ {
    char name[OSAL_PLATFORM_DEV_NAME_LEN];
    void *drvdata;          /* driver private data, set/get via osal_platform_set/get_drvdata */
    void *dev;              /* OS-native device pointer (struct platform_device * on Linux, NULL on AliOS) */
    void *resource;         /* resource pointer (struct resource * on Linux, osal_resource * table on AliOS) */
    unsigned int resource_count; /* number of entries in resource table (AliOS only) */
    int irq;                /* cached IRQ number, or -1 */
} osal_platform_device;

typedef struct osal_platform_driver_ {
    const char *name;
    int (*probe)(osal_platform_device *pdev);
    int (*remove)(osal_platform_device *pdev);
    osal_pmops *pmops;              /* reuse existing osal_pmops from osal_device.h */
    const char *of_match_table;     /* matching a device */
    const osal_resource *resource_table; /**< chip resource table (used on AliOS for _byname lookup) */
    unsigned int resource_count;    /**< number of entries in resource_table */
    void *private_data;             /* OS implementation private data */
} osal_platform_driver;

/**
 * @ingroup osal_platform
 * @brief Register an osal platform driver.
 *
 * @par Description:
 * On Linux, this translates osal_platform_driver to struct platform_driver and registers it.
 * On AliOS, this allocates an osal_platform_device and directly calls the probe function.
 *
 * @param drv [in] Pointer to osal_platform_driver.
 *
 * @return 0 on success, negative on failure.
 *
 * @par Support System:
 * linux alios.
 */
int osal_platform_driver_register(osal_platform_driver *drv);

/**
 * @ingroup osal_platform
 * @brief Unregister an osal platform driver.
 *
 * @par Description:
 * On Linux, this unregisters the translated struct platform_driver and frees context.
 * On AliOS, this calls the remove function and frees the osal_platform_device.
 *
 * @param drv [in] Pointer to osal_platform_driver.
 *
 * @par Support System:
 * linux alios.
 */
void osal_platform_driver_unregister(osal_platform_driver *drv);

/**
 * @ingroup osal_platform
 * @brief Set driver private data for an osal platform device.
 *
 * @param pdev [in] Pointer to osal_platform_device.
 * @param data [in] Driver private data pointer.
 *
 * @par Support System:
 * linux alios.
 */
void osal_platform_set_drvdata(osal_platform_device *pdev, void *data);

/**
 * @ingroup osal_platform
 * @brief Get driver private data from an osal platform device.
 *
 * @param pdev [in] Pointer to osal_platform_device.
 *
 * @return Driver private data pointer, or NULL if pdev is NULL.
 *
 * @par Support System:
 * linux alios.
 */
void *osal_platform_get_drvdata(osal_platform_device *pdev);

/**
 * @ingroup osal_platform
 * @brief Get a resource from an osal platform device.
 *
 * @par Description:
 * On Linux, this calls platform_get_resource on the native device.
 * On AliOS, this returns NULL (no platform resource framework).
 *
 * @param pdev [in] Pointer to osal_platform_device.
 * @param type [in] Resource type (e.g. IORESOURCE_MEM).
 * @param num [in] Resource index.
 *
 * @return Pointer to resource on Linux, NULL on AliOS.
 *
 * @par Support System:
 * linux.
 */
void *osal_platform_get_resource(osal_platform_device *pdev, unsigned int type, unsigned int num);

/**
 * @ingroup osal_platform
 * @brief Get a resource by name from an osal platform device.
 *
 * @par Support System:
 * linux.
 */
void *osal_platform_get_resource_byname(osal_platform_device *pdev, unsigned int type, const char *name);

/**
 * @ingroup osal_platform
 * @brief Get an IRQ number from an osal platform device.
 *
 * @par Support System:
 * linux alios.
 */
int osal_platform_get_irq(osal_platform_device *pdev, unsigned int num);

/**
 * @ingroup osal_platform
 * @brief Get an IRQ number by name from an osal platform device.
 *
 * @par Support System:
 * linux alios.
 */
int osal_platform_get_irq_byname(osal_platform_device *pdev, const char *name);

/**
 * @ingroup osal_platform
 * @brief Get an clk by name from an osal platform device.
 *
 * @par Support System:
 * linux alios.
 */
osal_clk *osal_platform_get_clk_byname(osal_platform_device *pdev, const char *name);

/**
 * @ingroup osal_platform
 * @brief Put a clk obtained via osal_platform_get_clk_byname.
 *
 * @par Description:
 * Releases a clock previously acquired with osal_platform_get_clk_byname,
 * resolving the native device from the osal platform device internally.
 * Callers must NOT use osal_clk_put for such clocks, since it expects
 * an OS-native device pointer rather than an osal_platform_device.
 *
 * @param pdev [in] Pointer to osal_platform_device.
 * @param clk [in] Pointer to osal_clk to release (NULL-safe).
 *
 * @par Support System:
 * linux alios rtt.
 */
void osal_platform_put_clk(osal_platform_device *pdev, osal_clk *clk);

/**
 * @ingroup osal_platform
 * @brief Device-managed ioremap — automatically freed on device removal.
 *
 * @par Description:
 * On Linux, this calls devm_ioremap() on the native platform_device,
 * so the mapping is automatically released when the device is unbound.
 * On AliOS (no MMU), this returns (void *)phys_addr like osal_ioremap,
 * but records the mapping internally for cleanup on driver removal.
 *
 * @param pdev [in] Pointer to osal_platform_device.
 * @param phys_addr [in] Bus/physical address of the memory region.
 * @param size [in] Size of the memory region to map.
 *
 * @return Virtual address on success, NULL on failure.
 *
 * @par Support System:
 * linux alios.
 */
void *osal_devm_ioremap(osal_platform_device *pdev, unsigned long phys_addr, unsigned long size);

/**
 * @ingroup osal_platform
 * @brief Device-managed ioremap resource by name — automatically freed on device removal.
 *
 * @par Description:
 * Convenience wrapper that looks up a MEM resource by name via
 * osal_platform_get_resource_byname, then calls osal_devm_ioremap
 * on the resource's start address and size.
 *
 * @param pdev [in] Pointer to osal_platform_device.
 * @param name [in] Resource name string (e.g. "vi_sys").
 *
 * @return Virtual address on success, NULL on failure.
 *
 * @par Support System:
 * linux alios.
 */
void *osal_devm_ioremap_resource_byname(osal_platform_device *pdev, const char *name);

#if defined(__KERNEL__)
#define osal_module_platform_driver(osal_drv)                                \
    static int __init osal_drv##_init(void)                                  \
    {                                                                        \
        return osal_platform_driver_register(&(osal_drv));                   \
    }                                                                        \
    osal_module_init(osal_drv##_init);                                       \
    static void __exit osal_drv##_exit(void)                                 \
    {                                                                        \
        osal_platform_driver_unregister(&(osal_drv));                        \
    }                                                                        \
    osal_module_exit(osal_drv##_exit);
#else
#define osal_module_platform_driver(osal_drv)                                \
    int osal_drv##_init(void)                                                \
    {                                                                        \
        return osal_platform_driver_register(&(osal_drv));                   \
    }                                                                        \
    osal_module_init(osal_drv##_init);                                       \
    void osal_drv##_exit(void)                                               \
    {                                                                        \
        osal_platform_driver_unregister(&(osal_drv));                        \
    }                                                                        \
    osal_module_exit(osal_drv##_exit);
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif /* __OSAL_PLATFORM_H__ */