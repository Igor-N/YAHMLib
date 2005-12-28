@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ Thunk format:
@ 
@ typedef struct{
@ 	UInt8 op[4 * 14];
@ }JumpRoutine;
@ 
@ typedef struct{
@ 	UInt8 op[4];
@ }ShortJumpRoutine;
@
@ typedef struct{
@ 	// ������� �� ������!
@ 	// ��� �������� �� ��� ���
@ 	JumpRoutine hackJumpIns;
@ 	// ����� ���� ���� ����� ��������
@ 	UInt32 hackCodeAddress;
@ 	// ���� ��� ����� ����� GOT
@ 	UInt32 R10_GOT;
@ 	UInt32 stackPtr;
@ 	// ��� �������� �� ������ �������
@ 	// �� ���������� ��� ����� ����� ���!!!
@ 	ShortJumpRoutine oldJumpIns;
@ 	// ����� ���� ���� ����� ��������
@ 	UInt32 oldestAddress;
@	UInt32 lockCount;
@ 	UInt32 creator;
@ 	YAHM_SyscallInfo5 syscallInfo;
@ }JumpThunkOS5;
@ 
@
@
@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
thunk_hack_address_offset = 56
thunk_r10_offset = thunk_hack_address_offset + 4
thunk_stack_offset = thunk_r10_offset + 4
thunk_old_trap_address_offset = thunk_stack_offset + 4
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ complex thunk with stack for registers
.section FullToNew, "ax"

.macro push_regs_to_spec_stack st_addr, st_reg, regs
    @ �������� ��������� ����� � ������� st_reg
	ldr		\st_reg, [\st_addr]
	@ ��������� ������ ��������� � �����
	stmfd	\st_reg!, \regs
	@ ��������� ��������� �����
	str		\st_reg, [\st_addr]
.endm

.macro pop_regs_from_spec_stack st_addr, st_reg, regs
    @ �������� ��������� ����� � ������� st_reg
	ldr		\st_reg, [\st_addr]
	@ ������������ �������� �� �����
	ldmfd	\st_reg!, \regs
	@ ��������� ��������� �����
	str		\st_reg, [\st_addr]
.endm


start_fat:

	@ ��������� ��������
	stmfd	sp!, {r1, r2} 
	@ ��������� � r2 ��������� �� ��������� ���������� �����
	ldr		r2, [pc, #start_fat + thunk_stack_offset - . - 8]	
	@ ��������� � ����� R10 � LR
	push_regs_to_spec_stack	r2, r1, "{r10, lr}"
	@ ������������ ��������
	ldmfd	sp!, {r1, r2}

    @ ��������� � R10 ����� ��������
	ldr		r10,[pc, #start_fat + thunk_r10_offset - . - 8]
    @ ������� ����� ����������
	mov		lr, pc
	ldr		pc, [pc, #start_fat + thunk_hack_address_offset - . - 8]

@--- ����� �������� �� ������� �����������

    @ ��������� � r2 ��������� �� ��������� ���������� �����
	ldr		r2, [pc, #start_fat + thunk_stack_offset - . - 8]	
	@ ������������ ������������ R10 � LR
	pop_regs_from_spec_stack r2, r1, "{r10, lr}"

	bx		lr

end_of_fat_thunk = .


.if end_of_fat_thunk - start_fat <> thunk_hack_address_offset
@ thunk size mismatch
.err
.endif

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ CW thunk. 
.section CWToNew, "ax"

start_cw:
	stmdb	sp!, {r10, lr}
	ldr		r10, [pc, #start_cw + thunk_r10_offset - . - 8]
	mov		lr, pc
	ldr		pc, [pc, #start_cw + thunk_hack_address_offset - . - 8]
@--- ����� �������� �� ������� �����������
	ldmia	sp!, {r10, lr}
	bx		lr

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ CW thunk. 

.section CW1ToNew, "ax"

start_cw1:
	stmdb	sp!, {r10, lr}
@ ��������� ��������� �� thunkReturnCode. �� ��������� �� 4 ����� ���� ��������� �����
	ldr		lr, [pc, #start_cw1 + thunk_stack_offset - . - 8]
	add		lr, lr, #4
	ldr		r10, [pc, #start_cw1 + thunk_r10_offset - . - 8]
	ldr		pc, [pc, #start_cw1 + thunk_hack_address_offset - . - 8]
@--- ����� �������� �� ������� �����������


@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ fast thunk to jump to new handler
.section ShortToNew, "ax"
start_short_to_new:
	ldr pc, [pc, #start_short_to_new + thunk_hack_address_offset - . - 8]


@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ FtrGet
.section FtrToNew, "ax"

start_ftr:
	stmfd	sp!, {r10, lr} 
	ldr		r10,[pc, #start_ftr + thunk_r10_offset - . - 8]
	adr		r3, start_ftr + thunk_old_trap_address_offset
@ ��������� ��������� �� thunkReturnCode. �� ��������� �� 4 ����� ���� ��������� �����
	ldr		lr, [pc, #start_ftr + thunk_stack_offset - . - 8]
	add		lr, lr, #4
	ldr		pc, [pc, #start_ftr + thunk_hack_address_offset - . - 8]


@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ LRInR3
.section LRInR3, "ax"

start_lrinr3:
	stmfd	sp!, {r10, lr} 
	ldr		r10, [pc, #start_lrinr3 + thunk_r10_offset - . - 8]
	mov		r3, lr
@ ��������� ��������� �� thunkReturnCode. �� ��������� �� 4 ����� ���� ��������� �����
	ldr		lr, [pc, #start_lrinr3 + thunk_stack_offset - . - 8]
	add		lr, lr, #4
	ldr		pc, [pc, #start_lrinr3 + thunk_hack_address_offset - . - 8]




@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ simplest thunk to jump to old handler
.section ShortToOld, "ax"
	ldr pc, [pc, #jump_addr_to_old-.-8]
jump_addr_to_old:

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ return from thunk
.section ThunkReturn, "ax"
	ldmfd	sp!, {r10, lr}
	bx		lr
