#include <aos/kernel.h>
#include <string.h>
#include <soc.h>
#include <csi_core.h>
#include "sched.h"
#include "osal.h"
#include "osal_inner.h"

#define PTHREAD_DEFAULT_PRIORITY   30
#define DEFAULT_TASK_STACK_SIZE 8192

struct task_sync_sem {
	aos_sem_t done_sem;

	int b_exited;
	int b_should_stop;
};

osal_task *osal_kthread_create(osal_kthread_handler handler, void *data, const char *name, unsigned int stack_size)
{
	osal_task *p = NULL;
	int policy, priority, ret;
	struct task_sync_sem *sync_sem = NULL;
	unsigned int stack_size_tmp;

	sync_sem = (struct task_sync_sem *)malloc(sizeof(*sync_sem));
	if (sync_sem == NULL) {
		osal_log("malloc sync_sem error!\n");
		return NULL;
	}
	memset(sync_sem, 0, sizeof(*sync_sem));

	p = (osal_task *)malloc(sizeof(*p));
	if (p == NULL) {
		osal_log("malloc osal_task error!\n");
		free(sync_sem);
		return NULL;
	}
	memset(p, 0, sizeof(*p));

	policy = aos_task_sched_policy_get_default();
	priority = aos_sched_get_priority_max(policy) - PTHREAD_DEFAULT_PRIORITY;
	if (priority < 0)
		priority = 0;

	stack_size_tmp = stack_size ? stack_size : DEFAULT_TASK_STACK_SIZE;
	ret = aos_task_create((aos_task_t *)&(p->task), name,
			      (void *)handler, data, NULL,
			      stack_size_tmp, priority, 1);
	if (ret != 0) {
		osal_log("kthread_create error!\n");
		free(sync_sem);
		free(p);
		return NULL;
	}

	aos_sem_new(&sync_sem->done_sem, 0);
	aos_task_ptcb_set((aos_task_t *)&(p->task), sync_sem);
	p->sync_sem = sync_sem; /* save in osal_task for safe destroy */
	return p;
}

int osal_kthread_set_priority(osal_task *task, unsigned int priority)
{
	uint8_t old_priority = 0;
	int ret = 0;

	if (task == NULL || task->task == NULL) {
		osal_log("task is invalid! caller: %p\n",
			 __builtin_return_address(0));
		return OSAL_FAILURE;
	}
	ret = aos_task_pri_get((aos_task_t *)&(task->task), &old_priority);

	ret |= aos_task_pri_change((aos_task_t *)&(task->task), priority, &old_priority);
	if (ret != 0) {
		return OSAL_FAILURE;
	}
	return OSAL_SUCCESS;
}

int osal_kthread_should_stop(void)
{
	struct task_sync_sem *sync_sem = NULL;
	aos_task_t task = aos_task_self();

	aos_task_ptcb_get(&task, (void **)&sync_sem);
	if (sync_sem && __sync_fetch_and_add(&sync_sem->b_should_stop, 0)) {
		/*
		 * __sync_lock_test_and_set ensures:
		 * 1. Atomic read-modify-write, prevents duplicate b_exited setting
		 * 2. Implied full memory barrier, ensures b_should_stop write
		 *    is visible before b_exited write on other cores
		 *
		 * Returns old value: if 0 -> first setter, proceed with exit flow
		 *                   if 1 -> exit() already called, skip signal
		 */
		if (__sync_lock_test_and_set(&sync_sem->b_exited, 1) == 0) {
			aos_sem_signal(&sync_sem->done_sem);
		}
		return OSAL_FAILURE;
	}
	return OSAL_SUCCESS;
}

void osal_kthread_exit(void)
{
	struct task_sync_sem *sync_sem = NULL;
	aos_task_t task = aos_task_self();

	aos_task_ptcb_get(&task, (void **)&sync_sem);
	if (sync_sem) {
		/*
		 * Atomic test-and-set: if first caller, set b_exited=1 and signal.
		 * If should_stop already called (b_exited already 1), skip signal.
		 */
		if (__sync_lock_test_and_set(&sync_sem->b_exited, 1) == 0) {
			aos_sem_signal(&sync_sem->done_sem);
		}
	}
}

void osal_kthread_destroy(osal_task *task, unsigned int stop_flag)
{
	struct task_sync_sem *sync_sem = NULL;

	if (task == NULL || task->task == NULL) {
		osal_log("parameter invalid! caller: %p\n",
			 __builtin_return_address(0));
		return;
	}

	/*
	 * Read sync_sem directly from the osal_task struct rather than
	 * from the ktask_t ptcb which may already be reclaimed by the kernel.
	 * sync_sem was saved to task->sync_sem at create time;
	 * as long as osal_task hasn't been freed, this pointer is valid.
	 */
	sync_sem = (struct task_sync_sem *)task->sync_sem;
	if (!sync_sem) {
		osal_log("sync_sem is null!\n");
		return;
	}
	if (stop_flag) {
		/*
		 * First read of b_exited: fast path
		 * If thread has already exited, skip the entire handshake
		 */
		if (__sync_fetch_and_add(&sync_sem->b_exited, 0) == 0) {
			__sync_lock_test_and_set(&sync_sem->b_should_stop, 1);

			/*
			 * Double-check (resolves race 1):
			 *
			 * Timeline A (normal):
			 *   destroy: read b_exited(0) -> b_should_stop=1 -> read b_exited(0) -> wait
			 *   thread:  should_stop sees flag -> set b_exited=1 -> signal -> return
			 *   destroy: woken up
			 *
			 * Timeline B (exit race):
			 *   destroy: read b_exited(0) -> ...           -> read b_exited(1) -> skip wait
			 *   thread:                    exit -> set 1 -> signal
			 *   Signal lost but destroy didn't wait, no impact
			 *
			 * Timeline C (exit between two checks):
			 *   destroy: read(0) -> stop=1 -> read(0) -> wait (blocked)
			 *   thread:                                       exit -> set 1 -> signal
			 *   destroy: woken up
			 */
			if (__sync_fetch_and_add(&sync_sem->b_exited, 0) == 0) {
				aos_sem_wait(&sync_sem->done_sem, -1);
			}
			/*
			 * else: exit() did test_and_set between our two checks.
			 *       b_exited is already 1, thread has already signaled (lost),
			 *       but thread is about to return, AliOS will reclaim the task.
			 */
		}
		/*
		 * Regardless of which branch, b_exited must be 1 now.
		 * Thread no longer accesses sync_sem (exit/should_stop are last access points).
		 * Safe to release.
		 */
	}

	aos_sem_free(&sync_sem->done_sem);
	task->sync_sem = NULL;
	task->task = NULL;
	free(task);
	free(sync_sem);
}

void osal_yield(void)
{
    aos_task_yield();
}

void osal_schedule(void)
{
    /* AliOS has no explicit schedule() API; yield the current task
     * to let the scheduler pick the next ready task. */
    aos_task_yield();
}

char *osal_get_current_taskname(void)
{
    return (char *)aos_task_name();
}

int osal_kthread_wakeup_process(osal_task *task)
{
    if (task == NULL) {
        return -1;
    }
    return aos_task_resume((aos_task_t *)&(task->task));
}

unsigned long osal_msleep(unsigned int msecs)
{
    aos_msleep(msecs);
    return 0;
}

void osal_msleep_uninterruptible(unsigned int msecs)
{
    aos_msleep(msecs);
}

static void _10udelay(void)
{
    uint64_t start = csi_clint_get_value();
    uint32_t cnt = (soc_get_coretim_freq() / 1000U / 100U);

    while (1) {
        uint64_t cur = csi_clint_get_value();

        if (start > cur) {
            if ((start - cur) >= cnt) {
                break;
            }
        } else {
            if (cur - start >= cnt) {
                break;
            }
        }
    }
}

void osal_udelay(unsigned int usecs)
{
    usecs /= 10U;

    while (usecs) {
        usecs--;
        _10udelay();
    }
}

static void _500usdelay(void)
{
    uint64_t start = csi_clint_get_value();
    uint64_t cur;
    uint32_t cnt = (soc_get_coretim_freq() / 1000U / 2U);

    while (1) {
        cur = csi_clint_get_value();

        if (start > cur) {
            if ((start - cur) >= cnt) {
                break;
            }
        } else {
            if (cur - start >= cnt) {
                break;
            }
        }
    }
}

void osal_mdelay(unsigned int msecs)
{
    while (msecs) {
        msecs--;
        _500usdelay();
        _500usdelay();
    }
}

void osal_usleep_range(unsigned long min, unsigned long max)
{
    if (min > max) {
        printf("min is greater than max!");
        return;
    }

    unsigned long range = max - min + 1;
    uint64_t time_ms = (osal_get_random_int() % range) + min;

    if (time_ms > 1000) {
        time_ms = time_ms/1000;
    } else {
        time_ms = 1;
    }
    aos_msleep(time_ms);
}