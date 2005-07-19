start:
	jump_addr = start + 56
	ldr pc, [pc, #jump_addr-.-8]
