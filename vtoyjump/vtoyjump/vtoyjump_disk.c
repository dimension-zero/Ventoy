/******************************************************************************
* vtoyjump.c
*
* Copyright (c) 2021, longpanda <admin@ventoy.net>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>
#include <virtdisk.h>
#include <winioctl.h>
#include <VersionHelpers.h>
#include "vtoyjump.h"
#include "fat_filelib.h"
#include "vtoyjump_priv.h"
int GetPhyDriveByLogicalDrive(int DriveLetter)
{
    BOOL Ret;
    DWORD dwSize;
    HANDLE Handle;
    VOLUME_DISK_EXTENTS DiskExtents;
    CHAR PhyPath[128];

    sprintf_s(PhyPath, sizeof(PhyPath), "\\\\.\\%C:", (CHAR)DriveLetter);

    Handle = CreateFileA(PhyPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (Handle == INVALID_HANDLE_VALUE)
    {
        Log("Could not open the disk<%s>, error:%u", PhyPath, GetLastError());
        return -1;
    }

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
        Log("DeviceIoControl IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS failed %s, error:%u", PhyPath, GetLastError());
        SAFE_CLOSE_HANDLE(Handle);
        return -1;
    }
    SAFE_CLOSE_HANDLE(Handle);

    Log("LogicalDrive:%s PhyDrive:%d Offset:%llu ExtentLength:%llu",
        PhyPath,
        DiskExtents.Extents[0].DiskNumber,
        DiskExtents.Extents[0].StartingOffset.QuadPart,
        DiskExtents.Extents[0].ExtentLength.QuadPart
        );

    return (int)DiskExtents.Extents[0].DiskNumber;
}


int DeleteVentoyPart2MountPoint(DWORD PhyDrive)
{
    CHAR Letter = 'A';
    DWORD Drives;
    DWORD PhyDisk;
    CHAR DriveName[] = "?:\\";

    Log("DeleteVentoyPart2MountPoint Phy%u ...", PhyDrive);

    Drives = GetLogicalDrives();
    while (Drives)
    {
        if ((Drives & 0x01) && IsFileExist("%C:\\ventoy\\ventoy.cpio", Letter))
        {
            Log("File %C:\\ventoy\\ventoy.cpio exist", Letter);

            PhyDisk = GetPhyDriveByLogicalDrive(Letter);
            Log("PhyDisk=%u for %C", PhyDisk, Letter);

            if (PhyDisk == PhyDrive)
            {
                DriveName[0] = Letter;
                DeleteVolumeMountPointA(DriveName);
                return 0;
            }
        }

        Letter++;
        Drives >>= 1;
    }

    return 1;
}

