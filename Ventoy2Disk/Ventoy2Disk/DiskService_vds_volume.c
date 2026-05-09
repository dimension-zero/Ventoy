/******************************************************************************
 * DiskService_vds.c
 *
 * Copyright (c) 2021, longpanda <admin@ventoy.net>
 * Copyright (c) 2011-2020, Pete Batard <pete@akeo.ie>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */
 
#include <Windows.h>
#include <winternl.h>
#include <commctrl.h>
#include <initguid.h>
#include <vds.h>
#include "Ventoy2Disk.h"
#include "DiskService.h"
#include "DiskService_vds_priv.h"

#define INTF_ADVANCEDDISK  1
#define INTF_ADVANCEDDISK2  2
#define INTF_CREATEPARTITIONEX  3
#define INTF_PARTITIONMF 4
#define INTF_VOLUME 5
#define INTF_VOLUME_MF3 6

/* 
 * Some code and functions in the file are copied from rufus.
 * https://github.com/pbatard/rufus
 */
#define VDS_SET_ERROR SetLastError
#define IVdsServiceLoader_LoadService(This, pwszMachineName, ppService) (This)->lpVtbl->LoadService(This, pwszMachineName, ppService)
#define IVdsServiceLoader_Release(This) (This)->lpVtbl->Release(This)
#define IVdsService_QueryProviders(This, masks, ppEnum) (This)->lpVtbl->QueryProviders(This, masks, ppEnum)
#define IVdsService_WaitForServiceReady(This) ((This)->lpVtbl->WaitForServiceReady(This))
#define IVdsService_CleanupObsoleteMountPoints(This) ((This)->lpVtbl->CleanupObsoleteMountPoints(This))
#define IVdsService_Refresh(This) ((This)->lpVtbl->Refresh(This))
#define IVdsService_Reenumerate(This) ((This)->lpVtbl->Reenumerate(This)) 
#define IVdsSwProvider_QueryInterface(This, riid, ppvObject) (This)->lpVtbl->QueryInterface(This, riid, ppvObject)
#define IVdsProvider_Release(This) (This)->lpVtbl->Release(This)
#define IVdsSwProvider_QueryPacks(This, ppEnum) (This)->lpVtbl->QueryPacks(This, ppEnum)
#define IVdsSwProvider_Release(This) (This)->lpVtbl->Release(This)
#define IVdsPack_QueryDisks(This, ppEnum) (This)->lpVtbl->QueryDisks(This, ppEnum)
#define IVdsDisk_GetProperties(This, pDiskProperties) (This)->lpVtbl->GetProperties(This, pDiskProperties)
#define IVdsDisk_Release(This) (This)->lpVtbl->Release(This)
#define IVdsDisk_QueryInterface(This, riid, ppvObject) (This)->lpVtbl->QueryInterface(This, riid, ppvObject)
#define IVdsAdvancedDisk_QueryPartitions(This, ppPartitionPropArray, plNumberOfPartitions) (This)->lpVtbl->QueryPartitions(This, ppPartitionPropArray, plNumberOfPartitions)
#define IVdsAdvancedDisk_DeletePartition(This, ullOffset, bForce, bForceProtected) (This)->lpVtbl->DeletePartition(This, ullOffset, bForce, bForceProtected)
#define IVdsAdvancedDisk_ChangeAttributes(This, ullOffset, para) (This)->lpVtbl->ChangeAttributes(This, ullOffset, para)
#define IVdsAdvancedDisk_CreatePartition(This, ullOffset, ullSize, para, ppAsync) (This)->lpVtbl->CreatePartition(This, ullOffset, ullSize, para, ppAsync)
#define IVdsAdvancedDisk_Clean(This, bForce, bForceOEM, bFullClean, ppAsync) (This)->lpVtbl->Clean(This, bForce, bForceOEM, bFullClean, ppAsync)
#define IVdsAdvancedDisk_Release(This) (This)->lpVtbl->Release(This)

#define IVdsAdvancedDisk2_ChangePartitionType(This, ullOffset, bForce, para) (This)->lpVtbl->ChangePartitionType(This, ullOffset, bForce, para)
#define IVdsAdvancedDisk2_Release(This) (This)->lpVtbl->Release(This)

#define IVdsCreatePartitionEx_CreatePartitionEx(This, ullOffset, ullSize, ulAlign, para, ppAsync) (This)->lpVtbl->CreatePartitionEx(This, ullOffset, ullSize, ulAlign, para, ppAsync)
#define IVdsCreatePartitionEx_Release(This) (This)->lpVtbl->Release(This)

#define IVdsPartitionMF_FormatPartitionEx(This, ullOffset, pwszFileSystemTypeName, usFileSystemRevision, ulDesiredUnitAllocationSize, pwszLabel, bForce, bQuickFormat, bEnableCompression, ppAsync) \
	(This)->lpVtbl->FormatPartitionEx(This, ullOffset, pwszFileSystemTypeName, usFileSystemRevision, ulDesiredUnitAllocationSize, pwszLabel, bForce, bQuickFormat, bEnableCompression, ppAsync)
#define IVdsPartitionMF_Release(This) (This)->lpVtbl->Release(This)

#define IEnumVdsObject_Next(This, celt, ppObjectArray, pcFetched) (This)->lpVtbl->Next(This, celt, ppObjectArray, pcFetched)
#define IVdsPack_QueryVolumes(This, ppEnum) (This)->lpVtbl->QueryVolumes(This, ppEnum)
#define IVdsVolume_QueryInterface(This, riid, ppvObject) (This)->lpVtbl->QueryInterface(This, riid, ppvObject)
#define IVdsVolume_Release(This) (This)->lpVtbl->Release(This)
#define IVdsVolumeMF3_QueryVolumeGuidPathnames(This, pwszPathArray, pulNumberOfPaths) (This)->lpVtbl->QueryVolumeGuidPathnames(This,pwszPathArray,pulNumberOfPaths)
#define IVdsVolumeMF_Format(This, type, pwsszLabel, dwUnitAllocationSize, bForce, bQuickFormat, bEnableCompression, ppAsync) (This)->lpVtbl->Format(This, type, pwsszLabel, dwUnitAllocationSize, bForce, bQuickFormat, bEnableCompression, ppAsync)
#define IVdsVolumeMF3_FormatEx2(This, pwszFileSystemTypeName, usFileSystemRevision, ulDesiredUnitAllocationSize, pwszLabel, Options, ppAsync) (This)->lpVtbl->FormatEx2(This, pwszFileSystemTypeName, usFileSystemRevision, ulDesiredUnitAllocationSize, pwszLabel, Options, ppAsync)
#define IVdsVolumeMF3_Release(This) (This)->lpVtbl->Release(This)
#define IVdsVolume_GetProperties(This, pVolumeProperties) (This)->lpVtbl->GetProperties(This,pVolumeProperties)
#define IVdsAsync_Cancel(This) (This)->lpVtbl->Cancel(This)
#define IVdsAsync_QueryStatus(This,pHrResult,pulPercentCompleted) (This)->lpVtbl->QueryStatus(This,pHrResult,pulPercentCompleted)
#define IVdsAsync_Wait(This,pHrResult,pAsyncOut) (This)->lpVtbl->Wait(This,pHrResult,pAsyncOut)
#define IVdsAsync_Release(This) (This)->lpVtbl->Release(This)

#define IVdsVolume_Shrink(This, ullNumberOfBytesToRemove, ppAsync) (This)->lpVtbl->Shrink(This, ullNumberOfBytesToRemove, ppAsync)

#define IUnknown_QueryInterface(This, a, b) (This)->lpVtbl->QueryInterface(This,a,b)
#define IUnknown_Release(This) (This)->lpVtbl->Release(This)

typedef BOOL(*VDS_Callback_PF)(void *pInterface, VDS_DISK_PROP *pDiskProp, UINT64 data);

STATIC BOOL VDS_VolumeCommProc(int intf, const WCHAR* wVolumeGuid, VDS_Callback_PF callback, UINT64 data)
{
	int Pos = 0;
	BOOL Find = FALSE;
	BOOL r = FALSE;
	HRESULT hr;
	ULONG ulFetched;
	IUnknown* pUnk = NULL;
	IEnumVdsObject* pEnum = NULL;
	IVdsService* pService = NULL;

	pService = VDS_InitService();
	if (!pService)
	{
		Log("Could not query VDS Service");
		goto out;
	}

	// Query the VDS Service Providers
	hr = IVdsService_QueryProviders(pService, VDS_QUERY_SOFTWARE_PROVIDERS, &pEnum);
	if (hr != S_OK)
	{
		VDS_SET_ERROR(hr);
		Log("Could not query VDS Service Providers: 0x%lx %u", hr, LASTERR);
		goto out;
	}

	while (IEnumVdsObject_Next(pEnum, 1, &pUnk, &ulFetched) == S_OK)
	{
		IVdsProvider* pProvider;
		IVdsSwProvider* pSwProvider;
		IEnumVdsObject* pEnumPack;
		IUnknown* pPackUnk;

		// Get VDS Provider
		hr = IUnknown_QueryInterface(pUnk, &IID_IVdsProvider, (void**)&pProvider);
		IUnknown_Release(pUnk);
		if (hr != S_OK)
		{
			VDS_SET_ERROR(hr);
			Log("Could not get VDS Provider: %u", LASTERR);
			goto out;
		}

		// Get VDS Software Provider
		hr = IVdsSwProvider_QueryInterface(pProvider, &IID_IVdsSwProvider, (void**)&pSwProvider);
		IVdsProvider_Release(pProvider);
		if (hr != S_OK)
		{
			VDS_SET_ERROR(hr);
			Log("Could not get VDS Software Provider: %u", LASTERR);
			goto out;
		}

		// Get VDS Software Provider Packs
		hr = IVdsSwProvider_QueryPacks(pSwProvider, &pEnumPack);
		IVdsSwProvider_Release(pSwProvider);
		if (hr != S_OK)
		{
			VDS_SET_ERROR(hr);
			Log("Could not get VDS Software Provider Packs: %u", LASTERR);
			goto out;
		}

		// Enumerate Provider Packs
		while (IEnumVdsObject_Next(pEnumPack, 1, &pPackUnk, &ulFetched) == S_OK)
		{
			IVdsPack* pPack;
			IEnumVdsObject* pEnumVolume;
			IUnknown* pVolumeUnk;

			hr = IUnknown_QueryInterface(pPackUnk, &IID_IVdsPack, (void**)&pPack);
			IUnknown_Release(pPackUnk);
			if (hr != S_OK)
			{
				VDS_SET_ERROR(hr);
				Log("Could not query VDS Software Provider Pack: %u", LASTERR);
				goto out;
			}

			// Use the pack interface to access the volume
			hr = IVdsPack_QueryVolumes(pPack, &pEnumVolume);;
			if (hr != S_OK) {
				VDS_SET_ERROR(hr);
				Log("Could not query VDS volume: %u", LASTERR);
				goto out;
			}

			// List disks
			while (IEnumVdsObject_Next(pEnumVolume, 1, &pVolumeUnk, &ulFetched) == S_OK)
			{
				IVdsVolume* pVolume;
				IVdsVolumeMF3* pVolumeMF3;
				LPWSTR* wszPathArray;
				ULONG ulNumberOfPaths;

				// Get the disk interface.
				hr = IUnknown_QueryInterface(pVolumeUnk, &IID_IVdsVolumeMF3, (void**)&pVolumeMF3);
				if (hr != S_OK) {
					VDS_SET_ERROR(hr);
					Log("Could not query VDS Volume Interface: %u", LASTERR);
					goto out;
				}

				// Get the volume properties
				hr = IVdsVolumeMF3_QueryVolumeGuidPathnames(pVolumeMF3, &wszPathArray, &ulNumberOfPaths);
				if ((hr != S_OK) && (hr != VDS_S_PROPERTIES_INCOMPLETE)) 
				{
					Log("Could not query VDS VolumeMF3 GUID PathNames: %s", GetVdsError(hr));
					IVdsVolume_Release(pVolumeMF3);
					IUnknown_Release(pVolumeUnk);
					continue;
				}

				Log("Get Volume %d %lu <%S>", intf, ulNumberOfPaths, wszPathArray[0]);

				if ((ulNumberOfPaths >= 1) && wcsstr(wszPathArray[0], wVolumeGuid))
				{
					Find = TRUE;
					Log("Call back for this Volume %d <%S>", intf, wVolumeGuid);

					if (INTF_VOLUME_MF3 == intf)
					{
						r = callback(pVolumeMF3, NULL, data);
					}
					else if (INTF_VOLUME == intf)
					{
						// Get the disk interface.
						hr = IUnknown_QueryInterface(pVolumeUnk, &IID_IVdsVolume, (void**)&pVolume);
						if (hr != S_OK) {
							VDS_SET_ERROR(hr);
							Log("Could not query VDS Volume Interface: %u", LASTERR);
						}
						else {
							r = callback(pVolume, NULL, data);
							IVdsVolume_Release(pVolume);
						}
					}
				}

				CoTaskMemFree(wszPathArray);
				IVdsVolume_Release(pVolumeMF3);
				IUnknown_Release(pVolumeUnk);

				if (Find)
				{
					goto out;
				}
			}
		}
	}

out:
	return r;
}

STATIC BOOL CHKDSK_Volume(CHAR LogicalDrive)
{
	CHAR CmdBuf[1024];
	STARTUPINFOA Si;
	PROCESS_INFORMATION Pi;

	if ((!IsFileExist("C:\\Windows\\System32\\chkdsk.exe")) || (LogicalDrive == 0))
	{
		return FALSE;
	}

	GetStartupInfoA(&Si);
	Si.dwFlags |= STARTF_USESHOWWINDOW;
	Si.wShowWindow = SW_HIDE;

	sprintf_s(CmdBuf, sizeof(CmdBuf), "C:\\Windows\\System32\\chkdsk.exe %C: /f", LogicalDrive);

	Log("CreateProcess <%s>", CmdBuf);
	CreateProcessA(NULL, CmdBuf, NULL, NULL, FALSE, 0, NULL, NULL, &Si, &Pi);

	Log("Wair process ...");
	WaitForSingleObject(Pi.hProcess, INFINITE);
	Log("Process finished...");

	CHECK_CLOSE_HANDLE(Pi.hProcess);
	CHECK_CLOSE_HANDLE(Pi.hThread);

	return TRUE;
}

STATIC HRESULT VDS_RealShrinkVolume(void* pInterface, VDS_DISK_PROP* pDiskProp, UINT64 data)
{
	HRESULT hr, hr2;
	IVdsVolume* pVolume = (IVdsVolume*)pInterface;
	ULONG completed;
	IVdsAsync* pAsync;
	VDS_PARA* VdsPara = (VDS_PARA*)data;

	(void)pDiskProp;

	Log("VDS_ShrinkVolume (%C:) (%llu) ...", VdsPara->DriveLetter, (ULONGLONG)VdsPara->Offset);

	hr = IVdsVolume_Shrink(pVolume, (ULONGLONG)VdsPara->Offset, &pAsync);

	while (SUCCEEDED(hr))
	{
		hr = IVdsAsync_QueryStatus(pAsync, &hr2, &completed);
		if (SUCCEEDED(hr))
		{
			hr = hr2;
			if (hr == S_OK)
			{
				Log("ShrinkVolume QueryStatus OK, %lu%%", completed);
				break;
			}
			else if (hr == VDS_E_OPERATION_PENDING)
			{
				Log("ShrinkVolume: %lu%%", completed);
				hr = S_OK;
			}
			else
			{
				Log("ShrinkVolume invalid status:0x%lx", hr);
			}
		}
		Sleep(1000);
	}

	return hr;
}

STATIC BOOL VDS_CallBack_ShrinkVolume(void* pInterface, VDS_DISK_PROP* pDiskProp, UINT64 data)
{
	int i;
	HRESULT hr;
	VDS_PARA *VdsPara = (VDS_PARA *)data;

	Log("VDS_CallBack_ShrinkVolume (%C:) (%llu) ...", VdsPara->DriveLetter, (ULONGLONG)VdsPara->Offset);

	hr = VDS_RealShrinkVolume(pInterface, pDiskProp, data);
	if (hr == VDS_E_SHRINK_DIRTY_VOLUME)
	{
		Log("Volume %C: is dirty, run chkdsk and retry.", VdsPara->DriveLetter);
		CHKDSK_Volume(VdsPara->DriveLetter);

		hr = VDS_RealShrinkVolume(pInterface, pDiskProp, data);
		if (hr == VDS_E_SHRINK_DIRTY_VOLUME)
		{
			Log("################################################################");
			Log("################################################################");
			for (i = 0; i < 20; i++)
			{
				Log("###### Volume dirty, Please run \"chkdsk /f %C:\" and retry. ######", VdsPara->Name[0]);
			}
			Log("################################################################");
			Log("################################################################");
		}
	}

	if (hr != S_OK)
	{
		VDS_SET_ERROR(hr);
		Log("Could not ShrinkVolume, 0x%x err:0x%lx (%s)", hr, LASTERR, WindowsErrorString(hr));

		VDS_SET_ERROR(hr);
		return FALSE;
	}

	return TRUE;
}

BOOL VDS_ShrinkVolume(int DriveIndex, const char* VolumeGuid, CHAR DriveLetter, UINT64 OldBytes, UINT64 ReduceBytes)
{
	int i;
	BOOL ret = FALSE;
	WCHAR wGuid[128] = { 0 };
	const char *guid = NULL;
	VDS_PARA Para;

	(VOID)DriveIndex;
	(VOID)OldBytes;

	guid = strstr(VolumeGuid, "{");
	if (!guid)
	{
		return FALSE;
	}

	for (i = 0; i < 128 && guid[i]; i++)
	{
		wGuid[i] = guid[i];
	}

	Para.Offset = ReduceBytes;
	Para.DriveLetter = DriveLetter;

	ret = VDS_VolumeCommProc(INTF_VOLUME, wGuid, VDS_CallBack_ShrinkVolume, (UINT64)&Para);
	Log("VDS_ShrinkVolume %C: ret:%d (%s)", DriveLetter, ret, ret ? "SUCCESS" : "FAIL");
	return ret;
}


STATIC BOOL VDS_CallBack_FormatVolume(void* pInterface, VDS_DISK_PROP* pDiskProp, UINT64 data)
{
	int fs;
	HRESULT hr, hr2;
	ULONG completed;
	IVdsAsync* pAsync;
	IVdsVolumeMF3* pVolume = (IVdsVolumeMF3*)pInterface;
	WCHAR* pFs = NULL;
	VDS_PARA* VdsPara = (VDS_PARA*)data;
	
	fs = (int)VdsPara->Attr;
	pFs = GetVentoyFsFmtNameByTypeW(fs);
	
	Log("VDS_CallBack_FormatVolume (%C:) (%s) ClusterSize:%u ...", VdsPara->DriveLetter, GetVentoyFsFmtNameByTypeA(fs), VdsPara->ClusterSize);

	WCHAR LabelW[64];
	MultiByteToWideChar(CP_ACP, 0, g_VolumeLabel, -1, LabelW, 64);
	hr = IVdsVolumeMF3_FormatEx2(pVolume, pFs, 0, VdsPara->ClusterSize, LabelW, VDS_FSOF_FORCE | VDS_FSOF_QUICK, &pAsync);
	while (SUCCEEDED(hr))
	{
		hr = IVdsAsync_QueryStatus(pAsync, &hr2, &completed);
		if (SUCCEEDED(hr))
		{
			hr = hr2;
			if (hr == S_OK)
			{
				Log("FormatVolume QueryStatus OK, %lu%%", completed);
				break;
			}
			else if (hr == VDS_E_OPERATION_PENDING)
			{
				Log("FormatVolume: %lu%%", completed);
				hr = S_OK;
			}
			else
			{
				Log("FormatVolume invalid status:0x%lx", hr);
			}
		}
		Sleep(1000);
	}

	if (hr != S_OK)
	{
		VDS_SET_ERROR(hr);
		Log("Could not FormatVolume, 0x%x err:0x%lx (%s)", hr, LASTERR, WindowsErrorString(hr));

		VDS_SET_ERROR(hr);
		return FALSE;
	}

	return TRUE;
}
BOOL VDS_FormatVolume(char DriveLetter, int fs, DWORD ClusterSize)
{
	int i;
	BOOL ret = FALSE;
	const char* guid = NULL;
	CHAR Drive[32] = { 0 };
	WCHAR wGuid[128] = { 0 };
	CHAR VolumeGuid[128] = { 0 };
	VDS_PARA Para;

	Drive[0] = DriveLetter;
	Drive[1] = ':';
	Drive[2] = '\\';
	GetVolumeNameForVolumeMountPointA(Drive, VolumeGuid, sizeof(VolumeGuid) / 2);

	guid = strstr(VolumeGuid, "{");
	if (!guid)
	{
		Log("Can not find volume GUID for %s:", Drive);
		return FALSE;
	}

	for (i = 0; i < 128 && guid[i]; i++)
	{
		wGuid[i] = guid[i];
	}
	Log("VDS_FormatVolume find GUID %C: <%s> ", DriveLetter, VolumeGuid);

	Para.Attr = fs;
	Para.DriveLetter = DriveLetter;
	Para.ClusterSize = ClusterSize;

	ret = VDS_VolumeCommProc(INTF_VOLUME_MF3, wGuid, VDS_CallBack_FormatVolume, (UINT64)&Para);
	Log("VDS_FormatVolume %C: <%s> ret:%d (%s)", DriveLetter, VolumeGuid, ret, ret ? "SUCCESS" : "FAIL");

	return ret;
}
