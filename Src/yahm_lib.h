#ifndef _YAHM_LIB_H_ 
#define _YAHM_LIB_H_

/*
	YAHM hack support library.
	(C)Igor Nesterov 2002-2004
*/

////////////////////////////////////////////////////////////////////////////////
// defines
#define HACK_ARM_RES_TYPE	'armc'
#define HACK_GOT_RES_TYPE	'.got'
#define YAHM_SET_TRAP_RES_ID 9999
#define YAHM_INIT_RES_ID	9998
#define HACK_CODE_INIT 999
#define HACK_CODE_RESOURCE_START 1000
#define TRAP_RESOURCE_TYPE5 'TRA5'
#define SAVE_STACK_SIZE 40
////////////////////////////////////////////////////////////////////////////////
// high level API: consider current app as hack
Boolean YAHM_InstallHack(void);
Boolean YAHM_UninstallHack(void);
////////////////////////////////////////////////////////////////////////////////
// middle level API: activate each trap independently
// execute 'armc' #999 (hack initialization)
Boolean YAHM_ExecuteInitialization(void *initCodeResource, Boolean init);
Boolean YAHM_InstallTrap(MemHandle hTrapCode, MemHandle hGot, MemHandle hTrapInfo, UInt32 creator, UInt16 resId);
void YAHM_UninstallTrap(MemHandle hCode, UInt32 creator, UInt16 resID);
void *YAHM_FixupGccCode(MemHandle hGot, void *codeResource, UInt32 *pGotPtr);
////////////////////////////////////////////////////////////////////////////////
// low level API: hand-made traps. no tracking for this traps at all
typedef struct YAHM_SyscallInfo5{
	UInt32 baseTableOffset;
	UInt32 offset;
	/*
		flags:
		0 - thumb
		3-1 - thumb type (0 - common, 1 - fastest...
	*/
	UInt32 flags;
}YAHM_SyscallInfo5;

// return current address
void *YAHM_GetTrapAddress(UInt32 base, UInt32 offset);
// blindly set trap address
void *YAHM_SetTrapAddress(YAHM_SyscallInfo5 *pTrapInfo, void *trapHandler);
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

extern void YAHM_warnAboutIncompatibleUpdate(void);
extern void YAHM_warnAboutFullThunk(void);

#endif // _YAHM_LIB_H_
