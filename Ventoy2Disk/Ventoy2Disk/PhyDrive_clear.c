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

int ClearVentoyFromPhyDrive(HWND hWnd, PHY_DRIVE_INFO *pPhyDrive, char *pDrvLetter)
{
    int i;
    int rc = 0;
    int state = 0;
    HANDLE hDrive;
    DWORD dwSize;
    BOOL bRet;
    CHAR MountDrive;
    CHAR DriveName[] = "?:\\";
    CHAR DriveLetters[MAX_PATH] = { 0 };
    LARGE_INTEGER liCurrentPosition;
    char *pTmpBuf = NULL;
    MBR_HEAD MBR;

    *pDrvLetter = 0;

    Log("ClearVentoyFromPhyDrive PhyDrive%d <<%s %s %dGB>>",
        pPhyDrive->PhyDrive, pPhyDrive->VendorId, pPhyDrive->ProductId,
        GetHumanReadableGBSize(pPhyDrive->SizeInBytes));

    PROGRESS_BAR_SET_POS(PT_LOCK_FOR_CLEAN);

    Log("Lock disk for clean ............................. ");

    hDrive = GetPhysicalHandle(pPhyDrive->PhyDrive, TRUE, FALSE, FALSE);
    if (hDrive == INVALID_HANDLE_VALUE)
    {
        Log("Failed to open physical disk");
        return 1;
    }

    GetLettersBelongPhyDrive(pPhyDrive->PhyDrive, DriveLetters, sizeof(DriveLetters));

    if (DriveLetters[0] == 0)
    {
        Log("No drive letter was assigned...");
        DriveName[0] = GetFirstUnusedDriveLetter();
        Log("GetFirstUnusedDriveLetter %C: ...", DriveName[0]);
    }
    else
    {
        // Unmount all mounted volumes that belong to this drive
        // Do it in reverse so that we always end on the first volume letter
        for (i = (int)strlen(DriveLetters); i > 0; i--)
        {
            DriveName[0] = DriveLetters[i - 1];
            bRet = DeleteVolumeMountPointA(DriveName);
            Log("Delete mountpoint %s ret:%u code:%u", DriveName, bRet, GetLastError());
        }
    }

    MountDrive = DriveName[0];
    Log("Will use '%C:' as volume mountpoint", DriveName[0]);

    // It kind of blows, but we have to relinquish access to the physical drive
    // for VDS to be able to delete the partitions that reside on it...
    DeviceIoControl(hDrive, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &dwSize, NULL);
    CHECK_CLOSE_HANDLE(hDrive);

    PROGRESS_BAR_SET_POS(PT_DEL_ALL_PART);

    if (!VDS_DeleteAllPartitions(pPhyDrive->PhyDrive))
    {
        Log("Notice: Could not delete partitions: %u", GetLastError());
    }

    Log("Deleting all partitions ......................... OK");

    PROGRESS_BAR_SET_POS(PT_LOCK_FOR_WRITE);

    Log("Lock disk for write ............................. ");
    hDrive = GetPhysicalHandle(pPhyDrive->PhyDrive, TRUE, TRUE, FALSE);
    if (hDrive == INVALID_HANDLE_VALUE)
    {
        Log("Failed to GetPhysicalHandle for write.");
        rc = 1;
        goto End;
    }

    // clear first and last 2MB space
    pTmpBuf = malloc(SIZE_2MB);
    if (!pTmpBuf)
    {
        Log("Failed to alloc memory.");
        rc = 1;
        goto End;
    }
    memset(pTmpBuf, 0, SIZE_2MB);

    SET_FILE_POS(512);
    bRet = WriteFile(hDrive, pTmpBuf, SIZE_2MB - 512, &dwSize, NULL);
    Log("Write fisrt 1MB ret:%d size:%u err:%d", bRet, dwSize, LASTERR);
    if (!bRet)
    {
        rc = 1;
        goto End;
    }

    SET_FILE_POS(pPhyDrive->SizeInBytes - SIZE_2MB);
    bRet = WriteFile(hDrive, pTmpBuf, SIZE_2MB, &dwSize, NULL);
    Log("Write 2nd 1MB ret:%d size:%u err:%d", bRet, dwSize, LASTERR);
    if (!bRet)
    {
        rc = 1;
        goto End;
    }

    SET_FILE_POS(0);

    if (pPhyDrive->SizeInBytes > 2199023255552ULL)
    {
        VTOY_GPT_INFO *pGptInfo;
        VTOY_GPT_HDR BackupHead;
        LARGE_INTEGER liCurrentPosition;

        pGptInfo = (VTOY_GPT_INFO *)pTmpBuf;

        VentoyFillWholeGpt(pPhyDrive->SizeInBytes, pGptInfo);

        SET_FILE_POS(pPhyDrive->SizeInBytes - 512);
        VentoyFillBackupGptHead(pGptInfo, &BackupHead);
        if (!WriteFile(hDrive, &BackupHead, sizeof(VTOY_GPT_HDR), &dwSize, NULL))
        {
            rc = 1;
            Log("Write GPT Backup Head Failed, dwSize:%u (%u) ErrCode:%u", dwSize, sizeof(VTOY_GPT_INFO), GetLastError());
            goto End;
        }

        SET_FILE_POS(pPhyDrive->SizeInBytes - 512 * 33);
        if (!WriteFile(hDrive, pGptInfo->PartTbl, sizeof(pGptInfo->PartTbl), &dwSize, NULL))
        {
            rc = 1;
            Log("Write GPT Backup Part Table Failed, dwSize:%u (%u) ErrCode:%u", dwSize, sizeof(VTOY_GPT_INFO), GetLastError());
            goto End;
        }

        SET_FILE_POS(0);
        if (!WriteFile(hDrive, pGptInfo, sizeof(VTOY_GPT_INFO), &dwSize, NULL))
        {
            rc = 1;
            Log("Write GPT Info Failed, dwSize:%u (%u) ErrCode:%u", dwSize, sizeof(VTOY_GPT_INFO), GetLastError());
            goto End;
        }

        Log("Write GPT Info OK ...");
    }
    else
    {
        bRet = ReadFile(hDrive, &MBR, sizeof(MBR), &dwSize, NULL);
        Log("Read MBR ret:%d size:%u err:%d", bRet, dwSize, LASTERR);
        if (!bRet)
        {
            rc = 1;
            goto End;
        }

        //clear boot code and partition table (reserved disk signature)
        memset(MBR.BootCode, 0, 440);
        memset(MBR.PartTbl, 0, sizeof(MBR.PartTbl));

        VentoyFillMBRLocation(pPhyDrive->SizeInBytes, 2048, (UINT32)(pPhyDrive->SizeInBytes / 512 - 2048), MBR.PartTbl);

        MBR.PartTbl[0].Active = 0x00; // bootable
        MBR.PartTbl[0].FsFlag = 0x07; // exFAT/NTFS/HPFS

        SET_FILE_POS(0);
        bRet = WriteFile(hDrive, &MBR, 512, &dwSize, NULL);
        Log("Write MBR ret:%d size:%u err:%d", bRet, dwSize, LASTERR);
        if (!bRet)
        {
            rc = 1;
            goto End;
        }
    }

    Log("Clear Ventoy successfully finished");

	//Refresh Drive Layout
	DeviceIoControl(hDrive, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, &dwSize, NULL);

End:
    
    PROGRESS_BAR_SET_POS(PT_MOUNT_VOLUME);
    PROGRESS_BAR_SET_POS(PT_REFORMAT_FINISH);
    
    if (pTmpBuf)
    {
        free(pTmpBuf);
    }

    if (rc == 0)
    {
        Log("Mounting Ventoy Partition ....................... ");
        Sleep(1000);

        state = 0;
        memset(DriveLetters, 0, sizeof(DriveLetters));
        GetLettersBelongPhyDrive(pPhyDrive->PhyDrive, DriveLetters, sizeof(DriveLetters));
        Log("Logical drive letter after write ventoy: <%s>", DriveLetters);

        for (i = 0; i < sizeof(DriveLetters) && DriveLetters[i]; i++)
        {
            DriveName[0] = DriveLetters[i];
            Log("%s is ventoy part1, already mounted", DriveName);
            state = 1;
        }

        if (state != 1)
        {
            Log("need to mount ventoy part1...");
            if (0 == GetVentoyVolumeName(pPhyDrive->PhyDrive, 2048, DriveLetters, sizeof(DriveLetters), FALSE))
            {
                DriveName[0] = MountDrive;
                bRet = SetVolumeMountPointA(DriveName, DriveLetters);
                Log("SetVolumeMountPoint <%s> <%s> bRet:%u code:%u", DriveName, DriveLetters, bRet, GetLastError());

                *pDrvLetter = MountDrive;
            }
            else
            {
                Log("Failed to find ventoy volume");
            }
        }

        Log("OK\n");
    }
    else
    {
        FindProcessOccupyDisk(hDrive, pPhyDrive);
    }

    CHECK_CLOSE_HANDLE(hDrive);
    return rc;
}

