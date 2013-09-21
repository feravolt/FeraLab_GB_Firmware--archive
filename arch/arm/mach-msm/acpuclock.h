#ifndef __ARCH_ARM_MACH_MSM_ACPUCLOCK_H
#define __ARCH_ARM_MACH_MSM_ACPUCLOCK_H

#include <linux/list.h>

enum setrate_reason {
	SETRATE_CPUFREQ = 0,
	SETRATE_SWFI,
	SETRATE_PC,
};

int acpuclk_set_rate(unsigned long rate, enum setrate_reason reason);
unsigned long acpuclk_get_rate(void);
uint32_t acpuclk_get_switch_time(void);
unsigned long acpuclk_wait_for_irq(void);
unsigned long acpuclk_power_collapse(void);

#endif

