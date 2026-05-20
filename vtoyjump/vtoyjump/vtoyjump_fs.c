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
BOOL IsDirExist(const char *Fmt, ...)
{
    va_list Arg;    
    DWORD Attr;
    int UTF8 = 0;
    CHAR FilePathA[MAX_PATH];
    WCHAR FilePathW[MAX_PATH];

    va_start(Arg, Fmt);
    vsnprintf_s(FilePathA, sizeof(FilePathA), sizeof(FilePathA), Fmt, Arg);
    va_end(Arg);

    UTF8 = IsUTF8Encode(FilePathA);

    if (UTF8)
    {
        Utf8ToUtf16(FilePathA, FilePathW);
        Attr = GetFileAttributesW(FilePathW);
    }
    else
    {
        Attr = GetFileAttributesA(FilePathA);
    }
    
    if (Attr != INVALID_FILE_ATTRIBUTES && (Attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        return TRUE;
    }

    return FALSE;
}

BOOL IsFileExist(const char *Fmt, ...)
{
    va_list Arg;
    HANDLE hFile;
    DWORD Attr;
    BOOL bRet = FALSE;
    int UTF8 = 0;
    CHAR FilePathA[MAX_PATH];
    WCHAR FilePathW[MAX_PATH];

    va_start(Arg, Fmt);
    vsnprintf_s(FilePathA, sizeof(FilePathA), sizeof(FilePathA), Fmt, Arg);
    va_end(Arg);

    UTF8 = IsUTF8Encode(FilePathA);

    if (UTF8)
    {
        Utf8ToUtf16(FilePathA, FilePathW);
        hFile = CreateFileW(FilePathW, FILE_READ_EA, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    }
    else
    {
        hFile = CreateFileA(FilePathA, FILE_READ_EA, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    }
    if (INVALID_HANDLE_VALUE == hFile)
    {
        goto out;
    }

    CloseHandle(hFile);

    if (UTF8)
    {
        Attr = GetFileAttributesW(FilePathW);
    }
    else
    {
        Attr = GetFileAttributesA(FilePathA);
    }
    
    if (Attr & FILE_ATTRIBUTE_DIRECTORY)
    {
        goto out;
    }

    bRet = TRUE;

out:
    Log("File <%s> %s", FilePathA, (bRet ? "exist" : "NOT exist"));
    return bRet;
}

int GetPhyDiskUUID(const char LogicalDrive, UINT8 *UUID, UINT32 *DiskSig, DISK_EXTENT *DiskExtent)
{
    BOOL Ret;
    DWORD dwSize;
    HANDLE Handle;
    VOLUME_DISK_EXTENTS DiskExtents;
    CHAR PhyPath[128];
    UINT8 SectorBuf[512];

    Log("GetPhyDiskUUID %C", LogicalDrive);

    sprintf_s(PhyPath, sizeof(PhyPath), "\\\\.\\%C:", LogicalDrive);
    Handle = CreateFileA(PhyPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (Handle == INVALID_HANDLE_VALUE)
    {
        Log("Could not open the disk<%s>, error:%u", PhyPath, GetLastError());
        return 1;
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
        Log("DeviceIoControl IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS failed, error:%u", GetLastError());
        CloseHandle(Handle);
        return 1;
    }
    CloseHandle(Handle);

    memcpy(DiskExtent, DiskExtents.Extents, sizeof(DISK_EXTENT));
    Log("%C: is in PhysicalDrive%d Offset:%llu", LogicalDrive, DiskExtents.Extents[0].DiskNumber, 
        (ULONGLONG)(DiskExtents.Extents[0].StartingOffset.QuadPart));

    sprintf_s(PhyPath, sizeof(PhyPath), "\\\\.\\PhysicalDrive%d", DiskExtents.Extents[0].DiskNumber);
    Handle = CreateFileA(PhyPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (Handle == INVALID_HANDLE_VALUE)
    {
        Log("Could not open the disk<%s>, error:%u", PhyPath, GetLastError());
        return 1;
    }

    if (!ReadFile(Handle, SectorBuf, sizeof(SectorBuf), &dwSize, NULL))
    {
        Log("ReadFile failed, dwSize:%u  error:%u", dwSize, GetLastError());
        CloseHandle(Handle);
        return 1;
    }
    
    memcpy(UUID, SectorBuf + 0x180, 16);
    if (DiskSig)
    {
        memcpy(DiskSig, SectorBuf + 0x1B8, 4);
    }

    CloseHandle(Handle);
    return 0;
}

