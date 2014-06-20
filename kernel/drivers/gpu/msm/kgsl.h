#ifndef _GSL_DRIVER_H
#define _GSL_DRIVER_H

#include <linux/types.h>
#include <linux/msm_kgsl.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/regulator/consumer.h>
#include <asm/atomic.h>
#include "kgsl_device.h"
#include "kgsl_sharedmem.h"

#define DRIVER_NAME "kgsl"
#define CLASS_NAME "msm_kgsl"
#define CHIP_REV_251 0x020501

enum kgsl_clk_freq {
	KGSL_AXI_HIGH_2D = 0,
	KGSL_AXI_HIGH_3D = 1,
	KGSL_2D_MIN_FREQ = 2,
	KGSL_2D_MAX_FREQ = 3,
	KGSL_3D_MIN_FREQ = 4,
	KGSL_3D_MAX_FREQ = 5,
	KGSL_NUM_FREQ
};

#define KGSL_CACHE_INV		0x00000000
#define KGSL_CACHE_CLEAN	0x00000001
#define KGSL_CACHE_FLUSH	0x00000002
#define KGSL_CACHE_USER_ADDR	0x00000010
#define KGSL_CACHE_VMALLOC_ADDR	0x00000020
#define DRM_KGSL_GEM_CACHE_OP_TO_DEV	0x0001
#define DRM_KGSL_GEM_CACHE_OP_FROM_DEV	0x0002

struct kgsl_driver {
	struct cdev cdev;
	dev_t dev_num;
	struct class *class;
	struct kgsl_device *devp[KGSL_DEVICE_MAX];
	struct device *base_dev[KGSL_DEVICE_MAX];
	int num_devs;
	struct platform_device *pdev;
	struct mutex mutex;
	int yamato_interrupt_num;
	int yamato_have_irq;
	int g12_interrupt_num;
	int g12_have_irq;
	struct clk *g12_grp_pclk;
	struct clk *g12_grp_clk;
	struct clk *yamato_grp_pclk;
	struct clk *yamato_grp_clk;
	struct clk *yamato_grp_src_clk;
	struct clk *imem_clk;
	unsigned int power_flags;
	unsigned int is_suspended;
	unsigned int clk_freq[KGSL_NUM_FREQ];
	struct regulator *yamato_reg;
	struct regulator *g12_reg;
	struct kgsl_devconfig g12_config;
	struct kgsl_devconfig yamato_config;
	struct kgsl_sharedmem shmem;
	struct list_head dev_priv_list;
	struct list_head pagetable_list;
	struct mutex pt_mutex;
	struct kgsl_pagetable *global_pt;
};

extern struct kgsl_driver kgsl_driver;

struct kgsl_mem_entry {
	struct kgsl_memdesc memdesc;
	struct file *file_ptr;
	struct list_head list;
	struct list_head free_list;
	uint32_t free_timestamp;
	struct kgsl_file_private *priv;
};

enum kgsl_status {
	KGSL_SUCCESS = 0,
	KGSL_FAILURE = 1
};

#define KGSL_TRUE 1
#define KGSL_FALSE 0
#define KGSL_PRE_HWACCESS() \
while (1) { \
	if (device == NULL) \
		break; \
	if (device->hwaccess_blocked == KGSL_FALSE) { \
		break; \
	} \
	if (kgsl_driver.is_suspended != KGSL_TRUE) { \
		device->ftbl.device_wake(device); \
		break; \
	} \
	mutex_unlock(&kgsl_driver.mutex); \
	wait_for_completion(&device->hwaccess_gate); \
	mutex_lock(&kgsl_driver.mutex); \
}

#define KGSL_POST_HWACCESS() \
	mutex_unlock(&kgsl_driver.mutex)

#ifdef CONFIG_MSM_KGSL_MMU_PAGE_FAULT
#define MMU_CONFIG 2
#else
#define MMU_CONFIG 1
#endif

void kgsl_remove_mem_entry(struct kgsl_mem_entry *entry);
int kgsl_pwrctrl(unsigned int pwrflag);
void kgsl_timer(unsigned long data);
void kgsl_idle_check(struct work_struct *work);
int kgsl_idle(struct kgsl_device *device, unsigned int timeout);
int kgsl_setstate(struct kgsl_device *device, uint32_t flags);
int kgsl_regread(struct kgsl_device *device, unsigned int offsetwords,
			unsigned int *value);
int kgsl_regwrite(struct kgsl_device *device, unsigned int offsetwords,
			unsigned int value);
int kgsl_check_timestamp(struct kgsl_device *device, unsigned int timestamp);
int kgsl_setup_pt(struct kgsl_pagetable *);
int kgsl_cleanup_pt(struct kgsl_pagetable *);
int kgsl_register_ts_notifier(struct kgsl_device *device,
			      struct notifier_block *nb);

int kgsl_unregister_ts_notifier(struct kgsl_device *device,
				struct notifier_block *nb);

#ifdef CONFIG_MSM_KGSL_DRM
extern int kgsl_drm_init(struct platform_device *dev);
extern void kgsl_drm_exit(void);
extern void kgsl_gpu_mem_flush(int op);
#else
static inline int kgsl_drm_init(struct platform_device *dev)
{
	return 0;
}

static inline void kgsl_drm_exit(void)
{
}
#endif
#endif
