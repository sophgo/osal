#ifndef __OSAL_DEF_H__
#define __OSAL_DEF_H__

#if defined(__KERNEL__)
#include <linux/module.h>
#include <linux/kernel.h>
#include <generated/compile.h>
#elif defined(CONFIG_KERNEL_RHINO)
#include "module.h"
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#if defined(__KERNEL__)
#define OSAL_THIS_MODULE THIS_MODULE
#define osal_module_export(_symbol) EXPORT_SYMBOL(_symbol)
#define osal_module_param(name, type, perm) module_param(name, type, perm)
#define osal_module_param_array(name, type, nump, perm) module_param_array(name, type, nump, perm)
#define OSAL_MODULE_PARM_DESC(_parm, desc) MODULE_PARM_DESC(_perm, desc)
#define osal_module_init(x) module_init(x)
#define osal_module_exit(x) module_exit(x)
#define OSAL_MODULE_AUTHOR(_author) MODULE_AUTHOR(_author)
#define OSAL_MODULE_DESCRIPTION(_description) MODULE_DESCRIPTION(_description)
#define OSAL_MODULE_LICENSE(_license) MODULE_LICENSE(_license)
#define OSAL_MODULE_VERSION(_version) MODULE_VERSION(_version)
#define OSAL_UTS_VERSION UTS_VERSION
#elif defined(CONFIG_KERNEL_RHINO)
#define OSAL_THIS_MODULE
#define osal_module_export(_symbol)
#define osal_module_param(name, type, perm)
#define osal_module_param_array(name, type, nump, perm)
#define OSAL_MODULE_PARM_DESC(_parm, desc)
#define osal_module_init(x) ALIOS_MODULE_REGISTER(x)
#define osal_module_exit(x)
#define OSAL_MODULE_AUTHOR(_author)
#define OSAL_MODULE_DESCRIPTION(_description)
#define OSAL_MODULE_LICENSE(_license)
#define OSAL_MODULE_VERSION(_version)
#define OSAL_UTS_VERSION __DATE__ " " __TIME__
#define __init
#else
#define OSAL_THIS_MODULE
#define osal_module_export(_symbol)
#define osal_module_param(name, type, perm)
#define osal_module_param_array(name, type, nump, perm)
#define OSAL_MODULE_PARM_DESC(_parm, desc)
#define osal_module_init(x)
#define osal_module_exit(x)
#define OSAL_MODULE_AUTHOR(_author)
#define OSAL_MODULE_DESCRIPTION(_description)
#define OSAL_MODULE_LICENSE(_license)
#define OSAL_MODULE_VERSION(_version)
#define OSAL_UTS_VERSION __DATE__ " " __TIME__
#define __init
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif // __OSAL_DEF_H__