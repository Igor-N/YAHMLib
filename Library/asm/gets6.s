start:
	jump_addr = start + 56
	r10_addr = jump_addr + 4

	stmdb	sp!, {r10, lr}
	ldr		r10, [pc, #r10_addr-.-8]
	ldr		r12, [pc, #jump_addr-.-8]
	mov		lr, pc
	bx		r12
	ldmia	sp!, {r10, lr}
	bx		lr

