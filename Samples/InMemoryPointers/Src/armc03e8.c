//#include <Standalone.h>
#include <endianutils.h>
#include "palmos5.h"
#include <PceNativeCall.h>
#include "InMemory.h"

#define RESID 1000


typedef UInt32 (*pfnFrmCustomAlert)(UInt32 alertId, const char * s1, const char * s2, const char * s3);
typedef void (*pfnEvtGetEvent)(EventType *event, Int32 timeout);

UInt32 myFrmCustomAlert(UInt32 alertId, char * s1, const char * s2, const char * s3);
void myEvtGetEvent (EventType *event, Int32 timeout);

UInt32 __ARMlet_Startup__(const void *emulStateP, UInt32 *funPtr, Call68KFuncType *call68KFuncP){
	funPtr[0] = ByteSwap32(((UInt32)myFrmCustomAlert));
	funPtr[1] = ByteSwap32(((UInt32)myEvtGetEvent));
	return 2;
}

UInt32 myFrmCustomAlert(UInt32 alertId, char * s1, const char * s2, const char * s3){
	pfnFrmCustomAlert oldTrap;
	FtrGet(appFileCreator, RESID, (UInt32 *)&oldTrap);
	return oldTrap(alertId, s1, "#2", s3);
}



#undef RESID
#define RESID 1001



void myEvtGetEvent (EventType *event, Int32 timeout){
	pfnEvtGetEvent oldTrap;

	FtrGet(appFileCreator, RESID, (UInt32 *)&oldTrap);
	oldTrap(event, timeout);
	if (event->eType == keyDownEvent){

		if (event->data.keyDown.chr == vchrCalc){
			event->data.keyDown.chr = vchrRonamatic;
		}
	}
}
