#include <palmos.h>
#include "yahm_lib.h"
#include "yahm_int.h"
#include "YAHMLib_Rsc.h"


////////////////////////////////////////////////////////////////////////////////
static void PrvProtectApp(Boolean protect);
////////////////////////////////////////////////////////////////////////////////
#ifdef __MWERKS__
#pragma warn_a5_access on
#endif
////////////////////////////////////////////////////////////////////////////////
static void PrvProtectApp(Boolean protect){
	UInt16 cardNo;
	LocalID lid;
	YAHM_runtimeSettings *pSettings;
	YAHM_persistSettings settings;

	YAHM_GetPersistSettings(&settings);
	pSettings = YAHM_GetRuntimeSettingsPtr();
	if (settings.protectYAHM && (SysCurAppDatabase(&cardNo, &lid) == errNone)){
		if (protect){
			++pSettings->activeHacksCount;
			if ((pSettings->activeHacksCount == 1)){
				DmDatabaseProtect(cardNo, lid, protect);
			}
		}else{
			--pSettings->activeHacksCount;
			if ((pSettings->activeHacksCount == 0)){
				DmDatabaseProtect(cardNo, lid, protect);
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////
Err YAHM_InstallHack(void){
	UInt16 cardNo;
	LocalID lid;
	UInt16 resNo;
	MemHandle hCode;
	UInt32 crid;
	if (SysCurAppDatabase(&cardNo, &lid) != errNone){
		return hackErrNoActiveApp;
	}

	DmDatabaseInfo(cardNo, lid, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &crid);
	// install resource execution
	hCode = DmGet1Resource(HACK_ARM_RES_TYPE, HACK_CODE_INIT);
	if (hCode != NULL){
		void *pfn =  MemHandleLock(hCode);
		Err err = YAHM_ExecuteInitialization(pfn, true);
		if (err != errNone){
			MemHandleUnlock(hCode);
			return err;
		}
		MemHandleUnlock(hCode);
		DmReleaseResource(hCode);
	}

	// install all code resources
	for(resNo = HACK_CODE_RESOURCE_START; (hCode = DmGet1Resource(HACK_ARM_RES_TYPE, resNo)) ; resNo++){
		MemHandle hGot = DmGet1Resource(HACK_GOT_RES_TYPE, resNo);
		MemHandle hTrapInfo = DmGet1Resource(TRAP_RESOURCE_TYPE5, resNo);
		Err err;
		err = YAHM_InstallTrap(hCode, hGot, hTrapInfo, crid, resNo);
		
		if (err != errNone){
			UInt16 resNo1;
			if (hGot != NULL){
				DmReleaseResource(hGot);
			}
			if (hTrapInfo != NULL){
				DmReleaseResource(hTrapInfo);
			}
			DmReleaseResource(hCode);
			for(resNo1 = HACK_CODE_RESOURCE_START; resNo1 < resNo; ++resNo){
				hCode = DmGet1Resource(HACK_ARM_RES_TYPE, resNo);
				YAHM_UninstallTrap(hCode, crid, resNo1);
				DmReleaseResource(hCode);
			}
			return err;
		}
		if (hGot != NULL){
			DmReleaseResource(hGot);
		}
		if (hTrapInfo != NULL){
			DmReleaseResource(hTrapInfo);
		}
		DmReleaseResource(hCode);
	}
	if (resNo == HACK_CODE_RESOURCE_START){
		return hackErrNoHackResources;
	}
	PrvProtectApp(true);
	return errNone;
}
////////////////////////////////////////////////////////////////////////////////
Err YAHM_UninstallHack(void){
	MemHandle hCode;
	UInt16 resID;
	UInt16 cardNo;
	LocalID lid;
	UInt32 crid;

	if (SysCurAppDatabase(&cardNo, &lid) != errNone){
		return hackErrNoActiveApp;
	}

	DmDatabaseInfo(cardNo, lid, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &crid);

	// uninstall all code resources
	for(resID = HACK_CODE_RESOURCE_START; (hCode = DmGet1Resource(HACK_ARM_RES_TYPE, resID)) ; ++resID){
		YAHM_UninstallTrap(hCode, crid, resID);
		DmReleaseResource(hCode);
	}
	// uninstall notification
	hCode = DmGet1Resource(HACK_ARM_RES_TYPE, HACK_CODE_INIT);
	if (hCode != NULL){
		void *pfn = MemHandleLock(hCode);
		YAHM_ExecuteInitialization(pfn, false);
		MemHandleUnlock(hCode);
		DmReleaseResource(hCode);
	}
	PrvProtectApp(false);
	return errNone;
}
#ifdef __MWERKS__
#pragma warn_a5_access off
#endif

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
