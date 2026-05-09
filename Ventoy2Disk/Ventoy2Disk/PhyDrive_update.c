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

static int DeleteVtoyEFIMountPoint(int PhyDrive)
{
    int i = 0;
	BOOL bRet;
	CHAR DriveLetters[MAX_PATH] = { 0 };
	CHAR DriveName[] = "?:\\";

    Log("Try to delete VtoyEFI mount point for PhyDrive %d\n", PhyDrive);

    GetLettersBelongPhyDrive(PhyDrive, DriveLetters, sizeof(DriveLetters));

	if (DriveLetters[0] == 0)
	{
		Log("No drive letter was assigned...");
	}
	else
	{
		// Unmount all mounted volumes that belong to this drive
		// Do it in reverse so that we always end on the first volume letter
		for (i = (int)strlen(DriveLetters); i > 0; i--)
		{
			DriveName[0] = DriveLetters[i - 1];
			if (IsVentoyLogicalDrive(DriveName[0]))
			{
				Log("%s is ventoy logical drive", DriveName);
				bRet = DeleteVolumeMountPointA(DriveName);
				Log("Delete mountpoint %s ret:%u code:%u", DriveName, bRet, LASTERR);
				break;
			}
		}
	}
    
    return 0;
}

int UpdateVentoy2PhyDrive(PHY_DRIVE_INFO *pPhyDrive, int TryId)
{
	int i;
	int rc = 0;
	int MaxRetry = 4;
	BOOL ForceMBR = FALSE;
	BOOL Esp2Basic = FALSE;
	BOOL ChangeAttr = FALSE;
	BOOL CleanDisk = FALSE;
	BOOL DelEFI = FALSE;
	BOOL bWriteBack = TRUE;
	BOOL bUpdateEFIAttr = FALSE;
	HANDLE hVolume;
	HANDLE hDrive;
	DWORD Status;
	DWORD dwSize;
	BOOL bRet;
	CHAR DriveName[] = "?:\\";
	CHAR DriveLetters[MAX_PATH] = { 0 };
	CHAR BackBinFile[MAX_PATH];
	UINT64 StartSector;
	UINT64 ReservedMB = 0;
	MBR_HEAD BootImg;
	MBR_HEAD MBR;
	BYTE *pBackup = NULL;
	VTOY_GPT_INFO *pGptInfo = NULL;
	VTOY_GPT_INFO *pGptBkup = NULL;
	UINT8 ReservedData[4096];

	Log("#####################################################");
	Log("UpdateVentoy2PhyDrive try%d %s PhyDrive%d <<%s %s %dGB>>", TryId,
		pPhyDrive->PartStyle ? "GPT" : "MBR", pPhyDrive->PhyDrive, pPhyDrive->VendorId, pPhyDrive->ProductId,
		GetHumanReadableGBSize(pPhyDrive->SizeInBytes));
	Log("#####################################################");

	PROGRESS_BAR_SET_POS(PT_LOCK_FOR_CLEAN);

	Log("Lock disk for umount ............................ ");

	hDrive = GetPhysicalHandle(pPhyDrive->PhyDrive, TRUE, FALSE, FALSE);
	if (hDrive == INVALID_HANDLE_VALUE)
	{
		Log("Failed to open physical disk");
		return 1;
	}

	if (pPhyDrive->PartStyle)
	{
		pGptInfo = malloc(2 * sizeof(VTOY_GPT_INFO));
		if (!pGptInfo)
		{
			return 1;
		}

		memset(pGptInfo, 0, 2 * sizeof(VTOY_GPT_INFO));
		pGptBkup = pGptInfo + 1;

		// Read GPT Info
		SetFilePointer(hDrive, 0, NULL, FILE_BEGIN);
		ReadFile(hDrive, pGptInfo, sizeof(VTOY_GPT_INFO), &dwSize, NULL);
		memcpy(pGptBkup, pGptInfo, sizeof(VTOY_GPT_INFO));

		//MBR will be used to compare with local boot image
		memcpy(&MBR, &pGptInfo->MBR, sizeof(MBR_HEAD));

		StartSector = pGptInfo->PartTbl[1].StartLBA;
		Log("GPT StartSector in PartTbl:%llu", (ULONGLONG)StartSector);

		ReservedMB = (pPhyDrive->SizeInBytes / 512 - (StartSector + VENTOY_EFI_PART_SIZE / 512) - 33) / 2048;
		Log("GPT Reserved Disk Space:%llu MB", (ULONGLONG)ReservedMB);
	}
	else
	{
		// Read MBR
		SetFilePointer(hDrive, 0, NULL, FILE_BEGIN);
		ReadFile(hDrive, &MBR, sizeof(MBR), &dwSize, NULL);

		StartSector = MBR.PartTbl[1].StartSectorId;
		Log("MBR StartSector in PartTbl:%llu", (ULONGLONG)StartSector);

		ReservedMB = (pPhyDrive->SizeInBytes / 512 - (StartSector + VENTOY_EFI_PART_SIZE / 512)) / 2048;
		Log("MBR Reserved Disk Space:%llu MB", (ULONGLONG)ReservedMB);
	}

	//Read Reserved Data
	SetFilePointer(hDrive, 512 * 2040, NULL, FILE_BEGIN);
	ReadFile(hDrive, ReservedData, sizeof(ReservedData), &dwSize, NULL);

    DeleteVtoyEFIMountPoint(pPhyDrive->PhyDrive);

	// It kind of blows, but we have to relinquish access to the physical drive
	// for VDS to be able to delete the partitions that reside on it...
	DeviceIoControl(hDrive, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &dwSize, NULL);
	CHECK_CLOSE_HANDLE(hDrive);

	if (pPhyDrive->PartStyle == 1)
	{
		Log("TryId=%d EFI GPT partition type is 0x%llx", TryId, pPhyDrive->Part2GPTAttr);
		PROGRESS_BAR_SET_POS(PT_DEL_ALL_PART);        

        if (pGptInfo->PartTbl[1].Attr != VENTOY_EFI_PART_ATTR)
        {
            bUpdateEFIAttr = TRUE;            
        }        


		if (TryId == 1)
		{
			Log("Change GPT partition type to ESP");
			if (DISK_ChangeVtoyEFI2ESP(pPhyDrive->PhyDrive, StartSector * 512ULL))
			{
				Esp2Basic = TRUE;
				Sleep(3000);
			}
		}
		else if (TryId == 2)
		{
			Log("Try2 Change GPT partition attribute to 0x%016llx", VENTOY_EFI_PART_ATTR & 0xFFFFFFFFFFFFFFFEULL);
			if (DISK_ChangeVtoyEFIAttr(pPhyDrive->PhyDrive, StartSector * 512ULL, VENTOY_EFI_PART_ATTR & 0xFFFFFFFFFFFFFFFEULL))
			{
				ChangeAttr = TRUE;
				Sleep(2000);
			}
		}
		else if (TryId == 3)
		{
			DISK_DeleteVtoyEFIPartition(pPhyDrive->PhyDrive, StartSector * 512ULL);
			DelEFI = TRUE;
		}
		else if (TryId == 4)
		{
			Log("Clean disk GPT partition table");
			if (BackupDataBeforeCleanDisk(pPhyDrive->PhyDrive, pPhyDrive->SizeInBytes, &pBackup))
			{
				sprintf_s(BackBinFile, sizeof(BackBinFile), ".\\ventoy\\phydrive%d_%u_%d.bin",
					pPhyDrive->PhyDrive, GetCurrentProcessId(), g_backup_bin_index++);
				SaveBufToFile(BackBinFile, pBackup, 4 * SIZE_1MB);
				Log("Save backup data to %s", BackBinFile);

				Log("Success to backup data before clean");
				CleanDisk = TRUE;
				DISK_CleanDisk(pPhyDrive->PhyDrive);
				Sleep(3000);
			}
			else
			{
				Log("Failed to backup data before clean");
			}
		}
	}
	
    PROGRESS_BAR_SET_POS(PT_LOCK_FOR_WRITE);

    Log("Lock disk for update ............................ ");
    hDrive = GetPhysicalHandle(pPhyDrive->PhyDrive, TRUE, TRUE, FALSE);
    if (hDrive == INVALID_HANDLE_VALUE)
    {
        Log("Failed to GetPhysicalHandle for write.");
        rc = 1;
        goto End;
    }

    PROGRESS_BAR_SET_POS(PT_LOCK_VOLUME);

    Log("Lock volume for update .......................... ");
    hVolume = INVALID_HANDLE_VALUE;

	//If we change VTOYEFI to ESP, it can not have s volume name, so don't try to get it.
	if (CleanDisk)
	{
		//writeback the last 2MB
		if (!WriteBackupDataToDisk(hDrive, pPhyDrive->SizeInBytes - SIZE_2MB, pBackup + SIZE_2MB, SIZE_2MB))
		{
			bWriteBack = FALSE;
		}

		//write the first 2MB except parttable
		if (!WriteBackupDataToDisk(hDrive, 34 * 512, pBackup + 34 * 512, SIZE_2MB - 34 * 512))
		{
			bWriteBack = FALSE;
		}

		Status = ERROR_NOT_FOUND;
	}
	else if (DelEFI)
	{
		Status = ERROR_NOT_FOUND;
	}
	else if (Esp2Basic)
	{
		Status = ERROR_NOT_FOUND;
	}
	else
	{
		for (i = 0; i < MaxRetry; i++)
		{
			Status = GetVentoyVolumeName(pPhyDrive->PhyDrive, StartSector, DriveLetters, sizeof(DriveLetters), TRUE);
			if (ERROR_SUCCESS == Status)
			{
				break;
			}
			else
			{
				Log("==== Volume not found, wait and retry %d... ====", i);
				Sleep(2);
			}
		}
	}
	
    if (ERROR_SUCCESS == Status)
    {
        Log("Now lock and dismount volume <%s>", DriveLetters);

        for (i = 0; i < MaxRetry; i++)
        {
            hVolume = CreateFileA(DriveLetters,
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ,
                NULL,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,
                NULL);

            if (hVolume == INVALID_HANDLE_VALUE)
            {
                Log("Failed to create file volume, errcode:%u, wait and retry ...", LASTERR);
                Sleep(2000);
            }
            else
            {
                break;
            }
        }

        if (hVolume == INVALID_HANDLE_VALUE)
        {
            Log("Failed to create file volume, errcode:%u", LASTERR);
        }
        else
        {
            bRet = DeviceIoControl(hVolume, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &dwSize, NULL);
            Log("FSCTL_LOCK_VOLUME bRet:%u code:%u", bRet, LASTERR);

            bRet = DeviceIoControl(hVolume, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &dwSize, NULL);
            Log("FSCTL_DISMOUNT_VOLUME bRet:%u code:%u", bRet, LASTERR);
        }
    }
    else if (ERROR_NOT_FOUND == Status)
    {
        Log("Volume not found, maybe not supported");
    }
    else
    {
        rc = 1;
        goto End;
    }

	bRet = TryWritePart2(hDrive, StartSector);
	if (FALSE == bRet && Esp2Basic)
	{
		Log("TryWritePart2 agagin ...");
		Sleep(3000);
		bRet = TryWritePart2(hDrive, StartSector);
	}

	if (!bRet)
    {
		if (pPhyDrive->PartStyle == 0)
		{
			if (DiskCheckWriteAccess(hDrive))
			{
				Log("MBR DiskCheckWriteAccess success");

				ForceMBR = TRUE;

				Log("Try write failed, now delete partition 2 for MBR...");
				CHECK_CLOSE_HANDLE(hDrive);

				Log("Now delete partition 2...");
				DISK_DeleteVtoyEFIPartition(pPhyDrive->PhyDrive, StartSector * 512ULL);

				hDrive = GetPhysicalHandle(pPhyDrive->PhyDrive, TRUE, TRUE, FALSE);
				if (hDrive == INVALID_HANDLE_VALUE)
				{
					Log("Failed to GetPhysicalHandle for write.");
					rc = 1;
					goto End;
				}
			}
			else
			{
				Log("MBR DiskCheckWriteAccess failed");
			}
		}
		else
		{
			Log("TryWritePart2 failed ....");
			rc = 1;
			goto End;
		}
    }

    PROGRESS_BAR_SET_POS(PT_FORMAT_PART2);

    Log("Write Ventoy to disk ............................ ");
    if (0 != FormatPart2Fat(hDrive, StartSector))
    {
        rc = 1;
        goto End;
    }

    if (hVolume != INVALID_HANDLE_VALUE)
    {
        bRet = DeviceIoControl(hVolume, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &dwSize, NULL);
        Log("FSCTL_UNLOCK_VOLUME bRet:%u code:%u", bRet, LASTERR);
        CHECK_CLOSE_HANDLE(hVolume);
    }

    Log("Updating Boot Image ............................. ");
    if (WriteGrubStage1ToPhyDrive(hDrive, pPhyDrive->PartStyle) != 0)
    {
        rc = 1;
        goto End;
    }

    //write reserved data
    SetFilePointer(hDrive, 512 * 2040, NULL, FILE_BEGIN);    
    bRet = WriteFile(hDrive, ReservedData, sizeof(ReservedData), &dwSize, NULL);
    Log("Write resv data ret:%u dwSize:%u Error:%u", bRet, dwSize, LASTERR);

    // Boot Image
    VentoyGetLocalBootImg(&BootImg);

    // Use Old UUID
    memcpy(BootImg.BootCode + 0x180, MBR.BootCode + 0x180, 16);
    if (pPhyDrive->PartStyle)
    {
        BootImg.BootCode[92] = 0x22;
    }

    if (ForceMBR == FALSE && memcmp(BootImg.BootCode, MBR.BootCode, 440) == 0)
    {
        Log("Boot image has no difference, no need to write.");
    }
    else
    {
        Log("Boot image need to write %u.", ForceMBR);

        SetFilePointer(hDrive, 0, NULL, FILE_BEGIN);

        memcpy(MBR.BootCode, BootImg.BootCode, 440);
        bRet = WriteFile(hDrive, &MBR, 512, &dwSize, NULL);
        Log("Write Boot Image ret:%u dwSize:%u Error:%u", bRet, dwSize, LASTERR);
    }

    if (pPhyDrive->PartStyle == 0)
    {
        if (0x00 == MBR.PartTbl[0].Active && 0x80 == MBR.PartTbl[1].Active)
        {
            Log("Need to chage 1st partition active and 2nd partition inactive.");

            MBR.PartTbl[0].Active = 0x80;
            MBR.PartTbl[1].Active = 0x00;

            SetFilePointer(hDrive, 0, NULL, FILE_BEGIN);
            bRet = WriteFile(hDrive, &MBR, 512, &dwSize, NULL);
            Log("Write NEW MBR ret:%u dwSize:%u Error:%u", bRet, dwSize, LASTERR);
        }
    }

	if (CleanDisk)
	{
		if (!WriteBackupDataToDisk(hDrive, 0, pBackup, 34 * 512))
		{
			bWriteBack = FALSE;
		}

		free(pBackup);

		if (bWriteBack)
		{
			Log("Write backup data success, now delete %s", BackBinFile);
			DeleteFileA(BackBinFile);
		}
		else
		{
			Log("Write backup data failed");
		}

		Sleep(1000);
	}
	else if (DelEFI)
	{
		VTOY_GPT_HDR BackupHdr;

		VentoyFillBackupGptHead(pGptBkup, &BackupHdr);
		if (!WriteBackupDataToDisk(hDrive, 512 * pGptBkup->Head.EfiBackupLBA, (BYTE*)(&BackupHdr), 512))
		{
			bWriteBack = FALSE;
		}

		if (!WriteBackupDataToDisk(hDrive, 512 * (pGptBkup->Head.EfiBackupLBA - 32), (BYTE*)(pGptBkup->PartTbl), 32 * 512))
		{
			bWriteBack = FALSE;
		}

		if (!WriteBackupDataToDisk(hDrive, 512, (BYTE*)pGptBkup + 512, 33 * 512))
		{
			bWriteBack = FALSE;
		}

		if (bWriteBack)
		{
			Log("Write backup partition table success");
		}
		else
		{
			Log("Write backup partition table failed");
		}

		Sleep(1000);
	}

    //Refresh Drive Layout
    DeviceIoControl(hDrive, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, &dwSize, NULL);

End:

	if (hVolume != INVALID_HANDLE_VALUE)
	{
		bRet = DeviceIoControl(hVolume, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &dwSize, NULL);
		Log("FSCTL_UNLOCK_VOLUME bRet:%u code:%u", bRet, LASTERR);
		CHECK_CLOSE_HANDLE(hVolume);
	}

    if (rc == 0)
    {
        Log("OK");
    }
    else
    {
		PROGRESS_BAR_SET_POS(PT_LOCK_FOR_CLEAN);
        FindProcessOccupyDisk(hDrive, pPhyDrive);
    }

    CHECK_CLOSE_HANDLE(hDrive);

    if (Esp2Basic)
    {
		Log("Recover GPT partition type to basic");
		DISK_ChangeVtoyEFI2Basic(pPhyDrive->PhyDrive, StartSector * 512);
    }


	if (pPhyDrive->PartStyle == 1)
	{
		if (ChangeAttr || bUpdateEFIAttr)
		{
			Log("Change EFI partition attr %u <0x%llx> to <0x%llx>", ChangeAttr, pGptInfo->PartTbl[1].Attr, VENTOY_EFI_PART_ATTR);
			if (DISK_ChangeVtoyEFIAttr(pPhyDrive->PhyDrive, StartSector * 512ULL, VENTOY_EFI_PART_ATTR))
			{
				Log("Change EFI partition attr success");
				pPhyDrive->Part2GPTAttr = VENTOY_EFI_PART_ATTR;

                Sleep(1000);
                DeleteVtoyEFIMountPoint(pPhyDrive->PhyDrive);
			}
			else
			{
				Log("Change EFI partition attr failed");
			}
		}
	}

    if (pGptInfo)
    {
        free(pGptInfo);
    }
    
    return rc;
}


