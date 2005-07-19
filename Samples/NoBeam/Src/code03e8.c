#include "palmos5.h"
#include "endianutils.h"

#define RESID 1000

#ifndef MY_CRID
#define MY_CRID 'BEAM'
#endif 

#ifdef __MWERKS__
#else
// GCC
#include <Standalone.h>
STANDALONE_CODE_RESOURCE_TYPESTR_ID("armc", RESID);
#endif


typedef UInt32 (*pfnFrmCustomAlert)(UInt32 alertId, const char * s1, const char * s2, const char * s3);

UInt32 ARMlet_Main(UInt32 alertId, char * s1, const char * s2, const char * s3){
	pfnFrmCustomAlert oldTrap;

	if (alertId == 11730){ //beam off
		return 1;
	}
	FtrGet(MY_CRID, RESID, (UInt32 *)&oldTrap);
	return oldTrap(alertId, s1, s2, s3);
}


