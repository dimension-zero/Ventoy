/******************************************************************************
 * WinDialog.c
 *
 * Copyright © 2017-2019 Pete Batard <pete@akeo.ie>
 * Copyright (c) 2020, longpanda <admin@ventoy.net>
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
#include <Shlobj.h>
#include <tlhelp32.h>
#include <Psapi.h>
#include <commctrl.h>
#include "resource.h"
#include "Language.h"
#include "Ventoy2Disk.h"
#include "DiskService.h"
#include "VentoyJson.h"

HINSTANCE g_hInst;

BOOL g_InvalidClusterSize = FALSE;
BOOL g_SecureBoot = TRUE;
CHAR g_VolumeLabel[64] = "Ventoy";
HWND g_DialogHwnd;
HWND g_ComboxHwnd;
HWND g_LocalClusterTipHwnd;
HWND g_DiskClusterTipHwnd;
HWND g_StaticLocalVerHwnd;
HWND g_StaticDiskVerHwnd;
HWND g_StaticLocalStyleHwnd;
HWND g_StaticDiskStyleHwnd;
HWND g_StaticLocalFsHwnd;
HWND g_StaticDevFsHwnd;
HWND g_BtnInstallHwnd;
HWND g_StaticDevHwnd;
HWND g_StaticLocalHwnd;
HWND g_StaticDiskHwnd;
HWND g_BtnUpdateHwnd;
HWND g_ProgressBarHwnd;
HWND g_StaticStatusHwnd;
HWND g_LocalIconSecureHwnd;
HWND g_DiskIconSecureHwnd;
HANDLE g_ThreadHandle = NULL;

HFONT g_language_normal_font = NULL;
HFONT g_language_bold_font = NULL;

int g_cur_part_style = 0; // 0:MBR  1:GPT
int g_language_count = 0;
int g_cur_lang_id = 0;
VENTOY_LANGUAGE *g_language_data = NULL;
VENTOY_LANGUAGE *g_cur_lang_data = NULL;

int m_MinWidth;
int m_MinHeight;
BOOL PartResizePreCheck(PHY_DRIVE_INFO** ppPhyDrive)
{
    int i;
	int Index;
    int Count;
    int nCurSel;
	int PartStyle;
    BOOL bRet;
	BOOL FindFlag = FALSE;
    BOOL bCheck = FALSE;
    UINT64 FreeSize, Offset;
	UINT64 Part1Start, Part1End, NextPartStart;
    CHAR Drive[8] = { 0 };
    CHAR FsName[MAX_PATH];
	CHAR WinDir[MAX_PATH];
    HANDLE hDrive = INVALID_HANDLE_VALUE;
    VTOY_GPT_INFO* pGPT = NULL;
    PHY_DRIVE_INFO* pPhyDrive = NULL; 
    DWORD dwSize;
    DWORD SectorsPerCluster, BytesPerSector, NumberOfFreeClusters, TotalNumberOfClusters;
    GUID ZeroGuid = { 0 };
    CHAR VolumeGuid[128];

    Log("PartResizePreCheck ...");

    if (g_CLI_Mode)
    {
        pPhyDrive = CLI_PhyDrvInfo();
    }
    else
    {
        nCurSel = (int)SendMessage(g_ComboxHwnd, CB_GETCURSEL, 0, 0);
        if (CB_ERR == nCurSel)
        {
            Log("Failed to get combox sel");
            goto out;
        }

        pPhyDrive = GetPhyDriveInfoById(nCurSel);
        if (!pPhyDrive)
        {
            goto out;
        }
    }

	pPhyDrive->ResizeNoShrink = FALSE;
	pPhyDrive->ResizeVolumeGuid[0] = 0;
	pPhyDrive->FsName[0] = 0;

	if (ppPhyDrive)
	{
		*ppPhyDrive = pPhyDrive;
	}


    if (pPhyDrive->VentoyVersion[0])
    {
        Log("###[FAIL] No need to resize part");
        goto out;
    }

	if (pPhyDrive->DriveLetters[0] == 0)
	{
		Log("###[FAIL] No logical drive letter found for this disk");
		goto out;
	}

	//Get the BytesPerSector parameter
	sprintf_s(Drive, sizeof(Drive), "%C:", pPhyDrive->DriveLetters[0]);
	bRet = GetDiskFreeSpaceA(Drive, &SectorsPerCluster, &BytesPerSector, &NumberOfFreeClusters, &TotalNumberOfClusters);
	if (!bRet)
	{
		Log("Failed to GetDiskFreeSpaceA <%s> %u", Drive, LASTERR);
		goto out;
	}
	Log("BytesPerSector for this disk is %u", BytesPerSector);

	WinDir[0] = 0;
	GetWindowsDirectoryA(WinDir, sizeof(WinDir));

	//Print all logical drive letters
	FindFlag = FALSE;
	Drive[0] = FsName[0] = 0;
	for (i = 0; i < 64 && pPhyDrive->DriveLetters[i]; i++)
	{
		if (WinDir[1] == ':' && WinDir[0] == pPhyDrive->DriveLetters[i])
		{
			FindFlag = TRUE;
		}

		sprintf_s(Drive, sizeof(Drive), "%C: ", pPhyDrive->DriveLetters[i]);
		strcat_s(FsName, sizeof(FsName), Drive);
	}
	Log("Logical drives in this disk: %s (WinDir:%s)", FsName, WinDir);

	if (FindFlag)
	{
		Log("###[FAIL] You can not do non-destructive installation on Windows system disk.");
		goto out;
	}



    pGPT = malloc(sizeof(VTOY_GPT_INFO));
    if (!pGPT)
    {
        goto out;
    }

    hDrive = GetPhysicalHandle(pPhyDrive->PhyDrive, FALSE, FALSE, FALSE);
    if (hDrive == INVALID_HANDLE_VALUE)
    {
        goto out;
    }

    bRet = ReadFile(hDrive, pGPT, sizeof(VTOY_GPT_INFO), &dwSize, NULL);
    if (!bRet)
    {
        Log("Failed to read disk %u %u", bRet, LASTERR);
        goto out;
    }

	memcpy(&pPhyDrive->Gpt, pGPT, sizeof(VTOY_GPT_INFO));

	if (pGPT->MBR.PartTbl[0].FsFlag == 0xEE && memcmp(pGPT->Head.Signature, "EFI PART", 8) == 0)
	{
		pPhyDrive->PartStyle = PartStyle = 1;
	}
	else
	{
		pPhyDrive->PartStyle = PartStyle = 0;
	}

    if (PartStyle == 0)
    {
		PART_TABLE *PartTbl = pGPT->MBR.PartTbl;

        for (Count = 0, i = 0; i < 4; i++)
        {
            if (PartTbl[i].SectorCount > 0)
            {
				Log("MBR Part%d SectorStart:%u SectorCount:%u", i + 1, PartTbl[i].StartSectorId, PartTbl[i].SectorCount);
                Count++;
            }
        }

		//We must have a free partition table for VTOYEFI partition
		if (Count >= 4)
		{
			Log("###[FAIL] 4 MBR partition tables are all used.");
			goto out;
		}

		if (PartTbl[0].SectorCount > 0)
		{
			Part1Start = PartTbl[0].StartSectorId;
			Part1End = PartTbl[0].SectorCount + Part1Start;
		}
		else
		{
			Log("###[FAIL] MBR Partition 1 is invalid");
			goto out;
		}

		Index = -1;
		NextPartStart = (pPhyDrive->SizeInBytes / BytesPerSector);
		for (i = 1; i < 4; i++)
		{
			if (PartTbl[i].SectorCount > 0 && NextPartStart > PartTbl[i].StartSectorId)
			{
				Index = i;
				NextPartStart = PartTbl[i].StartSectorId;
			}
		}

		NextPartStart *= (UINT64)BytesPerSector;
		Log("DiskSize:%llu NextPartStart:%llu(LBA:%llu) Index:%d", pPhyDrive->SizeInBytes, NextPartStart, NextPartStart / BytesPerSector, Index);
    }
    else
    {
		VTOY_GPT_PART_TBL *PartTbl = pGPT->PartTbl;

        for (Count = 0, i = 0; i < 128; i++)
        {
            if (memcmp(&(PartTbl[i].PartGuid), &ZeroGuid, sizeof(GUID)))
            {
				Log("GPT Part%d StartLBA:%llu LastLBA:%llu", i + 1, (ULONGLONG)PartTbl[i].StartLBA, (ULONGLONG)PartTbl[i].LastLBA);
                Count++;
            }
        }

		if (Count >= 128)
		{
			Log("###[FAIL] 128 GPT partition tables are all used.");
			goto out;
		}

		if (memcmp(&(PartTbl[0].PartGuid), &ZeroGuid, sizeof(GUID)))
		{
			Part1Start = PartTbl[0].StartLBA;
			Part1End = PartTbl[0].LastLBA + 1;
		}
		else
		{
			Log("###[FAIL] GPT Partition 1 is invalid");
			goto out;
		}

		Index = -1;
		NextPartStart = (pGPT->Head.PartAreaEndLBA + 1);
		for (i = 1; i < 128; i++)
		{
			if (memcmp(&(PartTbl[i].PartGuid), &ZeroGuid, sizeof(GUID)) && NextPartStart > PartTbl[i].StartLBA)
			{
				Index = i;
				NextPartStart = PartTbl[i].StartLBA;
			}
		}

		NextPartStart *= (UINT64)BytesPerSector;
		Log("DiskSize:%llu NextPartStart:%llu(LBA:%llu) Index:%d", (ULONGLONG)pPhyDrive->SizeInBytes, (ULONGLONG)NextPartStart, (ULONGLONG)NextPartStart / BytesPerSector, Index);
    }

	Log("Valid partition table (%s): Valid partition count:%d", (PartStyle == 0) ? "MBR" : "GPT", Count);

	//Partition 1 MUST start at 1MB
	Part1Start *= (UINT64)BytesPerSector;
	Part1End *= (UINT64)BytesPerSector;

	Log("Partition 1 start at: %llu %lluKB, end:%llu, NextPartStart:%llu", 
		(ULONGLONG)Part1Start, (ULONGLONG)Part1Start / 1024, (ULONGLONG)Part1End, (ULONGLONG)NextPartStart);
    if (Part1Start != SIZE_1MB)
    {
        Log("###[FAIL] Partition 1 is not start at 1MB");
        goto out;
    }

	pPhyDrive->ResizeOldPart1Size = Part1End - Part1Start;

	//If we have free space after partition 1
	if (NextPartStart - Part1End >= VENTOY_EFI_PART_SIZE)
	{
		Log("Free space after partition 1 (%llu) is enough for VTOYEFI part", (ULONGLONG)(NextPartStart - Part1End));
		pPhyDrive->ResizeNoShrink = TRUE;
		pPhyDrive->ResizePart2StartSector = Part1End / BytesPerSector;
		bCheck = TRUE;
		goto out;
	}
	else if (NextPartStart == Part1End)
	{
		Log("There is no free space after partition 1");
	}
	else
	{
		Log("The free space after partition 1 is not enough");
	}


	//We don't have enough free space after partition 1.
	//So we need to shrink partition 1, firstly let's check the free space of the volume.
	for (FindFlag = FALSE, i = 0; i < 64 && pPhyDrive->DriveLetters[i]; i++)
	{
		if (GetPhyDriveByLogicalDrive(pPhyDrive->DriveLetters[i], &Offset) >= 0)
		{
			if (Offset == Part1Start)
			{
				Log("Find the partition 1 logical drive is %C:", pPhyDrive->DriveLetters[i]);

				FindFlag = TRUE;
				pPhyDrive->Part1DriveLetter = pPhyDrive->DriveLetters[i];

				sprintf_s(Drive, sizeof(Drive), "%C:", pPhyDrive->DriveLetters[i]);
				bRet = GetDiskFreeSpaceA(Drive, &SectorsPerCluster, &BytesPerSector, &NumberOfFreeClusters, &TotalNumberOfClusters);
				if (!bRet)
				{
					Log("Failed to GetDiskFreeSpaceA <%s> %u", Drive, LASTERR);
					goto out;
				}

				FreeSize = NumberOfFreeClusters;
				FreeSize *= (UINT64)SectorsPerCluster;
				FreeSize *= (UINT64)BytesPerSector;

				Log("SectorsPerCluster:%u BytesPerSector:%u NumberOfFreeClusters:%u TotalNumberOfClusters:%u  ",
					SectorsPerCluster, BytesPerSector, NumberOfFreeClusters, TotalNumberOfClusters, (ULONGLONG)FreeSize);
				Log("<%s> freespace %llu %lluMB %lluGB", Drive, FreeSize, FreeSize / SIZE_1MB, FreeSize / SIZE_1GB);

				if (FreeSize < VENTOY_EFI_PART_SIZE * 2)
				{
					Log("###[FAIL] Free space is not engough");
					goto out;
				}

				break;
			}
		}
	}

	if (!FindFlag)
	{
		Log("Can not find the logical drive for partition 1");
		goto out;
	}


	//The volume has enough free space. Get the volume GUID for next shrink phase.
    Drive[2] = '\\';
    bRet = GetVolumeNameForVolumeMountPointA(Drive, VolumeGuid, sizeof(VolumeGuid) / 2);
    Drive[2] = 0;
    if (!bRet)
    {
        Log("GetVolumeNameForVolumeMountPointA failed <%s> %u", Drive, LASTERR);
        goto out;
    }

    strcpy_s(pPhyDrive->ResizeVolumeGuid, sizeof(pPhyDrive->ResizeVolumeGuid), VolumeGuid);
    Log("Volume GUID: <%s>", VolumeGuid);

    if (0 == GetVolumeInformationA(Drive, NULL, 0, NULL, NULL, NULL, FsName, MAX_PATH))
    {
        Log("GetVolumeInformationA failed %u", LASTERR);
        goto out;
    }


	//Only NTFS is supported.
	Log("Partition 1 is %s", FsName);
    if (_stricmp(FsName, "NTFS"))
    {
        Log("###[FAIL] Only NTFS is supported.");
        goto out;
    }

	strcpy_s(pPhyDrive->FsName, sizeof(pPhyDrive->FsName), FsName);




    Log("PartResizePreCheck success ...");
    bCheck = TRUE;

out:

    CHECK_FREE(pGPT);
    CHECK_CLOSE_HANDLE(hDrive);

    return bCheck;
}

void OnPartResize(void)
{
    PHY_DRIVE_INFO* pPhyDrive = NULL;

	if (g_ThreadHandle)
	{
		Log("Another thread is runing");
		return;
	}

    if (!PartResizePreCheck(&pPhyDrive))
    {
        Log("#### Part Resize PreCheck Failed ####");
        MessageBox(g_DialogHwnd, _G(STR_PART_RESIZE_UNSUPPORTED), _G(STR_WARNING), MB_OK | MB_ICONWARNING);
        return;
    }

    if (MessageBox(g_DialogHwnd, _G(STR_PART_RESIZE_TIP), _G(STR_INFO), MB_YESNO | MB_ICONQUESTION) != IDYES)
    {
        return;
    }

    EnableWindow(g_BtnInstallHwnd, FALSE);
    EnableWindow(g_BtnUpdateHwnd, FALSE);

    g_ThreadHandle = CreateThread(NULL, 0, PartResizeThread, (LPVOID)pPhyDrive, 0, NULL);
}

void OnClearVentoy(void)
{
    int nCurSel;
    int SpaceMB = 0;
    int SizeInMB = 0;
    PHY_DRIVE_INFO *pPhyDrive = NULL;

    if (MessageBox(g_DialogHwnd, _G(STR_INSTALL_TIP), _G(STR_WARNING), MB_YESNO | MB_ICONWARNING) != IDYES)
    {
        return;
    }

    if (MessageBox(g_DialogHwnd, _G(STR_INSTALL_TIP2), _G(STR_WARNING), MB_YESNO | MB_ICONWARNING) != IDYES)
    {
        return;
    }

    if (g_ThreadHandle)
    {
        Log("Another thread is runing");
        return;
    }

    nCurSel = (int)SendMessage(g_ComboxHwnd, CB_GETCURSEL, 0, 0);
    if (CB_ERR == nCurSel)
    {
        Log("Failed to get combox sel");
        return;;
    }

    pPhyDrive = GetPhyDriveInfoById(nCurSel);
    if (!pPhyDrive)
    {
        return;
    }

    EnableWindow(g_BtnInstallHwnd, FALSE);
    EnableWindow(g_BtnUpdateHwnd, FALSE);

    g_ThreadHandle = CreateThread(NULL, 0, ClearVentoyThread, (LPVOID)pPhyDrive, 0, NULL);
}

