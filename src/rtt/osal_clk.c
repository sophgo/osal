#include <rtthread.h>
#include <string.h>
#include "osal.h"
#include "osal_inner.h"
#include "osal_list.h"

#undef BIT
#define BIT(nr)      ((1ULL) << (nr))

struct clk_item {
	osal_clk_ctrl_info ctrl;
	int prepare_cnt;
	int enable_cnt;
	struct rt_spinlock lock;
	struct osal_list_head list;
};

static OSAL_LIST_HEAD(g_clk_list);
static struct rt_spinlock g_clk_spinlock;

static void clk_reg_set_bits(unsigned long reg_addr, unsigned int bits)
{
	volatile unsigned int value = *(volatile unsigned int *)reg_addr;

	value |= bits;
	*(volatile unsigned int *)reg_addr = value;
}

static void clk_reg_clr_bits(unsigned long reg_addr, unsigned int bits)
{
	volatile unsigned int value = *(volatile unsigned int *)reg_addr;

	value &= ~bits;
	*(volatile unsigned int *)reg_addr = value;
}

static unsigned int clk_reg_read(unsigned long reg_addr)
{
	return *(volatile unsigned int *)reg_addr;
}

int osal_clk_register(osal_clk_ctrl_info *info)
{
	struct clk_item *item;
	struct clk_item *obj;
	rt_base_t level;

	if (info == NULL || info->bit >= 32) {
		osal_log("bit error.\n");
		return OSAL_FAILURE;
	}
	if (!info->name[0]) {
		osal_log("name error.\n");
		return OSAL_FAILURE;
	}

	level = rt_spin_lock_irqsave(&g_clk_spinlock);
	osal_list_for_each_entry(obj, &g_clk_list, list) {
		if (!strncmp(obj->ctrl.name, info->name, 32)) {
			rt_spin_unlock_irqrestore(&g_clk_spinlock, level);
			return OSAL_SUCCESS;
		}
	}
	rt_spin_unlock_irqrestore(&g_clk_spinlock, level);

	item = (struct clk_item *)rt_malloc(sizeof(struct clk_item));
	if (item == NULL) {
		osal_log("rt_malloc failed.\n");
		return OSAL_FAILURE;
	}
	item->ctrl = *info;
	item->prepare_cnt = 0;
	item->enable_cnt = 0;
	rt_spin_lock_init(&item->lock);

	level = rt_spin_lock_irqsave(&g_clk_spinlock);
	osal_list_add_tail(&item->list, &g_clk_list);
	rt_spin_unlock_irqrestore(&g_clk_spinlock, level);

	return OSAL_SUCCESS;
}
// EXPORT_SYMBOL(osal_clk_register);

void osal_clk_unregister(osal_clk_ctrl_info *info)
{
	struct clk_item *item = NULL;
	int is_find = 0;
	rt_base_t level;

	if (info == NULL) {
		return;
	}

	level = rt_spin_lock_irqsave(&g_clk_spinlock);
	osal_list_for_each_entry(item, &g_clk_list, list) {
		if (!strncmp(item->ctrl.name, info->name, 32)) {
			osal_list_del_init(&item->list);
			is_find = 1;
			break;
		}
	}
	rt_spin_unlock_irqrestore(&g_clk_spinlock, level);

	if (is_find)
		rt_free(item);
}
// EXPORT_SYMBOL(osal_clk_unregister);

osal_clk *osal_clk_get(void *device, const char *name)
{
	struct clk_item *item = NULL;
	int is_find = 0;
	osal_clk *clk;
	rt_base_t level;

	RT_UNUSED(device);

	if (name == NULL) {
		return NULL;
	}

	level = rt_spin_lock_irqsave(&g_clk_spinlock);
	osal_list_for_each_entry(item, &g_clk_list, list) {
		if (!strncmp(item->ctrl.name, name, 32)) {
			is_find = 1;
			break;
		}
	}
	rt_spin_unlock_irqrestore(&g_clk_spinlock, level);

	if (!is_find)
		return NULL;

	clk = (osal_clk *)rt_malloc(sizeof(osal_clk));
	if (clk == NULL) {
		osal_log("rt_malloc error!\n");
		return NULL;
	}
	clk->clk = item;

	return clk;
}
// EXPORT_SYMBOL(osal_clk_get);

void osal_clk_put(void *device, osal_clk *clk)
{
	RT_UNUSED(device);

	if (clk == NULL || clk->clk == NULL) {
		osal_log("parameter invalid!\n");
		return;
	}

	clk->clk = NULL;
	rt_free(clk);
}
// EXPORT_SYMBOL(osal_clk_put);

void osal_clk_put_unregister(void *device, osal_clk *clk)
{
	struct clk_item *item = NULL;
	rt_base_t level;

	RT_UNUSED(device);

	if (clk == NULL || clk->clk == NULL) {
		osal_log("parameter invalid!\n");
		return;
	}
	item = (struct clk_item *)clk->clk;

	level = rt_spin_lock_irqsave(&g_clk_spinlock);
	osal_list_del_init(&item->list);
	rt_spin_unlock_irqrestore(&g_clk_spinlock, level);

	clk->clk = NULL;
	rt_free(clk);
	rt_free(item);
}
// EXPORT_SYMBOL(osal_clk_put_unregister);

void osal_clk_prepare(osal_clk *clk)
{
	struct clk_item *item = NULL;

	if (clk == NULL || clk->clk == NULL) {
		osal_log("parameter invalid!\n");
		return;
	}
	item = (struct clk_item *)clk->clk;
	/* No separate prepare step on RT-Thread; keep the ref count for
	 * symmetry with enable/disable. */
	item->prepare_cnt++;
}
// EXPORT_SYMBOL(osal_clk_prepare);

void osal_clk_unprepare(osal_clk *clk)
{
	struct clk_item *item = NULL;

	if (clk == NULL || clk->clk == NULL) {
		osal_log("parameter invalid!\n");
		return;
	}
	item = (struct clk_item *)clk->clk;

	if (item->prepare_cnt == 0)
		return;

	item->prepare_cnt--;
}
// EXPORT_SYMBOL(osal_clk_unprepare);

void osal_clk_enable(osal_clk *clk)
{
	struct clk_item *item = NULL;
	rt_base_t level;

	if (clk == NULL || clk->clk == NULL) {
		osal_log("parameter invalid!\n");
		return;
	}
	item = (struct clk_item *)clk->clk;

	if (item->prepare_cnt == 0) {
		osal_log("clk:%s unprepare!\n", item->ctrl.name);
	}

	level = rt_spin_lock_irqsave(&item->lock);
	if (!(clk_reg_read(item->ctrl.reg_addr) & BIT(item->ctrl.bit))) {
		clk_reg_set_bits(item->ctrl.reg_addr, BIT(item->ctrl.bit));
	}
	item->enable_cnt++;
	rt_spin_unlock_irqrestore(&item->lock, level);
}
// EXPORT_SYMBOL(osal_clk_enable);

void osal_clk_disable(osal_clk *clk)
{
	struct clk_item *item = NULL;
	rt_base_t level;

	if (clk == NULL || clk->clk == NULL) {
		osal_log("parameter invalid!\n");
		return;
	}
	item = (struct clk_item *)clk->clk;

	if (item->prepare_cnt == 0) {
		osal_log("clk:%s unprepare!\n", item->ctrl.name);
	}

	level = rt_spin_lock_irqsave(&item->lock);
	if (item->enable_cnt == 0) {
		rt_spin_unlock_irqrestore(&item->lock, level);
		return;
	} else if (item->enable_cnt == 1) {
		clk_reg_clr_bits(item->ctrl.reg_addr, BIT(item->ctrl.bit));
	}
	item->enable_cnt--;
	rt_spin_unlock_irqrestore(&item->lock, level);
}
// EXPORT_SYMBOL(osal_clk_disable);

void osal_clk_prepare_enable(osal_clk *clk)
{
	struct clk_item *item = NULL;
	rt_base_t level;

	if (clk == NULL || clk->clk == NULL) {
		osal_log("parameter invalid!\n");
		return;
	}
	item = (struct clk_item *)clk->clk;

	level = rt_spin_lock_irqsave(&item->lock);
	item->prepare_cnt++;
	item->enable_cnt++;
	if (!(clk_reg_read(item->ctrl.reg_addr) & BIT(item->ctrl.bit))) {
		clk_reg_set_bits(item->ctrl.reg_addr, BIT(item->ctrl.bit));
	}
	rt_spin_unlock_irqrestore(&item->lock, level);
}
// EXPORT_SYMBOL(osal_clk_prepare_enable);

void osal_clk_disable_unprepare(osal_clk *clk)
{
	struct clk_item *item = NULL;
	rt_base_t level;

	if (clk == NULL || clk->clk == NULL) {
		osal_log("parameter invalid!\n");
		return;
	}
	item = (struct clk_item *)clk->clk;

	level = rt_spin_lock_irqsave(&item->lock);
	if (item->enable_cnt == 1) {
		clk_reg_clr_bits(item->ctrl.reg_addr, BIT(item->ctrl.bit));
	}
	if (item->enable_cnt > 0)
		item->enable_cnt--;
	if (item->prepare_cnt > 0)
		item->prepare_cnt--;
	rt_spin_unlock_irqrestore(&item->lock, level);
}
// EXPORT_SYMBOL(osal_clk_disable_unprepare);

bool osal_clk_is_enabled(osal_clk *clk)
{
	struct clk_item *item = NULL;
	bool ret = false;

	if (clk == NULL || clk->clk == NULL) {
		osal_log("parameter invalid!\n");
		return false;
	}
	item = (struct clk_item *)clk->clk;

	if (item->enable_cnt > 0) {
		ret = true;
	} else {
		if (clk_reg_read(item->ctrl.reg_addr) & BIT(item->ctrl.bit))
			ret = true;
	}

	return ret;
}
// EXPORT_SYMBOL(osal_clk_is_enabled);
