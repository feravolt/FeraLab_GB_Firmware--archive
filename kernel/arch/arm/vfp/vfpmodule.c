#include <linux/types.h>
#include <linux/cpu.h>
#include <linux/hardirq.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <asm/cputype.h>
#include <asm/thread_notify.h>
#include <asm/vfp.h>
#include "vfpinstr.h"
#include "vfp.h"

void vfp_testing_entry(void);
void vfp_support_entry(void);
void vfp_null_entry(void);
void (*vfp_vector)(void) = vfp_null_entry;
unsigned int VFP_arch;
union vfp_state *vfp_current_hw_state[NR_CPUS];

static bool vfp_state_in_hw(unsigned int cpu, struct thread_info *thread)
{
#ifdef CONFIG_SMP
	if (thread->vfpstate.hard.cpu != cpu)
		return false;
#endif
	return vfp_current_hw_state[cpu] == &thread->vfpstate;
}
static void vfp_force_reload(unsigned int cpu, struct thread_info *thread)
{
	if (vfp_state_in_hw(cpu, thread)) {
		fmxr(FPEXC, fmrx(FPEXC) & ~FPEXC_EN);
		vfp_current_hw_state[cpu] = NULL;
	}
#ifdef CONFIG_SMP
	thread->vfpstate.hard.cpu = NR_CPUS;
	vfp_current_hw_state[cpu] = NULL;
#endif
}

static void vfp_thread_flush(struct thread_info *thread)
{
	union vfp_state *vfp = &thread->vfpstate;
	unsigned int cpu;
	cpu = get_cpu();
	if (vfp_current_hw_state[cpu] == vfp)
		vfp_current_hw_state[cpu] = NULL;
	fmxr(FPEXC, fmrx(FPEXC) & ~FPEXC_EN);
	put_cpu();
	memset(vfp, 0, sizeof(union vfp_state));
	vfp->hard.fpexc = FPEXC_EN;
	vfp->hard.fpscr = FPSCR_ROUND_NEAREST;
#ifdef CONFIG_SMP
	vfp->hard.cpu = NR_CPUS;
#endif
}

static void vfp_thread_exit(struct thread_info *thread)
{
	union vfp_state *vfp = &thread->vfpstate;
	unsigned int cpu = get_cpu();

	if (vfp_current_hw_state[cpu] == vfp)
		vfp_current_hw_state[cpu] = NULL;
	put_cpu();
}

static void vfp_thread_copy(struct thread_info *thread)
{
	struct thread_info *parent = current_thread_info();
	vfp_sync_hwstate(parent);
	thread->vfpstate = parent->vfpstate;
#ifdef CONFIG_SMP
	thread->vfpstate.hard.cpu = NR_CPUS;
#endif
}

static int vfp_notifier(struct notifier_block *self, unsigned long cmd, void *v)
{
	struct thread_info *thread = v;
	u32 fpexc;
#ifdef CONFIG_SMP
	unsigned int cpu;
#endif
	switch (cmd) {
	case THREAD_NOTIFY_SWITCH:
		fpexc = fmrx(FPEXC);

#ifdef CONFIG_SMP
		cpu = thread->cpu;
		if ((fpexc & FPEXC_EN) && vfp_current_hw_state[cpu])
			vfp_save_state(vfp_current_hw_state[cpu], fpexc);
			vfp_current_hw_state[cpu]->hard.cpu = cpu;
#endif
		fmxr(FPEXC, fpexc & ~FPEXC_EN);
		break;
	case THREAD_NOTIFY_FLUSH:
		vfp_thread_flush(thread);
		break;
	case THREAD_NOTIFY_EXIT:
		vfp_thread_exit(thread);
		break;
	case THREAD_NOTIFY_COPY:
		vfp_thread_copy(thread);
		break;
	}
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

	/*
	 * This is the same as NWFPE, because it's not clear what
	 * this is used for
	 */
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

/*
 * Process bitmask of exception conditions.
 */
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

/*
 * Emulate a VFP instruction.
 */
static u32 vfp_emulate_instruction(u32 inst, u32 fpscr, struct pt_regs *regs)
{
	u32 exceptions = VFP_EXCEPTION_ERROR;

	pr_debug("VFP: emulate: INST=0x%08x SCR=0x%08x\n", inst, fpscr);

	if (INST_CPRTDO(inst)) {
		if (!INST_CPRT(inst)) {
			/*
			 * CPDO
			 */
			if (vfp_single(inst)) {
				exceptions = vfp_single_cpdo(inst, fpscr);
			} else {
				exceptions = vfp_double_cpdo(inst, fpscr);
			}
		} else {
			/*
			 * A CPRT instruction can not appear in FPINST2, nor
			 * can it cause an exception.  Therefore, we do not
			 * have to emulate it.
			 */
		}
	} else {
		/*
		 * A CPDT instruction can not appear in FPINST2, nor can
		 * it cause an exception.  Therefore, we do not have to
		 * emulate it.
		 */
	}
	return exceptions & ~VFP_NAN_FLAG;
}

/*
 * Package up a bounce condition.
 */
void VFP_bounce(u32 trigger, u32 fpexc, struct pt_regs *regs)
{
	u32 fpscr, orig_fpscr, fpsid, exceptions;

	pr_debug("VFP: bounce: trigger %08x fpexc %08x\n", trigger, fpexc);

	/*
	 * At this point, FPEXC can have the following configuration:
	 *
	 *  EX DEX IXE
	 *  0   1   x   - synchronous exception
	 *  1   x   0   - asynchronous exception
	 *  1   x   1   - sychronous on VFP subarch 1 and asynchronous on later
	 *  0   0   1   - synchronous on VFP9 (non-standard subarch 1
	 *                implementation), undefined otherwise
	 *
	 * Clear various bits and enable access to the VFP so we can
	 * handle the bounce.
	 */
	fmxr(FPEXC, fpexc & ~(FPEXC_EX|FPEXC_DEX|FPEXC_FP2V|FPEXC_VV|FPEXC_TRAP_MASK));

	fpsid = fmrx(FPSID);
	orig_fpscr = fpscr = fmrx(FPSCR);

	/*
	 * Check for the special VFP subarch 1 and FPSCR.IXE bit case
	 */
	if ((fpsid & FPSID_ARCH_MASK) == (1 << FPSID_ARCH_BIT)
	    && (fpscr & FPSCR_IXE)) {
		/*
		 * Synchronous exception, emulate the trigger instruction
		 */
		goto emulate;
	}

	if (fpexc & FPEXC_EX) {
		/*
		 * Asynchronous exception. The instruction is read from FPINST
		 * and the interrupted instruction has to be restarted.
		 */
		trigger = fmrx(FPINST);
		regs->ARM_pc -= 4;
	} else if (!(fpexc & FPEXC_DEX)) {
		/*
		 * Illegal combination of bits. It can be caused by an
		 * unallocated VFP instruction but with FPSCR.IXE set and not
		 * on VFP subarch 1.
		 */
		 vfp_raise_exceptions(VFP_EXCEPTION_ERROR, trigger, fpscr, regs);
		goto exit;
	}

	/*
	 * Modify fpscr to indicate the number of iterations remaining.
	 * If FPEXC.EX is 0, FPEXC.DEX is 1 and the FPEXC.VV bit indicates
	 * whether FPEXC.VECITR or FPSCR.LEN is used.
	 */
	if (fpexc & (FPEXC_EX | FPEXC_VV)) {
		u32 len;

		len = fpexc + (1 << FPEXC_LENGTH_BIT);

		fpscr &= ~FPSCR_LENGTH_MASK;
		fpscr |= (len & FPEXC_LENGTH_MASK) << (FPSCR_LENGTH_BIT - FPEXC_LENGTH_BIT);
	}

	/*
	 * Handle the first FP instruction.  We used to take note of the
	 * FPEXC bounce reason, but this appears to be unreliable.
	 * Emulate the bounced instruction instead.
	 */
	exceptions = vfp_emulate_instruction(trigger, fpscr, regs);
	if (exceptions)
		vfp_raise_exceptions(exceptions, trigger, orig_fpscr, regs);

	/*
	 * If there isn't a second FP instruction, exit now. Note that
	 * the FPEXC.FP2V bit is valid only if FPEXC.EX is 1.
	 */
	if (fpexc ^ (FPEXC_EX | FPEXC_FP2V))
		goto exit;

	/*
	 * The barrier() here prevents fpinst2 being read
	 * before the condition above.
	 */
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
	u32 access;
	BUG_ON(preemptible());
	access = get_copro_access();
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
	/* On SMP, if VFP is enabled, save the old state */
	if ((fpexc & FPEXC_EN) && vfp_current_hw_state[cpu]) {
		vfp_current_hw_state[cpu]->hard.cpu = cpu;
#else
	/* If there is a VFP context we must save it. */
	if (vfp_current_hw_state[cpu]) {
		/* Enable VFP so we can save the old state. */
		fmxr(FPEXC, fpexc | FPEXC_EN);
		isb();
#endif
		vfp_save_state(vfp_current_hw_state[cpu], fpexc);
		fmxr(FPEXC, fmrx(FPEXC) & ~FPEXC_EN);
	} else if (vfp_current_hw_state[ti->cpu]) {
#ifndef CONFIG_SMP
		fmxr(FPEXC, fpexc | FPEXC_EN);
		vfp_save_state(vfp_current_hw_state[ti->cpu], fpexc);
		fmxr(FPEXC, fpexc);
#endif
	}
	vfp_current_hw_state[cpu] = NULL;

	local_irq_restore(flags);

	return saved;
}

void vfp_reinit(void)
{
	/* ensure we have access to the vfp */
	vfp_enable(NULL);

	/* and disable it to ensure the next usage restores the state */
	fmxr(FPEXC, fmrx(FPEXC) & ~FPEXC_EN);
}

#ifdef CONFIG_PM
#include <linux/sysdev.h>

static int vfp_pm_suspend(struct sys_device *dev, pm_message_t state)
{
	int saved = vfp_flush_context();
	if (saved)
	memset(vfp_current_hw_state, 0, sizeof(vfp_current_hw_state));
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

void vfp_sync_hwstate(struct thread_info *thread)
{
	unsigned int cpu = get_cpu();

	if (vfp_state_in_hw(cpu, thread)) {
		u32 fpexc = fmrx(FPEXC);
		fmxr(FPEXC, fpexc | FPEXC_EN);
		vfp_save_state(&thread->vfpstate, fpexc | FPEXC_EN);
		fmxr(FPEXC, fpexc);
	}

	put_cpu();
}

void vfp_flush_hwstate(struct thread_info *thread)
{
	unsigned int cpu = get_cpu();
	vfp_force_reload(cpu, thread);
	put_cpu();
}


static int vfp_hotplug(struct notifier_block *b, unsigned long action,
	void *hcpu)
{
	if (action == CPU_DYING || action == CPU_DYING_FROZEN) {
		vfp_force_reload((long)hcpu, current_thread_info());
	} else if (action == CPU_STARTING || action == CPU_STARTING_FROZEN)
		vfp_enable(NULL);
	return NOTIFY_OK;
}

void vfp_kmode_exception(void)
{
	if (fmrx(FPEXC) & FPEXC_EN)
		pr_crit("BUG: unsupported FP instruction in kernel mode\n");
	else
		pr_crit("BUG: FP instruction issued in kernel mode with FP unit disabled\n");
}

static int __init vfp_init(void)
{
	unsigned int vfpsid;
	unsigned int cpu_arch = cpu_architecture();

	if (cpu_arch >= CPU_ARCH_ARMv6)
		on_each_cpu(vfp_enable, NULL, 1);
	vfp_vector = vfp_testing_entry;
	barrier();
	vfpsid = fmrx(FPSID);
	barrier();
	vfp_vector = vfp_null_entry;

	printk(KERN_INFO "VFP v3: ");
	if (VFP_arch)
		printk("not present\n");
	else if (vfpsid & FPSID_NODOUBLE) {
		printk("no double precision support\n");
	} else {
		hotcpu_notifier(vfp_hotplug, 0);
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
		if ((read_cpuid_id() & 0x000f0000) == 0x000f0000) {
#ifdef CONFIG_NEON
			if ((fmrx(MVFR1) & 0x000fff00) == 0x00011100)
				elf_hwcap |= HWCAP_NEON;
#endif
			if ((fmrx(MVFR1) & 0xf0000000) == 0x10000000)
				elf_hwcap |= HWCAP_VFPv4;
		}}
	return 0;
}
core_initcall(vfp_init);

