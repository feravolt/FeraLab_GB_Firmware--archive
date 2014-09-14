#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>

enum { ASYNC, SYNC };

static const int sync_read_expire = (HZ / 8) * 3;	/* max time before a sync read is submitted. */
static const int sync_write_expire = (HZ / 8) * 15;	/* max time before a sync write is submitted. */
static const int async_read_expire = HZ * 4;	/* ditto for async, these limits are SOFT! */
static const int async_write_expire = HZ * 16;	/* ditto for async, these limits are SOFT! */
static const int writes_starved = 3;		/* max times reads can starve a write */
static const int fifo_batch     = 1;		/* # of sequential requests treated as one
						   by the above parameters. For throughput. */

struct sio_data {
	struct list_head fifo_list[2][2];
	unsigned int batched;
	unsigned int starved;
	int fifo_expire[2][2];
	int fifo_batch;
	int writes_starved;
};

static void
sio_merged_requests(struct request_queue *q, struct request *rq,
		    struct request *next)
{
	if (!list_empty(&rq->queuelist) && !list_empty(&next->queuelist)) {
		if (time_before(rq_fifo_time(next), rq_fifo_time(rq))) {
			list_move(&rq->queuelist, &next->queuelist);
			rq_set_fifo_time(rq, rq_fifo_time(next));
		}
	}

	rq_fifo_clear(next);
}

static void
sio_add_request(struct request_queue *q, struct request *rq)
{
	struct sio_data *sd = q->elevator->elevator_data;
	const int sync = rq_is_sync(rq);
	const int data_dir = rq_data_dir(rq);
	rq_set_fifo_time(rq, jiffies + sd->fifo_expire[sync][data_dir]);
	list_add_tail(&rq->queuelist, &sd->fifo_list[sync][data_dir]);
}

static int
sio_queue_empty(struct request_queue *q)
{
	struct sio_data *sd = q->elevator->elevator_data;
	return list_empty(&sd->fifo_list[SYNC][READ]) && list_empty(&sd->fifo_list[SYNC][WRITE]) &&
	       list_empty(&sd->fifo_list[ASYNC][READ]) && list_empty(&sd->fifo_list[ASYNC][WRITE]);
}

static struct request *
sio_expired_request(struct sio_data *sd, int sync, int data_dir)
{
	struct list_head *list = &sd->fifo_list[sync][data_dir];
	struct request *rq;

	if (list_empty(list))
		return NULL;
	rq = rq_entry_fifo(list->next);

	if (time_after_eq(jiffies, rq_fifo_time(rq)))
		return rq;

	return NULL;
}

static struct request *
sio_choose_expired_request(struct sio_data *sd)
{
	struct request *rq;
	sd->batched = 0;
	rq = sio_expired_request(sd, ASYNC, WRITE);
	if (rq)
		return rq;
	rq = sio_expired_request(sd, ASYNC, READ);
	if (rq)
		return rq;

	rq = sio_expired_request(sd, SYNC, WRITE);
	if (rq)
		return rq;
	rq = sio_expired_request(sd, SYNC, READ);
	if (rq)
		return rq;

	return NULL;
}

static struct request *
sio_choose_request(struct sio_data *sd, int data_dir)
{
	struct list_head *sync = sd->fifo_list[SYNC];
	struct list_head *async = sd->fifo_list[ASYNC];
	sd->batched++;

	if (!list_empty(&sync[data_dir]))
		return rq_entry_fifo(sync[data_dir].next);
	if (!list_empty(&async[data_dir]))
		return rq_entry_fifo(async[data_dir].next);

	if (!list_empty(&sync[!data_dir]))
		return rq_entry_fifo(sync[!data_dir].next);
	if (!list_empty(&async[!data_dir]))
		return rq_entry_fifo(async[!data_dir].next);

	return NULL;
}

static inline void
sio_dispatch_request(struct sio_data *sd, struct request *rq)
{
	rq_fifo_clear(rq);
	elv_dispatch_add_tail(rq->q, rq);

	if (rq_data_dir(rq)) {
		sd->starved = 0;
	} else {
		if (!list_empty(&sd->fifo_list[SYNC][WRITE]) || 
				!list_empty(&sd->fifo_list[ASYNC][WRITE]))
			sd->starved++;
	}
}

static int
sio_dispatch_requests(struct request_queue *q, int force)
{
	struct sio_data *sd = q->elevator->elevator_data;
	struct request *rq = NULL;
	int data_dir = READ;

	if (sd->batched >= sd->fifo_batch) {
		rq = sio_choose_expired_request(sd);
	}

	if (!rq) {
		if (sd->starved >= sd->writes_starved)
			data_dir = WRITE;

		rq = sio_choose_request(sd, data_dir);
		if (!rq)
			return 0;
	}

	sio_dispatch_request(sd, rq);
	return 1;
}

static struct request *
sio_former_request(struct request_queue *q, struct request *rq)
{
	struct sio_data *sd = q->elevator->elevator_data;
	const int sync = rq_is_sync(rq);
	const int data_dir = rq_data_dir(rq);

	if (rq->queuelist.prev == &sd->fifo_list[sync][data_dir])
		return NULL;

	return list_entry(rq->queuelist.prev, struct request, queuelist);
}

static struct request *
sio_latter_request(struct request_queue *q, struct request *rq)
{
	struct sio_data *sd = q->elevator->elevator_data;
	const int sync = rq_is_sync(rq);
	const int data_dir = rq_data_dir(rq);

	if (rq->queuelist.next == &sd->fifo_list[sync][data_dir])
		return NULL;

	return list_entry(rq->queuelist.next, struct request, queuelist);
}

static void *
sio_init_queue(struct request_queue *q)
{
	struct sio_data *sd;
	sd = kmalloc_node(sizeof(*sd), GFP_KERNEL, q->node);
	if (!sd)
		return NULL;

	INIT_LIST_HEAD(&sd->fifo_list[SYNC][READ]);
	INIT_LIST_HEAD(&sd->fifo_list[SYNC][WRITE]);
	INIT_LIST_HEAD(&sd->fifo_list[ASYNC][READ]);
	INIT_LIST_HEAD(&sd->fifo_list[ASYNC][WRITE]);
	sd->batched = 0;
	sd->fifo_expire[SYNC][READ] = sync_read_expire;
	sd->fifo_expire[SYNC][WRITE] = sync_write_expire;
	sd->fifo_expire[ASYNC][READ] = async_read_expire;
	sd->fifo_expire[ASYNC][WRITE] = async_write_expire;
	sd->fifo_batch = fifo_batch;
	sd->writes_starved = writes_starved;
	return sd;
}

static void
sio_exit_queue(struct elevator_queue *e)
{
	struct sio_data *sd = e->elevator_data;
	BUG_ON(!list_empty(&sd->fifo_list[SYNC][READ]));
	BUG_ON(!list_empty(&sd->fifo_list[SYNC][WRITE]));
	BUG_ON(!list_empty(&sd->fifo_list[ASYNC][READ]));
	BUG_ON(!list_empty(&sd->fifo_list[ASYNC][WRITE]));
	kfree(sd);
}

static ssize_t
sio_var_show(int var, char *page)
{
	return sprintf(page, "%d\n", var);
}

static ssize_t
sio_var_store(int *var, const char *page, size_t count)
{
	char *p = (char *) page;

	*var = simple_strtol(p, &p, 10);
	return count;
}

#define SHOW_FUNCTION(__FUNC, __VAR, __CONV)				\
static ssize_t __FUNC(struct elevator_queue *e, char *page)		\
{									\
	struct sio_data *sd = e->elevator_data;			\
	int __data = __VAR;						\
	if (__CONV)							\
		__data = jiffies_to_msecs(__data);			\
	return sio_var_show(__data, (page));			\
}
SHOW_FUNCTION(sio_sync_read_expire_show, sd->fifo_expire[SYNC][READ], 1);
SHOW_FUNCTION(sio_sync_write_expire_show, sd->fifo_expire[SYNC][WRITE], 1);
SHOW_FUNCTION(sio_async_read_expire_show, sd->fifo_expire[ASYNC][READ], 1);
SHOW_FUNCTION(sio_async_write_expire_show, sd->fifo_expire[ASYNC][WRITE], 1);
SHOW_FUNCTION(sio_fifo_batch_show, sd->fifo_batch, 0);
SHOW_FUNCTION(sio_writes_starved_show, sd->writes_starved, 0);
#undef SHOW_FUNCTION

#define STORE_FUNCTION(__FUNC, __PTR, MIN, MAX, __CONV)			\
static ssize_t __FUNC(struct elevator_queue *e, const char *page, size_t count)	\
{									\
	struct sio_data *sd = e->elevator_data;			\
	int __data;							\
	int ret = sio_var_store(&__data, (page), count);		\
	if (__data < (MIN))						\
		__data = (MIN);						\
	else if (__data > (MAX))					\
		__data = (MAX);						\
	if (__CONV)							\
		*(__PTR) = msecs_to_jiffies(__data);			\
	else								\
		*(__PTR) = __data;					\
	return ret;							\
}
STORE_FUNCTION(sio_sync_read_expire_store, &sd->fifo_expire[SYNC][READ], 0, INT_MAX, 1);
STORE_FUNCTION(sio_sync_write_expire_store, &sd->fifo_expire[SYNC][WRITE], 0, INT_MAX, 1);
STORE_FUNCTION(sio_async_read_expire_store, &sd->fifo_expire[ASYNC][READ], 0, INT_MAX, 1);
STORE_FUNCTION(sio_async_write_expire_store, &sd->fifo_expire[ASYNC][WRITE], 0, INT_MAX, 1);
STORE_FUNCTION(sio_fifo_batch_store, &sd->fifo_batch, 1, INT_MAX, 0);
STORE_FUNCTION(sio_writes_starved_store, &sd->writes_starved, 1, INT_MAX, 0);
#undef STORE_FUNCTION

#define DD_ATTR(name) \
	__ATTR(name, S_IRUGO|S_IWUSR, sio_##name##_show, \
				      sio_##name##_store)

static struct elv_fs_entry sio_attrs[] = {
	DD_ATTR(sync_read_expire),
	DD_ATTR(sync_write_expire),
	DD_ATTR(async_read_expire),
	DD_ATTR(async_write_expire),
	DD_ATTR(fifo_batch),
	DD_ATTR(writes_starved),
	__ATTR_NULL
};

static struct elevator_type iosched_sio = {
	.ops = {
		.elevator_merge_req_fn		= sio_merged_requests,
		.elevator_dispatch_fn		= sio_dispatch_requests,
		.elevator_add_req_fn		= sio_add_request,
		.elevator_queue_empty_fn	= sio_queue_empty,
		.elevator_former_req_fn		= sio_former_request,
		.elevator_latter_req_fn		= sio_latter_request,
		.elevator_init_fn		= sio_init_queue,
		.elevator_exit_fn		= sio_exit_queue,
	},

	.elevator_attrs = sio_attrs,
	.elevator_name = "sio",
	.elevator_owner = THIS_MODULE,
};

static int __init sio_init(void)
{
	elv_register(&iosched_sio);
	return 0;
}

static void __exit sio_exit(void)
{
	elv_unregister(&iosched_sio);
}

module_init(sio_init);
module_exit(sio_exit);

MODULE_AUTHOR("People");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simple IO scheduler");

