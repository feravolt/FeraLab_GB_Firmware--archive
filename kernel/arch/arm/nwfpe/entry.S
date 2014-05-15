#include <asm/opcodes.h>
#include <asm/asm-offsets.h>

	.globl	nwfpe_enter
nwfpe_enter:
	mov	r4, lr			@ save the failure-return addresses
	mov	sl, sp			@ we access the registers via 'sl'

	ldr	r5, [sp, #S_PC]		@ get contents of PC;
	mov	r6, r0			@ save the opcode
emulate:
	ldr	r1, [sp, #S_PSR]	@ fetch the PSR
	bl	checkCondition		@ check the condition
	cmp	r0, #ARM_OPCODE_CONDTEST_PASS	@ condition passed?

	@ if condition code failed to match, next insn
	bne	next			@ get the next instruction;

	mov	r0, r6			@ prepare for EmulateAll()
	bl	EmulateAll		@ emulate the instruction
	cmp	r0, #0			@ was emulation successful
	moveq	pc, r4			@ no, return failure

next:
.Lx1:	ldrt	r6, [r5], #4		@ get the next instruction and
					@ increment PC

	and	r2, r6, #0x0F000000	@ test for FP insns
	teq	r2, #0x0C000000
	teqne	r2, #0x0D000000
	teqne	r2, #0x0E000000
	movne	pc, r9			@ return ok if not a fp insn

	str	r5, [sp, #S_PC]		@ update PC copy in regs

	mov	r0, r6			@ save a copy
	b	emulate			@ check condition and emulate

	@ We need to be prepared for the instructions at .Lx1 and .Lx2 
	@ to fault.  Emit the appropriate exception gunk to fix things up.
	@ ??? For some reason, faults can happen at .Lx2 even with a
	@ plain LDR instruction.  Weird, but it seems harmless.
	.pushsection .fixup,"ax"
	.align	2
.Lfix:	mov	pc, r9			@ let the user eat segfaults
	.popsection

	.pushsection __ex_table,"a"
	.align	3
	.long	.Lx1, .Lfix
	.popsection

