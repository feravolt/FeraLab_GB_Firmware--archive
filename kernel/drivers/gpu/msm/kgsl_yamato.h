#ifndef _KGSL_YAMATO_H
#define _KGSL_YAMATO_H
#include "kgsl_drawctxt.h"

#define INTERVAL_YAMATO_TIMEOUT (HZ / 5)

struct kgsl_yamato_device {
	struct kgsl_device dev;
	struct kgsl_memregion gmemspace;
	unsigned int drawctxt_count;
	struct kgsl_drawctxt *drawctxt_active;
	struct kgsl_drawctxt drawctxt[KGSL_CONTEXT_MAX];
	wait_queue_head_t ib1_wq;
};

irqreturn_t kgsl_yamato_isr(int irq, void *data);
int __init kgsl_yamato_init(struct kgsl_device *device);
int kgsl_yamato_close(struct kgsl_device *device);
int kgsl_yamato_idle(struct kgsl_device *device, unsigned int timeout);
int kgsl_yamato_regread(struct kgsl_device *device, unsigned int offsetwords,
				unsigned int *value);
int kgsl_yamato_regwrite(struct kgsl_device *device, unsigned int offsetwords,
				unsigned int value);
struct kgsl_device *kgsl_get_yamato_generic_device(void);
int kgsl_yamato_getfunctable(struct kgsl_functable *ftbl);
#endif
