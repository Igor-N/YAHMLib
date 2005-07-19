@ with stack

.macro push_regs_to_spec_stack st_addr, st_reg, regs
	ldr		\st_reg, [\st_addr]
	stmfd	\st_reg!, \regs
	str		\st_reg, [\st_addr]
.endm

.macro pop_regs_from_spec_stack st_addr, st_reg, regs
	ldr		\st_reg, [\st_addr]
	ldmfd	\st_reg!, \regs
	str		\st_reg, [\st_addr]
.endm


thunk_to_hack_with_stack:

	stmfd	sp!, {r1, r2} 
	ldr		r2, [pc, #hack_stack_address-.-8]	
	push_regs_to_spec_stack	r2, r1, "{r10, lr}"
	ldmfd	sp!, {r1, r2} 

	ldr		r10,[pc, #hack_got_address-.-8]

	mov		lr, pc
	ldr		pc, [pc, #hack_proc_address-.-8]

	ldr		r2, [pc, #hack_stack_address-.-8]	
	pop_regs_from_spec_stack r2, r1, "{r10, lr}"
	bx		lr

hack_proc_address=.
hack_got_address=.+4
hack_stack_address=.+8
