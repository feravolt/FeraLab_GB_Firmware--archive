#include <linux/linkage.h>
#include <linux/init.h>
#include <asm/assembler.h>
#include <asm/ptrace.h>
#include <asm/asm-offsets.h>
#include <asm/thread_info.h>
#include <asm/system.h>

	.section ".text.head", "ax"
ENTRY(stext)
	msr	cpsr_c, #PSR_F_BIT | PSR_I_BIT | SVC_MODE @ ensure svc mode
						@ and irqs disabled
#ifndef CONFIG_CPU_CP15
	ldr	r9, =CONFIG_PROCESSOR_ID
#else
	mrc	p15, 0, r9, c0, c0		@ get processor id
#endif
	bl	__lookup_processor_type		@ r5=procinfo r9=cpuid
	movs	r10, r5				@ invalid processor (r5=0)?
	beq	__error_p				@ yes, error 'p'
	bl	__lookup_machine_type		@ r5=machinfo
	movs	r8, r5				@ invalid machine (r5=0)?
	beq	__error_a			@ yes, error 'a'
	ldr	r13, __switch_data		@ address to jump to after
	adr	lr, __after_proc_init		@ return (PIC) address
	add	pc, r10, #PROCINFO_INITFUNC
ENDPROC(stext)

__after_proc_init:
#ifdef CONFIG_CPU_CP15
#ifdef CONFIG_ALIGNMENT_TRAP
	orr	r0, r0, #CR_A
#else
	bic	r0, r0, #CR_A
#endif
#ifdef CONFIG_CPU_DCACHE_DISABLE
	bic	r0, r0, #CR_C
#endif
#ifdef CONFIG_CPU_BPREDICT_DISABLE
	bic	r0, r0, #CR_Z
#endif
#ifdef CONFIG_CPU_ICACHE_DISABLE
	bic	r0, r0, #CR_I
#endif
#ifdef CONFIG_CPU_HIGH_VECTOR
	orr	r0, r0, #CR_V
#else
	bic	r0, r0, #CR_V
#endif
	mcr	p15, 0, r0, c1, c0, 0		@ write control reg
#endif /* CONFIG_CPU_CP15 */
	mov	r3, r13
	mov	pc, r3				@ clear the BSS and jump
						@ to start_kernel
ENDPROC(__after_proc_init)
	.ltorg

#include "head-common.S"

