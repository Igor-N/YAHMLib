#include <palmos.h>
#include <PceNativeCall.h>
#include "endianutils.h"
#include "yahm_lib.h"
#define NOT_PILRC
#include "yahm_int.h"
#include "yahm_internals.h"

////////////////////////////////////////////////////////////////////////////////
typedef struct ArmParameterBlock{
	YAHM_SyscallInfo5 callInfo;
	UInt32 set;
	UInt32 newAddr;
}ArmParameterBlock; 
////////////////////////////////////////////////////////////////////////////////
static void *PrvCallGetSet(UInt32 base, UInt32 offset, Boolean set, void *addr){
	DECLARE_STRUCT_WITH_ARM_ALIGN(ArmParameterBlock, pSyscallInfo)
	MemHandle h;
	NativeFuncType *p;
	void *res;

	h = DmGetResource(HACK_ARM_RES_TYPE, YAHM_SET_TRAP_RES_ID);
	if (h == NULL){
		return 0; //hackErrNoLibraryArmlet;
	}
	p = MemHandleLock(h);
	if (set){
		pSyscallInfo->newAddr = SwapPtr32(addr);
	}
	pSyscallInfo->set = set;
	pSyscallInfo->callInfo.baseTableOffset = base;
	pSyscallInfo->callInfo.offset = offset;
	res = (void *)PceNativeCall(p, pSyscallInfo);
	MemHandleUnlock(h);
	DmReleaseResource(h);
	return res;
}
////////////////////////////////////////////////////////////////////////////////
void *YAHM_GetTrapAddress(UInt32 base, UInt32 offset){
	return PrvCallGetSet(base, offset, false, NULL);
}
////////////////////////////////////////////////////////////////////////////////
void *YAHM_SetTrapAddress(UInt32 base, UInt32 offset, void *addr){
	return PrvCallGetSet(base, offset, true, addr);
}
 