#include <linux/linkage.h>
#include <linux/init.h>
#include <asm/assembler.h>
#include <asm/domain.h>
#include <asm/ptrace.h>
#include <asm/asm-offsets.h>
#include <asm/memory.h>
#include <asm/thread_info.h>
#include <asm/system.h>

#if (PHYS_OFFSET & 0x001fffff)
#error "PHYS_OFFSET must be at an even 2MiB boundary!"
#endif

#define KERNEL_RAM_VADDR	(PAGE_OFFSET + TEXT_OFFSET)
#define KERNEL_RAM_PADDR	(PHYS_OFFSET + TEXT_OFFSET)

#if (KERNEL_RAM_VADDR & 0xffff) != 0x8000
#error KERNEL_RAM_VADDR must start at 0xXXXX8000
#endif
	.globl	swapper_pg_dir
	.equ	swapper_pg_dir, KERNEL_RAM_VADDR - 0x4000
	.macro	pgtbl, rd
	ldr	\rd, =(KERNEL_RAM_PADDR - 0x4000)
	.endm

#ifdef CONFIG_XIP_KERNEL
#define KERNEL_START	XIP_VIRT_ADDR(CONFIG_XIP_PHYS_ADDR)
#define KERNEL_END	_edata_loc
#else
#define KERNEL_START	KERNEL_RAM_VADDR
#define KERNEL_END	_end
#endif
	.section ".text.head", "ax"
ENTRY(stext)
	msr	cpsr_c, #PSR_F_BIT | PSR_I_BIT | SVC_MODE @ ensure svc mode
						@ and irqs disabled
	mrc	p15, 0, r9, c0, c0		@ get processor id
	bl	__lookup_processor_type		@ r5=procinfo r9=cpuid
	movs	r10, r5				@ invalid processor (r5=0)?
	beq	__error_p			@ yes, error 'p'
	bl	__lookup_machine_type		@ r5=machinfo
	movs	r8, r5				@ invalid machine (r5=0)?
	beq	__error_a			@ yes, error 'a'
	bl	__vet_atags
	bl	__create_page_tables
	ldr	r13, __switch_data		@ address to jump to after
						@ mmu has been enabled
	adr	lr, __enable_mmu		@ return (PIC) address
	add	pc, r10, #PROCINFO_INITFUNC
ENDPROC(stext)

#if defined(CONFIG_SMP)
ENTRY(secondary_startup)
	msr	cpsr_c, #PSR_F_BIT | PSR_I_BIT | SVC_MODE
	mrc	p15, 0, r9, c0, c0		@ get processor id
	bl	__lookup_processor_type
	movs	r10, r5				@ invalid processor?
	moveq	r0, #'p'			@ yes, error 'p'
	beq	__error
	adr	r4, __secondary_data
	ldmia	r4, {r5, r7, r12}		@ address to jump to after
	sub	r4, r4, r5			@ mmu has been enabled
	ldr	r4, [r7, r4]			@ get secondary_data.pgdir
	adr	lr, __enable_mmu		@ return address
	mov	r13, r12			@ __secondary_switched address
	add	pc, r10, #PROCINFO_INITFUNC	@ initialise processor
ENDPROC(secondary_startup)

ENTRY(__secondary_switched)
	ldr	sp, [r7, #4]			@ get secondary_data.stack
	mov	fp, #0
	b	secondary_start_kernel
ENDPROC(__secondary_switched)

	.type	__secondary_data, %object
__secondary_data:
	.long	.
	.long	secondary_data
	.long	__secondary_switched
#endif

__enable_mmu:
#ifdef CONFIG_ALLOW_CPU_ALIGNMENT
	bic	r0, r0, #CR_A
#else
	orr	r0, r0, #CR_A
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
	mov	r5, #(domain_val(DOMAIN_USER, DOMAIN_CLIENT) | \
		      domain_val(DOMAIN_KERNEL, DOMAIN_MANAGER) | \
		      domain_val(DOMAIN_TABLE, DOMAIN_CLIENT) | \
		      domain_val(DOMAIN_IO, DOMAIN_CLIENT))
	mcr	p15, 0, r5, c3, c0, 0		@ load domain access register
	mcr	p15, 0, r4, c2, c0, 0		@ load page table pointer
	b	__turn_mmu_on
ENDPROC(__enable_mmu)

	.align	5
__turn_mmu_on:
	mov	r0, r0
	mcr	p15, 0, r0, c1, c0, 0		@ write control reg
	mrc	p15, 0, r3, c0, c0, 0		@ read id reg
	mov	r3, r3
	mov	r3, r13
	mov	pc, r3
ENDPROC(__turn_mmu_on)

__create_page_tables:
	pgtbl	r4				@ page table address
	mov	r0, r4
	mov	r3, #0
	add	r6, r0, #0x4000
1:	str	r3, [r0], #4
	str	r3, [r0], #4
	str	r3, [r0], #4
	str	r3, [r0], #4
	teq	r0, r6
	bne	1b
	ldr	r7, [r10, #PROCINFO_MM_MMUFLAGS] @ mm_mmuflags
	mov	r6, pc
	mov	r6, r6, lsr #20			@ start of kernel section
	orr	r3, r7, r6, lsl #20		@ flags + kernel base
	str	r3, [r4, r6, lsl #2]		@ identity mapping
	add	r0, r4,  #(KERNEL_START & 0xff000000) >> 18
	str	r3, [r0, #(KERNEL_START & 0x00f00000) >> 18]!
	ldr	r6, =(KERNEL_END - 1)
	add	r0, r0, #4
	add	r6, r4, r6, lsr #18
1:	cmp	r0, r6
	add	r3, r3, #1 << 20
	strls	r3, [r0], #4
	bls	1b

#ifdef CONFIG_XIP_KERNEL
	orr	r3, r7, #(KERNEL_RAM_PADDR & 0xff000000)
	.if	(KERNEL_RAM_PADDR & 0x00f00000)
	orr	r3, r3, #(KERNEL_RAM_PADDR & 0x00f00000)
	.endif
	add	r0, r4,  #(KERNEL_RAM_VADDR & 0xff000000) >> 18
	str	r3, [r0, #(KERNEL_RAM_VADDR & 0x00f00000) >> 18]!
	ldr	r6, =(_end - 1)
	add	r0, r0, #4
	add	r6, r4, r6, lsr #18
1:	cmp	r0, r6
	add	r3, r3, #1 << 20
	strls	r3, [r0], #4
	bls	1b
#endif
	add	r0, r4, #PAGE_OFFSET >> 18
	orr	r6, r7, #(PHYS_OFFSET & 0xff000000)
	.if	(PHYS_OFFSET & 0x00f00000)
	orr	r6, r6, #(PHYS_OFFSET & 0x00f00000)
	.endif
	str	r6, [r0]

#ifdef CONFIG_DEBUG_LL
	ldr	r7, [r10, #PROCINFO_IO_MMUFLAGS] @ io_mmuflags
	ldr	r3, [r8, #MACHINFO_PGOFFIO]
	add	r0, r4, r3
	rsb	r3, r3, #0x4000			@ PTRS_PER_PGD*sizeof(long)
	cmp	r3, #0x0800			@ limit to 512MB
	movhi	r3, #0x0800
	add	r6, r0, r3
	ldr	r3, [r8, #MACHINFO_PHYSIO]
	orr	r3, r3, r7
1:	str	r3, [r0], #4
	add	r3, r3, #1 << 20
	teq	r0, r6
	bne	1b
#if defined(CONFIG_ARCH_NETWINDER) || defined(CONFIG_ARCH_CATS)
	add	r0, r4, #0xff000000 >> 18
	orr	r3, r7, #0x7c000000
	str	r3, [r0]
#endif
#ifdef CONFIG_ARCH_RPC
	add	r0, r4, #0x02000000 >> 18
	orr	r3, r7, #0x02000000
	str	r3, [r0]
	add	r0, r4, #0xd8000000 >> 18
	str	r3, [r0]
#endif
#endif
	mov	pc, lr
ENDPROC(__create_page_tables)
	.ltorg
#include "head-common.S"

