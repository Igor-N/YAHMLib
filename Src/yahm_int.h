#ifndef _YAHM_INT_H_
#define _YAHM_INT_H_

////////////////////////////////////////////////////////////////////////////////
// defines
#define HACK_ARM_RES_TYPE	'armc'
#define HACK_GOT_RES_TYPE	'.got'

#define YAHM_FAT_THUNK_RES_ID	9900
#define YAHM_SHORT_RES_ID	9901
#define YAHM_SHORT_OLD_RES_ID	9902
#define YAHM_CW_THUNK_RES_ID	9903

#define YAHM_INIT_RES_ID	9998
#define YAHM_SET_TRAP_RES_ID 9999

#define HACK_CODE_INIT 999
#define HACK_CODE_RESOURCE_START 1000
#define TRAP_RESOURCE_TYPE5 'TRA5'
#define SAVE_STACK_SIZE 40

#ifdef NOT_PILRC
////////////////////////////////////////////////////////////////////////////////
// align macros

#define ALIGNED_STRUCT_SIZE(structType) (sizeof(structType) + 3)
#define ALIGNED_ADDRESS(address) (void *)((((UInt32)address) + 3) & (~3))
#define DECLARE_STRUCT_WITH_ARM_ALIGN(structType, ptrName) \
	char ptrName##buf[ALIGNED_STRUCT_SIZE(structType)]; \
	structType *ptrName = (structType *)ALIGNED_ADDRESS(ptrName##buf);
#endif

#endif
