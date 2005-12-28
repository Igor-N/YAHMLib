#include <palmos.h>
#include <PceNativeCall.h>
#include "endianutils.h"
#include "yahm_lib.h"
#define NOT_PILRC
#include "yahm_int.h"
#include "yahm_internals.h"

#ifndef sysFtrNumDmAutoBackup
#define sysFtrNumDmAutoBackup   31 
#endif
/* no .got, no NVFS
 *    YAHM_FixupGccCodeEx: lock resource
 *    YAHM_FreeRelocatedChunk: unlock resource
 *
 * no .got, NVFS or .got
 *    YAHM_FixupGccCodeEx: lock resource, copy resource to chunk, unlock resource
 *    YAHM_FreeRelocatedChunk: free chunk
 *
 *
 *
 */
////////////////////////////////////////////////////////////////////////////////
void *MemHeapPtr(UInt16 heapID) SYS_TRAP(sysTrapMemHeapPtr);
////////////////////////////////////////////////////////////////////////////////
static void PrvFlushDataCache(void){
	UInt32 *p = MemHeapPtr(0);
	UInt32 i;
	volatile UInt32 sum;
	sum = 0;
	for(i = 0; i < 65535/4; ++i){
		sum += p[i];
	}
}
////////////////////////////////////////////////////////////////////////////////
inline UInt32 RoundCeil4(UInt32 x){
	return ((x + 3) / 4) * 4;
}
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// return:	NULL if no free memory
//			pointer to relocated code otherwise
// if return != NULL
//		*ppGotPtr points to .got in relocated code
// if return == pCodeResource, there were no relocation (no .got and no NVFS)
// PrvFixupGccCode doesn't  require pUnrelocatedCode and pUnrelocatedGot to be resources.
static void *PrvFixupGccCode(void *pUnrelocatedCode, UInt32 codeSize, UInt32 *pUnrelocatedGot, UInt32 gotSize, void **ppGotPtr){
	UInt32 *pRelocatedGot;
	UInt8 *pRelocatedCode;
	UInt32 roundCodeSize;
	UInt16 gotCnt;
	int i;
	UInt32 isNVFS = false;


	ErrFatalDisplayIf(pUnrelocatedCode == NULL, "NULL ptr can't be relocated");
	*ppGotPtr = 0;
	FtrGet (sysFtrCreator, sysFtrNumDmAutoBackup, &isNVFS);
	roundCodeSize = RoundCeil4(codeSize);
	if ((pUnrelocatedGot == NULL) && (!isNVFS)){
		return pUnrelocatedCode;
	}
	pRelocatedCode = MemPtrNew(roundCodeSize + gotSize);
	if (pRelocatedCode == NULL){
		return NULL;
	}
	MemPtrSetOwner(pRelocatedCode, 0);
	pRelocatedGot = REINTERPRET_CAST(UInt32 *, (pRelocatedCode + roundCodeSize));
	*ppGotPtr = pRelocatedGot;
	MemMove(pRelocatedCode, pUnrelocatedCode, codeSize);
	gotCnt = gotSize/sizeof(UInt32);
	for(i = 0; i < gotCnt; ++i){
		UInt32 relVal = pUnrelocatedGot[i];
		UInt32 fixedRelVal = ByteSwap32(relVal) + REINTERPRET_CAST(UInt32, pRelocatedCode);
		pRelocatedGot[i] = ByteSwap32(fixedRelVal);
	}
	PrvFlushDataCache();
	return pRelocatedCode;	
}
////////////////////////////////////////////////////////////////////////////////
void *YAHM_FixupGccCode(MemHandle hUnrelocatedGotSection, void *pCodeResource, void **ppGotPtr){
	void *pRelocatedCode;
	UInt32 *pUnrelocatedGotSection;
	UInt32 codeSize, gotSize; 

	pUnrelocatedGotSection = SafeMemHandleLock(hUnrelocatedGotSection);
	codeSize = SafeMemPtrSize(pCodeResource);
	gotSize = SafeMemPtrSize(pUnrelocatedGotSection);
	pRelocatedCode = PrvFixupGccCode(pCodeResource, codeSize, pUnrelocatedGotSection, gotSize, ppGotPtr);
	SafeMemHandleUnlock(hUnrelocatedGotSection);
	return pRelocatedCode;	
}
////////////////////////////////////////////////////////////////////////////////
// if no relocation, locked code resource returned
// else new code in dynamic memory returned. hCodeResource remain unlocked
void *YAHM_FixupGccCodeEx(MemHandle hUnrelocatedGotSection, MemHandle  hCodeResource, void **ppGotPtr){
	void *pRelocatedCode, *pCodeResource;
	if (hCodeResource == NULL){
		return NULL;
	}
	pCodeResource = MemHandleLock(hCodeResource);
	pRelocatedCode = YAHM_FixupGccCode(hUnrelocatedGotSection, pCodeResource, ppGotPtr);
	if (pCodeResource != pRelocatedCode){
		MemHandleUnlock(hCodeResource);
	}
	return pRelocatedCode;
}

////////////////////////////////////////////////////////////////////////////////
void YAHM_FreeRelocatedChunk(void *pAddress){
	if (MemPtrDataStorage(pAddress)){
		MemPtrUnlock(pAddress);
	}else{
		MemPtrFree(pAddress);
	}
}

