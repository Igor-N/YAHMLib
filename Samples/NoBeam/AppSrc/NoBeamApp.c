// YAHMLib sample
// Look at the bottom of file for YAHM-specific functions
#include <PalmOS.h>

#include "NoBeamApp.h"
#include "NoBeamApp_Rsc.h"
#include "yahm_lib.h"

#define RUNTIME_PTR_FTR 12
#define INSTALL_COUNT_FTR 13

////////////////////////////////////////////////////////////////////////////////
void InstallHack(void);
void UninstallHack(void);

#define ourMinVersion    sysMakeROMVersion(4,0,0,sysROMStageDevelopment,0)
#define kPalmOS20Version sysMakeROMVersion(2,0,0,sysROMStageDevelopment,0)

////////////////////////////////////////////////////////////////////////////////
static void * GetObjectPtr(UInt16 objectID)
{
	FormType * frmP;

	frmP = FrmGetActiveForm();
	return FrmGetObjectPtr(frmP, FrmGetObjectIndex(frmP, objectID));
}

////////////////////////////////////////////////////////////////////////////////
static void MainFormInit(FormType *frmP)
{
}

////////////////////////////////////////////////////////////////////////////////
static Boolean MainFormDoCommand(UInt16 command)
{
	Boolean handled = false;

	switch (command)
	{

	}

	return handled;
}

////////////////////////////////////////////////////////////////////////////////
static Boolean MainFormHandleEvent(EventType * eventP)
{
	Boolean handled = false;
	FormType * frmP;

	switch (eventP->eType) 
	{
		case menuEvent:
			return MainFormDoCommand(eventP->data.menu.itemID);

		case frmOpenEvent:
			frmP = FrmGetActiveForm();
			FrmDrawForm(frmP);
			MainFormInit(frmP);
			handled = true;
			break;
            
        case frmUpdateEvent:
			break;
			
		case ctlSelectEvent:
		{
			if (eventP->data.ctlSelect.controlID == MainEnableButton){
				InstallHack();
			}else if (eventP->data.ctlSelect.controlID == MainDisableButton){
				UninstallHack();
			}
			break;
		}
	}
    
	return handled;
}

////////////////////////////////////////////////////////////////////////////////
static Boolean AppHandleEvent(EventType * eventP)
{
	UInt16 formId;
	FormType * frmP;

	if (eventP->eType == frmLoadEvent)
	{
		formId = eventP->data.frmLoad.formID;
		frmP = FrmInitForm(formId);
		FrmSetActiveForm(frmP);

		switch (formId)
		{
			case MainForm:
				FrmSetEventHandler(frmP, MainFormHandleEvent);
				break;

		}
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////
static void AppEventLoop(void)
{
	UInt16 error;
	EventType event;

	do 
	{
		EvtGetEvent(&event, evtWaitForever);

		if (! SysHandleEvent(&event))
		{
			if (! MenuHandleEvent(0, &event, &error))
			{
				if (! AppHandleEvent(&event))
				{
					FrmDispatchEvent(&event);
				}
			}
		}
	} while (event.eType != appStopEvent);
}

////////////////////////////////////////////////////////////////////////////////
static Err AppStart(void)
{

	return errNone;
}


////////////////////////////////////////////////////////////////////////////////
static void AppStop(void)
{
	FrmCloseAllForms();

}

////////////////////////////////////////////////////////////////////////////////
static Err RomVersionCompatible(UInt32 requiredVersion, UInt16 launchFlags)
{
	UInt32 romVersion;

	FtrGet(sysFtrCreator, sysFtrNumROMVersion, &romVersion);
	if (romVersion < requiredVersion)
	{
		if ((launchFlags & 
			(sysAppLaunchFlagNewGlobals | sysAppLaunchFlagUIApp)) ==
			(sysAppLaunchFlagNewGlobals | sysAppLaunchFlagUIApp))
		{
			FrmAlert (RomIncompatibleAlert);

			if (romVersion < kPalmOS20Version)
			{
				AppLaunchWithCommand(
					sysFileCDefaultApp, 
					sysAppLaunchCmdNormalLaunch, NULL);
			}
		}

		return sysErrRomIncompatible;
	}

	return errNone;
}

////////////////////////////////////////////////////////////////////////////////
UInt32 PilotMain(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags)
{
	Err error;

	error = RomVersionCompatible (ourMinVersion, launchFlags);
	if (error) return (error);

	switch (cmd)
	{
		case sysAppLaunchCmdNormalLaunch:
			error = AppStart();
			if (error) 
				return error;

			FrmGotoForm(MainForm);
			AppEventLoop();

			AppStop();
			break;
	}

	return errNone;
}


////////////////////////////////////////////////////////////////////////////////
// YAHMLib callback function.
// This function fill persistent settings. Those settings can't be changed during execution.
void YAHM_GetPersistSettings(YAHM_persistSettings *pSettings){
	pSettings->protectYAHM = true;
	pSettings->thunkCount = 40;
}

////////////////////////////////////////////////////////////////////////////////
// YAHMLib callback function.
// This functions is responsible for saving YAHMLib runtime info
// Runtime info should not be released after app exit. 
// Usually runtime info pointer is saved into feature
YAHM_runtimeSettings *YAHM_GetRuntimeSettingsPtr(void){
	YAHM_runtimeSettings *pSet = NULL;
	FtrGet(appFileCreator, RUNTIME_PTR_FTR, (UInt32 *)&pSet);
	if (pSet == NULL){
		pSet = MemPtrNew(sizeof(YAHM_runtimeSettings));
		FtrSet(appFileCreator, RUNTIME_PTR_FTR, (UInt32)pSet);
		MemPtrSetOwner(pSet, 0);
		MemSet(pSet, sizeof(YAHM_runtimeSettings), 0);
	}
	return pSet;
}

////////////////////////////////////////////////////////////////////////////////
// YAHMLib callback function.
// This function calls when new YAHMLib found incompatible runtime info. 
// The only way to releae old runtime is reset
void YAHM_warnAboutIncompatibleUpdate(void){
	FrmAlert(IncomatibleUpdateAlert);
	SysReset();
}

////////////////////////////////////////////////////////////////////////////////
// Simple install hack wrapper with double activation prevention.
void InstallHack(void){
	UInt32 cnt = 0;
	FtrGet(appFileCreator, INSTALL_COUNT_FTR, &cnt);
	if (cnt == 0){
		Err err = YAHM_InstallHack();
		if (err == errNone){
			FtrSet(appFileCreator, INSTALL_COUNT_FTR, 1);
		}else{
			char buf[20];
			StrIToA(buf, err);
			FrmCustomAlert(PatchInstalledWithErrorAlert, buf, "", "");
		}
	}else{
		FrmCustomAlert(PatchAlreadyInstalledAlert, "1", "2", "3");
	}
}
				
////////////////////////////////////////////////////////////////////////////////
// Simple uninstall hack wrapper with double activation prevention.
void UninstallHack(void){
	UInt32 cnt = 0;
	FtrGet(appFileCreator, INSTALL_COUNT_FTR, &cnt);
	if (cnt == 1){
		Err err = YAHM_UninstallHack();
		if (err == errNone){
			FtrUnregister(appFileCreator, INSTALL_COUNT_FTR);
		}else{
			char buf[20];
			StrIToA(buf, err);
			FrmCustomAlert(PatchUninstalledWithErrorAlert, buf, "", "");
		}
	}else{
		FrmAlert(PatchNotInstalledAlert);
	}
}
