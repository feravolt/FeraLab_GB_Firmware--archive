#ifndef _KGSL_YAMATO_H
#define _KGSL_YAMATO_H

#include "kgsl_drawctxt.h"
#include "kgsl_ringbuffer.h"

struct kgsl_yamato_device {
	struct kgsl_device dev;    /* Must be first field in this struct */
	struct kgsl_memregion gmemspace;
	struct kgsl_yamato_context *drawctxt_active;
	wait_queue_head_t ib1_wq;
	unsigned int *pfp_fw;
	size_t pfp_fw_size;
	unsigned int *pm4_fw;
	size_t pm4_fw_size;
	struct kgsl_ringbuffer ringbuffer;
};


irqreturn_t kgsl_yamato_isr(int irq, void *data);
int __init kgsl_yamato_init(struct kgsl_device *device);
int __init kgsl_yamato_init_pwrctrl(struct kgsl_device *device);

int kgsl_yamato_close(struct kgsl_device *device);
int kgsl_yamato_idle(struct kgsl_device *device, unsigned int timeout);
void kgsl_yamato_regread(struct kgsl_device *device, unsigned int offsetwords,
				unsigned int *value);
void kgsl_yamato_regwrite(struct kgsl_device *device, unsigned int offsetwords,
				unsigned int value);
void kgsl_yamato_regread_isr(struct kgsl_device *device,
			     unsigned int offsetwords,
			     unsigned int *value);
void kgsl_yamato_regwrite_isr(struct kgsl_device *device,
			      unsigned int offsetwords,
			      unsigned int value);
struct kgsl_device *kgsl_get_yamato_generic_device(void);
int kgsl_yamato_getfunctable(struct kgsl_functable *ftbl);

#endif
