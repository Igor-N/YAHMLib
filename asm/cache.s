.global HALInvalidateICacheEx
HALInvalidateICacheEx:
	mrs r0, cpsr
	ands r0,r0,#F
	beq with_syscall
	mov	r0,#0
	mcr	p15,0,r0,c7,c10,4
	mcr	p15,0,r0,c7,c7,0
@	mov	r0, #0
	bx lr

with_syscall:
@	stmfd sp!, {lr}
	ldr	r12, [r9, #-4]
@	mov	lr, pc
	ldr	pc, [r12, #0x2E8]
@	mov	r0, #1
@	ldmfd sp!, {pc}
