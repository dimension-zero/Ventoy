/******************************************************************************
 * PhyDrive.c
 *
 * Copyright (c) 2020, longpanda <admin@ventoy.net>
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
#include <time.h>
#include <winternl.h>
#include <commctrl.h>
#include <initguid.h>
#include "resource.h"
#include "Language.h"
#include "Ventoy2Disk.h"
#include "fat_filelib.h"
#include "ff.h"
#include "DiskService.h"
#include "PhyDrive_priv.h"

int PartitionResizeForVentoy(PHY_DRIVE_INFO *pPhyDrive)
{
	int i, j;
	int rc = 1;
	int PhyDrive;
	int PartStyle;
	UINT64 RecudeBytes;
	GUID Guid;
	MBR_HEAD MBR;
	VTOY_GPT_INFO *pGPT;
	MBR_HEAD *pMBR;
	DWORD dwSize = 0;
	VTOY_GPT_HDR BackupHead;
	HANDLE hDrive = INVALID_HANDLE_VALUE;
	GUID ZeroGuid = { 0 };
	static GUID WindowsDataPartType = { 0xebd0a0a2, 0xb9e5, 0x4433, { 0x87, 0xc0, 0x68, 0xb6, 0xb7, 0x26, 0x99, 0xc7 } };
	static GUID EspPartType = { 0xc12a7328, 0xf81f, 0x11d2, { 0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b } };
	static GUID BiosGrubPartType = { 0x21686148, 0x6449, 0x6e6f, { 0x74, 0x4e, 0x65, 0x65, 0x64, 0x45, 0x46, 0x49 } };

	Log("#####################################################");
	Log("PartitionResizeForVentoy PhyDrive%d <<%s %s %dGB>>",
		pPhyDrive->PhyDrive, pPhyDrive->VendorId, pPhyDrive->ProductId,
		GetHumanReadableGBSize(pPhyDrive->SizeInBytes));
	Log("#####################################################");

	pGPT = &(pPhyDrive->Gpt);
	pMBR = &(pPhyDrive->Gpt.MBR);
	Log("Disksize:%llu Part2Start:%llu", pPhyDrive->SizeInBytes, pPhyDrive->ResizePart2StartSector * 512);

	if (pMBR->PartTbl[0].FsFlag == 0xEE && memcmp(pGPT->Head.Signature, "EFI PART", 8) == 0)
	{
		PartStyle = 1;
	}
	else
	{
		PartStyle = 0;
	}

	PROGRESS_BAR_SET_POS(PT_LOCK_FOR_CLEAN);

	RecudeBytes = VENTOY_EFI_PART_SIZE;

	if (pPhyDrive->ResizeNoShrink == FALSE)
	{
		Log("Need to shrink the volume");
		if (DISK_ShrinkVolume(pPhyDrive->PhyDrive, pPhyDrive->ResizeVolumeGuid, pPhyDrive->Part1DriveLetter, pPhyDrive->ResizeOldPart1Size, RecudeBytes))
		{
			Log("Shrink volume success, now check again");

			hDrive = GetPhysicalHandle(pPhyDrive->PhyDrive, TRUE, TRUE, FALSE);
			if (hDrive == INVALID_HANDLE_VALUE)
			{
				Log("Failed to GetPhysicalHandle for update.");
				goto End;
			}

			//Refresh Drive Layout
			DeviceIoControl(hDrive, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, &dwSize, NULL);

			CHECK_CLOSE_HANDLE(hDrive);


			if (PartResizePreCheck(NULL) && pPhyDrive->ResizeNoShrink)
			{
				Log("Recheck after Shrink volume success");
				Log("After shrink Disksize:%llu Part2Start:%llu", pPhyDrive->SizeInBytes, pPhyDrive->ResizePart2StartSector * 512);
			}
			else
			{
				Log("Recheck after Shrink volume failed %u", pPhyDrive->ResizeNoShrink);
				goto End;
			}
		}
		else
		{
			Log("Shrink volume failed");
			goto End;
		}
	}


	//Now try write data
	hDrive = GetPhysicalHandle(pPhyDrive->PhyDrive, TRUE, TRUE, FALSE);
	if (hDrive == INVALID_HANDLE_VALUE)
	{
		Log("Failed to GetPhysicalHandle for update.");
		goto End;
	}


	//Write partition 2 data
	PROGRESS_BAR_SET_POS(PT_FORMAT_PART2);
	if (0 != FormatPart2Fat(hDrive, pPhyDrive->ResizePart2StartSector))
	{
		Log("FormatPart2Fat failed.");
		goto End;
	}

	//Write grub stage2 gap
	PROGRESS_BAR_SET_POS(PT_WRITE_STG1_IMG);
	Log("Writing Boot Image ............................. ");
	if (WriteGrubStage1ToPhyDrive(hDrive, PartStyle) != 0)
	{
		Log("WriteGrubStage1ToPhyDrive failed.");
		goto End;
	}


	//Write partition table
	PROGRESS_BAR_SET_POS(PT_WRITE_PART_TABLE);
	Log("Writing partition table ............................. ");

	VentoyGetLocalBootImg(&MBR);
	CoCreateGuid(&Guid);
	memcpy(MBR.BootCode + 0x180, &Guid, 16);
	memcpy(pMBR->BootCode, MBR.BootCode, 440);

	if (PartStyle == 0)
	{
		for (i = 1; i < 4; i++)
		{
			if (pMBR->PartTbl[i].SectorCount == 0)
			{
				break;
			}
		}

		if (i >= 4)
		{
			Log("Can not find MBR free partition table");
			goto End;
		}

		for (j = i - 1; j > 0; j--)
		{
			Log("Move MBR partition table %d --> %d", j + 1, j + 2);
			memcpy(pMBR->PartTbl + (j + 1), pMBR->PartTbl + j, sizeof(PART_TABLE));
		}

        memset(pMBR->PartTbl + 1, 0, sizeof(PART_TABLE));
		VentoyFillMBRLocation(pPhyDrive->SizeInBytes, (UINT32)pPhyDrive->ResizePart2StartSector, VENTOY_EFI_PART_SIZE / 512, pMBR->PartTbl + 1);
		pMBR->PartTbl[0].Active = 0x80; // bootable
		pMBR->PartTbl[1].Active = 0x00;
		pMBR->PartTbl[1].FsFlag = 0xEF; // EFI System Partition

		if (!WriteDataToPhyDisk(hDrive, 0, pMBR, 512))
		{
			Log("Legacy BIOS write MBR failed");
			goto End;
		}
	}
	else
	{
		for (i = 1; i < 128; i++)
		{
			if (memcmp(&(pGPT->PartTbl[i].PartGuid), &ZeroGuid, sizeof(GUID)) == 0)
			{
				break;
			}
		}

		if (i >= 128)
		{
			Log("Can not find GPT free partition table");
			goto End;
		}

		for (j = i - 1; j > 0; j--)
		{
			Log("Move GPT partition table %d --> %d", j + 1, j + 2);
			memcpy(pGPT->PartTbl + (j + 1), pGPT->PartTbl + j, sizeof(VTOY_GPT_PART_TBL));
		}


		pMBR->BootCode[92] = 0x22;

		// to fix windows issue
        memset(pGPT->PartTbl + 1, 0, sizeof(VTOY_GPT_PART_TBL));
		memcpy(&(pGPT->PartTbl[1].PartType), &WindowsDataPartType, sizeof(GUID));
		CoCreateGuid(&(pGPT->PartTbl[1].PartGuid));

		pGPT->PartTbl[1].StartLBA = pGPT->PartTbl[0].LastLBA + 1;
		pGPT->PartTbl[1].LastLBA = pGPT->PartTbl[1].StartLBA + VENTOY_EFI_PART_SIZE / 512 - 1;
		pGPT->PartTbl[1].Attr = VENTOY_EFI_PART_ATTR;
		memcpy(pGPT->PartTbl[1].Name, L"VTOYEFI", 7 * 2);

		//Update CRC
		pGPT->Head.PartTblCrc = VentoyCrc32(pGPT->PartTbl, sizeof(pGPT->PartTbl));
        pGPT->Head.Crc = 0;
		pGPT->Head.Crc = VentoyCrc32(&(pGPT->Head), pGPT->Head.Length);

		Log("pGPT->Head.EfiStartLBA=%llu", (ULONGLONG)pGPT->Head.EfiStartLBA);
		Log("pGPT->Head.EfiBackupLBA=%llu", (ULONGLONG)pGPT->Head.EfiBackupLBA);

		VentoyFillBackupGptHead(pGPT, &BackupHead);
		if (!WriteDataToPhyDisk(hDrive, pGPT->Head.EfiBackupLBA * 512, &BackupHead, 512))
		{
			Log("UEFI write backup head failed");
			goto End;
		}

		if (!WriteDataToPhyDisk(hDrive, (pGPT->Head.EfiBackupLBA - 32) * 512, pGPT->PartTbl, 512 * 32))
		{
			Log("UEFI write backup partition table failed");
			goto End;
		}

		if (!WriteDataToPhyDisk(hDrive, 0, pGPT, 512 * 34))
		{
			Log("UEFI write MBR & Main partition table failed");
			goto End;
		}
	}



	//Refresh Drive Layout
	DeviceIoControl(hDrive, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, &dwSize, NULL);
	
	//We must close handle here, because it will block the refresh bellow
	CHECK_CLOSE_HANDLE(hDrive);

	Sleep(2000);

    if (g_CLI_Mode)
    {
        Log("### Ventoy non-destructive CLI installation successfully finished.");
    }
    else
    {
        //Refresh disk list
        PhyDrive = pPhyDrive->PhyDrive;

        Log("#### Now Refresh PhyDrive ####");
        Ventoy2DiskDestroy();
        Ventoy2DiskInit();

        pPhyDrive = GetPhyDriveInfoByPhyDrive(PhyDrive);
        if (pPhyDrive)
        {
            if (pPhyDrive->VentoyVersion[0] == 0)
            {
                Log("After process the Ventoy version is still invalid");
                goto End;
            }

            Log("### Ventoy non-destructive installation successfully finished <%s>", pPhyDrive->VentoyVersion);
        }
        else
        {
            Log("### Ventoy non-destructive installation successfully finished <not found>");
        }

        InitComboxCtrl(g_DialogHwnd, PhyDrive);
    }

	rc = 0;

End:
	CHECK_CLOSE_HANDLE(hDrive);
	return rc;
}


static BOOL DiskCheckWriteAccess(HANDLE hDrive)
{
	DWORD dwSize;
	BOOL ret = FALSE;
	BOOL bRet = FALSE;
	BYTE Buffer[512];
	LARGE_INTEGER liCurPosition;
	LARGE_INTEGER liNewPosition;

	liCurPosition.QuadPart = 2039 * 512;
	liNewPosition.QuadPart = 0;
	if (0 == SetFilePointerEx(hDrive, liCurPosition, &liNewPosition, FILE_BEGIN) ||
		liNewPosition.QuadPart != liCurPosition.QuadPart)
	{
		Log("SetFilePointer1 Failed %u", LASTERR);
		goto out;
	}


	dwSize = 0;
	ret = ReadFile(hDrive, Buffer, 512, &dwSize, NULL);
	if ((!ret) || (dwSize != 512))
	{
		Log("Failed to read %d %u 0x%x", ret, dwSize, LASTERR);
		goto out;
	}


	liCurPosition.QuadPart = 2039 * 512;
	liNewPosition.QuadPart = 0;
	if (0 == SetFilePointerEx(hDrive, liCurPosition, &liNewPosition, FILE_BEGIN) ||
		liNewPosition.QuadPart != liCurPosition.QuadPart)
	{
		Log("SetFilePointer2 Failed %u", LASTERR);
		goto out;
	}

	dwSize = 0;
	ret = WriteFile(hDrive, Buffer, 512, &dwSize, NULL);
	if ((!ret) || dwSize != 512)
	{
		Log("Failed to write %d %u %u", ret, dwSize, LASTERR);
		goto out;
	}

	bRet = TRUE;

out:
	
	return bRet;
}

static BOOL BackupDataBeforeCleanDisk(int PhyDrive, UINT64 DiskSize, BYTE **pBackup)
{
	DWORD dwSize;
	DWORD dwStatus;
	BOOL Return = FALSE;
	BOOL ret = FALSE;
	BYTE *backup = NULL;
	UINT64 offset;
	HANDLE hDrive = INVALID_HANDLE_VALUE;
	LARGE_INTEGER liCurPosition;
	LARGE_INTEGER liNewPosition;
	VTOY_GPT_INFO *pGPT = NULL;

	Log("BackupDataBeforeCleanDisk %d", PhyDrive);

	// step1: check write access
	hDrive = GetPhysicalHandle(PhyDrive, TRUE, TRUE, FALSE);
	if (hDrive == INVALID_HANDLE_VALUE)
	{
		Log("Failed to GetPhysicalHandle for write.");
		goto out;
	}

	if (DiskCheckWriteAccess(hDrive))
	{
		Log("DiskCheckWriteAccess success");
		CHECK_CLOSE_HANDLE(hDrive);
	}
	else
	{
		Log("DiskCheckWriteAccess failed");
		goto out;
	}

	//step2 backup 4MB data
	backup = malloc(SIZE_1MB * 4);
	if (!backup)
	{
		goto out;
	}

	hDrive = GetPhysicalHandle(PhyDrive, FALSE, FALSE, FALSE);
	if (hDrive == INVALID_HANDLE_VALUE)
	{
		goto out;
	}

	//read first 2MB
	dwStatus = SetFilePointer(hDrive, 0, NULL, FILE_BEGIN);
	if (dwStatus != 0)
	{
		goto out;
	}
	
	dwSize = 0;
	ret = ReadFile(hDrive, backup, SIZE_2MB, &dwSize, NULL);
	if ((!ret) || (dwSize != SIZE_2MB))
	{
		Log("Failed to read %d %u 0x%x", ret, dwSize, LASTERR);
		goto out;
	}
	
	pGPT = (VTOY_GPT_INFO *)backup;
	offset = pGPT->Head.EfiBackupLBA * 512;
	if (offset >= (DiskSize - SIZE_2MB) && offset < DiskSize)
	{
		Log("EFI partition table check success"); 
	}
	else
	{
		Log("Backup EFI LBA not in last 2MB range: %llu", pGPT->Head.EfiBackupLBA);
		goto out;
	}

	//read last 2MB
	liCurPosition.QuadPart = DiskSize - SIZE_2MB;
	liNewPosition.QuadPart = 0;
	if (0 == SetFilePointerEx(hDrive, liCurPosition, &liNewPosition, FILE_BEGIN) ||
		liNewPosition.QuadPart != liCurPosition.QuadPart)
	{
		goto out;
	}

	dwSize = 0;
	ret = ReadFile(hDrive, backup + SIZE_2MB, SIZE_2MB, &dwSize, NULL);
	if ((!ret) || (dwSize != SIZE_2MB))
	{
		Log("Failed to read %d %u 0x%x", ret, dwSize, LASTERR);
		goto out;
	}

	*pBackup = backup;
	backup = NULL; //For don't free later
	Return = TRUE;

out:
	CHECK_CLOSE_HANDLE(hDrive);
	if (backup)
		free(backup);

	return Return;
}


static BOOL WriteBackupDataToDisk(HANDLE hDrive, UINT64 Offset, BYTE *Data, DWORD Length)
{
	DWORD dwSize = 0;
	BOOL ret = FALSE;
	LARGE_INTEGER liCurPosition;
	LARGE_INTEGER liNewPosition;

	Log("WriteBackupDataToDisk %llu %p %u", Offset, Data, Length);

	liCurPosition.QuadPart = Offset;
	liNewPosition.QuadPart = 0;
	if (0 == SetFilePointerEx(hDrive, liCurPosition, &liNewPosition, FILE_BEGIN) ||
		liNewPosition.QuadPart != liCurPosition.QuadPart)
	{
		return FALSE;
	}

	ret = WriteFile(hDrive, Data, Length, &dwSize, NULL);
	if ((!ret) || dwSize != Length)
	{
		Log("Failed to write %d %u %u", ret, dwSize, LASTERR);
		return FALSE;
	}

	Log("WriteBackupDataToDisk %llu %p %u success", Offset, Data, Length);
	return TRUE;
}

