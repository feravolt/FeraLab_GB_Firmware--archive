#ifndef _KGSL_DEVICE_H
#define _KGSL_DEVICE_H

#include <linux/types.h>
#include <linux/irqreturn.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/msm_kgsl.h>
#include <linux/idr.h>
#include <linux/wakelock.h>
#include <asm/atomic.h>
#include "kgsl_mmu.h"
#include "kgsl_pwrctrl.h"

#define KGSL_TIMEOUT_NONE       0
#define KGSL_TIMEOUT_DEFAULT    0xFFFFFFFF
#define FIRST_TIMEOUT (HZ / 2)
#define KGSL_CHIPID_YAMATODX_REV21  0x20100
#define KGSL_CHIPID_YAMATODX_REV211 0x20101
#define KGSL_STATE_NONE		0x00000000
#define KGSL_STATE_INIT		0x00000001
#define KGSL_STATE_ACTIVE	0x00000002
#define KGSL_STATE_NAP		0x00000004
#define KGSL_STATE_SLEEP	0x00000008
#define KGSL_STATE_SUSPEND	0x00000010
#define KGSL_STATE_HUNG		0x00000020
#define KGSL_GRAPHICS_MEMORY_LOW_WATERMARK  0x1000000
#define KGSL_IS_PAGE_ALIGNED(addr) (!((addr) & (~PAGE_MASK)))

struct kgsl_device;
struct platform_device;
struct kgsl_device_private;
struct kgsl_context;

struct kgsl_functable {
	void (*device_regread) (struct kgsl_device *device,
					unsigned int offsetwords,
					unsigned int *value);
	void (*device_regwrite) (struct kgsl_device *device,
					unsigned int offsetwords,
					unsigned int value);
	void (*device_regread_isr) (struct kgsl_device *device,
					unsigned int offsetwords,
					unsigned int *value);
	void (*device_regwrite_isr) (struct kgsl_device *device,
					unsigned int offsetwords,
					unsigned int value);
	int (*device_setstate) (struct kgsl_device *device, uint32_t flags);
	int (*device_idle) (struct kgsl_device *device, unsigned int timeout);
	unsigned int (*device_isidle) (struct kgsl_device *device);
	int (*device_suspend_context) (struct kgsl_device *device);
	int (*device_resume_context) (struct kgsl_device *device);
	int (*device_start) (struct kgsl_device *device, unsigned int init_ram);
	int (*device_stop) (struct kgsl_device *device);
	int (*device_getproperty) (struct kgsl_device *device,
					enum kgsl_property_type type,
					void *value,
					unsigned int sizebytes);
	int (*device_waittimestamp) (struct kgsl_device *device,
					unsigned int timestamp,
					unsigned int msecs);
	unsigned int (*device_cmdstream_readtimestamp) (
					struct kgsl_device *device,
					enum kgsl_timestamp_type type);
	int (*device_issueibcmds) (struct kgsl_device_private *dev_priv,
				struct kgsl_context *context,
				struct kgsl_ibdesc *ibdesc,
				unsigned int sizedwords,
				uint32_t *timestamp,
				unsigned int flags);
	int (*device_drawctxt_create) (struct kgsl_device_private *dev_priv,
					uint32_t flags,
					struct kgsl_context *context);
	int (*device_drawctxt_destroy) (struct kgsl_device *device,
					struct kgsl_context *context);
	long (*device_ioctl) (struct kgsl_device_private *dev_priv,
					unsigned int cmd,
					unsigned long arg);
	int (*device_setup_pt)(struct kgsl_device *device,
			       struct kgsl_pagetable *pagetable);

	int (*device_cleanup_pt)(struct kgsl_device *device,
				 struct kgsl_pagetable *pagetable);

};

struct kgsl_memregion {
	unsigned char  *mmio_virt_base;
	unsigned int   mmio_phys_base;
	uint32_t      gpu_base;
	unsigned int   sizebytes;
};

struct kgsl_device {
	struct device *dev;
	const char *name;
	uint32_t       flags;
	enum kgsl_deviceid    id;
	unsigned int      chip_id;
	struct kgsl_memregion regspace;
	struct kgsl_memdesc memstore;

	struct kgsl_mmu 	  mmu;
	struct completion hwaccess_gate;
	struct kgsl_functable ftbl;
	struct work_struct idle_check_ws;
	struct timer_list idle_timer;
	struct kgsl_pwrctrl pwrctrl;
	atomic_t open_count;

	struct atomic_notifier_head ts_notifier_list;
	struct mutex mutex;
	uint32_t		state;
	uint32_t		requested_state;

	struct list_head memqueue;
	unsigned int active_cnt;
	struct completion suspend_gate;

	struct workqueue_struct *work_queue;
	struct idr context_idr;
	struct wake_lock idle_wakelock;
};

struct kgsl_context {
	uint32_t id;

	/* Pointer to the owning device instance */
	struct kgsl_device_private *dev_priv;
	void *devctxt;
};

struct kgsl_process_private {
	unsigned int refcnt;
	pid_t pid;
	spinlock_t mem_lock;
	struct list_head mem_list;
	struct kgsl_pagetable *pagetable;
	struct list_head list;
};

struct kgsl_device_private {
	struct kgsl_device *device;
	struct kgsl_process_private *process_priv;
};

struct kgsl_devconfig {
	struct kgsl_memregion regspace;

	unsigned int     mmu_config;
	uint32_t        mpu_base;
	int              mpu_range;
	uint32_t        va_base;
	unsigned int     va_range;

	struct kgsl_memregion gmemspace;
};

struct kgsl_device *kgsl_get_device(int dev_idx);

static inline struct kgsl_mmu *
kgsl_get_mmu(struct kgsl_device *device)
{
	return (struct kgsl_mmu *) (device ? &device->mmu : NULL);
}

static inline int kgsl_create_device_workqueue(struct kgsl_device *device)
{
	device->work_queue = create_workqueue(device->name);
	if (!device->work_queue) {
		return -EINVAL;
	}
	return 0;
}

static inline struct kgsl_context *
kgsl_find_context(struct kgsl_device_private *dev_priv, uint32_t id)
{
	struct kgsl_context *ctxt =
		idr_find(&dev_priv->device->context_idr, id);
	return  (ctxt && ctxt->dev_priv == dev_priv) ? ctxt : NULL;
}

#endif  /* _KGSL_DEVICE_H */
