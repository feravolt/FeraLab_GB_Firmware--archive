#ifndef __ASSEMBLY__
#error "Only include this from assembly code"
#endif

#include <asm/ptrace.h>

#ifndef __ARMEB__
#define pull            lsr
#define push            lsl
#define get_byte_0      lsl #0
#define get_byte_1	lsr #8
#define get_byte_2	lsr #16
#define get_byte_3	lsr #24
#define put_byte_0      lsl #0
#define put_byte_1	lsl #8
#define put_byte_2	lsl #16
#define put_byte_3	lsl #24
#else
#define pull            lsl
#define push            lsr
#define get_byte_0	lsr #24
#define get_byte_1	lsr #16
#define get_byte_2	lsr #8
#define get_byte_3      lsl #0
#define put_byte_0	lsl #24
#define put_byte_1	lsl #16
#define put_byte_2	lsl #8
#define put_byte_3      lsl #0
#endif

#if __LINUX_ARM_ARCH__ >= 5
#define PLD(code...)	code
#else
#define PLD(code...)
#endif
#ifdef CONFIG_CPU_FEROCEON
#define CALGN(code...) code
#else
#define CALGN(code...)
#endif
#if __LINUX_ARM_ARCH__ >= 6
	.macro	disable_irq
	cpsid	i
	.endm
	.macro	enable_irq
	cpsie	i
	.endm
#else
	.macro	disable_irq
	msr	cpsr_c, #PSR_I_BIT | SVC_MODE
	.endm
	.macro	enable_irq
	msr	cpsr_c, #SVC_MODE
	.endm
#endif
	.macro	save_and_disable_irqs, oldcpsr
	mrs	\oldcpsr, cpsr
	disable_irq
	.endm
	.macro	restore_irqs, oldcpsr
	msr	cpsr_c, \oldcpsr
	.endm

#define USER(x...)				\
9999:	x;					\
	.section __ex_table,"a";		\
	.align	3;				\
	.long	9999b,9001f;			\
	.previous

	.macro	smp_dmb
#ifdef CONFIG_SMP
#if __LINUX_ARM_ARCH__ >= 7
	dmb
#elif __LINUX_ARM_ARCH__ == 6
	mcr	p15, 0, r0, c7, c10, 5	@ dmb
#endif
#endif
	.endm