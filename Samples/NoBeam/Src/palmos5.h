#ifndef _FIX_OS5_H_
#define _FIX_OS5_H_



#define AttnGetAttention AttnGetAttentionV40
#define DmDatabaseInfo DmDatabaseInfoV40
#define SysCurAppDatabase SysCurAppDatabaseV40
#define DmGetNextDatabaseByTypeCreator DmGetNextDatabaseByTypeCreatorV40
#define SysUIAppSwitch SysUIAppSwitchV40
#define DmCreateDatabase DmCreateDatabaseV40
#define DmFindDatabase DmFindDatabaseV40
#define DmSetDatabaseInfo DmSetDatabaseInfoV40
#define DmOpenDatabaseInfo DmOpenDatabaseInfoV40
#define DmCreateDatabase DmCreateDatabaseV40
#define SysTaskDelay SysTaskDelayV40
#define SysNotifyRegister SysNotifyRegisterV40
#define SysNotifyUnregister SysNotifyUnregisterV40 
#define DmOpenDatabase DmOpenDatabaseV40
#include <palmos.h>
#undef AttnGetAttention 
#undef DmDatabaseInfo
#undef SysCurAppDatabase 
#undef DmGetNextDatabaseByTypeCreator
#undef SysUIAppSwitch
#undef DmCreateDatabase
#undef DmFindDatabase
#undef DmSetDatabaseInfo
#undef DmOpenDatabaseInfo 
#undef DmCreateDatabase
#undef SysNotifyRegister
#undef SysNotifyUnregister
#undef DmOpenDatabase

Err		DmDatabaseInfo(LocalID	dbID, Char *nameP,
					UInt16 *attributesP, UInt16 *versionP, UInt32 *crDateP,
					UInt32 *	modDateP, UInt32 *bckUpDateP,
					UInt32 *	modNumP, LocalID *appInfoIDP,
					LocalID *sortInfoIDP, UInt32 *typeP,
					UInt32 *creatorP);

Err 		SysCurAppDatabase(LocalID *dbIDP);

Err AttnGetAttention (LocalID dbID, UInt32 userData,
	AttnCallbackProc *callbackFnP, AttnLevelType level, AttnFlagsType flags,
	UInt16 nagRateInSeconds, UInt16 nagRepeatLimit);

Err SysCurAppDatabase(LocalID *dbIDP);


#endif
