/*FeraLab*/

#include <linux/utsname.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/spinlock.h>
#include <linux/percpu.h>
#include <linux/cryptohash.h>
#include <asm/processor.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>

#define INPUT_POOL_WORDS 128
#define OUTPUT_POOL_WORDS 32
#define SEC_XFER_SIZE 512
#define EXTRACT_SIZE 10


static int random_read_wakeup_thresh = 512;

static int random_write_wakeup_thresh = 1024;

static int trickle_thresh __read_mostly = INPUT_POOL_WORDS * 28;

static DEFINE_PER_CPU(int, trickle_count);

static struct poolinfo {
	int poolwords;
	int tap1, tap2, tap3, tap4, tap5;
} poolinfo_table[] = {
	{ 128,	103,	76,	51,	25,	1 },
	{ 32,	26,	20,	14,	7,	1 },
};

#define POOLBITS	poolwords*32
#define POOLBYTES	poolwords*4

static DECLARE_WAIT_QUEUE_HEAD(random_read_wait);
static DECLARE_WAIT_QUEUE_HEAD(random_write_wait);
static struct fasync_struct *fasync;

#define DEBUG_ENT(fmt, arg...) do {} while (0)

struct entropy_store;
struct entropy_store {
	struct poolinfo *poolinfo;
	__u32 *pool;
	const char *name;
	struct entropy_store *pull;
	int limit;
	spinlock_t lock;
	unsigned add_ptr;
	int entropy_count;
	int input_rotate;
	__u8 last_data[EXTRACT_SIZE];
};

static __u32 input_pool_data[INPUT_POOL_WORDS];
static __u32 blocking_pool_data[OUTPUT_POOL_WORDS];
static __u32 nonblocking_pool_data[OUTPUT_POOL_WORDS];

static struct entropy_store input_pool = {
	.poolinfo = &poolinfo_table[0],
	.name = "input",
	.limit = 1,
	.lock = __SPIN_LOCK_UNLOCKED(&input_pool.lock),
	.pool = input_pool_data
};

static struct entropy_store blocking_pool = {
	.poolinfo = &poolinfo_table[1],
	.name = "blocking",
	.limit = 1,
	.pull = &input_pool,
	.lock = __SPIN_LOCK_UNLOCKED(&blocking_pool.lock),
	.pool = blocking_pool_data
};

static struct entropy_store nonblocking_pool = {
	.poolinfo = &poolinfo_table[1],
	.name = "nonblocking",
	.pull = &input_pool,
	.lock = __SPIN_LOCK_UNLOCKED(&nonblocking_pool.lock),
	.pool = nonblocking_pool_data
};

static void mix_pool_bytes_extract(struct entropy_store *r, const void *in,
				   int nbytes, __u8 out[64])
{
	static __u32 const twist_table[8] = {
		0x00000000, 0x3b6e20c8, 0x76dc4190, 0x4db26158,
		0xedb88320, 0xd6d6a3e8, 0x9b64c2b0, 0xa00ae278 };
	unsigned long i, j, tap1, tap2, tap3, tap4, tap5;
	int input_rotate;
	int wordmask = r->poolinfo->poolwords - 1;
	const char *bytes = in;
	__u32 w;
	unsigned long flags;

	tap1 = r->poolinfo->tap1;
	tap2 = r->poolinfo->tap2;
	tap3 = r->poolinfo->tap3;
	tap4 = r->poolinfo->tap4;
	tap5 = r->poolinfo->tap5;
	spin_lock_irqsave(&r->lock, flags);
	input_rotate = r->input_rotate;
	i = r->add_ptr;

	while (nbytes--) {
		w = rol32(*bytes++, input_rotate & 31);
		i = (i - 1) & wordmask;
		w ^= r->pool[i];
		w ^= r->pool[(i + tap1) & wordmask];
		w ^= r->pool[(i + tap2) & wordmask];
		w ^= r->pool[(i + tap3) & wordmask];
		w ^= r->pool[(i + tap4) & wordmask];
		w ^= r->pool[(i + tap5) & wordmask];
		r->pool[i] = (w >> 3) ^ twist_table[w & 7];
		input_rotate += i ? 7 : 14;
	}

	r->input_rotate = input_rotate;
	r->add_ptr = i;

	if (out)
		for (j = 0; j < 16; j++)
			((__u32 *)out)[j] = r->pool[(i - j) & wordmask];

	spin_unlock_irqrestore(&r->lock, flags);
}

static void mix_pool_bytes(struct entropy_store *r, const void *in, int bytes)
{
       mix_pool_bytes_extract(r, in, bytes, NULL);
}

static void credit_entropy_bits(struct entropy_store *r, int nbits)
{
	unsigned long flags;
	int entropy_count;

	if (!nbits)
		return;

	spin_lock_irqsave(&r->lock, flags);

	DEBUG_ENT("added %d entropy credits to %s\n", nbits, r->name);
	entropy_count = r->entropy_count;
	entropy_count += nbits;
	if (entropy_count < 0) {
		DEBUG_ENT("negative entropy/overflow\n");
		entropy_count = 0;
	} else if (entropy_count > r->poolinfo->POOLBITS)
		entropy_count = r->poolinfo->POOLBITS;
	r->entropy_count = entropy_count;

	if (r == &input_pool && entropy_count >= random_read_wakeup_thresh) {
		wake_up_interruptible(&random_read_wait);
		kill_fasync(&fasync, SIGIO, POLL_IN);
	}
	spin_unlock_irqrestore(&r->lock, flags);
}

struct timer_rand_state {
	cycles_t last_time;
	long last_delta, last_delta2;
	unsigned dont_count_entropy:1;
};

#ifndef CONFIG_SPARSE_IRQ

static struct timer_rand_state *irq_timer_state[NR_IRQS];

static struct timer_rand_state *get_timer_rand_state(unsigned int irq)
{
	return irq_timer_state[irq];
}

static void set_timer_rand_state(unsigned int irq,
				 struct timer_rand_state *state)
{
	irq_timer_state[irq] = state;
}

#else

static struct timer_rand_state *get_timer_rand_state(unsigned int irq)
{
	struct irq_desc *desc;

	desc = irq_to_desc(irq);

	return desc->timer_rand_state;
}

static void set_timer_rand_state(unsigned int irq,
				 struct timer_rand_state *state)
{
	struct irq_desc *desc;

	desc = irq_to_desc(irq);

	desc->timer_rand_state = state;
}
#endif

static struct timer_rand_state input_timer_state;

static void add_timer_randomness(struct timer_rand_state *state, unsigned num)
{
	struct {
		cycles_t cycles;
		long jiffies;
		unsigned num;
	} sample;
	long delta, delta2, delta3;

	preempt_disable();
	if (input_pool.entropy_count > trickle_thresh &&
	    (__get_cpu_var(trickle_count)++ & 0xfff))
		goto out;

	sample.jiffies = jiffies;
	sample.cycles = get_cycles();
	sample.num = num;
	mix_pool_bytes(&input_pool, &sample, sizeof(sample));

	if (!state->dont_count_entropy) {
		delta = sample.jiffies - state->last_time;
		state->last_time = sample.jiffies;

		delta2 = delta - state->last_delta;
		state->last_delta = delta;

		delta3 = delta2 - state->last_delta2;
		state->last_delta2 = delta2;

		if (delta < 0)
			delta = -delta;
		if (delta2 < 0)
			delta2 = -delta2;
		if (delta3 < 0)
			delta3 = -delta3;
		if (delta > delta2)
			delta = delta2;
		if (delta > delta3)
			delta = delta3;

		credit_entropy_bits(&input_pool,
				    min_t(int, fls(delta>>1), 11));
	}
out:
	preempt_enable();
}

void add_input_randomness(unsigned int type, unsigned int code,
				 unsigned int value)
{
	static unsigned char last_value;

	if (value == last_value)
		return;

	DEBUG_ENT("input event\n");
	last_value = value;
	add_timer_randomness(&input_timer_state,
			     (type << 4) ^ code ^ (code >> 4) ^ value);
}
EXPORT_SYMBOL_GPL(add_input_randomness);

void add_interrupt_randomness(int irq)
{
	struct timer_rand_state *state;

	state = get_timer_rand_state(irq);

	if (state == NULL)
		return;

	DEBUG_ENT("irq event %d\n", irq);
	add_timer_randomness(state, 0x100 + irq);
}

#ifdef CONFIG_BLOCK
void add_disk_randomness(struct gendisk *disk)
{
	if (!disk || !disk->random)
		return;
	DEBUG_ENT("disk event %d:%d\n",
		  MAJOR(disk_devt(disk)), MINOR(disk_devt(disk)));

	add_timer_randomness(disk->random, 0x100 + disk_devt(disk));
}
#endif

static ssize_t extract_entropy(struct entropy_store *r, void *buf,
			       size_t nbytes, int min, int rsvd);

static void xfer_secondary_pool(struct entropy_store *r, size_t nbytes)
{
	__u32 tmp[OUTPUT_POOL_WORDS];

	if (r->pull && r->entropy_count < nbytes * 8 &&
	    r->entropy_count < r->poolinfo->POOLBITS) {
		int rsvd = r->limit ? 0 : random_read_wakeup_thresh/4;
		int bytes = nbytes;

		bytes = max_t(int, bytes, random_read_wakeup_thresh / 8);
		bytes = min_t(int, bytes, sizeof(tmp));

		DEBUG_ENT("going to reseed %s with %d bits "
			  "(%d of %d requested)\n",
			  r->name, bytes * 8, nbytes * 8, r->entropy_count);

		bytes = extract_entropy(r->pull, tmp, bytes,
					random_read_wakeup_thresh / 8, rsvd);
		mix_pool_bytes(r, tmp, bytes);
		credit_entropy_bits(r, bytes*8);
	}
}

static size_t account(struct entropy_store *r, size_t nbytes, int min,
		      int reserved)
{
	unsigned long flags;

	spin_lock_irqsave(&r->lock, flags);
	BUG_ON(r->entropy_count > r->poolinfo->POOLBITS);
	DEBUG_ENT("trying to extract %d bits from %s\n",
		  nbytes * 8, r->name);

	if (r->entropy_count / 8 < min + reserved) {
		nbytes = 0;
	} else {
		if (r->limit && nbytes + reserved >= r->entropy_count / 8)
			nbytes = r->entropy_count/8 - reserved;

		if (r->entropy_count / 8 >= nbytes + reserved)
			r->entropy_count -= nbytes*8;
		else
			r->entropy_count = reserved;

		if (r->entropy_count < random_write_wakeup_thresh) {
			wake_up_interruptible(&random_write_wait);
			kill_fasync(&fasync, SIGIO, POLL_OUT);
		}
	}

	DEBUG_ENT("debiting %d entropy credits from %s%s\n",
		  nbytes * 8, r->name, r->limit ? "" : " (unlimited)");

	spin_unlock_irqrestore(&r->lock, flags);

	return nbytes;
}

static void extract_buf(struct entropy_store *r, __u8 *out)
{
	int i;
	__u32 hash[5], workspace[SHA_WORKSPACE_WORDS];
	__u8 extract[64];
	sha_init(hash);

	for (i = 0; i < r->poolinfo->poolwords; i += 16)
		sha_transform(hash, (__u8 *)(r->pool + i), workspace);

	mix_pool_bytes_extract(r, hash, sizeof(hash), extract);
	sha_transform(hash, extract, workspace);
	memset(extract, 0, sizeof(extract));
	memset(workspace, 0, sizeof(workspace));
	hash[0] ^= hash[3];
	hash[1] ^= hash[4];
	hash[2] ^= rol32(hash[2], 16);
	memcpy(out, hash, EXTRACT_SIZE);
	memset(hash, 0, sizeof(hash));
}

static ssize_t extract_entropy(struct entropy_store *r, void *buf,
			       size_t nbytes, int min, int reserved)
{
	ssize_t ret = 0, i;
	__u8 tmp[EXTRACT_SIZE];

	xfer_secondary_pool(r, nbytes);
	nbytes = account(r, nbytes, min, reserved);

	while (nbytes) {
		extract_buf(r, tmp);
		i = min_t(int, nbytes, EXTRACT_SIZE);
		memcpy(buf, tmp, i);
		nbytes -= i;
		buf += i;
		ret += i;
	}

	memset(tmp, 0, sizeof(tmp));
	return ret;
}

static ssize_t extract_entropy_user(struct entropy_store *r, void __user *buf,
				    size_t nbytes)
{
	ssize_t ret = 0, i;
	__u8 tmp[EXTRACT_SIZE];

	xfer_secondary_pool(r, nbytes);
	nbytes = account(r, nbytes, 0, 0);

	while (nbytes) {
		if (need_resched()) {
			if (signal_pending(current)) {
				if (ret == 0)
					ret = -ERESTARTSYS;
				break;
			}
			schedule();
		}

		extract_buf(r, tmp);
		i = min_t(int, nbytes, EXTRACT_SIZE);
		if (copy_to_user(buf, tmp, i)) {
			ret = -EFAULT;
			break;
		}

		nbytes -= i;
		buf += i;
		ret += i;
	}

	memset(tmp, 0, sizeof(tmp));

	return ret;
}

void get_random_bytes(void *buf, int nbytes)
{
	extract_entropy(&nonblocking_pool, buf, nbytes, 0, 0);
}
EXPORT_SYMBOL(get_random_bytes);

static void init_std_data(struct entropy_store *r)
{
	ktime_t now;
	unsigned long flags;
	spin_lock_irqsave(&r->lock, flags);
	r->entropy_count = 0;
	spin_unlock_irqrestore(&r->lock, flags);
	now = ktime_get_real();
	mix_pool_bytes(r, &now, sizeof(now));
	mix_pool_bytes(r, utsname(), sizeof(*(utsname())));
}

static int rand_initialize(void)
{
	init_std_data(&input_pool);
	init_std_data(&blocking_pool);
	init_std_data(&nonblocking_pool);
	return 0;
}
module_init(rand_initialize);

void rand_initialize_irq(int irq)
{
	struct timer_rand_state *state;

	state = get_timer_rand_state(irq);

	if (state)
		return;

	state = kzalloc(sizeof(struct timer_rand_state), GFP_KERNEL);
	if (state)
		set_timer_rand_state(irq, state);
}

#ifdef CONFIG_BLOCK
void rand_initialize_disk(struct gendisk *disk)
{
	struct timer_rand_state *state;
	state = kzalloc(sizeof(struct timer_rand_state), GFP_KERNEL);
	if (state)
		disk->random = state;
}
#endif

static ssize_t
random_read(struct file *file, char __user *buf, size_t nbytes, loff_t *ppos)
{
	return extract_entropy_user(&nonblocking_pool, buf, nbytes);
}

static ssize_t
urandom_read(struct file *file, char __user *buf, size_t nbytes, loff_t *ppos)
{
	return extract_entropy_user(&nonblocking_pool, buf, nbytes);
}

static unsigned int
random_poll(struct file *file, poll_table * wait)
{
	unsigned int mask;

	poll_wait(file, &random_read_wait, wait);
	poll_wait(file, &random_write_wait, wait);
	mask = 0;
	if (input_pool.entropy_count >= random_read_wakeup_thresh)
		mask |= POLLIN | POLLRDNORM;
	if (input_pool.entropy_count < random_write_wakeup_thresh)
		mask |= POLLOUT | POLLWRNORM;
	return mask;
}

static int
write_pool(struct entropy_store *r, const char __user *buffer, size_t count)
{
	size_t bytes;
	__u32 buf[16];
	const char __user *p = buffer;

	while (count > 0) {
		bytes = min(count, sizeof(buf));
		if (copy_from_user(&buf, p, bytes))
			return -EFAULT;

		count -= bytes;
		p += bytes;

		mix_pool_bytes(r, buf, bytes);
		cond_resched();
	}

	return 0;
}

static ssize_t random_write(struct file *file, const char __user *buffer,
			    size_t count, loff_t *ppos)
{
	size_t ret;
	ret = write_pool(&blocking_pool, buffer, count);
	if (ret)
		return ret;
	ret = write_pool(&nonblocking_pool, buffer, count);
	if (ret)
		return ret;
	return (ssize_t)count;
}

static long random_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	int size, ent_count;
	int __user *p = (int __user *)arg;
	int retval;

	switch (cmd) {
	case RNDGETENTCNT:
		if (put_user(input_pool.entropy_count, p))
			return -EFAULT;
		return 0;
	case RNDADDTOENTCNT:
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		if (get_user(ent_count, p))
			return -EFAULT;
		credit_entropy_bits(&input_pool, ent_count);
		return 0;
	case RNDADDENTROPY:
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		if (get_user(ent_count, p++))
			return -EFAULT;
		if (ent_count < 0)
			return -EINVAL;
		if (get_user(size, p++))
			return -EFAULT;
		retval = write_pool(&input_pool, (const char __user *)p,
				    size);
		if (retval < 0)
			return retval;
		credit_entropy_bits(&input_pool, ent_count);
		return 0;
	case RNDZAPENTCNT:
	case RNDCLEARPOOL:
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		rand_initialize();
		return 0;
	default:
		return -EINVAL;
	}
}

static int random_fasync(int fd, struct file *filp, int on)
{
	return fasync_helper(fd, filp, on, &fasync);
}

const struct file_operations random_fops = {
	.read  = random_read,
	.write = random_write,
	.poll  = random_poll,
	.unlocked_ioctl = random_ioctl,
	.fasync = random_fasync,
};

const struct file_operations urandom_fops = {
	.read  = urandom_read,
	.write = random_write,
	.unlocked_ioctl = random_ioctl,
	.fasync = random_fasync,
};

void generate_random_uuid(unsigned char uuid_out[16])
{
	get_random_bytes(uuid_out, 16);
	uuid_out[6] = (uuid_out[6] & 0x0F) | 0x40;
	uuid_out[8] = (uuid_out[8] & 0x3F) | 0x80;
}
EXPORT_SYMBOL(generate_random_uuid);

#ifdef CONFIG_SYSCTL

#include <linux/sysctl.h>

static int min_read_thresh = 8, min_write_thresh;
static int max_read_thresh = INPUT_POOL_WORDS * 32;
static int max_write_thresh = INPUT_POOL_WORDS * 32;
static char sysctl_bootid[16];
static int proc_do_uuid(ctl_table *table, int write, struct file *filp,
			void __user *buffer, size_t *lenp, loff_t *ppos)
{
	ctl_table fake_table;
	unsigned char buf[64], tmp_uuid[16], *uuid;

	uuid = table->data;
	if (!uuid) {
		uuid = tmp_uuid;
		uuid[8] = 0;
	}
	if (uuid[8] == 0)
		generate_random_uuid(uuid);

	sprintf(buf, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-"
		"%02x%02x%02x%02x%02x%02x",
		uuid[0],  uuid[1],  uuid[2],  uuid[3],
		uuid[4],  uuid[5],  uuid[6],  uuid[7],
		uuid[8],  uuid[9],  uuid[10], uuid[11],
		uuid[12], uuid[13], uuid[14], uuid[15]);
	fake_table.data = buf;
	fake_table.maxlen = sizeof(buf);

	return proc_dostring(&fake_table, write, filp, buffer, lenp, ppos);
}

static int uuid_strategy(ctl_table *table,
			 void __user *oldval, size_t __user *oldlenp,
			 void __user *newval, size_t newlen)
{
	unsigned char tmp_uuid[16], *uuid;
	unsigned int len;

	if (!oldval || !oldlenp)
		return 1;

	uuid = table->data;
	if (!uuid) {
		uuid = tmp_uuid;
		uuid[8] = 0;
	}
	if (uuid[8] == 0)
		generate_random_uuid(uuid);

	if (get_user(len, oldlenp))
		return -EFAULT;
	if (len) {
		if (len > 16)
			len = 16;
		if (copy_to_user(oldval, uuid, len) ||
		    put_user(len, oldlenp))
			return -EFAULT;
	}
	return 1;
}

static int sysctl_poolsize = INPUT_POOL_WORDS * 32;
ctl_table random_table[] = {
	{
		.ctl_name 	= RANDOM_POOLSIZE,
		.procname	= "poolsize",
		.data		= &sysctl_poolsize,
		.maxlen		= sizeof(int),
		.mode		= 0444,
		.proc_handler	= &proc_dointvec,
	},
	{
		.ctl_name	= RANDOM_ENTROPY_COUNT,
		.procname	= "entropy_avail",
		.maxlen		= sizeof(int),
		.mode		= 0444,
		.proc_handler	= &proc_dointvec,
		.data		= &input_pool.entropy_count,
	},
	{
		.ctl_name	= RANDOM_READ_THRESH,
		.procname	= "read_wakeup_threshold",
		.data		= &random_read_wakeup_thresh,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= &proc_dointvec_minmax,
		.strategy	= &sysctl_intvec,
		.extra1		= &min_read_thresh,
		.extra2		= &max_read_thresh,
	},
	{
		.ctl_name	= RANDOM_WRITE_THRESH,
		.procname	= "write_wakeup_threshold",
		.data		= &random_write_wakeup_thresh,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= &proc_dointvec_minmax,
		.strategy	= &sysctl_intvec,
		.extra1		= &min_write_thresh,
		.extra2		= &max_write_thresh,
	},
	{
		.ctl_name	= RANDOM_BOOT_ID,
		.procname	= "boot_id",
		.data		= &sysctl_bootid,
		.maxlen		= 16,
		.mode		= 0444,
		.proc_handler	= &proc_do_uuid,
		.strategy	= &uuid_strategy,
	},
	{
		.ctl_name	= RANDOM_UUID,
		.procname	= "uuid",
		.maxlen		= 16,
		.mode		= 0444,
		.proc_handler	= &proc_do_uuid,
		.strategy	= &uuid_strategy,
	},
	{ .ctl_name = 0 }
};
#endif

#define F(x, y, z) ((z) ^ ((x) & ((y) ^ (z))))
#define G(x, y, z) (((x) & (y)) + (((x) ^ (y)) & (z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define ROUND(f, a, b, c, d, x, s)	\
	(a += f(b, c, d) + x, a = (a << s) | (a >> (32 - s)))
#define K1 0
#define K2 013240474631UL
#define K3 015666365641UL

#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)

static __u32 twothirdsMD4Transform(__u32 const buf[4], __u32 const in[12])
{
	__u32 a = buf[0], b = buf[1], c = buf[2], d = buf[3];

	ROUND(F, a, b, c, d, in[ 0] + K1,  3);
	ROUND(F, d, a, b, c, in[ 1] + K1,  7);
	ROUND(F, c, d, a, b, in[ 2] + K1, 11);
	ROUND(F, b, c, d, a, in[ 3] + K1, 19);
	ROUND(F, a, b, c, d, in[ 4] + K1,  3);
	ROUND(F, d, a, b, c, in[ 5] + K1,  7);
	ROUND(F, c, d, a, b, in[ 6] + K1, 11);
	ROUND(F, b, c, d, a, in[ 7] + K1, 19);
	ROUND(F, a, b, c, d, in[ 8] + K1,  3);
	ROUND(F, d, a, b, c, in[ 9] + K1,  7);
	ROUND(F, c, d, a, b, in[10] + K1, 11);
	ROUND(F, b, c, d, a, in[11] + K1, 19);
	ROUND(G, a, b, c, d, in[ 1] + K2,  3);
	ROUND(G, d, a, b, c, in[ 3] + K2,  5);
	ROUND(G, c, d, a, b, in[ 5] + K2,  9);
	ROUND(G, b, c, d, a, in[ 7] + K2, 13);
	ROUND(G, a, b, c, d, in[ 9] + K2,  3);
	ROUND(G, d, a, b, c, in[11] + K2,  5);
	ROUND(G, c, d, a, b, in[ 0] + K2,  9);
	ROUND(G, b, c, d, a, in[ 2] + K2, 13);
	ROUND(G, a, b, c, d, in[ 4] + K2,  3);
	ROUND(G, d, a, b, c, in[ 6] + K2,  5);
	ROUND(G, c, d, a, b, in[ 8] + K2,  9);
	ROUND(G, b, c, d, a, in[10] + K2, 13);
	ROUND(H, a, b, c, d, in[ 3] + K3,  3);
	ROUND(H, d, a, b, c, in[ 7] + K3,  9);
	ROUND(H, c, d, a, b, in[11] + K3, 11);
	ROUND(H, b, c, d, a, in[ 2] + K3, 15);
	ROUND(H, a, b, c, d, in[ 6] + K3,  3);
	ROUND(H, d, a, b, c, in[10] + K3,  9);
	ROUND(H, c, d, a, b, in[ 1] + K3, 11);
	ROUND(H, b, c, d, a, in[ 5] + K3, 15);
	ROUND(H, a, b, c, d, in[ 9] + K3,  3);
	ROUND(H, d, a, b, c, in[ 0] + K3,  9);
	ROUND(H, c, d, a, b, in[ 4] + K3, 11);
	ROUND(H, b, c, d, a, in[ 8] + K3, 15);

	return buf[1] + b;
}
#endif

#undef ROUND
#undef F
#undef G
#undef H
#undef K1
#undef K2
#undef K3

#define REKEY_INTERVAL (300 * HZ)
#define COUNT_BITS 8
#define COUNT_MASK ((1 << COUNT_BITS) - 1)
#define HASH_BITS 24
#define HASH_MASK ((1 << HASH_BITS) - 1)

static struct keydata {
	__u32 count;
	__u32 secret[12];
} ____cacheline_aligned ip_keydata[2];

static unsigned int ip_cnt;
static void rekey_seq_generator(struct work_struct *work);
static DECLARE_DELAYED_WORK(rekey_work, rekey_seq_generator);
static void rekey_seq_generator(struct work_struct *work)
{
	struct keydata *keyptr = &ip_keydata[1 ^ (ip_cnt & 1)];

	get_random_bytes(keyptr->secret, sizeof(keyptr->secret));
	keyptr->count = (ip_cnt & COUNT_MASK) << HASH_BITS;
	smp_wmb();
	ip_cnt++;
	schedule_delayed_work(&rekey_work, REKEY_INTERVAL);
}

static inline struct keydata *get_keyptr(void)
{
	struct keydata *keyptr = &ip_keydata[ip_cnt & 1];

	smp_rmb();

	return keyptr;
}

static __init int seqgen_init(void)
{
	rekey_seq_generator(NULL);
	return 0;
}
late_initcall(seqgen_init);

#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
__u32 secure_tcpv6_sequence_number(__be32 *saddr, __be32 *daddr,
				   __be16 sport, __be16 dport)
{
	__u32 seq;
	__u32 hash[12];
	struct keydata *keyptr = get_keyptr();
	memcpy(hash, saddr, 16);
	hash[4] = ((__force u16)sport << 16) + (__force u16)dport;
	memcpy(&hash[5], keyptr->secret, sizeof(__u32) * 7);
	seq = twothirdsMD4Transform((const __u32 *)daddr, hash) & HASH_MASK;
	seq += keyptr->count;
	seq += ktime_to_ns(ktime_get_real());
	return seq;
}
EXPORT_SYMBOL(secure_tcpv6_sequence_number);
#endif

__u32 secure_ip_id(__be32 daddr)
{
	struct keydata *keyptr;
	__u32 hash[4];

	keyptr = get_keyptr();
	hash[0] = (__force __u32)daddr;
	hash[1] = keyptr->secret[9];
	hash[2] = keyptr->secret[10];
	hash[3] = keyptr->secret[11];

	return half_md4_transform(hash, keyptr->secret);
}

#ifdef CONFIG_INET

__u32 secure_tcp_sequence_number(__be32 saddr, __be32 daddr,
				 __be16 sport, __be16 dport)
{
	__u32 seq;
	__u32 hash[4];
	struct keydata *keyptr = get_keyptr();
	hash[0] = (__force u32)saddr;
	hash[1] = (__force u32)daddr;
	hash[2] = ((__force u16)sport << 16) + (__force u16)dport;
	hash[3] = keyptr->secret[11];
	seq = half_md4_transform(hash, keyptr->secret) & HASH_MASK;
	seq += keyptr->count;
	seq += ktime_to_ns(ktime_get_real()) >> 6;
	return seq;
}

u32 secure_ipv4_port_ephemeral(__be32 saddr, __be32 daddr, __be16 dport)
{
	struct keydata *keyptr = get_keyptr();
	u32 hash[4];
	hash[0] = (__force u32)saddr;
	hash[1] = (__force u32)daddr;
	hash[2] = (__force u32)dport ^ keyptr->secret[10];
	hash[3] = keyptr->secret[11];

	return half_md4_transform(hash, keyptr->secret);
}
EXPORT_SYMBOL_GPL(secure_ipv4_port_ephemeral);

#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
u32 secure_ipv6_port_ephemeral(const __be32 *saddr, const __be32 *daddr,
			       __be16 dport)
{
	struct keydata *keyptr = get_keyptr();
	u32 hash[12];

	memcpy(hash, saddr, 16);
	hash[4] = (__force u32)dport;
	memcpy(&hash[5], keyptr->secret, sizeof(__u32) * 7);

	return twothirdsMD4Transform((const __u32 *)daddr, hash);
}
#endif

#if defined(CONFIG_IP_DCCP) || defined(CONFIG_IP_DCCP_MODULE)
u64 secure_dccp_sequence_number(__be32 saddr, __be32 daddr,
				__be16 sport, __be16 dport)
{
	u64 seq;
	__u32 hash[4];
	struct keydata *keyptr = get_keyptr();

	hash[0] = (__force u32)saddr;
	hash[1] = (__force u32)daddr;
	hash[2] = ((__force u16)sport << 16) + (__force u16)dport;
	hash[3] = keyptr->secret[11];

	seq = half_md4_transform(hash, keyptr->secret);
	seq |= ((u64)keyptr->count) << (32 - HASH_BITS);

	seq += ktime_to_ns(ktime_get_real());
	seq &= (1ull << 48) - 1;

	return seq;
}
EXPORT_SYMBOL(secure_dccp_sequence_number);
#endif
#endif

unsigned int get_random_int(void)
{
	return secure_ip_id((__force __be32)(current->pid + jiffies));
}

unsigned long
randomize_range(unsigned long start, unsigned long end, unsigned long len)
{
	unsigned long range = end - len - start;

	if (end <= start + len)
		return 0;
	return PAGE_ALIGN(get_random_int() % range + start);
}

