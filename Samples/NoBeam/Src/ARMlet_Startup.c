// ARMlet_Startup.c
//
// Copyright (C) 2002-2003, Metrowerks Corporation
// All rights reserved

#pragma thumb off
#pragma PIC   off

extern void ARMlet_Main(void);
extern void __ARMlet_Startup__(void);
extern void * __ARMlet_Take_Func_Addr__(void *f);

// these symbols aren't really functions, but are linker-generated symbols
// that mark the start and end of the various data sections
extern void __DataStart__(void);
extern void __RODataStart__(void);
extern void __BSSStart__(void);
extern void __BSSEnd__(void);
extern void __CodeRelocStart__(void);
extern void __CodeRelocEnd__(void);
extern void __DataRelocStart__(void);
extern void __DataRelocEnd__(void);

// This __ARMlet_Startup__ code relies on never being called
// as the entry point to a function that has arguments on the
// stack, since it uses the stack to save the original value
// of the R10 register.

// when reading this code, remember that when PC is read in an 
// instruction, its value is 8 bytes ahead of the instruction
// that's executing (unless there's a shift address mode).  This is why
// the offsets in the instructions are larger than you would expect.

asm void __ARMlet_Startup__(void)
{
    stmdb    sp!, {r10, lr}      //   0 save R10 and LR for restoration upon return
    sub      r10, pc, #12        //   4 setup R10 to point to start of PNO
    ldr      r12, [pc, #16]      //   8 R12 gets the address of ARMlet_Main
    add      r12, r12, r10       //  12 add in the PNO base
    mov      lr, pc              //  16 setup return address
    bx       r12                 //  20 jump to entry, switching modes if needed
    ldmia    sp!, {r10, lr}      //  24 restore R10 and LR
    bx       lr                  //  28 return to caller
    dcd      ARMlet_Main         //  32
    dcd      'cdwr'              //  36
    dcd      __DataStart__       //  40
    dcd      __RODataStart__     //  44
    dcd      __BSSStart__        //  48
    dcd      __BSSEnd__          //  52
    dcd      __CodeRelocStart__  //  56
    dcd      __CodeRelocEnd__    //  60
    dcd      __DataRelocStart__  //  64
    dcd      __DataRelocEnd__    //  68
    stmdb    sp!, {r10, lr}      //  72 save R10 and LR for restoration upon return
    ldr      r10, [r1,#0]        //  76 R10 is loaded from the first item in the "caller" structure
    ldr      r12, [r1,#4]        //  80 R12 gets the address of ARMlet_Main set by loader
	ldr      r1,  [r1,#8]        //  84 load R1 with the original caller value
    mov      lr, pc              //  88 setup return address
    bx       r12                 //  92 jump to entry, switching modes if needed
    ldmia    sp!, {r10, lr}      //  96 restore R10 and LR
    bx       lr                  // 100 return to caller
}
    
asm void * __ARMlet_Take_Func_Addr__(void *f)
{
    sub     r0, r0, r10         //  0 convert pointer to zero-based address
    ldr     r12, [pc, #8]       //  4 load zero-based address of this routine plus offset into r12
    sub     r12, pc, r12        //  8 compute start of PNO by subtracting this from PC
    add     r0, r0, r12         // 12 add PNO start to function pointer
    bx      lr                  // 16 return to caller
    dcd     __ARMlet_Take_Func_Addr__ + 16    // 20
}

#pragma thumb reset
