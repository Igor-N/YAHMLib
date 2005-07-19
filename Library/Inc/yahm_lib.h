#ifndef _YAHM_LIB_H_ 
#define _YAHM_LIB_H_

/*
	YAHM hack support library.
	(C)Igor Nesterov 2002-2004
*/

////////////////////////////////////////////////////////////////////////////////
#define hackErrorClass appErrorClass
// YAHM_SyscallInfo5 resource wrong
#define hackErrWrongTrapInfo (hackErrorClass | 1)
// thunk pool empty
#define hackErrNoFreeThunk (hackErrorClass | 2)
// can't take current app info
#define hackErrNoActiveApp (hackErrorClass | 3)
// initialization resource return error
#define hackErrInitializationFailed (hackErrorClass | 4)
// no hack resources in current app
#define hackErrNoHackResources (hackErrorClass | 5)
// can't find library own armlets
#define hackErrNoLibraryArmlet (hackErrorClass | 6)

////////////////////////////////////////////////////////////////////////////////
// High level API: consider current app as hack. All resources should be similar to hack resources.
Err YAHM_InstallHack(void);
Err YAHM_UninstallHack(void);

////////////////////////////////////////////////////////////////////////////////
// Middle level API: activate each trap independently
typedef struct YAHM_SyscallInfo5{
	UInt32 baseTableOffset;
	UInt32 offset;
	/*
		flags:
		0 - thumb
		3-1 - thunk type (0 - common, 1 - fastest, 2 -CW9
	*/
	UInt32 flags;
}YAHM_SyscallInfo5;

enum {
	THUNK_COMMON = 0,
	THUNK_FAST,
	THUNK_CW
};

// execute 'armc' #999 (hack initialization)
Err YAHM_ExecuteInitialization(void *initCodeResource, Boolean init);
// 3 resource handles: arm code, ,got secion (can be NULL) and trap description
// creator and resId are used for feature set.
Err YAHM_InstallTrap(MemHandle hTrapCode, MemHandle hGot, MemHandle hTrapInfo, UInt32 creator, UInt16 resId);
void YAHM_UninstallTrap(MemHandle hCode, UInt32 creator, UInt16 resID);

Err YAHM_InstallTrapFromMemory(void *pTrapCode, YAHM_SyscallInfo5 *pTrapInfo, void *pPnoletStart, UInt32 creator, UInt16 resId);
void YAHM_UninstallTrapFromMemory(void *pTrapCode, UInt32 creator, UInt16 resID);

Err YAHM_InstallTraps(UInt32 trapCount, void **pTrapCodes, YAHM_SyscallInfo5 * pTrapInfos, void *pPnoletStart);
Err YAHM_UninstallTraps(UInt32 trapCount, void **pTrapCodes);


void *YAHM_FixupGccCode(MemHandle hGot, void *codeResource, UInt32 *pGotPtr);


////////////////////////////////////////////////////////////////////////////////
// low level API: hand-made traps. no tracking for this traps at all
// return current address
void *YAHM_GetTrapAddress(UInt32 base, UInt32 offset);
// blindly set trap address
void *YAHM_SetTrapAddress(UInt32 base, UInt32 offset, void *trapHandler);
////////////////////////////////////////////////////////////////////////////////
// functions, supported by library user

typedef struct{ // Global YAHM settings. Should be saved in preferences.
	UInt32 protectYAHM; // if true, then YAHM protect application database
	UInt32 thunkCount;	// number of trap slots. allocate at least 40-50 slots.
}YAHM_persistSettings;

typedef struct{ // Runtime YAHM settings. They are valid on single YAHM execution. Should be saved in feature pointer
	UInt32 activeHacksCount;
	void *pPool;
}YAHM_runtimeSettings;

// copy persist settings to inside strcuture, allocated by YAHM
extern void YAHM_GetPersistSettings(YAHM_persistSettings *pSettings);

extern void YAHM_SetRuntimeSettings(YAHM_runtimeSettings *pSettings);
// return ptr to runtime settings structure.
extern YAHM_runtimeSettings *YAHM_GetRuntimeSettingsPtr(void);
// callback function. 
extern void YAHM_warnAboutIncompatibleUpdate(void);

#endif // _YAHM_LIB_H_
