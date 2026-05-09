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

ventoy_os_param g_os_param;
ventoy_windows_data g_windows_data;
UINT8 g_os_param_reserved[32];
INT g_system_bit = VTOY_BIT;
ventoy_guid g_ventoy_guid = VENTOY_GUID;
HANDLE g_vtoylog_mutex = NULL;
HANDLE g_vtoyins_mutex = NULL;

BOOL g_wimboot_mode = FALSE;

DWORD g_vtoy_disk_drive;

CHAR g_prog_full_path[MAX_PATH];
CHAR g_prog_dir[MAX_PATH];
CHAR g_prog_name[MAX_PATH];

#define VTOY_PECMD_PATH      "X:\\Windows\\system32\\ventoy\\PECMD.EXE"
#define ORG_PECMD_PATH       "X:\\Windows\\system32\\PECMD.EXE"
#define ORG_PECMD_BK_PATH    "X:\\Windows\\system32\\VTOYJUMP.EXE"

#define WIMBOOT_FILE         "X:\\Windows\\system32\\vtoy_wimboot"
#define WIMBOOT_DONE         "X:\\Windows\\system32\\vtoy_wimboot_done"

#define AUTO_RUN_BAT    "X:\\VentoyAutoRun.bat"
#define AUTO_RUN_LOG    "X:\\VentoyAutoRun.log"

#define VTOY_AUTO_FILE   "X:\\_vtoy_auto_install"

#define LOG_FILE  "X:\\Windows\\system32\\ventoy.log"
#define MUTEX_LOCK(hmutex)  if (hmutex != NULL) LockStatus = WaitForSingleObject(hmutex, INFINITE)
#define MUTEX_UNLOCK(hmutex)  if (hmutex != NULL && WAIT_OBJECT_0 == LockStatus) ReleaseMutex(hmutex)

#define BREAK()  BreakAndLaunchCmd(__LINE__)

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

int VentoyMountAnywhere(HANDLE Handle)
{
    DWORD Status;
    ATTACH_VIRTUAL_DISK_PARAMETERS AttachParameters;

    Log("VentoyMountAnywhere");

    memset(&AttachParameters, 0, sizeof(AttachParameters));
    AttachParameters.Version = ATTACH_VIRTUAL_DISK_VERSION_1;

    Status = AttachVirtualDisk(Handle, NULL, ATTACH_VIRTUAL_DISK_FLAG_READ_ONLY | ATTACH_VIRTUAL_DISK_FLAG_PERMANENT_LIFETIME, 0, &AttachParameters, NULL);
    if (Status != ERROR_SUCCESS)
    {
        Log("Failed to attach virtual disk ErrorCode:%u", Status);
        return 1;
    }

    return 0;
}

int VentoyMountY(HANDLE Handle)
{
    int  i;
    BOOL  bRet = FALSE;
    DWORD Status;
    DWORD physicalDriveNameSize;
    CHAR *Pos = NULL;
    WCHAR physicalDriveName[MAX_PATH];
    CHAR physicalDriveNameA[MAX_PATH];
    CHAR cdromDriveName[MAX_PATH];
    ATTACH_VIRTUAL_DISK_PARAMETERS AttachParameters;

    Log("VentoyMountY");

    memset(&AttachParameters, 0, sizeof(AttachParameters));
    AttachParameters.Version = ATTACH_VIRTUAL_DISK_VERSION_1;

    Status = AttachVirtualDisk(Handle, NULL, ATTACH_VIRTUAL_DISK_FLAG_READ_ONLY | ATTACH_VIRTUAL_DISK_FLAG_NO_DRIVE_LETTER | ATTACH_VIRTUAL_DISK_FLAG_PERMANENT_LIFETIME, 0, &AttachParameters, NULL);
    if (Status != ERROR_SUCCESS)
    {
        Log("Failed to attach virtual disk ErrorCode:%u", Status);
        return 1;
    }

    memset(physicalDriveName, 0, sizeof(physicalDriveName));
    memset(physicalDriveNameA, 0, sizeof(physicalDriveNameA));

    physicalDriveNameSize = MAX_PATH;
    Status = GetVirtualDiskPhysicalPath(Handle, &physicalDriveNameSize, physicalDriveName);
    if (Status != ERROR_SUCCESS)
    {
        Log("Failed GetVirtualDiskPhysicalPath ErrorCode:%u", Status);
        return 1;
    }

    for (i = 0; physicalDriveName[i]; i++)
    {
        physicalDriveNameA[i] = (CHAR)toupper((CHAR)(physicalDriveName[i]));
    }

    Log("physicalDriveNameA=<%s>", physicalDriveNameA);

    Pos = strstr(physicalDriveNameA, "CDROM");
    if (!Pos)
    {
        Log("Not cdrom phy drive");
        return 1;
    }

    sprintf_s(cdromDriveName, sizeof(cdromDriveName), "\\Device\\%s", Pos);
    Log("cdromDriveName=<%s>", cdromDriveName);

    for (i = 0; i < 3 && (bRet == FALSE); i++)
    {
        Sleep(1000);
        bRet = DefineDosDeviceA(DDD_RAW_TARGET_PATH, "Y:", cdromDriveName);
        Log("DefineDosDeviceA %s", bRet ? "success" : "failed");
    }

    return bRet ? 0 : 1;
}

BOOL VentoyAPINeedMountY(const char *IsoPath)
{
    (void)IsoPath;

    /* TBD */
    return FALSE;
}

int VentoyAttachVirtualDisk(HANDLE Handle, const char *IsoPath)
{
    int DriveYFree;
    DWORD Drives;
    
    Drives = GetLogicalDrives();
    if ((1 << 24) & Drives)
    {
        Log("Y: is occupied");
        DriveYFree = 0;
    }
    else
    {
        Log("Y: is free now");
        DriveYFree = 1;
    }

    if (DriveYFree && VentoyAPINeedMountY(IsoPath))
    {
        return VentoyMountY(Handle);
    }
    else
    {
        return VentoyMountAnywhere(Handle);
    }
}

int VentoyMountISOByAPI(const char *IsoPath)
{
    int i;
    HANDLE Handle;
    DWORD Status;
    WCHAR wFilePath[512] = { 0 };
    VIRTUAL_STORAGE_TYPE StorageType;
    OPEN_VIRTUAL_DISK_PARAMETERS OpenParameters;

    Log("VentoyMountISOByAPI <%s>", IsoPath);

    if (IsUTF8Encode(IsoPath))
    {
        Log("This is UTF8 encoding");
        MultiByteToWideChar(CP_UTF8, 0, IsoPath, (int)strlen(IsoPath), wFilePath, (int)(sizeof(wFilePath) / sizeof(WCHAR)));
    }
    else
    {
        Log("This is ANSI encoding");
        MultiByteToWideChar(CP_ACP, 0, IsoPath, (int)strlen(IsoPath), wFilePath, (int)(sizeof(wFilePath) / sizeof(WCHAR)));
    }

    memset(&StorageType, 0, sizeof(StorageType));
    memset(&OpenParameters, 0, sizeof(OpenParameters));
    
    OpenParameters.Version = OPEN_VIRTUAL_DISK_VERSION_1;

    for (i = 0; i < 10; i++)
    {
        Status = OpenVirtualDisk(&StorageType, wFilePath, VIRTUAL_DISK_ACCESS_READ, 0, &OpenParameters, &Handle);
        if (ERROR_FILE_NOT_FOUND == Status || ERROR_PATH_NOT_FOUND == Status)
        {
            Log("OpenVirtualDisk ErrorCode:%u, now wait and retry...", Status);
            Sleep(1000);
        }
        else
        {
            if (ERROR_SUCCESS == Status)
            {
                Log("OpenVirtualDisk success");
            }
            else if (ERROR_VIRTDISK_PROVIDER_NOT_FOUND == Status)
            {
                Log("VirtualDisk for ISO file is not supported in current system");
            }
            else
            {
                Log("Failed to open virtual disk ErrorCode:%u", Status);
            }
            break;
        }
    }

    if (Status != ERROR_SUCCESS)
    {
        return 1;
    }

    Log("OpenVirtualDisk success");

    Status = VentoyAttachVirtualDisk(Handle, IsoPath);
    if (Status != ERROR_SUCCESS)
    {
        Log("Failed to attach virtual disk ErrorCode:%u", Status);
        CloseHandle(Handle);
        return 1;
    }

    Log("VentoyAttachVirtualDisk success");

    CloseHandle(Handle);
    return 0;
}


HANDLE g_FatPhyDrive;
