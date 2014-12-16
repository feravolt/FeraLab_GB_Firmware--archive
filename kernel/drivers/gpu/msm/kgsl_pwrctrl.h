#ifndef _GSL_PWRCTRL_H
#define _GSL_PWRCTRL_H

#include <linux/types.h>
#include <linux/wait.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <mach/clk.h>
#include <mach/internal_power_rail.h>
#include <linux/pm_qos_params.h>

#define KGSL_PWRFLAGS_POWER_OFF		0x00000001
#define KGSL_PWRFLAGS_POWER_ON		0x00000002
#define KGSL_PWRFLAGS_CLK_ON		0x00000004
#define KGSL_PWRFLAGS_CLK_OFF		0x00000008
#define KGSL_PWRFLAGS_AXI_ON		0x00000010
#define KGSL_PWRFLAGS_AXI_OFF		0x00000020
#define KGSL_PWRFLAGS_IRQ_ON		0x00000040
#define KGSL_PWRFLAGS_IRQ_OFF		0x00000080

#define BW_INIT 0
#define BW_MAX  1

enum kgsl_clk_freq {
	KGSL_AXI_HIGH = 0,
	KGSL_MIN_FREQ = 1,
	KGSL_DEFAULT_FREQ = 2,
	KGSL_MAX_FREQ = 3,
	KGSL_NUM_FREQ = 4
};

struct kgsl_pwrctrl {
	int interrupt_num;
	int have_irq;
	unsigned int pwr_rail;
	struct clk *ebi1_clk;
	struct clk *grp_clk;
	struct clk *grp_pclk;
	struct clk *grp_src_clk;
	struct clk *imem_clk;
	struct clk *imem_pclk;
	unsigned int power_flags;
	unsigned int clk_freq[KGSL_NUM_FREQ];
	unsigned int interval_timeout;
	struct regulator *gpu_reg;
	uint32_t pcl;
	unsigned int nap_allowed;
	unsigned int io_fraction;
	unsigned int io_count;
	struct kgsl_yamato_context *suspended_ctxt;
	bool pwrrail_first;
};

int kgsl_pwrctrl_clk(struct kgsl_device *device, unsigned int pwrflag);
int kgsl_pwrctrl_axi(struct kgsl_device *device, unsigned int pwrflag);
int kgsl_pwrctrl_pwrrail(struct kgsl_device *device, unsigned int pwrflag);
int kgsl_pwrctrl_irq(struct kgsl_device *device, unsigned int pwrflag);
void kgsl_pwrctrl_close(struct kgsl_device *device);
void kgsl_timer(unsigned long data);
void kgsl_idle_check(struct work_struct *work);
void kgsl_pre_hwaccess(struct kgsl_device *device);
void kgsl_check_suspended(struct kgsl_device *device);
int kgsl_pwrctrl_sleep(struct kgsl_device *device);
int kgsl_pwrctrl_wake(struct kgsl_device *device);

int kgsl_pwrctrl_init_sysfs(struct kgsl_device *device);
void kgsl_pwrctrl_uninit_sysfs(struct kgsl_device *device);

#endif /* _GSL_PWRCTRL_H */
