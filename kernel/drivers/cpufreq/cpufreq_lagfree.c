#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ctype.h>
#include <linux/cpufreq.h>
#include <linux/sysctl.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/sysfs.h>
#include <linux/cpu.h>
#include <linux/kmod.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/kernel_stat.h>
#include <linux/percpu.h>
#include <linux/mutex.h>
#include <linux/earlysuspend.h>

#define DEF_FREQUENCY_UP_THRESHOLD	(50)
#define DEF_FREQUENCY_DOWN_THRESHOLD	(15)
#define FREQ_STEP_DOWN			(160000)
#define FREQ_SLEEP_MAX			(320000)
#define FREQ_AWAKE_MIN			(480000)
#define FREQ_STEP_UP_SLEEP_PERCENT	(20)

static unsigned int def_sampling_rate;
unsigned int suspended = 0;
#define MIN_SAMPLING_RATE_RATIO		(2)
#define MIN_STAT_SAMPLING_RATE		\
	(MIN_SAMPLING_RATE_RATIO * jiffies_to_usecs(CONFIG_CPU_FREQ_MIN_TICKS))
#define MIN_SAMPLING_RATE		\
			(def_sampling_rate / MIN_SAMPLING_RATE_RATIO)
#define MAX_SAMPLING_RATE		(500 * def_sampling_rate)
#define DEF_SAMPLING_DOWN_FACTOR	(4)
#define MAX_SAMPLING_DOWN_FACTOR	(10)
#define TRANSITION_LATENCY_LIMIT	(10 * 1000 * 1000)

static void do_dbs_timer(struct work_struct *work);

struct cpu_dbs_info_s {
	struct cpufreq_policy *cur_policy;
	unsigned int prev_cpu_idle_up;
	unsigned int prev_cpu_idle_down;
	unsigned int enable;
	unsigned int down_skip;
	unsigned int requested_freq;
};
static DEFINE_PER_CPU(struct cpu_dbs_info_s, cpu_dbs_info);

static unsigned int dbs_enable;
static DEFINE_MUTEX (dbs_mutex);
static DECLARE_DELAYED_WORK(dbs_work, do_dbs_timer);

struct dbs_tuners {
	unsigned int sampling_rate;
	unsigned int sampling_down_factor;
	unsigned int up_threshold;
	unsigned int down_threshold;
	unsigned int ignore_nice;
};

static struct dbs_tuners dbs_tuners_ins = {
	.up_threshold = DEF_FREQUENCY_UP_THRESHOLD,
	.down_threshold = DEF_FREQUENCY_DOWN_THRESHOLD,
	.sampling_down_factor = DEF_SAMPLING_DOWN_FACTOR,
	.ignore_nice = 1,
};

static inline unsigned int get_cpu_idle_time(unsigned int cpu)
{
	unsigned int add_nice = 0, ret;

	if (dbs_tuners_ins.ignore_nice)
		add_nice = kstat_cpu(cpu).cpustat.nice;

	ret = kstat_cpu(cpu).cpustat.idle +
		kstat_cpu(cpu).cpustat.iowait +
		add_nice;

	return ret;
}

static int
dbs_cpufreq_notifier(struct notifier_block *nb, unsigned long val,
		     void *data)
{
	struct cpufreq_freqs *freq = data;
	struct cpu_dbs_info_s *this_dbs_info = &per_cpu(cpu_dbs_info,
							freq->cpu);

	if (!this_dbs_info->enable)
		return 0;

	this_dbs_info->requested_freq = freq->new;
	return 0;
}

static struct notifier_block dbs_cpufreq_notifier_block = {
	.notifier_call = dbs_cpufreq_notifier
};

static ssize_t show_sampling_rate_max(struct cpufreq_policy *policy, char *buf)
{
	return sprintf (buf, "%u\n", MAX_SAMPLING_RATE);
}

static ssize_t show_sampling_rate_min(struct cpufreq_policy *policy, char *buf)
{
	return sprintf (buf, "%u\n", MIN_SAMPLING_RATE);
}

#define define_one_ro(_name)				\
static struct freq_attr _name =				\
__ATTR(_name, 0444, show_##_name, NULL)

define_one_ro(sampling_rate_max);
define_one_ro(sampling_rate_min);

#define show_one(file_name, object)					\
static ssize_t show_##file_name						\
(struct cpufreq_policy *unused, char *buf)				\
{									\
	return sprintf(buf, "%u\n", dbs_tuners_ins.object);		\
}
show_one(sampling_rate, sampling_rate);
show_one(sampling_down_factor, sampling_down_factor);
show_one(up_threshold, up_threshold);
show_one(down_threshold, down_threshold);
show_one(ignore_nice_load, ignore_nice);

static ssize_t store_sampling_down_factor(struct cpufreq_policy *unused,
		const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf (buf, "%u", &input);
	if (ret != 1 || input > MAX_SAMPLING_DOWN_FACTOR || input < 1)
		return -EINVAL;

	mutex_lock(&dbs_mutex);
	dbs_tuners_ins.sampling_down_factor = input;
	mutex_unlock(&dbs_mutex);
	return count;
}

static ssize_t store_sampling_rate(struct cpufreq_policy *unused,
		const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf (buf, "%u", &input);
	mutex_lock(&dbs_mutex);
	if (ret != 1 || input > MAX_SAMPLING_RATE || input < MIN_SAMPLING_RATE) {
		mutex_unlock(&dbs_mutex);
		return -EINVAL;
	}

	dbs_tuners_ins.sampling_rate = input;
	mutex_unlock(&dbs_mutex);

	return count;
}

static ssize_t store_up_threshold(struct cpufreq_policy *unused,
		const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf (buf, "%u", &input);

	mutex_lock(&dbs_mutex);
	if (ret != 1 || input > 100 || input <= dbs_tuners_ins.down_threshold) {
		mutex_unlock(&dbs_mutex);
		return -EINVAL;
	}

	dbs_tuners_ins.up_threshold = input;
	mutex_unlock(&dbs_mutex);
	return count;
}

static ssize_t store_down_threshold(struct cpufreq_policy *unused,
		const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf (buf, "%u", &input);
	mutex_lock(&dbs_mutex);
	if (ret != 1 || input > 100 || input >= dbs_tuners_ins.up_threshold) {
		mutex_unlock(&dbs_mutex);
		return -EINVAL;
	}

	dbs_tuners_ins.down_threshold = input;
	mutex_unlock(&dbs_mutex);

	return count;
}

static ssize_t store_ignore_nice_load(struct cpufreq_policy *policy,
		const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	unsigned int j;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	if (input > 1)
		input = 1;

	mutex_lock(&dbs_mutex);
	if (input == dbs_tuners_ins.ignore_nice) { /* nothing to do */
		mutex_unlock(&dbs_mutex);
		return count;
	}
	dbs_tuners_ins.ignore_nice = input;
	for_each_online_cpu(j) {
		struct cpu_dbs_info_s *j_dbs_info;
		j_dbs_info = &per_cpu(cpu_dbs_info, j);
		j_dbs_info->prev_cpu_idle_up = get_cpu_idle_time(j);
		j_dbs_info->prev_cpu_idle_down = j_dbs_info->prev_cpu_idle_up;
	}
	mutex_unlock(&dbs_mutex);

	return count;
}

#define define_one_rw(_name) \
static struct freq_attr _name = \
__ATTR(_name, 0644, show_##_name, store_##_name)

define_one_rw(sampling_rate);
define_one_rw(sampling_down_factor);
define_one_rw(up_threshold);
define_one_rw(down_threshold);
define_one_rw(ignore_nice_load);

static struct attribute * dbs_attributes[] = {
	&sampling_rate_max.attr,
	&sampling_rate_min.attr,
	&sampling_rate.attr,
	&sampling_down_factor.attr,
	&up_threshold.attr,
	&down_threshold.attr,
	&ignore_nice_load.attr,
	NULL
};

static struct attribute_group dbs_attr_group = {
	.attrs = dbs_attributes,
	.name = "lagfree",
};

static void dbs_check_cpu(int cpu)
{
	unsigned int idle_ticks, up_idle_ticks, down_idle_ticks;
	unsigned int tmp_idle_ticks, total_idle_ticks;
	unsigned int freq_target;
	unsigned int freq_down_sampling_rate;
	struct cpu_dbs_info_s *this_dbs_info = &per_cpu(cpu_dbs_info, cpu);
	struct cpufreq_policy *policy;

	if (!this_dbs_info->enable)
		return;

	policy = this_dbs_info->cur_policy;
	idle_ticks = UINT_MAX;
	total_idle_ticks = get_cpu_idle_time(cpu);
	tmp_idle_ticks = total_idle_ticks -
		this_dbs_info->prev_cpu_idle_up;
	this_dbs_info->prev_cpu_idle_up = total_idle_ticks;
	if (tmp_idle_ticks < idle_ticks)
		idle_ticks = tmp_idle_ticks;
	idle_ticks *= 100;
	up_idle_ticks = (100 - dbs_tuners_ins.up_threshold) *
			usecs_to_jiffies(dbs_tuners_ins.sampling_rate);

	if (idle_ticks < up_idle_ticks) {
		this_dbs_info->down_skip = 0;
		this_dbs_info->prev_cpu_idle_down =
			this_dbs_info->prev_cpu_idle_up;
		if (this_dbs_info->requested_freq == policy->max && !suspended)
			return;
		if (suspended)
			freq_target = (FREQ_STEP_UP_SLEEP_PERCENT * policy->max) / 100;
		else
			freq_target = policy->max;
		if (unlikely(freq_target == 0))
			freq_target = 5;

		this_dbs_info->requested_freq += freq_target;
		if (this_dbs_info->requested_freq > policy->max)
			this_dbs_info->requested_freq = policy->max;
		if (suspended && this_dbs_info->requested_freq > FREQ_SLEEP_MAX)
		    this_dbs_info->requested_freq = FREQ_SLEEP_MAX;
		if (!suspended && this_dbs_info->requested_freq < FREQ_AWAKE_MIN)
		    this_dbs_info->requested_freq = FREQ_AWAKE_MIN;

		__cpufreq_driver_target(policy, this_dbs_info->requested_freq,
			CPUFREQ_RELATION_H);
		return;
	}

	this_dbs_info->down_skip++;
	if (this_dbs_info->down_skip < dbs_tuners_ins.sampling_down_factor)
		return;

	total_idle_ticks = this_dbs_info->prev_cpu_idle_up;
	tmp_idle_ticks = total_idle_ticks -
		this_dbs_info->prev_cpu_idle_down;
	this_dbs_info->prev_cpu_idle_down = total_idle_ticks;

	if (tmp_idle_ticks < idle_ticks)
		idle_ticks = tmp_idle_ticks;
	idle_ticks *= 100;
	this_dbs_info->down_skip = 0;
	freq_down_sampling_rate = dbs_tuners_ins.sampling_rate *
		dbs_tuners_ins.sampling_down_factor;
	down_idle_ticks = (100 - dbs_tuners_ins.down_threshold) *
		usecs_to_jiffies(freq_down_sampling_rate);

	if (idle_ticks > down_idle_ticks) {
		if (this_dbs_info->requested_freq == policy->min && suspended)
			return;
		freq_target = FREQ_STEP_DOWN;

		if (unlikely(freq_target == 0))
			freq_target = 5;
		if(freq_target > this_dbs_info->requested_freq)
			this_dbs_info->requested_freq = policy->min;
		else
			this_dbs_info->requested_freq -= freq_target;
		
		if (this_dbs_info->requested_freq < policy->min)
			this_dbs_info->requested_freq = policy->min;
		if (!suspended && this_dbs_info->requested_freq < FREQ_AWAKE_MIN)
		    this_dbs_info->requested_freq = FREQ_AWAKE_MIN;
		if (suspended && this_dbs_info->requested_freq > FREQ_SLEEP_MAX)
		    this_dbs_info->requested_freq = FREQ_SLEEP_MAX;
		__cpufreq_driver_target(policy, this_dbs_info->requested_freq,
				CPUFREQ_RELATION_H);
		return; }}

static void do_dbs_timer(struct work_struct *work)
{
	int i;
	mutex_lock(&dbs_mutex);
	for_each_online_cpu(i)
		dbs_check_cpu(i);
	schedule_delayed_work(&dbs_work,
			usecs_to_jiffies(dbs_tuners_ins.sampling_rate));
	mutex_unlock(&dbs_mutex);
}

static inline void dbs_timer_init(void)
{
	init_timer_deferrable(&dbs_work.timer);
	schedule_delayed_work(&dbs_work,
			usecs_to_jiffies(dbs_tuners_ins.sampling_rate));
	return;
}

static inline void dbs_timer_exit(void)
{
	cancel_delayed_work(&dbs_work);
	return;
}

static int cpufreq_governor_dbs(struct cpufreq_policy *policy,
				   unsigned int event)
{
	unsigned int cpu = policy->cpu;
	struct cpu_dbs_info_s *this_dbs_info;
	unsigned int j;
	int rc;

	this_dbs_info = &per_cpu(cpu_dbs_info, cpu);
	switch (event) {
	case CPUFREQ_GOV_START:
		if ((!cpu_online(cpu)) || (!policy->cur))
			return -EINVAL;
		if (this_dbs_info->enable)
			break;
		mutex_lock(&dbs_mutex);
		rc = sysfs_create_group(&policy->kobj, &dbs_attr_group);
		if (rc) {
			mutex_unlock(&dbs_mutex);
			return rc;
		}

		for_each_cpu(j, policy->cpus) {
			struct cpu_dbs_info_s *j_dbs_info;
			j_dbs_info = &per_cpu(cpu_dbs_info, j);
			j_dbs_info->cur_policy = policy;
			j_dbs_info->prev_cpu_idle_up = get_cpu_idle_time(cpu);
			j_dbs_info->prev_cpu_idle_down
				= j_dbs_info->prev_cpu_idle_up;
		}
		this_dbs_info->enable = 1;
		this_dbs_info->down_skip = 0;
		this_dbs_info->requested_freq = policy->cur;
		dbs_enable++;
		if (dbs_enable == 1) {
			unsigned int latency;
			latency = policy->cpuinfo.transition_latency / 1000;
			if (latency == 0)
				latency = 1;

			def_sampling_rate = 10 * latency *
				CONFIG_CPU_FREQ_SAMPLING_LATENCY_MULTIPLIER;
			if (def_sampling_rate < MIN_STAT_SAMPLING_RATE)
				def_sampling_rate = MIN_STAT_SAMPLING_RATE;

			dbs_tuners_ins.sampling_rate = def_sampling_rate;
			dbs_timer_init();
			cpufreq_register_notifier(
					&dbs_cpufreq_notifier_block,
					CPUFREQ_TRANSITION_NOTIFIER);
		}
		mutex_unlock(&dbs_mutex);
		break;

	case CPUFREQ_GOV_STOP:
		mutex_lock(&dbs_mutex);
		this_dbs_info->enable = 0;
		sysfs_remove_group(&policy->kobj, &dbs_attr_group);
		dbs_enable--;

		if (dbs_enable == 0) {
			dbs_timer_exit();
			cpufreq_unregister_notifier(
					&dbs_cpufreq_notifier_block,
					CPUFREQ_TRANSITION_NOTIFIER);
		}

		mutex_unlock(&dbs_mutex);
		break;

	case CPUFREQ_GOV_LIMITS:
		mutex_lock(&dbs_mutex);
		if (policy->max < this_dbs_info->cur_policy->cur)
			__cpufreq_driver_target(
					this_dbs_info->cur_policy,
					policy->max, CPUFREQ_RELATION_H);
		else if (policy->min > this_dbs_info->cur_policy->cur)
			__cpufreq_driver_target(
					this_dbs_info->cur_policy,
					policy->min, CPUFREQ_RELATION_L);
		mutex_unlock(&dbs_mutex);
		break;
	}
	return 0;
}

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_LAGFREE
static
#endif
struct cpufreq_governor cpufreq_gov_lagfree = {
	.name			= "lagfree",
	.governor		= cpufreq_governor_dbs,
	.max_transition_latency	= TRANSITION_LATENCY_LIMIT,
	.owner			= THIS_MODULE,
};

static void lagfree_early_suspend(struct early_suspend *handler) {
	suspended = 1;
}

static void lagfree_late_resume(struct early_suspend *handler) {
	suspended = 0;
}

static struct early_suspend lagfree_power_suspend = {
	.suspend = lagfree_early_suspend,
	.resume = lagfree_late_resume,
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1,
};

static int __init cpufreq_gov_dbs_init(void)
{
	register_early_suspend(&lagfree_power_suspend);
	return cpufreq_register_governor(&cpufreq_gov_lagfree);
}

static void __exit cpufreq_gov_dbs_exit(void)
{
	flush_scheduled_work();
	unregister_early_suspend(&lagfree_power_suspend);
	cpufreq_unregister_governor(&cpufreq_gov_lagfree);
}


MODULE_AUTHOR ("Emilio López <turl@tuxfamily.org>");
MODULE_DESCRIPTION ("'cpufreq_lagfree' - A dynamic cpufreq governor for "
		"Low Latency Frequency Transition capable processors "
		"optimised for use in a battery environment"
		"Based on conservative by Alexander Clouter");
MODULE_LICENSE ("GPL");
#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_LAGFREE
fs_initcall(cpufreq_gov_dbs_init);
#else
module_init(cpufreq_gov_dbs_init);
#endif
module_exit(cpufreq_gov_dbs_exit);

