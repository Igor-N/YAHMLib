/*
 * YAHMLib.c
 *
 * main file for YAHMLib
 *
 * This wizard-generated code is based on code adapted from the
 * stationery files distributed as part of the Palm OS SDK 4.0.
 *
 * Copyright (c) 1999-2000 Palm, Inc. or its subsidiaries.
 * All rights reserved.
 */
 
#include <PalmOS.h>
#include <PalmOSGlue.h>
#include <PceNativeCall.h>
#include "InMemory.h"


#include "YAHMLib_Rsc.h"
#include "yahm_lib.h"

#define ourMinVersion    sysMakeROMVersion(5,0,0,sysROMStageDevelopment,0)
#define kPalmOS20Version sysMakeROMVersion(2,0,0,sysROMStageDevelopment,0)

static void * GetObjectPtr(UInt16 objectID)
{
	FormType * frmP;

	frmP = FrmGetActiveForm();
	return FrmGetObjectPtr(frmP, FrmGetObjectIndex(frmP, objectID));
}

static void MainFormInit(FormType *frmP)
{
}

static Boolean MainFormDoCommand(UInt16 command)
{
	Boolean handled = false;

	switch (command)
	{
		case ALERT_MENU:
			FrmCustomAlert(DebugAlert, "foo", "bar", "boz");
	}

	return handled;
}

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
			if (eventP->data.ctlSelect.controlID == MainEnableButton)
			{
				YAHM_SyscallInfo5 si[2];
				MemHandle h;
				void *p;
				void *pp;
				pp = MemPtrNew(sizeof(void *) * 2);
				si[0].baseTableOffset = 12;
				si[0].offset = 0x204;
				si[0].flags = THUNK_CW << 1;
				
				si[1].baseTableOffset = 8;
				si[1].offset = 0x824;
				si[1].flags = THUNK_CW << 1;
				h = DmGet1Resource('armc', 1000);
				p = MemHandleLock(h);
				PceNativeCall(p, pp);
				DmReleaseResource(h);
				YAHM_InstallTraps(2, pp, si, p);
				MemPtrFree(pp);
			}else if (eventP->data.ctlSelect.controlID == MainDisableButton)
			{
				MemHandle h;
				void *p;
				void *pp;
				pp = MemPtrNew(sizeof(void *) * 2);

				h = DmGet1Resource('armc', 1000);
				p = MemHandleLock(h);
				PceNativeCall(p, pp);
				DmReleaseResource(h);
				MemHandleUnlock(h);
				MemHandleUnlock(h);
				YAHM_UninstallTraps(2, pp);
				MemPtrFree(pp);
			}

			break;
		}
	}
    
	return handled;
}

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

static Err AppStart(void)
{

	return errNone;
}

static void AppStop(void)
{
        
	FrmCloseAllForms();

}

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

void YAHM_GetPersistSettings(YAHM_persistSettings *pSettings){
	pSettings->protectYAHM = true;
	pSettings->thunkCount = 40;
}

 void YAHM_SetRuntimeSettings(YAHM_runtimeSettings *pSettings)
 {
 	FtrSet('STNT', 12, (UInt32)pSettings);
 }

 YAHM_runtimeSettings *YAHM_GetRuntimeSettingsPtr(void)
{
	YAHM_runtimeSettings *pSet = NULL;
	FtrGet('STNT', 12, (UInt32 *)&pSet);
	if (pSet == NULL){
		pSet = MemPtrNew(sizeof(YAHM_runtimeSettings));
		YAHM_SetRuntimeSettings(pSet);
		MemPtrSetOwner(pSet, 0);
		MemSet(pSet, sizeof(YAHM_runtimeSettings), 0);
	}
	return pSet;
}

void YAHM_warnAboutIncompatibleUpdate(void)
{
}

