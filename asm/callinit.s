	@ save r10 and lr	
	stmfd sp!, {r10, lr}
	@ retrieve parameters: address and got ptr
	ldr r10, [r1, #4]		@ got
	ldr r1, [r1, #0] 		@ param
	@ move isInit to r0
	and	r0,r1,#1 
	@ move call address to r1
	bic r1,r1,#1
	@ save values and flush cache
	stmfd sp!, {r0,r1}
	mov	r1, #0
	bl HALInvalidateICacheEx
	ldmfd sp!, {r0, r1}
	@ call routine
	mov lr, pc
	mov pc, r1
	@ restore lr and return
	ldmfd sp!, {r10, pc}

