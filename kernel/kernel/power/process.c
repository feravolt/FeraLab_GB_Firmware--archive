#undef DEBUG
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/freezer.h>
#include <linux/wakelock.h>
#include <linux/delay.h>

unsigned int __read_mostly freeze_timeout_msecs = 2 * MSEC_PER_SEC;

static inline int freezeable(struct task_struct * p)
{
	if ((p == current) ||
	    (p->flags & PF_NOFREEZE) ||
	    (p->exit_state != 0))
		return 0;
	return 1;
}

static int try_to_freeze_tasks(bool sig_only)
{
	struct task_struct *g, *p;
	unsigned long end_time;
	unsigned int todo;
	struct timeval start, end;
	u64 elapsed_csecs64;
	unsigned int elapsed_csecs;
	unsigned int wakeup = 0;

	do_gettimeofday(&start);
	end_time = jiffies + msecs_to_jiffies(freeze_timeout_msecs);

	while (true) {
		todo = 0;
		read_lock(&tasklist_lock);
		do_each_thread(g, p) {
			if (frozen(p) || !freezeable(p))
				continue;

			if (!freeze_task(p, sig_only))
				continue;

			if (!task_is_stopped_or_traced(p) &&
			    !freezer_should_skip(p))
				todo++;
		} while_each_thread(g, p);
		read_unlock(&tasklist_lock);
		if (todo && has_wake_lock(WAKE_LOCK_SUSPEND)) {
		  wakeup = 1;
		  break;
		}
		if (!todo || time_after(jiffies, end_time))
			break;
	msleep(9);
	}

	do_gettimeofday(&end);
	elapsed_csecs64 = timeval_to_ns(&end) - timeval_to_ns(&start);
	do_div(elapsed_csecs64, NSEC_PER_SEC / 100);
	elapsed_csecs = elapsed_csecs64;

	if (todo) {
		if(wakeup) {
			printk("\n");
			printk(KERN_ERR "Freezing of %s aborted\n",
					sig_only ? "user space " : "tasks ");
		}
		else {
			printk("\n");
			printk(KERN_ERR "Freezing of tasks failed after %d.%02d seconds "
					"(%d tasks refusing to freeze):\n",
					elapsed_csecs / 100, elapsed_csecs % 100, todo);
			show_state();
		}
		if (!wakeup) {
			read_lock(&tasklist_lock);
			do_each_thread(g, p) {
			task_lock(p);
			if (p != current && !freezer_should_skip(p)
				&& freezing(p) && !frozen(p) &&
                                elapsed_csecs > 100)
	                sched_show_task(p);
			cancel_freezing(p);
			task_unlock(p);
		} while_each_thread(g, p);
	        read_unlock(&tasklist_lock);
        }
	} else {
		printk("(elapsed %d.%02d seconds) ", elapsed_csecs / 100,
			elapsed_csecs % 100);
	}

	return todo ? -EBUSY : 0;
}

int freeze_processes(void)
{
	int error;

	printk("Freezing user space processes ... ");
	error = try_to_freeze_tasks(true);
	if (error)
		goto Exit;
	printk("done.\n");

	printk("Freezing remaining freezable tasks ... ");
	error = try_to_freeze_tasks(false);
	if (error)
		goto Exit;
	printk("done.");
 Exit:
	BUG_ON(in_atomic());
	printk("\n");
	return error;
}

static void thaw_tasks(bool nosig_only)
{
	struct task_struct *g, *p;

	read_lock(&tasklist_lock);
	do_each_thread(g, p) {
		if (!freezeable(p))
			continue;

		if (nosig_only && should_send_signal(p))
			continue;

		if (cgroup_frozen(p))
			continue;

		thaw_process(p);
	} while_each_thread(g, p);
	read_unlock(&tasklist_lock);
}

void thaw_processes(void)
{
	printk("Restarting tasks ... ");
	thaw_tasks(true);
	thaw_tasks(false);
	schedule();
	printk("done.\n");
}

