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


#define POOL_VERSION 8
#ifdef __MWERKS__
#pragma warn_a5_access on
#endif
////////////////////////////////////////////////////////////////////////////////
#define offsetof(s,m)   (UInt32)&(((s *)0)->m)
////////////////////////////////////////////////////////////////////////////////
#define THUNK_LOCK_COUNT_REC	0x80000000
#define THUNK_LOCK_COUNT_CHUNK	0x00000000
////////////////////////////////////////////////////////////////////////////////
typedef struct{
	UInt8 op[4 * 14];
}JumpRoutine;

////////////////////////////////////////////////////////////////////////////////
typedef struct{
	UInt8 op[4];
}ShortJumpRoutine;

////////////////////////////////////////////////////////////////////////////////
typedef struct{
	UInt8 op[4 * 2];
}ThunkReturnRoutine;
////////////////////////////////////////////////////////////////////////////////
typedef struct{
	// порядок не менять!
	// код перехода на наш хак
	// don't change order!
	// jump to patch code
	JumpRoutine hackJumpIns;
	// после кода идет адрес перехода
	// patch code address
	UInt32 hackCodeAddress;
	// сюда яхм пишет адрес GOT
	// R10 value
	UInt32 R10_GOT;
	// pointer to stack pointer
	UInt32 stackPtr;
	// код перехода на старый сисколл
	// он использует два слова перед ним!!!
	// jump to old trap code
	ShortJumpRoutine oldJumpIns;
	// после кода идет адрес перехода
	// old trap code address
	UInt32 oldestAddress;
	UInt32 lockCount;
	// information fields
	UInt32 creator;
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
	ThunkReturnRoutine thunkReturnCode;
	JumpThunkOS5 thunks[1];
}ThunkPoolOS5;
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
#define POOL_SIZE_IN_BYTES(cnt) (sizeof(ThunkPoolOS5) + ((cnt) - 1) * sizeof(JumpThunkOS5))
////////////////////////////////////////////////////////////////////////////////
inline UInt16 PrvGetThunkType(YAHM_SyscallInfo5 *pCi){
	return (pCi->flags >> 1) & 7;
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
static void PrvInitThunks(void);
static JumpThunkOS5 *PrvAllocThunk(ThunkPoolOS5 *pts);
static JumpThunkOS5 *PrvFindThunkByPrevAddress(ThunkPoolOS5 *pts, void *prevAddress, Boolean isPrev);
static void PrvSqueezeThunks(ThunkPoolOS5 **pts);
static Err PrvGetTrapNum(YAHM_SyscallInfo5 *pTrapInfo, MemHandle hTrapInfo);
////////////////////////////////////////////////////////////////////////////////
static inline void PrvCopyCreator(char *s, UInt32 crid){
	MemMove(s, &crid, 4);
	s[4] = 0;
}
////////////////////////////////////////////////////////////////////////////////
static Err PrvCopyThunk(void *buf, UInt16 id){
	// was Get1. Changed because sometimes it called  over loaded hack
	MemHandle h = DmGetResource(HACK_ARM_RES_TYPE, id);
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
static void PrvInitThunks(void){
	YAHM_persistSettings settings;
	YAHM_runtimeSettings *pSettings = YAHM_GetRuntimeSettingsPtr();
	ThunkPoolOS5 * pPool;
	UInt32 *ppp;
	Err err;

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
	pPool->saveStackPtr = SwapPtr(pPool->saveStackPtr);
	err = PrvCopyThunk(&pPool->thunkReturnCode, YAHM_THUNK_RETURN_RES_ID);
	ErrFatalDisplayIf(err != errNone, "no thunk resource");	
}
////////////////////////////////////////////////////////////////////////////////
static void PrvCheckPoolVersion(ThunkPoolOS5 *pPool){
	if (pPool->version != POOL_VERSION){
		YAHM_warnAboutIncompatibleUpdate();
		SysReset();
	}
}

////////////////////////////////////////////////////////////////////////////////
JumpThunkOS5 *PrvAllocThunk(ThunkPoolOS5 *pts){ 
	int i;
	ErrFatalDisplayIf(pts == NULL, "Uninitialized thunk pool on allocation");
	for(i = 0; i < pts->poolSize; ++i){
		JumpThunkOS5 *pThunk = pts->thunks + i;
		if (IS_FREE_THUNK(pThunk)){
			if (i > pts->maxCurSize){
				pts->maxCurSize = i;
			}
			return pThunk;
		}
	}
	return NULL;
}
////////////////////////////////////////////////////////////////////////////////
static void PrvFreeThunk(JumpThunkOS5 *pThunk){
	pThunk->hackCodeAddress = 0;
	pThunk->creator = 'FREE';
	
}
////////////////////////////////////////////////////////////////////////////////
// search thunk by syscall pointer (either current or previous). 
// function returns NULL if pinter points outside of thunk pool
JumpThunkOS5 *PrvFindThunkByPrevAddress(ThunkPoolOS5 *pts, void *prevAddress, Boolean isPrev){
	UInt32 prevOffset;
	UInt32 pp = REINTERPRET_CAST(UInt32, prevAddress);
	UInt32 pp0;
	JumpThunkOS5 *pThunk;
	prevOffset = isPrev ? offsetof(JumpThunkOS5, oldJumpIns) : offsetof(JumpThunkOS5, hackJumpIns);
	ErrFatalDisplayIf(pts == NULL, "Uninitialized thunk pool on allocation");
	pp0 = REINTERPRET_CAST(UInt32, pts->thunks);
	pp -= prevOffset; // ptr to current thunk;
	if ( ((pp - pp0) % sizeof(JumpThunkOS5)) == 0){
		Int32 idx;
		pThunk = REINTERPRET_CAST(JumpThunkOS5 *, pp);
		idx = pThunk - pts->thunks;
		if ((idx >= 0) && (idx <= pts->maxCurSize) && !IS_FREE_THUNK(pThunk)){
			return pThunk;
		}
	}
	return NULL;
}
////////////////////////////////////////////////////////////////////////////////
static void PrvSqueezeThunks(ThunkPoolOS5 **ppThunkPool){
	int i,j;
	Boolean found;
	ThunkPoolOS5 *pPool;

	if (*ppThunkPool == NULL){
		*ppThunkPool = YAHM_GetRuntimeSettingsPtr()->pPool;
		if (*ppThunkPool == NULL){
			return;
		}
		PrvCheckPoolVersion(*ppThunkPool);
	}
	pPool = *ppThunkPool;
	while(true){
		found = false;
		for(i = 0; i <= pPool->maxCurSize; ++i){
			if (IS_FREE_THUNK(pPool->thunks + i)){
				continue;
			}
			if (IS_DUMMY_THUNK(pPool->thunks + i)){
				UInt32 curHack = ByteSwap32(((UInt32)&pPool->thunks[i].hackJumpIns));
				for(j = 0; j <= pPool->maxCurSize; ++j){
					if ((i == j) || (IS_FREE_THUNK(pPool->thunks + j))){
						continue;
					}
					if (!IS_DUMMY_THUNK(pPool->thunks + j)){
						if (pPool->thunks[j].oldestAddress == curHack){ // use[j]_dummy[i]
							pPool->thunks[j].oldestAddress = pPool->thunks[i].oldestAddress;
							PrvFreeThunk(pPool->thunks + i);
							if (i == pPool->maxCurSize){
								--pPool->maxCurSize;
							}
							found = true;
						}
						if (pPool->thunks[i].oldestAddress == SwapPtr32((&pPool->thunks[j].hackJumpIns))) ; 
					}
				}
			}
		}
		if (!found)
			break;
	}
}

////////////////////////////////////////////////////////////////////////////////
static Err PrvInstallTrap(void *pCode, void *pGotPtr, YAHM_SyscallInfo5 *pTrapInfo, UInt32 creator, UInt16 resID){
	JumpThunkOS5 *pThunk;
	ThunkPoolOS5 *pPool = NULL;
	UInt32 prevAddress, addr;
	UInt16 thunkType, thunkId;
	UInt32 **pStack;
	UInt32 oldFtr = 0;
	Err err = errNone;
	void *oldAddr;
	Boolean ftrWasSet = false;

	// resource IDs with thunk code
#ifdef __MWERKS__
	// CW can place into data resource
	UInt16 thunk2res[THUNK_MAX_TYPE];
	thunk2res[THUNK_COMMON]  = YAHM_FAT_THUNK_RES_ID;
	thunk2res[THUNK_FAST]  = YAHM_SHORT_RES_ID;
	thunk2res[THUNK_CW]  = YAHM_CW_THUNK_RES_ID;
	thunk2res[THUNK_FTR_GET]  =  YAHM_FTR_GET_THUNK_RES_ID;
	thunk2res[THUNK_LR_IN_R3]  =  YAHM_LR_IN_R3_THUNK_RES_ID;
#else
	//gcc places it into code resource
	static const UInt16 thunk2res[] = {YAHM_FAT_THUNK_RES_ID, YAHM_SHORT_RES_ID, YAHM_CW_THUNK_RES_ID, YAHM_FTR_GET_THUNK_RES_ID, YAHM_LR_IN_R3_THUNK_RES_ID};
#endif	

	prevAddress = REINTERPRET_CAST(UInt32, YAHM_GetTrapAddress(pTrapInfo->baseTableOffset, pTrapInfo->offset));
	if (prevAddress == NULL){
		return hackErrBadResultOfGetTrapAddress;
	}

	if ((FtrGet(creator, resID, &oldFtr) == errNone) && (oldFtr != 0)){
		return hackErrBadFeatureValue;
	}

	thunkType = PrvGetThunkType(pTrapInfo);

	pPool = YAHM_GetRuntimeSettingsPtr()->pPool;
	if (pPool == NULL){
		PrvInitThunks();
		pPool = YAHM_GetRuntimeSettingsPtr()->pPool;
	}
	PrvCheckPoolVersion(pPool);

	pThunk = PrvAllocThunk(pPool);
	if (pThunk == NULL){
		err = hackErrNoFreeThunk;
		goto Exit;
	}
	// set thunk to previous code
	err = PrvCopyThunk(&pThunk->oldJumpIns, YAHM_SHORT_OLD_RES_ID);
	if (err != errNone){
		goto Exit;
	}
	pThunk->oldestAddress = ByteSwap32(prevAddress);
	
	// set thunk to hack code
	ErrFatalDisplayIf((thunkType >= THUNK_MAX_TYPE), "Wrong thunk type");
	thunkId = thunk2res[thunkType];
	err = PrvCopyThunk(&pThunk->hackJumpIns, thunkId);
	if (err != errNone){
		goto Exit;
	}

	addr = REINTERPRET_CAST(UInt32, pCode);
	if (pTrapInfo->flags & YAHM_FLAG_THUMB){
		addr |= 1;
	}
	pThunk->hackCodeAddress = ByteSwap32(addr);
	
	// set R10. special value for CW
	pThunk->R10_GOT = ((thunkType == THUNK_CW) ? addr : SwapPtr32(pGotPtr));
	// set lock field
	pThunk->lockCount = ByteSwap32(1);
	// set info fields
	pThunk->syscallInfo = *pTrapInfo;
	pThunk->creator = creator;
	// set stack
	pStack = &pPool->saveStackPtr;
	pThunk->stackPtr = SwapPtr32(pStack);
	
	// set feature
	FtrSet(creator, resID, REINTERPRET_CAST(UInt32, (&pThunk->oldJumpIns)));
	ftrWasSet = true;

	// address set must be the latest operation to ensure all things are ready for trap call.
	oldAddr = YAHM_SetTrapAddress(pTrapInfo->baseTableOffset, pTrapInfo->offset, &pThunk->hackJumpIns);
	if (oldAddr == NULL){
		err = hackErrBadResultOfGetTrapAddress;
		goto Exit;
	}
	PrvSqueezeThunks(&pPool);
Exit:	
	if (err != errNone){
		if (pThunk != NULL){
			PrvFreeThunk(pThunk);
		}
		if (ftrWasSet){
			FtrUnregister(creator, resID);
		}
	}
	return err;
}
////////////////////////////////////////////////////////////////////////////////
Err YAHM_InstallTrap(MemHandle hTrapCode, MemHandle hGot, MemHandle hTrapInfo, UInt32 creator, UInt16 resID){
	void *pGotPtr = NULL;
	void *pFixedHackCode;
	Err err;
	YAHM_SyscallInfo5 ci;

	err = PrvGetTrapNum(&ci, hTrapInfo);
	if (err != errNone){
		return err;
	}
	pFixedHackCode = YAHM_FixupGccCodeEx(hGot, hTrapCode, &pGotPtr);
	//TODO: pGotPtr != NULL - критерий переноса в память
	//ErrFatalDisplayIf(((pGotPtr != NULL) && (getThunkType(&ci) !=  THUNK_COMMON)), "Wrong thunk type for .got support");
	return PrvInstallTrap(pFixedHackCode, pGotPtr, &ci, creator, resID);
}
////////////////////////////////////////////////////////////////////////////////
Err YAHM_InstallTrapFromMemory(void *pCode, YAHM_SyscallInfo5 *pTrapInfo, void *pPnoletStart, UInt32 creator, UInt16 resID){
	return PrvInstallTrap(pCode, pPnoletStart, pTrapInfo, creator, resID);
}

////////////////////////////////////////////////////////////////////////////////
static void PrvUninstallTrap(UInt32 creator, UInt16 resID, void **ppRelocatedCode, UInt32 *pLockCount){
	void *pPrevAddress = NULL;
	JumpThunkOS5 *pThunk;
	UInt32 uRelocatedCode;
	YAHM_SyscallInfo5 ci;
	ThunkPoolOS5 *pPool = YAHM_GetRuntimeSettingsPtr()->pPool;
	UInt32 lockCount;
	
	ErrFatalDisplayIf(pPool == NULL, "No thunk info in trap uninstall");
	PrvCheckPoolVersion(pPool);
	
	// get address of previous thunk  and find thunk address
	FtrGet(creator, resID, REINTERPRET_CAST(UInt32 *, &pPrevAddress));
	if (pPrevAddress == NULL){
		ErrFatalDisplayIf(pPrevAddress == NULL, "Wrong patch uninstall");
		return;
	}
	pThunk = PrvFindThunkByPrevAddress(pPool, pPrevAddress, true);
	if (pThunk == NULL){
		char buff[80];
		StrPrintF(buff, "Broken chain %lx %lx", pPrevAddress, &(pPool->thunks[0]));
		ErrFatalDisplayIf(pThunk == NULL, buff);
	}
	ci  = pThunk->syscallInfo;
	
	uRelocatedCode = pThunk->hackCodeAddress;
	uRelocatedCode = (ByteSwap32(uRelocatedCode)) & ~1;// remove thumb
	
	lockCount = ByteSwap32(pThunk->lockCount);
	
	// make dummy thunk
	pThunk->hackCodeAddress = SwapPtr32(&pThunk->oldJumpIns);
	// if we were the latest catcher, clear dummy thunk
	if ((YAHM_GetTrapAddress(ci.baseTableOffset, ci.offset) == &pThunk->hackJumpIns)){
		JumpThunkOS5 *pT = pThunk;
		do{
			void *pp = REINTERPRET_CAST(void *, pT->oldestAddress);
			pp = SwapPtr(pp);
			YAHM_SetTrapAddress(ci.baseTableOffset, ci.offset, pp);
			PrvFreeThunk(pT);
			pT = PrvFindThunkByPrevAddress(pPool, pp, true);
		}while((pT != NULL) && IS_DUMMY_THUNK(pT));
	}
	PrvSqueezeThunks(&pPool);
	if (lockCount == 1){
		FtrUnregister(creator, resID);
	}else{
		FtrSet(creator, resID, lockCount - 1);
	}
	if (pLockCount != NULL){
		*pLockCount = lockCount - 1;
	}
	if (ppRelocatedCode != NULL){
		*ppRelocatedCode = REINTERPRET_CAST(void *, uRelocatedCode);
	}
}
////////////////////////////////////////////////////////////////////////////////
void YAHM_UninstallTrap(MemHandle hCode, UInt32 creator, UInt16 resID){
	void *hackAddress;
	UInt32 lockCount;
	PrvUninstallTrap(creator, resID, &hackAddress, &lockCount);
	if (lockCount == 0){
		YAHM_FreeRelocatedChunk(hackAddress);
	}
}
////////////////////////////////////////////////////////////////////////////////
// assumes initialization in proper order: no check for hcode for trapNo
void YAHM_UninstallTrapFromMemory(void *pTrapCode, UInt32 creator, UInt16 resID){
	PrvUninstallTrap(creator, resID, NULL, NULL);
}


#ifdef YAHM_ITSELF
#include "trapcontrol5_info.c"
#endif
////////////////////////////////////////////////////////////////////////////////
void DumpRuntimeInfo(char *pBuf)
{
	UInt32 i;
	YAHM_runtimeSettings *pSettings = YAHM_GetRuntimeSettingsPtr();
	ThunkPoolOS5 * pPool;
	if (pSettings == NULL){
		StrPrintF(pBuf, "NULL pointer to runtime\n");
		return;

	}
	StrPrintF(pBuf, "YAHMLib runtime\n %ld active hacks\n", pSettings->activeHacksCount);
	pPool = pSettings->pPool;
	if (pPool == NULL){
		StrPrintF(pBuf + StrLen(pBuf), "Thunk array memory doesn't allocated\n");
		return;
	}
	StrPrintF(pBuf + StrLen(pBuf), "Pool data: ver %ld, size %ld, curSize %ld, stackBottom %lx, stackPtr %lx\n", pPool->version, pPool->poolSize,
		pPool->maxCurSize, pPool->stackBottom, pPool->saveStackPtr);
	StrPrintF(pBuf + StrLen(pBuf), "num, dummy, preCode, hackAddr, postCode, oldHackAddr, R10, \n");

	for(i = 0; i < pPool->poolSize; ++i){
		char cr[5];
		if (IS_FREE_THUNK(pPool->thunks + i)){
			continue;
		}
		PrvCopyCreator(cr, pPool->thunks[i].creator);

		StrPrintF(pBuf + StrLen(pBuf), " \x95%ld%s" ", %lx, %lx, %lx\n", 
			i, 
			(IS_DUMMY_THUNK(pPool->thunks + i) ? "*" : " "), 
			&(pPool->thunks[i].hackJumpIns), 
			pPool->thunks[i].hackCodeAddress,
			&(pPool->thunks[i].oldJumpIns)
		);

		StrPrintF(pBuf + StrLen(pBuf), " %lx, %lx, %ld," " %s, [%ld/%ld], %ld\n", 
			pPool->thunks[i].oldestAddress,
			pPool->thunks[i].R10_GOT, 
			pPool->thunks[i].lockCount, 
			cr, 
			pPool->thunks[i].syscallInfo.baseTableOffset, 
			pPool->thunks[i].syscallInfo.offset,
			pPool->thunks[i].syscallInfo.flags);

	}

}
