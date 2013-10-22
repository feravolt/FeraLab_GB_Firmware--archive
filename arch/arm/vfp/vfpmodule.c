#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/export.h>
#include <asm/cputype.h>
#include <asm/thread_notify.h>
#include <asm/vfp.h>
#include "vfpinstr.h"
#include "vfp.h"

void vfp_testing_entry(void);
void vfp_support_entry(void);
void vfp_null_entry(void);

void (*vfp_vector)(void) = vfp_null_entry;
union vfp_state *last_VFP_context[NR_CPUS];

unsigned int VFP_arch;

static int vfp_notifier(struct notifier_block *self, unsigned long cmd, void *v)
{
	struct thread_info *thread = v;
	union vfp_state *vfp;
	u32 cpu = thread->cpu;

	if (likely(cmd == THREAD_NOTIFY_SWITCH)) {
		u32 fpexc = fmrx(FPEXC);

#ifdef CONFIG_SMP
		if ((fpexc & FPEXC_EN) && last_VFP_context[cpu]) {
			vfp_save_state(last_VFP_context[cpu], fpexc);
			last_VFP_context[cpu]->hard.cpu = cpu;
		}
		if (thread->vfpstate.hard.cpu != cpu)
			last_VFP_context[cpu] = NULL;
#endif
		fmxr(FPEXC, fpexc & ~FPEXC_EN);
		return NOTIFY_DONE;
	}

	vfp = &thread->vfpstate;
	if (cmd == THREAD_NOTIFY_FLUSH) {
		memset(vfp, 0, sizeof(union vfp_state));
		vfp->hard.fpexc = FPEXC_EN;
		vfp->hard.fpscr = FPSCR_ROUND_NEAREST;
		fmxr(FPEXC, fmrx(FPEXC) & ~FPEXC_EN);
	}

	if (last_VFP_context[cpu] == vfp)
		last_VFP_context[cpu] = NULL;

	return NOTIFY_DONE;
}

static struct notifier_block vfp_notifier_block = {
	.notifier_call	= vfp_notifier,
};

static void vfp_raise_sigfpe(unsigned int sicode, struct pt_regs *regs)
{
	siginfo_t info;
	memset(&info, 0, sizeof(info));
	info.si_signo = SIGFPE;
	info.si_code = sicode;
	info.si_addr = (void __user *)(instruction_pointer(regs) - 4);
	current->thread.error_code = 0;
	current->thread.trap_no = 6;
	send_sig_info(SIGFPE, &info, current);
}

static void vfp_panic(char *reason, u32 inst)
{
	int i;

	printk(KERN_ERR "VFP: Error: %s\n", reason);
	printk(KERN_ERR "VFP: EXC 0x%08x SCR 0x%08x INST 0x%08x\n",
		fmrx(FPEXC), fmrx(FPSCR), inst);
	for (i = 0; i < 32; i += 2)
		printk(KERN_ERR "VFP: s%2u: 0x%08x s%2u: 0x%08x\n",
		       i, vfp_get_float(i), i+1, vfp_get_float(i+1));
}

static void vfp_raise_exceptions(u32 exceptions, u32 inst, u32 fpscr, struct pt_regs *regs)
{
	int si_code = 0;

	pr_debug("VFP: raising exceptions %08x\n", exceptions);

	if (exceptions == VFP_EXCEPTION_ERROR) {
		vfp_panic("unhandled bounce", inst);
		vfp_raise_sigfpe(0, regs);
		return;
	}

	if (exceptions & (FPSCR_N|FPSCR_Z|FPSCR_C|FPSCR_V))
		fpscr &= ~(FPSCR_N|FPSCR_Z|FPSCR_C|FPSCR_V);

	fpscr |= exceptions;

	fmxr(FPSCR, fpscr);

#define RAISE(stat,en,sig)				\
	if (exceptions & stat && fpscr & en)		\
		si_code = sig;

	RAISE(FPSCR_DZC, FPSCR_DZE, FPE_FLTDIV);
	RAISE(FPSCR_IXC, FPSCR_IXE, FPE_FLTRES);
	RAISE(FPSCR_UFC, FPSCR_UFE, FPE_FLTUND);
	RAISE(FPSCR_OFC, FPSCR_OFE, FPE_FLTOVF);
	RAISE(FPSCR_IOC, FPSCR_IOE, FPE_FLTINV);

	if (si_code)
		vfp_raise_sigfpe(si_code, regs);
}

static u32 vfp_emulate_instruction(u32 inst, u32 fpscr, struct pt_regs *regs)
{
	u32 exceptions = VFP_EXCEPTION_ERROR;

	pr_debug("VFP: emulate: INST=0x%08x SCR=0x%08x\n", inst, fpscr);

	if (INST_CPRTDO(inst)) {
		if (!INST_CPRT(inst)) {
			if (vfp_single(inst)) {
				exceptions = vfp_single_cpdo(inst, fpscr);
			} else {
				exceptions = vfp_double_cpdo(inst, fpscr);
			}
		} else {
		}
	} else {
	}
	return exceptions & ~VFP_NAN_FLAG;
}

void VFP_bounce(u32 trigger, u32 fpexc, struct pt_regs *regs)
{
	u32 fpscr, orig_fpscr, fpsid, exceptions;

	pr_debug("VFP: bounce: trigger %08x fpexc %08x\n", trigger, fpexc);
	fmxr(FPEXC, fpexc & ~(FPEXC_EX|FPEXC_DEX|FPEXC_FP2V|FPEXC_VV|FPEXC_TRAP_MASK));

	fpsid = fmrx(FPSID);
	orig_fpscr = fpscr = fmrx(FPSCR);
	if ((fpsid & FPSID_ARCH_MASK) == (1 << FPSID_ARCH_BIT)
	    && (fpscr & FPSCR_IXE)) {
		goto emulate;
	}

	if (fpexc & FPEXC_EX) {
		trigger = fmrx(FPINST);
		regs->ARM_pc -= 4;
	} else if (!(fpexc & FPEXC_DEX)) {
		 vfp_raise_exceptions(VFP_EXCEPTION_ERROR, trigger, fpscr, regs);
		goto exit;
	}
	if (fpexc & (FPEXC_EX | FPEXC_VV)) {
		u32 len;

		len = fpexc + (1 << FPEXC_LENGTH_BIT);

		fpscr &= ~FPSCR_LENGTH_MASK;
		fpscr |= (len & FPEXC_LENGTH_MASK) << (FPSCR_LENGTH_BIT - FPEXC_LENGTH_BIT);
	}
	exceptions = vfp_emulate_instruction(trigger, fpscr, regs);
	if (exceptions)
		vfp_raise_exceptions(exceptions, trigger, orig_fpscr, regs);
	if (fpexc ^ (FPEXC_EX | FPEXC_FP2V))
		goto exit;
	barrier();
	trigger = fmrx(FPINST2);

 emulate:
	exceptions = vfp_emulate_instruction(trigger, orig_fpscr, regs);
	if (exceptions)
		vfp_raise_exceptions(exceptions, trigger, orig_fpscr, regs);
 exit:
	preempt_enable();
}

static void vfp_enable(void *unused)
{
	u32 access = get_copro_access();
	set_copro_access(access | CPACC_FULL(10) | CPACC_FULL(11));
}

int vfp_flush_context(void)
{
	unsigned long flags;
	struct thread_info *ti;
	u32 fpexc;
	u32 cpu;
	int saved = 0;

	local_irq_save(flags);

	ti = current_thread_info();
	fpexc = fmrx(FPEXC);
	cpu = ti->cpu;

#ifdef CONFIG_SMP
	if ((fpexc & FPEXC_EN) && last_VFP_context[cpu]) {
		last_VFP_context[cpu]->hard.cpu = cpu;
#else
	if (last_VFP_context[cpu]) {
		fmxr(FPEXC, fpexc | FPEXC_EN);
		isb();
#endif
		vfp_save_state(last_VFP_context[cpu], fpexc);
		fmxr(FPEXC, fmrx(FPEXC) & ~FPEXC_EN);
		saved = 1;
	}
	last_VFP_context[cpu] = NULL;
	local_irq_restore(flags);
	return saved;
}

void vfp_reinit(void)
{
	vfp_enable(NULL);
	fmxr(FPEXC, fmrx(FPEXC) & ~FPEXC_EN);
}

#ifdef CONFIG_PM
#include <linux/sysdev.h>

static int vfp_pm_suspend(struct sys_device *dev, pm_message_t state)
{
	int saved = vfp_flush_context();

	if (saved)
		printk(KERN_DEBUG "%s: saved vfp state\n", __func__);
	memset(last_VFP_context, 0, sizeof(last_VFP_context));

	return 0;
}

static int vfp_pm_resume(struct sys_device *dev)
{
	vfp_reinit();

	return 0;
}

static struct sysdev_class vfp_pm_sysclass = {
	.name		= "vfp",
	.suspend	= vfp_pm_suspend,
	.resume		= vfp_pm_resume,
};

static struct sys_device vfp_pm_sysdev = {
	.cls	= &vfp_pm_sysclass,
};

static void vfp_pm_init(void)
{
	sysdev_class_register(&vfp_pm_sysclass);
	sysdev_register(&vfp_pm_sysdev);
}


#else
static inline void vfp_pm_init(void) { }
#endif

void vfp_sync_state(struct thread_info *thread)
{
	unsigned int cpu = get_cpu();

	if (last_VFP_context[cpu] == &thread->vfpstate) {
		u32 fpexc = fmrx(FPEXC);
		fmxr(FPEXC, fpexc | FPEXC_EN);
		vfp_save_state(&thread->vfpstate, fpexc | FPEXC_EN);
		fmxr(FPEXC, fpexc & ~FPEXC_EN);
		last_VFP_context[cpu] = NULL;
	}
#ifdef CONFIG_SMP
	thread->vfpstate.hard.cpu = NR_CPUS;
#endif
	put_cpu();
}

#include <linux/smp.h>

#ifdef CONFIG_KERNEL_MODE_NEON
void kernel_neon_begin(void)
{
        struct thread_info *thread = current_thread_info();
        unsigned int cpu;
        u32 fpexc;
        BUG_ON(in_interrupt());
        cpu = get_cpu();
        fpexc = fmrx(FPEXC) | FPEXC_EN;
        fmxr(FPEXC, fpexc);

        if (vfp_state_in_hw(cpu, thread))
                vfp_save_state(&thread->vfpstate, fpexc);
#ifndef CONFIG_SMP
        else if (vfp_current_hw_state[cpu] != NULL)
                vfp_save_state(vfp_current_hw_state[cpu], fpexc);
#endif
        vfp_current_hw_state[cpu] = NULL;
}
EXPORT_SYMBOL(kernel_neon_begin);

void kernel_neon_end(void)
{
        fmxr(FPEXC, fmrx(FPEXC) & ~FPEXC_EN);
        put_cpu();
}
EXPORT_SYMBOL(kernel_neon_end);
#endif

static int __init vfp_init(void)
{
	unsigned int vfpsid;
	unsigned int cpu_arch = cpu_architecture();

	if (cpu_arch >= CPU_ARCH_ARMv6)
		vfp_enable(NULL);
	vfp_vector = vfp_testing_entry;
	barrier();
	vfpsid = fmrx(FPSID);
	barrier();
	vfp_vector = vfp_null_entry;

	printk(KERN_INFO "VFP support v0.3: ");
	if (VFP_arch)
		printk("not present\n");
	else if (vfpsid & FPSID_NODOUBLE) {
		printk("no double precision support\n");
	} else {
		smp_call_function(vfp_enable, NULL, 1);

		VFP_arch = (vfpsid & FPSID_ARCH_MASK) >> FPSID_ARCH_BIT;
		printk("implementor %02x architecture %d part %02x variant %x rev %x\n",
			(vfpsid & FPSID_IMPLEMENTER_MASK) >> FPSID_IMPLEMENTER_BIT,
			(vfpsid & FPSID_ARCH_MASK) >> FPSID_ARCH_BIT,
			(vfpsid & FPSID_PART_MASK) >> FPSID_PART_BIT,
			(vfpsid & FPSID_VARIANT_MASK) >> FPSID_VARIANT_BIT,
			(vfpsid & FPSID_REV_MASK) >> FPSID_REV_BIT);

		vfp_vector = vfp_support_entry;

		thread_register_notifier(&vfp_notifier_block);
		vfp_pm_init();
		elf_hwcap |= HWCAP_VFP;
#ifdef CONFIG_VFPv3
		if (VFP_arch >= 2) {
			elf_hwcap |= HWCAP_VFPv3;
			if (((fmrx(MVFR0) & MVFR0_A_SIMD_MASK)) == 1)
				elf_hwcap |= HWCAP_VFPv3D16;
		}
#endif
#ifdef CONFIG_NEON
		if ((read_cpuid_id() & 0x000f0000) == 0x000f0000) {
			if ((fmrx(MVFR1) & 0x000fff00) == 0x00011100)
				elf_hwcap |= HWCAP_NEON;
		}
#endif
	}
	return 0;
}

core_initcall(vfp_init);

