	.syntax unified

@ A second thread for Roguémon's map generator, so maps can be generated in
@ the spare time at the end of each frame (see src/rogue.c).
@
@ void RogueThread_Switch(u32 *saveSp, u32 newSp)
@ Saves the callee-saved registers and the return address on the current
@ stack, stores the stack pointer in *saveSp, switches to newSp and returns
@ to whatever that stack was switched away from (or to the entry function
@ placed on a new stack).

	.arm
	.section .text
	.align 2

.global RogueThread_Switch
.type RogueThread_Switch, %function
RogueThread_Switch:
	push {r4-r11, lr}
	str sp, [r0]
	mov sp, r1
	pop {r4-r11, lr}
	bx lr
