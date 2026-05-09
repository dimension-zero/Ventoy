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

int g_backup_bin_index = 0;


BOOL WriteDataToPhyDisk(HANDLE hDrive, UINT64 Offset, VOID *buffer, DWORD len)
{
	BOOL bRet;
	DWORD dwSize = 0;
	LARGE_INTEGER liCurPosition;
	LARGE_INTEGER liNewPosition;

	liCurPosition.QuadPart = (LONGLONG)Offset;
	liNewPosition.QuadPart = 0;
	if (0 == SetFilePointerEx(hDrive, liCurPosition, &liNewPosition, FILE_BEGIN) ||
		liNewPosition.QuadPart != liCurPosition.QuadPart)
	{
		Log("SetFilePointerEx Failed %u", LASTERR);
		return FALSE;
	}

	bRet = WriteFile(hDrive, buffer, len, &dwSize, NULL);
	if (bRet == FALSE || dwSize != len)
	{
		Log("Write file error %u %u", dwSize, LASTERR);
		return FALSE;
	}

	return TRUE;
}


DWORD GetVentoyVolumeName(int PhyDrive, UINT64 StartSectorId, CHAR *NameBuf, UINT32 BufLen, BOOL DelSlash)
{
    size_t len;
    BOOL bRet;
    DWORD dwSize;
    HANDLE hDrive;
    HANDLE hVolume;
    UINT64 PartOffset;
    DWORD Status = ERROR_NOT_FOUND;
    DISK_EXTENT *pExtents = NULL;
    CHAR VolumeName[MAX_PATH] = { 0 };
    VOLUME_DISK_EXTENTS DiskExtents;

    PartOffset = 512ULL * StartSectorId;

	Log("GetVentoyVolumeName PhyDrive %d SectorStart:%llu PartOffset:%llu", PhyDrive, (ULONGLONG)StartSectorId, (ULONGLONG)PartOffset);

    hVolume = FindFirstVolumeA(VolumeName, sizeof(VolumeName));
    if (hVolume == INVALID_HANDLE_VALUE)
    {
        return 1;
    }

    do {

        len = strlen(VolumeName);
        Log("Find volume:%s", VolumeName);

        VolumeName[len - 1] = 0;

        hDrive = CreateFileA(VolumeName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hDrive == INVALID_HANDLE_VALUE)
        {
            continue;
        }

        bRet = DeviceIoControl(hDrive,
            IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
            NULL,
            0,
            &DiskExtents,
            (DWORD)(sizeof(DiskExtents)),
            (LPDWORD)&dwSize,
            NULL);

        Log("IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS bRet:%u code:%u", bRet, LASTERR);
        Log("NumberOfDiskExtents:%u DiskNumber:%u", DiskExtents.NumberOfDiskExtents, DiskExtents.Extents[0].DiskNumber);

        if (bRet && DiskExtents.NumberOfDiskExtents == 1)
        {
            pExtents = DiskExtents.Extents;

            Log("This volume DiskNumber:%u offset:%llu", pExtents->DiskNumber, (ULONGLONG)pExtents->StartingOffset.QuadPart);
            if ((int)pExtents->DiskNumber == PhyDrive && pExtents->StartingOffset.QuadPart == PartOffset)
            {
                Log("This volume match");

                if (!DelSlash)
                {
                    VolumeName[len - 1] = '\\';
                }

                sprintf_s(NameBuf, BufLen, "%s", VolumeName);
                Status = ERROR_SUCCESS;
                CloseHandle(hDrive);
                break;
            }
        }

        CloseHandle(hDrive);
    } while (FindNextVolumeA(hVolume, VolumeName, sizeof(VolumeName)));

    FindVolumeClose(hVolume);

    Log("GetVentoyVolumeName return %u", Status);
    return Status;
}

int GetLettersBelongPhyDrive(int PhyDrive, char *DriveLetters, size_t Length)
{
    int n = 0;
    DWORD DataSize = 0;
    CHAR *Pos = NULL;
    CHAR *StringBuf = NULL;

    DataSize = GetLogicalDriveStringsA(0, NULL);
    StringBuf = (CHAR *)malloc(DataSize + 1);
    if (StringBuf == NULL)
    {
        return 1;
    }

    GetLogicalDriveStringsA(DataSize, StringBuf);

    for (Pos = StringBuf; *Pos; Pos += strlen(Pos) + 1)
    {
        if (n < (int)Length && PhyDrive == GetPhyDriveByLogicalDrive(Pos[0], NULL))
        {
            Log("%C: is belong to phydrive%d", Pos[0], PhyDrive);
            DriveLetters[n++] = Pos[0];
        }
    }

    free(StringBuf);
    return 0;
}

HANDLE GetPhysicalHandle(int Drive, BOOLEAN bLockDrive, BOOLEAN bWriteAccess, BOOLEAN bWriteShare)
{
    int i;
    DWORD dwSize;
    DWORD LastError;
    UINT64 EndTime;
    HANDLE hDrive = INVALID_HANDLE_VALUE;
    CHAR PhyDrive[128];
    CHAR DevPath[MAX_PATH] = { 0 };

    safe_sprintf(PhyDrive, "\\\\.\\PhysicalDrive%d", Drive);

    if (0 == QueryDosDeviceA(PhyDrive + 4, DevPath, sizeof(DevPath)))
    {
        Log("QueryDosDeviceA failed error:%u", GetLastError());
        strcpy_s(DevPath, sizeof(DevPath), "???");
    }
    else
    {
        Log("QueryDosDeviceA success %s", DevPath);
    }

    for (i = 0; i < DRIVE_ACCESS_RETRIES; i++)
    {
        // Try without FILE_SHARE_WRITE (unless specifically requested) so that
        // we won't be bothered by the OS or other apps when we set up our data.
        // However this means we might have to wait for an access gap...
        // We keep FILE_SHARE_READ though, as this shouldn't hurt us any, and is
        // required for enumeration.
        hDrive = CreateFileA(PhyDrive,
            GENERIC_READ | (bWriteAccess ? GENERIC_WRITE : 0),
            FILE_SHARE_READ | (bWriteShare ? FILE_SHARE_WRITE : 0),
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,
            NULL);

        LastError = GetLastError();
        Log("[%d] CreateFileA %s code:%u %p", i, PhyDrive, LastError, hDrive);

        if (hDrive != INVALID_HANDLE_VALUE)
        {
            break;
        }

        if ((LastError != ERROR_SHARING_VIOLATION) && (LastError != ERROR_ACCESS_DENIED))
        {
            break;
        }

        if (i == 0)
        {
            Log("Waiting for access on %s [%s]...", PhyDrive, DevPath);
        }
        else if (!bWriteShare && (i > DRIVE_ACCESS_RETRIES / 3))
        {
            // If we can't seem to get a hold of the drive for some time, try to enable FILE_SHARE_WRITE...
            Log("Warning: Could not obtain exclusive rights. Retrying with write sharing enabled...");
            bWriteShare = TRUE;

            // Try to report the process that is locking the drive
            // We also use bit 6 as a flag to indicate that SearchProcess was called.
            //access_mask = SearchProcess(DevPath, SEARCH_PROCESS_TIMEOUT, TRUE, TRUE, FALSE) | 0x40;

        }
        Sleep(DRIVE_ACCESS_TIMEOUT / DRIVE_ACCESS_RETRIES);
    }

    if (hDrive == INVALID_HANDLE_VALUE)
    {
        Log("Could not open %s %u", PhyDrive, LASTERR);
        goto End;
    }

    if (bWriteAccess)
    {
        Log("Opened %s for %s write access", PhyDrive, bWriteShare ? "shared" : "exclusive");
    }

    if (bLockDrive)
    {
        if (DeviceIoControl(hDrive, FSCTL_ALLOW_EXTENDED_DASD_IO, NULL, 0, NULL, 0, &dwSize, NULL))
        {
            Log("I/O boundary checks disabled");
        }

        EndTime = GetTickCount64() + DRIVE_ACCESS_TIMEOUT;

        do {
            if (DeviceIoControl(hDrive, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &dwSize, NULL))
            {
                Log("FSCTL_LOCK_VOLUME success");
                goto End;
            }
            Sleep(DRIVE_ACCESS_TIMEOUT / DRIVE_ACCESS_RETRIES);
        } while (GetTickCount64() < EndTime);

        // If we reached this section, either we didn't manage to get a lock or the user cancelled
        Log("Could not lock access to %s %u", PhyDrive, LASTERR);

        // See if we can report the processes are accessing the drive
        //if (!IS_ERROR(FormatStatus) && (access_mask == 0))
        //    access_mask = SearchProcess(DevPath, SEARCH_PROCESS_TIMEOUT, TRUE, TRUE, FALSE);
        // Try to continue if the only access rights we saw were for read-only
        //if ((access_mask & 0x07) != 0x01)
        //    safe_closehandle(hDrive);

        CHECK_CLOSE_HANDLE(hDrive);
    }

End:

    if (hDrive == INVALID_HANDLE_VALUE)
    {
        Log("Can get handle of %s, maybe some process control it.", DevPath);
    }

    return hDrive;
}

int GetPhyDriveByLogicalDrive(int DriveLetter, UINT64 *Offset)
{
    BOOL Ret = FALSE;
    DWORD dwSize = 0;
    HANDLE Handle = INVALID_HANDLE_VALUE;
    VOLUME_DISK_EXTENTS DiskExtents;
    CHAR PhyPath[128];

    safe_sprintf(PhyPath, "\\\\.\\%C:", (CHAR)DriveLetter);

    Handle = CreateFileA(PhyPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (Handle == INVALID_HANDLE_VALUE)
    {
        Log("Could not open the disk<%s>, error:%u", PhyPath, LASTERR);
        return -1;
    }

    memset(&DiskExtents, 0, sizeof(DiskExtents));
    Ret = DeviceIoControl(Handle,
        IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
        NULL,
        0,
        &DiskExtents,
        (DWORD)(sizeof(DiskExtents)),
        (LPDWORD)&dwSize,
        NULL);

    if (!Ret || DiskExtents.NumberOfDiskExtents == 0)
    {
        Log("DeviceIoControl IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS failed %s, error:%u", PhyPath, LASTERR);
        CHECK_CLOSE_HANDLE(Handle);
        return -1;
    }
    CHECK_CLOSE_HANDLE(Handle);

    Log("LogicalDrive:%s PhyDrive:%d Num:%d Offset:%llu ExtentLength:%llu",
        PhyPath,
        DiskExtents.Extents[0].DiskNumber,
        DiskExtents.NumberOfDiskExtents,
        DiskExtents.Extents[0].StartingOffset.QuadPart,
        DiskExtents.Extents[0].ExtentLength.QuadPart
        );

	if (Offset)
	{
		*Offset = (UINT64)(DiskExtents.Extents[0].StartingOffset.QuadPart);
	}

    return (int)DiskExtents.Extents[0].DiskNumber;
}

int GetAllPhysicalDriveInfo(PHY_DRIVE_INFO *pDriveList, DWORD *pDriveCount)
{
    int i;
    int Count;
    int id;
    int Letter = 'A';
    BOOL  bRet;
    DWORD dwBytes;
    DWORD DriveCount = 0;
    HANDLE Handle = INVALID_HANDLE_VALUE;
    CHAR PhyDrive[128];
    PHY_DRIVE_INFO *CurDrive = pDriveList;
    GET_LENGTH_INFORMATION LengthInfo;
    STORAGE_PROPERTY_QUERY Query;
    STORAGE_DESCRIPTOR_HEADER DevDescHeader;
    STORAGE_DEVICE_DESCRIPTOR *pDevDesc;
    STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR diskAlignment;
    int PhyDriveId[VENTOY_MAX_PHY_DRIVE];

    Count = GetPhysicalDriveCount();

    for (i = 0; i < Count && i < VENTOY_MAX_PHY_DRIVE; i++)
    {
        PhyDriveId[i] = i;
    }

    dwBytes = GetLogicalDrives();
    Log("Logical Drives: 0x%x", dwBytes);
    while (dwBytes)
    {
        if (dwBytes & 0x01)
        {
			id = GetPhyDriveByLogicalDrive(Letter, NULL);
            Log("%C --> %d", Letter, id);
            if (id >= 0)
            {
                for (i = 0; i < Count; i++)
                {
                    if (PhyDriveId[i] == id)
                    {
                        break;
                    }
                }

                if (i >= Count)
                {
                    Log("Add phy%d to list", i);
                    PhyDriveId[Count] = id;
                    Count++;
                }
            }
        }

        Letter++;
        dwBytes >>= 1;
    }

    for (i = 0; i < Count && DriveCount < VENTOY_MAX_PHY_DRIVE; i++)
    {
        CHECK_CLOSE_HANDLE(Handle);

        safe_sprintf(PhyDrive, "\\\\.\\PhysicalDrive%d", PhyDriveId[i]);
        Handle = CreateFileA(PhyDrive, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);        
        Log("Create file Handle:%p %s status:%u", Handle, PhyDrive, LASTERR);

        if (Handle == INVALID_HANDLE_VALUE)
        {
            continue;
        }

        bRet = DeviceIoControl(Handle,
                               IOCTL_DISK_GET_LENGTH_INFO, NULL,
                               0,
                               &LengthInfo,
                               sizeof(LengthInfo),
                               &dwBytes,
                               NULL);
        if (!bRet)
        {
            Log("DeviceIoControl IOCTL_DISK_GET_LENGTH_INFO failed error:%u", LASTERR);
            continue;
        }

        Log("PHYSICALDRIVE%d size %llu bytes", i, (ULONGLONG)LengthInfo.Length.QuadPart);

        Query.PropertyId = StorageDeviceProperty;
        Query.QueryType = PropertyStandardQuery;

        bRet = DeviceIoControl(Handle,
                               IOCTL_STORAGE_QUERY_PROPERTY,
                               &Query,
                               sizeof(Query),
                               &DevDescHeader,
                               sizeof(STORAGE_DESCRIPTOR_HEADER),
                               &dwBytes,
                               NULL);
        if (!bRet)
        {
            Log("DeviceIoControl1 error:%u dwBytes:%u", LASTERR, dwBytes);
            continue;
        }

        if (DevDescHeader.Size < sizeof(STORAGE_DEVICE_DESCRIPTOR))
        {
            Log("Invalid DevDescHeader.Size:%u", DevDescHeader.Size);
            continue;
        }

        pDevDesc = (STORAGE_DEVICE_DESCRIPTOR *)malloc(DevDescHeader.Size);
        if (!pDevDesc)
        {
            Log("failed to malloc error:%u len:%u", LASTERR, DevDescHeader.Size);
            continue;
        }

        bRet = DeviceIoControl(Handle,
                               IOCTL_STORAGE_QUERY_PROPERTY,
                               &Query,
                               sizeof(Query),
                               pDevDesc,
                               DevDescHeader.Size,
                               &dwBytes,
                               NULL);
        if (!bRet)
        {
            Log("DeviceIoControl2 error:%u dwBytes:%u", LASTERR, dwBytes);
            free(pDevDesc);
            continue;
        }



        memset(&Query, 0, sizeof(STORAGE_PROPERTY_QUERY));
        Query.PropertyId = StorageAccessAlignmentProperty;
        Query.QueryType = PropertyStandardQuery;
        memset(&diskAlignment, 0, sizeof(STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR));

        bRet = DeviceIoControl(Handle,
                               IOCTL_STORAGE_QUERY_PROPERTY,
                               &Query,
                               sizeof(STORAGE_PROPERTY_QUERY),
                               &diskAlignment,
                               sizeof(STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR),
                               &dwBytes,
                               NULL);
        if (!bRet)
        {
            Log("DeviceIoControl3 error:%u dwBytes:%u", LASTERR, dwBytes);            
        }

        CurDrive->PhyDrive = i;
        CurDrive->SizeInBytes = LengthInfo.Length.QuadPart;
        CurDrive->DeviceType = pDevDesc->DeviceType;
        CurDrive->RemovableMedia = pDevDesc->RemovableMedia;
        CurDrive->BusType = pDevDesc->BusType;

        CurDrive->BytesPerLogicalSector = diskAlignment.BytesPerLogicalSector;
        CurDrive->BytesPerPhysicalSector = diskAlignment.BytesPerPhysicalSector;

        if (pDevDesc->VendorIdOffset)
        {
            safe_strcpy(CurDrive->VendorId, (char *)pDevDesc + pDevDesc->VendorIdOffset);
            TrimString(CurDrive->VendorId);
        }

        if (pDevDesc->ProductIdOffset)
        {
            safe_strcpy(CurDrive->ProductId, (char *)pDevDesc + pDevDesc->ProductIdOffset);
            TrimString(CurDrive->ProductId);
        }

        if (pDevDesc->ProductRevisionOffset)
        {
            safe_strcpy(CurDrive->ProductRev, (char *)pDevDesc + pDevDesc->ProductRevisionOffset);
            TrimString(CurDrive->ProductRev);
        }

        if (pDevDesc->SerialNumberOffset)
        {
            safe_strcpy(CurDrive->SerialNumber, (char *)pDevDesc + pDevDesc->SerialNumberOffset);
            TrimString(CurDrive->SerialNumber);
        }

        CurDrive++;
        DriveCount++;

        free(pDevDesc);

        CHECK_CLOSE_HANDLE(Handle);
    }

    for (i = 0, CurDrive = pDriveList; i < (int)DriveCount; i++, CurDrive++)
    {
        Log("PhyDrv:%d BusType:%-4s Removable:%u Size:%dGB(%llu) Sector:%u/%u Name:%s %s",
            CurDrive->PhyDrive, GetBusTypeString(CurDrive->BusType), CurDrive->RemovableMedia,
            GetHumanReadableGBSize(CurDrive->SizeInBytes), CurDrive->SizeInBytes,
            CurDrive->BytesPerLogicalSector, CurDrive->BytesPerPhysicalSector,
            CurDrive->VendorId, CurDrive->ProductId);
    }

    *pDriveCount = DriveCount;

    return 0;
}

