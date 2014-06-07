/* FeraVolt */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/oom.h>
#include <linux/sched.h>
#include <linux/rcupdate.h>
#include <linux/notifier.h>
#include <linux/swap.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#define SEC_ADJUST_LMK

static int lowmem_adj[6] = {
	0,
	1,
	6,
	12,
};
static int lowmem_adj_size = 4;
static int lowmem_minfree[6] = {
	3 * 512,
	2 * 1024,
	4 * 1024,
	16 * 1024,
};
static int lowmem_minfree_size = 4;
static struct task_struct *lowmem_deathpending;
static unsigned long lowmem_deathpending_timeout;
static DEFINE_MUTEX(scan_mutex);
static int fudgeswap = 1024;

static int task_notify_func(struct notifier_block *self, unsigned long val, void *data);
static struct notifier_block task_nb = {
      .notifier_call = task_notify_func,
};

static int task_notify_func(struct notifier_block *self, unsigned long val, void *data)
{
struct task_struct *task = data;

      if (task == lowmem_deathpending)
	lowmem_deathpending = NULL;
      return NOTIFY_OK;
}

static int lowmem_shrink(int nr_to_scan, gfp_t gfp_mask)
{
	struct task_struct *tsk;
	struct task_struct *selected = NULL;
	int rem = 0;
	int tasksize;
	int i;
	int min_adj = OOM_ADJUST_MAX + 1;
	int selected_tasksize = 0;
	int selected_oom_adj;
	int array_size = ARRAY_SIZE(lowmem_adj);
	int other_free;
	int other_file;
	other_free = global_page_state(NR_FREE_PAGES);
	other_file = global_page_state(NR_INACTIVE_FILE) + global_page_state(NR_ACTIVE_FILE);

	if(fudgeswap != 0){
	  struct sysinfo si;
	  si_swapinfo(&si);
	
	if(si.freeswap > 0){
	  if(fudgeswap > si.freeswap)
	    other_file += si.freeswap;
	}}
	
	if (lowmem_deathpending && time_before_eq(jiffies, lowmem_deathpending_timeout))
	  return 0;

	if (lowmem_adj_size < array_size)
		array_size = lowmem_adj_size;
	if (lowmem_minfree_size < array_size)
		array_size = lowmem_minfree_size;
	for (i = 0; i < array_size; i++) {
                if ((other_free + other_file) < lowmem_minfree[i]) {
                        min_adj = lowmem_adj[i];
                        break;
        }}

	rem = global_page_state(NR_ACTIVE_ANON) +
		global_page_state(NR_ACTIVE_FILE) +
		global_page_state(NR_INACTIVE_ANON) +
		global_page_state(NR_INACTIVE_FILE);

	if (nr_to_scan <= 0 || min_adj == OOM_ADJUST_MAX + 1) {
	  if (nr_to_scan > 0)
	    mutex_unlock(&scan_mutex);
	return rem;
	}

	selected_oom_adj = min_adj;
	rcu_read_lock();
	for_each_process(tsk) {
		struct task_struct *p;
		int oom_adj;
		task_lock(p);
		if (!p->mm) {
			task_unlock(p);
			continue;
		}

		oom_adj = p->oomkilladj;
		if (oom_adj < min_adj) {
			task_unlock(p);
			continue;
		}
		tasksize = get_mm_rss(p->mm);
		task_unlock(p);
		if (tasksize <= 0)
			continue;
		if (selected) {
			if (oom_adj < selected_oom_adj)
				continue;
			if (oom_adj == selected_oom_adj &&
			    tasksize <= selected_tasksize)
				continue;
		}
		selected = p;
		selected_tasksize = tasksize;
		selected_oom_adj = oom_adj;
	}
	if (selected) {
		lowmem_deathpending = selected;
		lowmem_deathpending_timeout = jiffies + HZ;
		send_sig(SIGKILL, selected, 0);
		rem -= selected_tasksize;
		msleep_interruptible(20);
	}
	else
	rcu_read_unlock();
	mutex_unlock(&scan_mutex);
	return rem;
}

static struct shrinker lowmem_shrinker = {
	.shrink = lowmem_shrink,
	.seeks = DEFAULT_SEEKS * 16
};

static int __init lowmem_init(void)
{
	task_free_register(&task_nb);
	register_shrinker(&lowmem_shrinker);
	return 0;
}

static void __exit lowmem_exit(void)
{
	unregister_shrinker(&lowmem_shrinker);
	task_free_unregister(&task_nb);
}

module_param_named(cost, lowmem_shrinker.seeks, int, S_IRUGO | S_IWUSR);
module_param_array_named(adj, lowmem_adj, int, &lowmem_adj_size, S_IRUGO | S_IWUSR);
module_param_array_named(minfree, lowmem_minfree, uint, &lowmem_minfree_size, S_IRUGO | S_IWUSR);
module_param_named(fudgeswap, fudgeswap, int, S_IRUGO | S_IWUSR);
module_init(lowmem_init);
module_exit(lowmem_exit);
MODULE_LICENSE("GPL");
