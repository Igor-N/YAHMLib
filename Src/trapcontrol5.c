#include <palmos.h>
#include <PceNativeCall.h>
#include "endianutils.h"
#include "yahm_lib.h"
#include "YAHMLib_Rsc.h"

#define POOL_VERSION 5
//#pragma warn_a5_access on
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

const ShortJumpRoutine toold = {
{ // size = 4
0x04, 0xf0, 0x1f, 0xe5 
} 
};

const JumpRoutine togoodhack = {
#include "thunk/short_hack.c"
};



const JumpRoutine tohack = {
#include "thunk/fatjumphack.c"
};


typedef struct{
	// порядок не менять!
	// код перехода на наш хак
	JumpRoutine hackJumpIns;
	// после кода идет адрес перехода
	UInt32 hackCodeAddress;
	// сюда яхм пишет адрес GOT
	UInt32 R10_GOT;
	UInt32 stackPtr;
	// код перехода на старый сисколл
	// он использует два слова перед ним!!!
	//JumpRoutine oldJumpIns;
	ShortJumpRoutine oldJumpIns;
	// после кода идет адрес перехода
	UInt32 oldestAddress;

	YAHM_SyscallInfo5 syscallInfo;
}JumpThunkOS5;

/*
	  ip - R12
	  lr - R14
	  sp - R13
	  pc - R15
*/
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
static JumpThunkOS5 *PrvGetFreeThunk(ThunkStateOS5 *pts);
static JumpThunkOS5 *FindThunkByPrevAddress(ThunkStateOS5 *pts, void *prevAddress, Boolean isPrev);
static void SqueezeThunks(ThunkStateOS5 *pts);
static Boolean PrvGetTrapNum(YAHM_SyscallInfo5 *pTrapInfo, MemHandle hTrapInfo);

////////////////////////////////////////////////////////////////////////////////
static UInt32 SysGetTrapAddress1(ArmParameterBlock *pTrapInfo){
	MemHandle h;
	void *p;
	UInt32 res;

	pTrapInfo->set = 0;	
	h = DmGetResource(HACK_ARM_RES_TYPE, YAHM_SET_TRAP_RES_ID);
	p = MemHandleLock(h);
	res = PceNativeCall((NativeFuncType *)p, pTrapInfo);
	MemHandleUnlock(h);
	DmReleaseResource(h);
	return res;
}
////////////////////////////////////////////////////////////////////////////////
void *YAHM_GetTrapAddress(UInt32 base, UInt32 offset){
	UInt32 res;
	ArmParameterBlock *pTrapInfo = MemPtrNew(sizeof(ArmParameterBlock));
	pTrapInfo->callInfo.baseTableOffset = base;
	pTrapInfo->callInfo.offset = offset;
	res = SysGetTrapAddress1(pTrapInfo);
	MemPtrFree(pTrapInfo);
	return (void *)res;
}
////////////////////////////////////////////////////////////////////////////////
static UInt32 SysSetTrapAddress1(ArmParameterBlock * pTrapInfo, UInt32 addr){
	MemHandle h;
	void *p;
	UInt32 res;

	pTrapInfo->set = 1;
	pTrapInfo->newAddr = ByteSwap32(addr);
	h = DmGetResource(HACK_ARM_RES_TYPE, YAHM_SET_TRAP_RES_ID);
	p = MemHandleLock(h);
	res = PceNativeCall((NativeFuncType *)p, pTrapInfo);
	MemHandleUnlock(h);
	DmReleaseResource(h);
	return res;
}

////////////////////////////////////////////////////////////////////////////////
void *YAHM_SetTrapAddress(YAHM_SyscallInfo5 *pTrapInfo, void *trapHandler){
	ArmParameterBlock *pSetTrapInfo = MemPtrNew(sizeof(ArmParameterBlock));
	pSetTrapInfo->callInfo = *pTrapInfo;
	return (void *)SysSetTrapAddress1(pSetTrapInfo, (UInt32)trapHandler);
}
////////////////////////////////////////////////////////////////////////////////
static Boolean PrvGetTrapNum(YAHM_SyscallInfo5 *pTrapInfo, MemHandle hTrapInfo){
	if ((hTrapInfo == NULL) || (MemHandleSize(hTrapInfo) < sizeof(YAHM_SyscallInfo5))){
		MemSet(pTrapInfo, sizeof(YAHM_SyscallInfo5), 0);
		return false;
	}
	*pTrapInfo = *(YAHM_SyscallInfo5 *)MemHandleLock(hTrapInfo);
	MemHandleUnlock(hTrapInfo);	
	return true;
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
JumpThunkOS5 *PrvGetFreeThunk(ThunkStateOS5 *pts){ //TODO: examine treshold
	int i;
	if (pts->pPool == NULL){
		pts->pPool = YAHM_GetRuntimeSettingsPtr()->pPool;
		if (pts->pPool == NULL){
			InitThunks();
			pts->pPool = YAHM_GetRuntimeSettingsPtr()->pPool;
		}
		PrvCheckPoolVersion(pts->pPool);
	}
	for(i = 0; i < pts->pPool->poolSize; ++i){
		JumpThunkOS5 *pThunk = pts->pPool->thunks + i;
		if (IS_FREE_THUNK(pThunk)){
			if (i > pts->pPool->maxCurSize){
				pts->pPool->maxCurSize = i;
			}
			return pThunk;
		}
	}
	return NULL;
}
////////////////////////////////////////////////////////////////////////////////
JumpThunkOS5 *FindThunkByPrevAddress(ThunkStateOS5 *pts, void *prevAddress, Boolean isPrev){
	UInt32 prevOffset;
	char *pp = (char *)prevAddress;
	char *pp0;
	JumpThunkOS5 *pThunk;
	if (isPrev){
		prevOffset = (UInt32)(&((JumpThunkOS5*)0)->oldJumpIns);
	}else{
		prevOffset = (UInt32)(&((JumpThunkOS5*)0)->hackJumpIns);
	}
	if (pts->pPool == NULL){
		pts->pPool = YAHM_GetRuntimeSettingsPtr()->pPool;
		if (pts->pPool == NULL)
			return NULL;
		PrvCheckPoolVersion(pts->pPool);
	}
	pp0 = (char *)pts->pPool->thunks;
	pp -= prevOffset; // ptr to current thunk;
	if ( ((pp - pp0) % sizeof(JumpThunkOS5)) == 0){
		Int32 idx;
		pThunk = (JumpThunkOS5 *)pp;
		idx = pThunk - pts->pPool->thunks;
		if ((idx >= 0) && (idx <= pts->pPool->maxCurSize) && !IS_FREE_THUNK(pThunk)){
			return pThunk;
		}
	}
	return NULL;
}
////////////////////////////////////////////////////////////////////////////////
static void SqueezeThunks(ThunkStateOS5 *pts){
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
						if (pPool->thunks[i].oldestAddress == ByteSwap32(((UInt32)&pPool->thunks[j].hackJumpIns))) ; //TODO optimize dummy_use
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
Boolean YAHM_InstallTrap(MemHandle hTrapCode, MemHandle hGot, MemHandle hTrapInfo, UInt32 creator, UInt16 resId){
	JumpThunkOS5 *pThunkDb;
	ThunkStateOS5 ts;
	UInt32 gotPtr = 0;
	// should be in heap for ARM alignment!
	ArmParameterBlock *pTrapInfo; // = MemPtrNew(sizeof(ArmParameterBlock));
	
	void *pHackCode;
	void *pFixedHackCode;
	UInt32 prevAddress, addr;
	Boolean commonJump;
	UInt32 **pStack;

	pTrapInfo = MemPtrNew(sizeof(ArmParameterBlock));
	if (!PrvGetTrapNum(&pTrapInfo->callInfo, hTrapInfo)){
		MemPtrFree(pTrapInfo);
		return false;
	}
	pHackCode = MemHandleLock(hTrapCode);

	prevAddress = SysGetTrapAddress1(pTrapInfo);
	ClearState(&ts);
	commonJump = (pTrapInfo->callInfo.flags >> 1) & 7;

	pThunkDb = PrvGetFreeThunk(&ts);
	if (pThunkDb == NULL){
	//TODO
		YAHM_warnAboutFullThunk();
		MemPtrFree(pTrapInfo);
		return false;
	}
	pFixedHackCode = YAHM_FixupGccCode(hGot, pHackCode, &gotPtr);
	if (pFixedHackCode != pHackCode){
		MemPtrUnlock(pHackCode);
		commonJump = 0;
	}
	// oldest points to prevAddress
	pThunkDb->oldJumpIns = toold;
	pThunkDb->oldestAddress = ByteSwap32((prevAddress));
	// earliest points to our handler
	//
	pThunkDb->hackJumpIns = (commonJump == 1) ? togoodhack : tohack;
	addr = (UInt32)pFixedHackCode;
	if (pTrapInfo->callInfo.flags & 1){
		addr |= 1;
	}
	//FrmCustomAlert(DebugAlert, "inst5", "", "");
	addr = ByteSwap32(addr);
	pThunkDb->hackCodeAddress = addr;
	pThunkDb->syscallInfo = pTrapInfo->callInfo;
	pThunkDb->R10_GOT = ByteSwap32(gotPtr);
	pStack = &ts.pPool->saveStackPtr;
	pThunkDb->stackPtr = ByteSwap32(((UInt32)pStack));
	FtrSet(creator, resId, (UInt32)(&pThunkDb->oldJumpIns));

	SysSetTrapAddress1(pTrapInfo, (UInt32)&pThunkDb->hackJumpIns);
	MemPtrFree(pTrapInfo);
	SqueezeThunks(&ts);
	return true;
}

////////////////////////////////////////////////////////////////////////////////
// assumes initialization in proper order: no check for hcode for trapNo
void YAHM_UninstallTrap(MemHandle hCode, UInt32 creator, UInt16 resID){
	UInt32 prevAddress;
	JumpThunkOS5 *pThunkDb;
	ArmParameterBlock *pTrapInfo = MemPtrNew(sizeof(ArmParameterBlock));
	void *hackAddress;
	char buff[80];
	UInt32 addr;
	
	ThunkStateOS5 ts;
	ClearState(&ts);
	// find creator/resID record block
	FtrGet(creator, resID, &prevAddress);
	// make dummy thunk
	pThunkDb = FindThunkByPrevAddress(&ts, (void *)prevAddress, true);
	StrPrintF(buff, "%lx %lx invalid tc", prevAddress, &(ts.pPool->thunks[0]));
	ErrFatalDisplayIf(pThunkDb == NULL, buff);
	pTrapInfo->callInfo = pThunkDb->syscallInfo;
	addr = ByteSwap32(((UInt32)pThunkDb->hackCodeAddress));
	addr &= ~1; // remove thunk
	hackAddress = (void *)(addr);
	pThunkDb->hackCodeAddress = ByteSwap32(((UInt32)&pThunkDb->oldJumpIns));

	// if we was the latest catcher, clear dummy thunk
	if ((SysGetTrapAddress1(pTrapInfo) == (UInt32)&pThunkDb->hackJumpIns)){
		SysSetTrapAddress1(pTrapInfo, ByteSwap32(pThunkDb->oldestAddress));
		FreeThunk(pThunkDb);
		// skip also all dummiez
		{
			UInt32 pp = ByteSwap32(pThunkDb->oldestAddress);
			while(true){
				JumpThunkOS5 *pT = FindThunkByPrevAddress(&ts, (void *)pp, true);
				if ((pT == NULL) ||!IS_DUMMY_THUNK(pT) ){
					break;
				}
				pp = ByteSwap32(pT->oldestAddress);
				SysSetTrapAddress1(pTrapInfo, pp);
				FreeThunk(pT);
			}
		}
	}
	SqueezeThunks(&ts);
	if (MemPtrDataStorage(hackAddress)){
		MemHandleUnlock(hCode);
	}else{
		MemPtrFree(hackAddress);
	}
	FtrUnregister(creator, resID);
}

#ifdef __MWERKS__
#pragma warn_a5_access on
#endif

////////////////////////////////////////////////////////////////////////////////
typedef struct{
	UInt32 param;
	UInt32 gotPtr;
}InitParam;
////////////////////////////////////////////////////////////////////////////////
Boolean YAHM_ExecuteInitialization(void *initCode, Boolean init){

	MemHandle h = DmGetResource(HACK_ARM_RES_TYPE, YAHM_INIT_RES_ID);
	MemHandle hGot;
	InitParam *par;

	NativeFuncType *initStub;
	void *fixedInitCode;
	UInt32 res;
	UInt32 param = init ? 1 : 0;

	if (h == NULL){
		return false;
	}
	hGot = DmGet1Resource(HACK_GOT_RES_TYPE, HACK_CODE_INIT);
	par = MemPtrNew(sizeof(InitParam));
	initStub = MemHandleLock(h);

	par->gotPtr	 = 0;
	fixedInitCode = YAHM_FixupGccCode(hGot, initCode, &par->gotPtr);
	param |= (UInt32)fixedInitCode;
	par->param = ByteSwap32(param);
	par->gotPtr = ByteSwap32(par->gotPtr);
	res = PceNativeCall(initStub, par);

	MemHandleUnlock(h);
	DmReleaseResource(h);

	if (fixedInitCode != initCode){
		MemPtrFree(fixedInitCode);
	}

	MemPtrFree(par);

	if (hGot != NULL){
		DmReleaseResource(hGot);
	}

	return res != 0;
}
