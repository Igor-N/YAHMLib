#include <palmos.h>
#include "endianutils.h"

// return true in user mode
Boolean HALInvalidateICacheEx(void *, UInt32);

enum TableID {DAL_Table = 0, Boot_Table, UI_Table};

Boolean SysPatchEntry(enum TableID table, UInt32 entry, void* NewHandlerAddress, void** OldHandlerAddress);

typedef void Call68KFuncType;

typedef struct ArmTrapInfo{
	UInt32 baseTableOffset;
	UInt32 offset;
	UInt32 thumb;
	UInt32 set;
	UInt32 newAddr;
}ArmTrapInfo;

void *	MemHeapPtr(UInt16 heapID);


unsigned long NativeFuncType
		(const void *emulStateP, 
		ArmTrapInfo *pTrapInfo, 
		Call68KFuncType *call68KFuncP)
{
	UInt32 baseTableOffset = (pTrapInfo->baseTableOffset >> 24) & 0xFF;
	UInt32 offset = ByteSwap32(pTrapInfo->offset);

	if (pTrapInfo->set){
		//Boolean res;
		enum TableID tbl = Boot_Table;
		UInt32 newAddr = pTrapInfo->newAddr;
		UInt32 prevAddr;
//		UInt32 thumb = pTrapInfo->thumb;
//		thumb = (ByteSwap32(thumb)) & 1;

		switch (baseTableOffset){
		case 4:
			tbl = DAL_Table;
			break;
		case 8:
			tbl = Boot_Table;
			break;
		case 12:
			tbl = UI_Table;
			break;
		}
/*
		if (thumb){
			newAddr |= 1;
		}
*/
		{
			// flush dcache
			UInt32 *p = MemHeapPtr(0);
			UInt32 i;
			volatile UInt32 sum;
			sum = 0;
			for(i = 0; i < 65535/4; ++i){
				sum += p[i];
			}
		}
		HALInvalidateICacheEx((void *)newAddr, 0);
		/* res = */ SysPatchEntry(tbl, offset/4, (void *)newAddr, (void **)&prevAddr);
		return prevAddr;
	}else{
		UInt32 r9;
		UInt32 w;
		asm("STR	R9, %0" :"=m" (r9));
		w = *(UInt32 *)(r9 - baseTableOffset);
		return *(UInt32 *)(w + offset);
	}
}

