#include <aos/kernel.h>
#include <drv/irq.h>
#include <aos/debug.h>
#include <string.h>
#include "osal.h"
#include "osal_inner.h"

#define MAX_IRQ_NAME_LEN 32

#define CONFIG_IRQ_NUM              1023U
#define CONFIG_REAL_IRQ_CNT         105

// csi_dev_t *g_irq_table[CONFIG_IRQ_NUM];
extern csi_dev_t g_irq_dev_table[CONFIG_REAL_IRQ_CNT];

void *osal_irq_get_private_dev(void *param_dev)
{
    if (param_dev == NULL) {
        osal_log("Invalid parameter! caller: %p\n", __builtin_return_address(0));
        return NULL;
    }

    return param_dev;
}

int osal_irq_request(unsigned int irq, osal_irq_handler handler, osal_irq_handler thread_fn,
    const char *name, void *dev)
{
    csi_irq_attach2(irq, handler, &g_irq_dev_table[irq], dev);
    csi_irq_enable(irq);

    return OSAL_SUCCESS;
}

void osal_irq_free(unsigned int irq, void *dev)
{
    if (dev == NULL) {
        osal_log("Invalid parameter! caller: %p\n", __builtin_return_address(0));
        return;
    }

    csi_irq_detach(irq);
    csi_irq_disable(irq);
}

int osal_irq_set_priority(unsigned int irq, unsigned short priority)
{
    csi_irq_priority(irq, priority);
    return OSAL_SUCCESS;
}

void osal_irq_restore(unsigned int irq_status)
{
    csi_irq_restore(irq_status);
}

int osal_in_interrupt(void)
{
    return csi_irq_context();
}

void osal_irq_enable(unsigned int irq)
{
    csi_irq_enable(irq);
}

void osal_irq_disable(unsigned int irq)
{
    csi_irq_disable(irq);
}

typedef struct {
    void (*origin_handler)(unsigned long);
    unsigned long origin_data;
    aos_mutex_t lock;
} tasklet_adapter;

typedef struct {
    aos_timer_t *timer;
    tasklet_adapter *adapter;
} tasklet_internal;

static void aos_tasklet_adapter(void *timer_hdl, void *arg)
{
    tasklet_adapter *adapter = (tasklet_adapter*)arg;
    aos_mutex_lock(&adapter->lock, AOS_WAIT_FOREVER);

    if (adapter->origin_handler) {
        adapter->origin_handler(adapter->origin_data);
    }

    aos_mutex_unlock(&adapter->lock);
}

int osal_tasklet_init(osal_tasklet *tasklet)
{
    if (tasklet == NULL || tasklet->tasklet != NULL) {
        osal_log("init tasklet is NULL ! \n");
        return OSAL_FAILURE;
    }

    tasklet_internal *internal = aos_malloc(sizeof(tasklet_internal));
    if (!internal) return OSAL_FAILURE;

    tasklet_adapter *adapter = aos_malloc(sizeof(tasklet_adapter));
    if (!adapter) {
        aos_free(internal);
        return OSAL_FAILURE;
    }

    adapter->origin_handler = tasklet->handler;
    adapter->origin_data = tasklet->data;
    aos_mutex_new(&adapter->lock);

    aos_timer_t *timer_ptr = aos_malloc(sizeof(aos_timer_t));
    if (!timer_ptr) {
        aos_mutex_free(&adapter->lock);
        aos_free(adapter);
        aos_free(internal);
        return OSAL_FAILURE;
    }

    if (aos_timer_new(timer_ptr, aos_tasklet_adapter, adapter, 1, false) != 0) {
        aos_mutex_free(&adapter->lock);
        aos_free(adapter);
        aos_free(timer_ptr);
        aos_free(internal);
        return OSAL_FAILURE;
    }

    internal->timer = timer_ptr;
    internal->adapter = adapter;

    tasklet->tasklet = internal;
    return OSAL_SUCCESS;
}

int osal_tasklet_update(osal_tasklet *tasklet)
{
    if (!tasklet || !tasklet->tasklet) return OSAL_FAILURE;
    tasklet_internal *internal = (tasklet_internal *)tasklet->tasklet;
    tasklet_adapter *adapter = internal->adapter;

    aos_mutex_lock(&adapter->lock, AOS_WAIT_FOREVER);
    adapter->origin_handler = tasklet->handler;
    adapter->origin_data = tasklet->data;
    aos_mutex_unlock(&adapter->lock);

    return OSAL_SUCCESS;
}

int osal_tasklet_schedule(osal_tasklet *tasklet)
{
    if (!tasklet || !tasklet->tasklet) return OSAL_FAILURE;
    tasklet_internal *internal = (tasklet_internal *)tasklet->tasklet;
    aos_timer_start(internal->timer);
    return OSAL_SUCCESS;
}

int osal_tasklet_kill(osal_tasklet *tasklet)
{
    if (!tasklet || !tasklet->tasklet) return OSAL_FAILURE;
    tasklet_internal *internal = (tasklet_internal *)tasklet->tasklet;
    aos_timer_stop(internal->timer);
    aos_timer_free(internal->timer);
    aos_free(internal->timer);

    aos_mutex_free(&internal->adapter->lock);
    aos_free(internal->adapter);

    aos_free(internal);
    tasklet->tasklet = NULL;
    return OSAL_SUCCESS;
}
