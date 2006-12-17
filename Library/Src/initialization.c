#include <palmos.h>
#include <PceNativeCall.h>
#include "endianutils.h"
#include "yahm_lib.h"
#define NOT_PILRC
#include "yahm_int.h"
#include "yahm_internals.h"
#ifdef YAHM_ITSELF
#include "log.h"
#include "yahmrs.h"
#endif

////////////////////////////////////////////////////////////////////////////////
typedef struct{
	UInt32 param;
	UInt32 gotPtr;
}InitParam;
////////////////////////////////////////////////////////////////////////////////
// pInitializationCode must be locked pointer to start of chunk
static Err PrvExecuteInitialization(void *pInitializationCode, Boolean init, UInt32 *pRes){
	Err err = errNone;
	void *pRelocatedInitializationCode;
	
	MemHandle hInitializationStub;
	NativeFuncType *pInitializationStub;
	
	MemHandle hUnrelocatedGotSection;
	
	DECLARE_STRUCT_WITH_ARM_ALIGN(InitParam, par);
	
	*pRes = 0;
	
	// no code, no initialization
	if (pInitializationCode == NULL){
		return err;
	}
	// load arm code for calling initialization function from relocated resource
	hInitializationStub = DmGetResource(HACK_ARM_RES_TYPE, YAHM_INIT_RES_ID);
	if (hInitializationStub == NULL){
		err = hackErrNoLibraryArmlet;
		return err;
	}
	pInitializationStub = MemHandleLock(hInitializationStub);
	// load .got if exist.
	hUnrelocatedGotSection = DmGetResource(HACK_GOT_RES_TYPE, HACK_CODE_INIT);
	par->gotPtr = 0;
	pRelocatedInitializationCode = YAHM_FixupGccCode(hUnrelocatedGotSection, pInitializationCode, REINTERPRET_CAST(void **, &par->gotPtr));

	if (pRelocatedInitializationCode == NULL){
		err = memErrNotEnoughSpace;
		goto Exit;
	}
	par->param = REINTERPRET_CAST(UInt32, pRelocatedInitializationCode) | (init ? 1 : 0);
	par->param = ByteSwap32(par->param);
	par->gotPtr = ByteSwap32(par->gotPtr);
	*pRes = PceNativeCall(pInitializationStub, par);
	
	if (pInitializationCode != pRelocatedInitializationCode){
		//BUGFIX: if YAHM_FixupGccCode does nothing and returns original pointer, 
		//YAHM_FreeRelocatedChunk does more than it should.
		YAHM_FreeRelocatedChunk(pRelocatedInitializationCode);
	}
	
Exit:	
	MemHandleUnlock(hInitializationStub);
	DmReleaseResource(hInitializationStub);
	
	if (hUnrelocatedGotSection != 0){
		DmReleaseResource(hUnrelocatedGotSection);
	}
	return err;
}
////////////////////////////////////////////////////////////////////////////////
Err YAHM_ExecuteInitializationEx(MemHandle hInitializationResource, Boolean init){
	void *pInitializationCode;
	UInt32 res = 0;
	Err err = errNone;

	if (hInitializationResource == NULL){
		return errNone;
	}

	pInitializationCode = MemHandleLock(hInitializationResource);
	err = PrvExecuteInitialization(pInitializationCode, init, &res);
	MemHandleUnlock(hInitializationResource);
	return (err != errNone) ? err : (res ? errNone : hackErrInitializationFailed);
}
////////////////////////////////////////////////////////////////////////////////
Err YAHM_ExecuteInitialization(void *pInitializationCode, Boolean init){
	UInt32 res;
	Err err;

	err = PrvExecuteInitialization(pInitializationCode, init, &res);
	return (err != errNone) ? err : (res ? errNone : hackErrInitializationFailed);
}

