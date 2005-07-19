#include <palmos.h>
#include <PceNativeCall.h>
#include "endianutils.h"
#include "yahm_lib.h"
#define NOT_PILRC
#include "yahm_int.h"

#define POOL_VERSION 6
#ifdef __MWERKS__
#pragma warn_a5_access on
#endif
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
typedef struct ArmParameterBlock{
	YAHM_SyscallInfo5 callInfo;
	UInt32 set;
	UInt32 newAddr;
}ArmParameterBlock; 
////////////////////////////////////////////////////////////////////////////////
typedef struct{
	UInt8 op[4 * 14];
}JumpRoutine;

typedef struct{
	UInt8 op[4];
}ShortJumpRoutine;

typedef struct{
	// don't change order!
	// jump to patch code
	JumpRoutine hackJumpIns;
	// patch code address
	UInt32 hackCodeAddress;
	// R10 value
	UInt32 R10_GOT;
	// pointer to stack pointer
	UInt32 stackPtr;
	// jump to old trap code
	ShortJumpRoutine oldJumpIns;
	// old trap code address
	UInt32 oldestAddress;
	// information fields
	UInt32 creator;
	YAHM_SyscallInfo5 syscallInfo;
}JumpThunkOS5;

////////////////////////////////////////////////////////////////////////////////
typedef struct{
	UInt32 version;
	UInt32 poolSize;
	UInt32 maxCurSize;
	UInt32 *stackBottom;
	UInt32 *saveStackPtr;
	JumpThunkOS5 thunks[1];
}ThunkPoolOS5;
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
#define POOL_SIZE_IN_BYTES(cnt) (sizeof(ThunkPoolOS5) + ((cnt) - 1) * sizeof(JumpThunkOS5))
////////////////////////////////////////////////////////////////////////////////
typedef struct{
	ThunkPoolOS5 *pPool;
}ThunkStateOS5;

////////////////////////////////////////////////////////////////////////////////
static inline void ClearState(ThunkStateOS5 *pts){
	pts->pPool = 0;
}
////////////////////////////////////////////////////////////////////////////////
/*
	Thunk idea:
	trap->toEarliest->realHandler->toOldest
	trap points to earliest thunk
	earliest thunk points to handler
	handler points to feature
	feature hold oldest thunk address
	oldest thunk jumps to previous handler
	
	After removing:
	trap points to earliest thunk
	earliest thunk points to OLDEST address
	oldest thunk jumps to previous handler
	
*/

////////////////////////////////////////////////////////////////////////////////
static void InitThunks(void);
#if 0
static void DestroyThunks(void);
#endif
static JumpThunkOS5 *PrvGetFreeThunk(ThunkPoolOS5 **pts);
static JumpThunkOS5 *PrvFindThunkByPrevAddress(ThunkPoolOS5 **ppts, void *prevAddress, Boolean isPrev);
static void PrvSqueezeThunks(ThunkStateOS5 *pts);
static Err PrvGetTrapNum(YAHM_SyscallInfo5 *pTrapInfo, MemHandle hTrapInfo);

////////////////////////////////////////////////////////////////////////////////
static Err PrvCopyThunk(void *buf, UInt16 id){
	MemHandle h = DmGet1Resource(HACK_ARM_RES_TYPE, id);
	void *p;
	if (h == 0){
		return hackErrNoLibraryArmlet;
	}
	p = MemHandleLock(h);
	MemMove(buf, p, MemHandleSize(h));
	MemHandleUnlock(h);
	DmReleaseResource(h);
	return errNone;
}
////////////////////////////////////////////////////////////////////////////////
void *YAHM_GetTrapAddress(UInt32 base, UInt32 offset){
	DECLARE_STRUCT_WITH_ARM_ALIGN(ArmParameterBlock, pSyscallInfo)
	MemHandle h;
	void *p;
	void *res;

	h = DmGetResource(HACK_ARM_RES_TYPE, YAHM_SET_TRAP_RES_ID);
	if (h == NULL){
		return 0; //hackErrNoLibraryArmlet;
	}
	p = MemHandleLock(h);
	pSyscallInfo->callInfo.baseTableOffset = base;
	pSyscallInfo->callInfo.offset = offset;
	pSyscallInfo->set = 0;
	res = (void *)PceNativeCall((NativeFuncType *)p, pSyscallInfo);
	MemHandleUnlock(h);
	DmReleaseResource(h);
	return res;
}
////////////////////////////////////////////////////////////////////////////////
void *YAHM_SetTrapAddress(UInt32 base, UInt32 offset, void *addr){
	DECLARE_STRUCT_WITH_ARM_ALIGN(ArmParameterBlock, pSyscallInfo)
	MemHandle h;
	void *p;
	void *res;

	pSyscallInfo->callInfo.baseTableOffset = base;
	pSyscallInfo->callInfo.offset = offset;
	pSyscallInfo->set = 1;
	pSyscallInfo->newAddr = ByteSwap32(((UInt32)addr));
	h = DmGetResource(HACK_ARM_RES_TYPE, YAHM_SET_TRAP_RES_ID);
	if (h == NULL){
		return 0; //hackErrNoLibraryArmlet;
	}
	p = MemHandleLock(h);
	res = (void *)PceNativeCall((NativeFuncType *)p, pSyscallInfo);
	MemHandleUnlock(h);
	DmReleaseResource(h);
	return res;
}

////////////////////////////////////////////////////////////////////////////////
static Err PrvGetTrapNum(YAHM_SyscallInfo5 *pTrapInfo, MemHandle hTrapInfo){
	if ((hTrapInfo == NULL) || (MemHandleSize(hTrapInfo) < sizeof(YAHM_SyscallInfo5))){
		MemSet(pTrapInfo, sizeof(YAHM_SyscallInfo5), 0);
		return hackErrWrongTrapInfo;
	}
	*pTrapInfo = *(YAHM_SyscallInfo5 *)MemHandleLock(hTrapInfo);
	MemHandleUnlock(hTrapInfo);	
	return errNone;
}
////////////////////////////////////////////////////////////////////////////////
static inline Boolean IS_FREE_THUNK(JumpThunkOS5 *pThunk){
	return pThunk->hackCodeAddress == 0;
}
////////////////////////////////////////////////////////////////////////////////
static inline Boolean IS_DUMMY_THUNK(JumpThunkOS5 *pThunk){
	return ByteSwap32(pThunk->hackCodeAddress) == (UInt32)&(pThunk->oldJumpIns);
}
////////////////////////////////////////////////////////////////////////////////
static inline void FreeThunk(JumpThunkOS5 *pThunk){
	pThunk->hackCodeAddress = 0;
	pThunk->creator = 'FREE';
	
}
////////////////////////////////////////////////////////////////////////////////
void InitThunks(void){
	YAHM_persistSettings settings;
	YAHM_runtimeSettings *pSettings = YAHM_GetRuntimeSettingsPtr();
	ThunkPoolOS5 * pPool;
	UInt32 *ppp;

	YAHM_GetPersistSettings(&settings);

	pPool = pSettings->pPool = MemPtrNew(POOL_SIZE_IN_BYTES(settings.thunkCount));
	ErrFatalDisplayIf(pPool == NULL, "no space for thunks");
	MemPtrSetOwner(pPool, 0);
	MemSet(pPool, POOL_SIZE_IN_BYTES(settings.thunkCount), 0);
	pPool->poolSize = settings.thunkCount;
	pPool->maxCurSize = 0;
	pPool->version = POOL_VERSION;
	ppp = MemPtrNew(sizeof(UInt32) * SAVE_STACK_SIZE);
	MemPtrSetOwner(ppp, 0);

	pPool->stackBottom = pPool->saveStackPtr = ppp + SAVE_STACK_SIZE;
	pPool->saveStackPtr = (UInt32 *)ByteSwap32(((UInt32)pPool->saveStackPtr));
}
////////////////////////////////////////////////////////////////////////////////
#if 0
void DestroyThunks(void){
	ThunkPool *pPool;
	pPool = YAHM_GetRuntimeSettingsPtr()->pPool;
	if (pPool != NULL)
		MemPtrFree(pPool);
}
#endif
////////////////////////////////////////////////////////////////////////////////
static void PrvCheckPoolVersion(ThunkPoolOS5 *pPool){
	if (pPool->version != POOL_VERSION){
		YAHM_warnAboutIncompatibleUpdate();
		SysReset();
	}
}

////////////////////////////////////////////////////////////////////////////////
JumpThunkOS5 *PrvGetFreeThunk(ThunkPoolOS5 **ppts){ 
	int i;
	if (*ppts == NULL){
		*ppts = YAHM_GetRuntimeSettingsPtr()->pPool;
		if (*ppts == NULL){
			InitThunks();
			*ppts = YAHM_GetRuntimeSettingsPtr()->pPool;
		}
		PrvCheckPoolVersion(*ppts);
	}
	for(i = 0; i < (*ppts)->poolSize; ++i){
		JumpThunkOS5 *pThunk = (*ppts)->thunks + i;
		if (IS_FREE_THUNK(pThunk)){
			if (i > (*ppts)->maxCurSize){
				(*ppts)->maxCurSize = i;
			}
			return pThunk;
		}
	}
	return NULL;
}
////////////////////////////////////////////////////////////////////////////////
JumpThunkOS5 *PrvFindThunkByPrevAddress(ThunkPoolOS5 **ppts, void *prevAddress, Boolean isPrev){
	UInt32 prevOffset;
	char *pp = (char *)prevAddress;
	char *pp0;
	JumpThunkOS5 *pThunk;
	if (isPrev){
		prevOffset = (UInt32)(&((JumpThunkOS5*)0)->oldJumpIns);
	}else{
		prevOffset = (UInt32)(&((JumpThunkOS5*)0)->hackJumpIns);
	}
	if (*ppts == NULL){
		*ppts = YAHM_GetRuntimeSettingsPtr()->pPool;
		if (*ppts == NULL)
			return NULL;
		PrvCheckPoolVersion(*ppts);
	}
	pp0 = (char *)(*ppts)->thunks;
	pp -= prevOffset; // ptr to current thunk;
	if ( ((pp - pp0) % sizeof(JumpThunkOS5)) == 0){
		Int32 idx;
		pThunk = (JumpThunkOS5 *)pp;
		idx = pThunk - (*ppts)->thunks;
		if ((idx >= 0) && (idx <= (*ppts)->maxCurSize) && !IS_FREE_THUNK(pThunk)){
			return pThunk;
		}
	}
	return NULL;
}
////////////////////////////////////////////////////////////////////////////////
static void PrvSqueezeThunks(ThunkStateOS5 *pts){
	int i,j;
	Boolean found;
	ThunkPoolOS5 *pPool;

	if (pts->pPool == NULL){
		pts->pPool = YAHM_GetRuntimeSettingsPtr()->pPool;
		if (pts->pPool == NULL){
			return;
		}
		PrvCheckPoolVersion(pts->pPool);
	}
	pPool = pts->pPool;
	while(true){
		found = false;
		for(i = 0; i <= pPool->maxCurSize; ++i){
			if (IS_FREE_THUNK(pPool->thunks + i)){
				continue;
			}
			if (IS_DUMMY_THUNK(pPool->thunks + i)){
				for(j = 0; j <= pPool->maxCurSize; ++j){
					if ((i == j) || (IS_FREE_THUNK(pPool->thunks + j))){
						continue;
					}
					if (!IS_DUMMY_THUNK(pPool->thunks + j)){
						if (pPool->thunks[j].oldestAddress == ByteSwap32(((UInt32)&pPool->thunks[i].hackJumpIns))){ // use[j]_dummy[i]
							pPool->thunks[j].oldestAddress = pPool->thunks[i].oldestAddress;
							FreeThunk(pPool->thunks + i);
							if (i == pPool->maxCurSize){
								--pPool->maxCurSize;
							}
							found = true;
						}
						if (pPool->thunks[i].oldestAddress == ByteSwap32(((UInt32)&pPool->thunks[j].hackJumpIns))) ; 
					}
				}
			}
		}
		if (!found)
			break;
	}
}

////////////////////////////////////////////////////////////////////////////////
inline UInt32 RoundCeil4(UInt32 x){
	return ((x + 3) / 4) * 4;
}
////////////////////////////////////////////////////////////////////////////////
void *YAHM_FixupGccCode(MemHandle hGot, void *codeInResource, UInt32 *pGotPtr){
	UInt32 *pFixups;
	void *pChunk;
	UInt32 codeSize, gotSize, roundCodeSize;
	int i;

	*pGotPtr = 0;
	if (hGot == NULL){ // no fixup section
		return codeInResource;
	}

	codeSize = MemPtrSize(codeInResource);
	roundCodeSize = RoundCeil4(codeSize);
	gotSize = MemHandleSize(hGot);

	pChunk = MemPtrNew(roundCodeSize + gotSize);
	if (pChunk == NULL){
		return NULL;
	}
	MemPtrSetOwner(pChunk, 0);
	pFixups = (UInt32 *)((char *)pChunk + roundCodeSize);
	*pGotPtr = (UInt32)pFixups;
	MemMove(pChunk, codeInResource, codeSize);
	MemMove(pFixups, MemHandleLock(hGot), gotSize);
	MemHandleUnlock(hGot);
	for(i = 0; i < gotSize/sizeof(UInt32); ++i){
		UInt32 x = ByteSwap32(pFixups[i]) + (UInt32)pChunk;
		pFixups[i] = ByteSwap32(x);
	}
	return pChunk;	
}

////////////////////////////////////////////////////////////////////////////////
static Err InstallTrap(void *pTrapCode, YAHM_SyscallInfo5 *pTrapInfo, UInt32 creator, UInt16 resId, void *gotPtr){
	JumpThunkOS5 *pThunkDb;
	ThunkStateOS5 ts;

	UInt32 prevAddress, addr;
	UInt16 commonJump, thunkId;
	UInt32 **pStack;
	Err err;
	UInt32 gotValue;

	prevAddress = (UInt32)YAHM_GetTrapAddress(pTrapInfo->baseTableOffset, pTrapInfo->offset);
	ClearState(&ts);
	commonJump = (pTrapInfo->flags >> 1) & 7;

	pThunkDb = PrvGetFreeThunk(&ts.pPool);
	if (pThunkDb == NULL){
		return hackErrNoFreeThunk;
	}
	// oldest points to prevAddress
	err = PrvCopyThunk(&pThunkDb->oldJumpIns, YAHM_SHORT_OLD_RES_ID);
	if (err != errNone){
		return err; 
	}
	pThunkDb->oldestAddress = ByteSwap32((prevAddress));
	// earliest points to our handler
	switch(commonJump){
	default:
	case THUNK_COMMON:
		gotValue = ByteSwap32(((UInt32)gotPtr));
		thunkId = YAHM_FAT_THUNK_RES_ID;
		break;
	case THUNK_FAST:
		gotValue = 0;
		thunkId = YAHM_SHORT_RES_ID;
		break;
	case THUNK_CW:
		gotValue = ByteSwap32(((UInt32)gotPtr));
		thunkId = YAHM_CW_THUNK_RES_ID;
		break;
	}
	
	err = PrvCopyThunk(&pThunkDb->hackJumpIns, thunkId);
	if (err != errNone){
		return err;
	}
	addr = (UInt32)pTrapCode;
	if (pTrapInfo->flags & 1){
		addr |= 1;
	}
	addr = ByteSwap32(addr);
	pThunkDb->hackCodeAddress = addr;
	pThunkDb->syscallInfo = *pTrapInfo;
	pThunkDb->R10_GOT = gotValue;
	pThunkDb->creator = creator;
	pStack = &ts.pPool->saveStackPtr;
	pThunkDb->stackPtr = ByteSwap32(((UInt32)pStack));
	FtrSet(creator, resId, (UInt32)(&pThunkDb->oldJumpIns));

	YAHM_SetTrapAddress(pTrapInfo->baseTableOffset, pTrapInfo->offset, (void *)&pThunkDb->hackJumpIns);
	PrvSqueezeThunks(&ts);
	return errNone;
}
////////////////////////////////////////////////////////////////////////////////
Err YAHM_InstallTrap(MemHandle hTrapCode, MemHandle hGot, MemHandle hTrapInfo, UInt32 creator, UInt16 resId){
	UInt32 gotPtr = 0;
	void *pHackCode;
	void *pFixedHackCode;
	Err err;
	YAHM_SyscallInfo5 ci;

	err = PrvGetTrapNum(&ci, hTrapInfo);
	if (err != errNone){
		return err;
	}
	pHackCode = MemHandleLock(hTrapCode);

	pFixedHackCode = YAHM_FixupGccCode(hGot, pHackCode, &gotPtr);
	if (pFixedHackCode != pHackCode){
		// unlock hack resource
		MemPtrUnlock(pHackCode);
		ci.flags &= ~0xE; // make thunk#0
	}
	return InstallTrap(pFixedHackCode, &ci, creator, resId, (void *)gotPtr);
}
////////////////////////////////////////////////////////////////////////////////
Err YAHM_InstallTrapFromMemory(void *pTrapCode, YAHM_SyscallInfo5 *pTrapInfo, void *pPnoletStart, UInt32 creator, UInt16 resId){
	return InstallTrap(pTrapCode, pTrapInfo, creator, resId, pPnoletStart);
}

////////////////////////////////////////////////////////////////////////////////
static void UninstallTrap(UInt32 creator, UInt16 resID, void **ppTrapCode){
	UInt32 prevAddress;
	JumpThunkOS5 *pThunkDb;
	void *hackAddress;
	char buff[80];
	UInt32 addr;
	YAHM_SyscallInfo5 ci;
	ThunkStateOS5 ts;
	
	ClearState(&ts);
	// find creator/resID record block
	FtrGet(creator, resID, &prevAddress);
	// make dummy thunk
	pThunkDb = PrvFindThunkByPrevAddress(&ts.pPool, (void *)prevAddress, true);
	StrPrintF(buff, "%lx %lx invalid tc", prevAddress, &(ts.pPool->thunks[0]));
	ErrFatalDisplayIf(pThunkDb == NULL, buff);
	ci  = pThunkDb->syscallInfo;
	addr = ByteSwap32(((UInt32)pThunkDb->hackCodeAddress));
	addr &= ~1; // remove thunk
	hackAddress = (void *)(addr);
	pThunkDb->hackCodeAddress = ByteSwap32(((UInt32)&pThunkDb->oldJumpIns));

	// if we were the latest catcher, clear dummy thunk
	if (((UInt32)YAHM_GetTrapAddress(ci.baseTableOffset, ci.offset) == (UInt32)&pThunkDb->hackJumpIns)){
		YAHM_SetTrapAddress(ci.baseTableOffset, ci.offset, (void *)(ByteSwap32(pThunkDb->oldestAddress)));
		FreeThunk(pThunkDb);
		// skip also all dummiez
		{
			UInt32 pp = ByteSwap32(pThunkDb->oldestAddress);
			while(true){
				JumpThunkOS5 *pT = PrvFindThunkByPrevAddress(&ts.pPool, (void *)pp, true);
				if ((pT == NULL) ||!IS_DUMMY_THUNK(pT) ){
					break;
				}
				pp = ByteSwap32(pT->oldestAddress);
				YAHM_SetTrapAddress(ci.baseTableOffset, ci.offset, (void *)pp);
				FreeThunk(pT);
			}
		}
	}
	PrvSqueezeThunks(&ts);
	FtrUnregister(creator, resID);
	if (ppTrapCode != NULL){
		*ppTrapCode = hackAddress;
	}
}
////////////////////////////////////////////////////////////////////////////////
// assumes initialization in proper order: no check for hcode for trapNo
void YAHM_UninstallTrap(MemHandle hCode, UInt32 creator, UInt16 resID){
	void *hackAddress;
	UninstallTrap(creator, resID, &hackAddress);
	if (MemPtrDataStorage(hackAddress)){
		// unlock hack resource
		MemHandleUnlock(hCode);
	}else{
		// free memory chunk
		MemPtrFree(hackAddress);
	}
}

////////////////////////////////////////////////////////////////////////////////
// assumes initialization in proper order: no check for hcode for trapNo
void YAHM_UninstallTrapFromMemory(void *pTrapCode, UInt32 creator, UInt16 resID){
	UninstallTrap(creator, resID, NULL);
}

////////////////////////////////////////////////////////////////////////////////
typedef struct{
	UInt32 param;
	UInt32 gotPtr;
}InitParam;
////////////////////////////////////////////////////////////////////////////////
Err YAHM_ExecuteInitialization(void *initCode, Boolean init){

	MemHandle hInitStub = DmGetResource(HACK_ARM_RES_TYPE, YAHM_INIT_RES_ID);
	MemHandle hGot;
	InitParam *par;

	NativeFuncType *initStub;
	void *fixedInitCode;
	UInt32 res;
	UInt32 param = init ? 1 : 0;

	if (hInitStub == NULL){
		return hackErrNoLibraryArmlet;
	}
	hGot = DmGet1Resource(HACK_GOT_RES_TYPE, HACK_CODE_INIT);
	par = MemPtrNew(sizeof(InitParam));
	initStub = MemHandleLock(hInitStub);

	par->gotPtr = 0;
	fixedInitCode = YAHM_FixupGccCode(hGot, initCode, &par->gotPtr);
	param |= (UInt32)fixedInitCode;
	par->param = ByteSwap32(param);
	par->gotPtr = ByteSwap32(par->gotPtr);
	
	res = PceNativeCall(initStub, par);

	MemHandleUnlock(hInitStub);
	DmReleaseResource(hInitStub);

	if (fixedInitCode != initCode){
		MemPtrFree(fixedInitCode);
	}

	MemPtrFree(par);

	if (hGot != NULL){
		DmReleaseResource(hGot);
	}

	return res ? errNone : hackErrInitializationFailed;
}

#ifdef YAHM_ITSELF
#include "trapcontrol5_info.c"
#endif
