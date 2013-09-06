/*
 * ROW (Read Over Write) I/O scheduler.
 *
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/compiler.h>
#include <linux/blktrace_api.h>
#include <linux/jiffies.h>

enum row_queue_prio {
	ROWQ_PRIO_HIGH_READ = 0,
	ROWQ_PRIO_REG_READ,
	ROWQ_PRIO_HIGH_SWRITE,
	ROWQ_PRIO_REG_SWRITE,
	ROWQ_PRIO_REG_WRITE,
	ROWQ_PRIO_LOW_READ,
	ROWQ_PRIO_LOW_SWRITE,
	ROWQ_MAX_PRIO,
};

struct row_queue_params {
	bool idling_enabled;
	int quantum;
	bool is_urgent;
};

static const struct row_queue_params row_queues_def[] = {
	{true, 100, true},	/* ROWQ_PRIO_HIGH_READ */
	{true, 100, true},	/* ROWQ_PRIO_REG_READ */
	{false, 2, false},	/* ROWQ_PRIO_HIGH_SWRITE */
	{false, 1, false},	/* ROWQ_PRIO_REG_SWRITE */
	{false, 1, false},	/* ROWQ_PRIO_REG_WRITE */
	{false, 1, false},	/* ROWQ_PRIO_LOW_READ */
	{false, 1, false}	/* ROWQ_PRIO_LOW_SWRITE */
};

#define ROW_IDLE_TIME_MSEC 5
#define ROW_READ_FREQ_MSEC 20

struct rowq_idling_data {
	ktime_t			last_insert_time;
	bool			begin_idling;
};

struct row_queue {
	struct row_data		*rdata;
	struct list_head	fifo;
	enum row_queue_prio	prio;

	unsigned int		nr_dispatched;
	unsigned int		slice;

	unsigned int		nr_req;
	int			disp_quantum;

	struct rowq_idling_data	idle_data;
};

struct idling_data {
	unsigned long			idle_time;
	u32				freq;

	struct workqueue_struct	*idle_workqueue;
	struct delayed_work		idle_work;
};

struct row_data {
	struct request_queue		*dispatch_queue;

	struct row_queue row_queues[ROWQ_MAX_PRIO];

	enum row_queue_prio		curr_queue;

	struct idling_data		read_idle;
	unsigned int			nr_reqs[2];

	unsigned int			cycle_flags;
};

#define RQ_ROWQ(rq) ((struct row_queue *) ((rq)->elv.priv[0]))

#define row_log(q, fmt, args...)   \
	blk_add_trace_msg(q, "%s():" fmt , __func__, ##args)
#define row_log_rowq(rdata, rowq_id, fmt, args...)		\
	blk_add_trace_msg(rdata->dispatch_queue, "rowq%d " fmt, \
		rowq_id, ##args)

static inline void row_mark_rowq_unserved(struct row_data *rd,
					 enum row_queue_prio qnum)
{
	rd->cycle_flags |= (1 << qnum);
}

static inline void row_clear_rowq_unserved(struct row_data *rd,
					  enum row_queue_prio qnum)
{
	rd->cycle_flags &= ~(1 << qnum);
}

static inline int row_rowq_unserved(struct row_data *rd,
				   enum row_queue_prio qnum)
{
	return rd->cycle_flags & (1 << qnum);
}

static inline void __maybe_unused row_dump_queues_stat(struct row_data *rd)
{
	int i;

	row_log(rd->dispatch_queue, " Queues status:");
	for (i = 0; i < ROWQ_MAX_PRIO; i++)
		row_log(rd->dispatch_queue,
			"queue%d: dispatched= %d, nr_req=%d", i,
			rd->row_queues[i].nr_dispatched,
			rd->row_queues[i].nr_req);
}

static void kick_queue(struct work_struct *work)
{
	struct delayed_work *idle_work = to_delayed_work(work);
	struct idling_data *read_data =
		container_of(idle_work, struct idling_data, idle_work);
	struct row_data *rd =
		container_of(read_data, struct row_data, read_idle);

	row_log_rowq(rd, rd->curr_queue, "Performing delayed work");
	rd->row_queues[rd->curr_queue].idle_data.begin_idling = false;

	if (!(rd->nr_reqs[0] + rd->nr_reqs[1]))
		row_log(rd->dispatch_queue, "No requests in scheduler");
	else {
		spin_lock_irq(rd->dispatch_queue->queue_lock);
		__blk_run_queue(rd->dispatch_queue);
		spin_unlock_irq(rd->dispatch_queue->queue_lock);
	}
}

static inline void row_restart_disp_cycle(struct row_data *rd)
{
	int i;

	for (i = 0; i < ROWQ_MAX_PRIO; i++)
		rd->row_queues[i].nr_dispatched = 0;

	rd->curr_queue = ROWQ_PRIO_HIGH_READ;
	row_log(rd->dispatch_queue, "Restarting cycle");
}

static inline void row_get_next_queue(struct row_data *rd)
{
	rd->curr_queue++;
	if (rd->curr_queue == ROWQ_MAX_PRIO)
		row_restart_disp_cycle(rd);
}

static void row_add_request(struct request_queue *q,
			    struct request *rq)
{
	struct row_data *rd = (struct row_data *)q->elevator->elevator_data;
	struct row_queue *rqueue = RQ_ROWQ(rq);

	list_add_tail(&rq->queuelist, &rqueue->fifo);
	rd->nr_reqs[rq_data_dir(rq)]++;
	rqueue->nr_req++;
	rq_set_fifo_time(rq, jiffies); /* for statistics*/

	if (row_queues_def[rqueue->prio].idling_enabled) {
		if (delayed_work_pending(&rd->read_idle.idle_work))
			(void)cancel_delayed_work(
				&rd->read_idle.idle_work);
		if (ktime_to_ms(ktime_sub(ktime_get(),
				rqueue->idle_data.last_insert_time)) <
				rd->read_idle.freq) {
			rqueue->idle_data.begin_idling = true;
			row_log_rowq(rd, rqueue->prio, "Enable idling");
		} else {
			rqueue->idle_data.begin_idling = false;
			row_log_rowq(rd, rqueue->prio, "Disable idling");
		}

		rqueue->idle_data.last_insert_time = ktime_get();
	}
	if (row_queues_def[rqueue->prio].is_urgent &&
	    row_rowq_unserved(rd, rqueue->prio)) {
		row_log_rowq(rd, rqueue->prio,
			"added urgent request (total on queue=%d)",
			rqueue->nr_req);
	} else
		row_log_rowq(rd, rqueue->prio,
			"added request (total on queue=%d)", rqueue->nr_req);
}

static int row_reinsert_req(struct request_queue *q,
			    struct request *rq)
{
	struct row_data    *rd = q->elevator->elevator_data;
	struct row_queue   *rqueue = RQ_ROWQ(rq);

	if (rqueue->prio >= ROWQ_MAX_PRIO) {
		pr_err("\n\nROW BUG: row_reinsert_req() rqueue->prio = %d\n",
			   rqueue->prio);
		blk_dump_rq_flags(rq, "");
		return -EIO;
	}

	list_add(&rq->queuelist, &rqueue->fifo);
	rd->nr_reqs[rq_data_dir(rq)]++;
	rqueue->nr_req++;

	row_log_rowq(rd, rqueue->prio,
		"request reinserted (total on queue=%d)", rqueue->nr_req);

	return 0;
}

static bool row_urgent_pending(struct request_queue *q)
{
	struct row_data *rd = q->elevator->elevator_data;
	int i;

	for (i = 0; i < ROWQ_MAX_PRIO; i++)
		if (row_queues_def[i].is_urgent && row_rowq_unserved(rd, i) &&
		    !list_empty(&rd->row_queues[i].fifo)) {
			row_log_rowq(rd, i,
				     "Urgent request pending (curr=%i)",
				     rd->curr_queue);
			return true;
		}

	return false;
}

static void row_remove_request(struct request_queue *q,
			       struct request *rq)
{
	struct row_data *rd = (struct row_data *)q->elevator->elevator_data;
	struct row_queue *rqueue = RQ_ROWQ(rq);

	rq_fifo_clear(rq);
	rqueue->nr_req--;
	rd->nr_reqs[rq_data_dir(rq)]--;
}

static void row_dispatch_insert(struct row_data *rd)
{
	struct request *rq;

	rq = rq_entry_fifo(rd->row_queues[rd->curr_queue].fifo.next);
	row_remove_request(rd->dispatch_queue, rq);
	elv_dispatch_add_tail(rd->dispatch_queue, rq);
	rd->row_queues[rd->curr_queue].nr_dispatched++;
	row_clear_rowq_unserved(rd, rd->curr_queue);
	row_log_rowq(rd, rd->curr_queue, " Dispatched request nr_disp = %d",
		     rd->row_queues[rd->curr_queue].nr_dispatched);
}

static int row_choose_queue(struct row_data *rd)
{
	int prev_curr_queue = rd->curr_queue;

	if (!(rd->nr_reqs[0] + rd->nr_reqs[1])) {
		row_log(rd->dispatch_queue, "No more requests in scheduler");
		return 0;
	}

	row_get_next_queue(rd);

	while (list_empty(&rd->row_queues[rd->curr_queue].fifo)
	       && rd->curr_queue != prev_curr_queue) {
		row_mark_rowq_unserved(rd, rd->curr_queue);
		row_get_next_queue(rd);
	}

	return 1;
}

static int row_dispatch_requests(struct request_queue *q, int force)
{
	struct row_data *rd = (struct row_data *)q->elevator->elevator_data;
	int ret = 0, currq, i;

	currq = rd->curr_queue;

	for (i = 0; i < currq; i++) {
		if (row_rowq_unserved(rd, i) &&
		    !list_empty(&rd->row_queues[i].fifo)) {
			row_log_rowq(rd, currq,
				" Preemting for unserved rowq%d. (nr_req=%u)",
				i, rd->row_queues[currq].nr_req);
			rd->curr_queue = i;
			row_dispatch_insert(rd);
			ret = 1;
			goto done;
		}
	}

	if (rd->row_queues[currq].nr_dispatched >=
	    rd->row_queues[currq].disp_quantum) {
		rd->row_queues[currq].nr_dispatched = 0;
		row_log_rowq(rd, currq, "Expiring rqueue");
		ret = row_choose_queue(rd);
		if (ret)
			row_dispatch_insert(rd);
		goto done;
	}

	if (list_empty(&rd->row_queues[currq].fifo)) {
		if (delayed_work_pending(&rd->read_idle.idle_work)) {
			if (force) {
				(void)cancel_delayed_work(
				&rd->read_idle.idle_work);
				row_log_rowq(rd, currq,
					"Canceled delayed work - forced dispatch");
			} else {
				row_log_rowq(rd, currq,
						 "Delayed work pending. Exiting");
				goto done;
			}
		}

		if (!force && row_queues_def[currq].idling_enabled &&
		    rd->row_queues[currq].idle_data.begin_idling) {
			if (!queue_delayed_work(rd->read_idle.idle_workqueue,
						&rd->read_idle.idle_work,
						rd->read_idle.idle_time)) {
				row_log_rowq(rd, currq,
					     "Work already on queue!");
				pr_err("ROW_BUG: Work already on queue!");
			} else
				row_log_rowq(rd, currq,
				     "Scheduled delayed work. exiting");
			goto done;
		} else {
			row_log_rowq(rd, currq,
				     "Currq empty. Choose next queue");
			ret = row_choose_queue(rd);
			if (!ret)
				goto done;
		}
	}

	ret = 1;
	row_dispatch_insert(rd);

done:
	return ret;
}

static void *row_init_queue(struct request_queue *q)
{

	struct row_data *rdata;
	int i;

	rdata = kmalloc_node(sizeof(*rdata),
			     GFP_KERNEL | __GFP_ZERO, q->node);
	if (!rdata)
		return NULL;

	for (i = 0; i < ROWQ_MAX_PRIO; i++) {
		INIT_LIST_HEAD(&rdata->row_queues[i].fifo);
		rdata->row_queues[i].disp_quantum = row_queues_def[i].quantum;
		rdata->row_queues[i].rdata = rdata;
		rdata->row_queues[i].prio = i;
		rdata->row_queues[i].idle_data.begin_idling = false;
		rdata->row_queues[i].idle_data.last_insert_time =
			ktime_set(0, 0);
	}

	rdata->read_idle.idle_time = msecs_to_jiffies(ROW_IDLE_TIME_MSEC);

	if (!rdata->read_idle.idle_time)
		rdata->read_idle.idle_time = 1;
	rdata->read_idle.freq = ROW_READ_FREQ_MSEC;
	rdata->read_idle.idle_workqueue = alloc_workqueue("row_idle_work",
					    WQ_MEM_RECLAIM | WQ_HIGHPRI, 0);
	if (!rdata->read_idle.idle_workqueue)
		panic("Failed to create idle workqueue\n");
	INIT_DELAYED_WORK(&rdata->read_idle.idle_work, kick_queue);

	rdata->curr_queue = ROWQ_PRIO_HIGH_READ;
	rdata->dispatch_queue = q;

	rdata->nr_reqs[READ] = rdata->nr_reqs[WRITE] = 0;

	return rdata;
}

static void row_exit_queue(struct elevator_queue *e)
{
	struct row_data *rd = (struct row_data *)e->elevator_data;
	int i;

	for (i = 0; i < ROWQ_MAX_PRIO; i++)
		BUG_ON(!list_empty(&rd->row_queues[i].fifo));
	(void)cancel_delayed_work_sync(&rd->read_idle.idle_work);
	BUG_ON(delayed_work_pending(&rd->read_idle.idle_work));
	destroy_workqueue(rd->read_idle.idle_workqueue);
	kfree(rd);
}

static void row_merged_requests(struct request_queue *q, struct request *rq,
				 struct request *next)
{
	struct row_queue   *rqueue = RQ_ROWQ(next);

	list_del_init(&next->queuelist);
	rqueue->nr_req--;

	rqueue->rdata->nr_reqs[rq_data_dir(rq)]--;
}

static enum row_queue_prio get_queue_type(struct request *rq)
{
	const int data_dir = rq_data_dir(rq);
	const bool is_sync = rq_is_sync(rq);

	if (data_dir == READ)
		return ROWQ_PRIO_REG_READ;
	else if (is_sync)
		return ROWQ_PRIO_REG_SWRITE;
	else
		return ROWQ_PRIO_REG_WRITE;
}

static int
row_set_request(struct request_queue *q, struct request *rq, gfp_t gfp_mask)
{
	struct row_data *rd = (struct row_data *)q->elevator->elevator_data;
	unsigned long flags;

	spin_lock_irqsave(q->queue_lock, flags);
	rq->elv.priv[0] =
		(void *)(&rd->row_queues[get_queue_type(rq)]);
	spin_unlock_irqrestore(q->queue_lock, flags);

	return 0;
}

static ssize_t row_var_show(int var, char *page)
{
	return snprintf(page, 100, "%d\n", var);
}

static ssize_t row_var_store(int *var, const char *page, size_t count)
{
	int err;
	err = kstrtoul(page, 10, (unsigned long *)var);

	return count;
}

#define SHOW_FUNCTION(__FUNC, __VAR, __CONV)				\
static ssize_t __FUNC(struct elevator_queue *e, char *page)		\
{									\
	struct row_data *rowd = e->elevator_data;			\
	int __data = __VAR;						\
	if (__CONV)							\
		__data = jiffies_to_msecs(__data);			\
	return row_var_show(__data, (page));			\
}
SHOW_FUNCTION(row_hp_read_quantum_show,
	rowd->row_queues[ROWQ_PRIO_HIGH_READ].disp_quantum, 0);
SHOW_FUNCTION(row_rp_read_quantum_show,
	rowd->row_queues[ROWQ_PRIO_REG_READ].disp_quantum, 0);
SHOW_FUNCTION(row_hp_swrite_quantum_show,
	rowd->row_queues[ROWQ_PRIO_HIGH_SWRITE].disp_quantum, 0);
SHOW_FUNCTION(row_rp_swrite_quantum_show,
	rowd->row_queues[ROWQ_PRIO_REG_SWRITE].disp_quantum, 0);
SHOW_FUNCTION(row_rp_write_quantum_show,
	rowd->row_queues[ROWQ_PRIO_REG_WRITE].disp_quantum, 0);
SHOW_FUNCTION(row_lp_read_quantum_show,
	rowd->row_queues[ROWQ_PRIO_LOW_READ].disp_quantum, 0);
SHOW_FUNCTION(row_lp_swrite_quantum_show,
	rowd->row_queues[ROWQ_PRIO_LOW_SWRITE].disp_quantum, 0);
SHOW_FUNCTION(row_read_idle_show, rowd->read_idle.idle_time, 0);
SHOW_FUNCTION(row_read_idle_freq_show, rowd->read_idle.freq, 0);
#undef SHOW_FUNCTION

#define STORE_FUNCTION(__FUNC, __PTR, MIN, MAX, __CONV)			\
static ssize_t __FUNC(struct elevator_queue *e,				\
		const char *page, size_t count)				\
{									\
	struct row_data *rowd = e->elevator_data;			\
	int __data;						\
	int ret = row_var_store(&__data, (page), count);		\
	if (__CONV)							\
		__data = (int)msecs_to_jiffies(__data);			\
	if (__data < (MIN))						\
		__data = (MIN);						\
	else if (__data > (MAX))					\
		__data = (MAX);						\
	*(__PTR) = __data;						\
	return ret;							\
}
STORE_FUNCTION(row_hp_read_quantum_store,
&rowd->row_queues[ROWQ_PRIO_HIGH_READ].disp_quantum, 1, INT_MAX, 0);
STORE_FUNCTION(row_rp_read_quantum_store,
			&rowd->row_queues[ROWQ_PRIO_REG_READ].disp_quantum,
			1, INT_MAX, 0);
STORE_FUNCTION(row_hp_swrite_quantum_store,
			&rowd->row_queues[ROWQ_PRIO_HIGH_SWRITE].disp_quantum,
			1, INT_MAX, 0);
STORE_FUNCTION(row_rp_swrite_quantum_store,
			&rowd->row_queues[ROWQ_PRIO_REG_SWRITE].disp_quantum,
			1, INT_MAX, 0);
STORE_FUNCTION(row_rp_write_quantum_store,
			&rowd->row_queues[ROWQ_PRIO_REG_WRITE].disp_quantum,
			1, INT_MAX, 0);
STORE_FUNCTION(row_lp_read_quantum_store,
			&rowd->row_queues[ROWQ_PRIO_LOW_READ].disp_quantum,
			1, INT_MAX, 0);
STORE_FUNCTION(row_lp_swrite_quantum_store,
			&rowd->row_queues[ROWQ_PRIO_LOW_SWRITE].disp_quantum,
			1, INT_MAX, 1);
STORE_FUNCTION(row_read_idle_store, &rowd->read_idle.idle_time, 1, INT_MAX, 0);
STORE_FUNCTION(row_read_idle_freq_store, &rowd->read_idle.freq, 1, INT_MAX, 0);

#undef STORE_FUNCTION

#define ROW_ATTR(name) \
	__ATTR(name, S_IRUGO|S_IWUSR, row_##name##_show, \
				      row_##name##_store)

static struct elv_fs_entry row_attrs[] = {
	ROW_ATTR(hp_read_quantum),
	ROW_ATTR(rp_read_quantum),
	ROW_ATTR(hp_swrite_quantum),
	ROW_ATTR(rp_swrite_quantum),
	ROW_ATTR(rp_write_quantum),
	ROW_ATTR(lp_read_quantum),
	ROW_ATTR(lp_swrite_quantum),
	ROW_ATTR(read_idle),
	ROW_ATTR(read_idle_freq),
	__ATTR_NULL
};

static struct elevator_type iosched_row = {
	.ops = {
		.elevator_merge_req_fn		= row_merged_requests,
		.elevator_dispatch_fn		= row_dispatch_requests,
		.elevator_add_req_fn		= row_add_request,
		.elevator_reinsert_req_fn	= row_reinsert_req,
		.elevator_is_urgent_fn		= row_urgent_pending,
		.elevator_former_req_fn		= elv_rb_former_request,
		.elevator_latter_req_fn		= elv_rb_latter_request,
		.elevator_set_req_fn		= row_set_request,
		.elevator_init_fn		= row_init_queue,
		.elevator_exit_fn		= row_exit_queue,
	},

	.elevator_attrs = row_attrs,
	.elevator_name = "row",
	.elevator_owner = THIS_MODULE,
};

static int __init row_init(void)
{
	elv_register(&iosched_row);
	return 0;
}

static void __exit row_exit(void)
{
	elv_unregister(&iosched_row);
}

module_init(row_init);
module_exit(row_exit);

MODULE_LICENSE("GPLv2");
MODULE_DESCRIPTION("Read Over Write IO scheduler");
