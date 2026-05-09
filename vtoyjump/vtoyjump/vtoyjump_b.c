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

UINT64 g_Part2StartSec;

int CopyFileFromFatDisk(const CHAR* SrcFile, const CHAR *DstFile)
{
    int rc = 1;
    int size = 0;
    char *buf = NULL;
    void *flfile = NULL;

    Log("CopyFileFromFatDisk (%s)==>(%s)", SrcFile, DstFile);

    flfile = fl_fopen(SrcFile, "rb");
    if (flfile)
    {
        fl_fseek(flfile, 0, SEEK_END);
        size = (int)fl_ftell(flfile);
        fl_fseek(flfile, 0, SEEK_SET);

        buf = (char *)malloc(size);
        if (buf)
        {
            fl_fread(buf, 1, size, flfile);

            rc = 0;
            SaveBuffer2File(DstFile, buf, size);
            free(buf);
        }

        fl_fclose(flfile);
    }

    return rc;
}

int VentoyFatDiskRead(uint32 Sector, uint8 *Buffer, uint32 SectorCount)
{
    DWORD dwSize;
    BOOL bRet;
    DWORD ReadSize;
    LARGE_INTEGER liCurrentPosition;

    liCurrentPosition.QuadPart = Sector + g_Part2StartSec;
    liCurrentPosition.QuadPart *= 512;
    SetFilePointerEx(g_FatPhyDrive, liCurrentPosition, &liCurrentPosition, FILE_BEGIN);

    ReadSize = (DWORD)(SectorCount * 512);

    bRet = ReadFile(g_FatPhyDrive, Buffer, ReadSize, &dwSize, NULL);
    if (bRet == FALSE || dwSize != ReadSize)
    {
        Log("ReadFile error bRet:%u WriteSize:%u dwSize:%u ErrCode:%u", bRet, ReadSize, dwSize, GetLastError());
    }

    return 1;
}

BOOL Is2K10PE(void)
{
    BOOL bRet = FALSE;
    FILE *fp = NULL;
    CHAR szLine[1024];

    fopen_s(&fp, "X:\\Windows\\System32\\PECMD.INI", "r");
    if (!fp)
    {
        return FALSE;
    }

    memset(szLine, 0, sizeof(szLine));
    while (fgets(szLine, sizeof(szLine) - 1, fp))
    {
        if (strstr(szLine, "2k10\\"))
        {
            bRet = TRUE;
            break;
        }
    }

    fclose(fp);
    return bRet;
}

CHAR GetIMDiskMountLogicalDrive(const char *suffix)
{
    CHAR Letter = 'Y';
    DWORD Drives;
    DWORD Mask = 0x1000000;

    // fixed use M as mountpoint for 2K10 PE
    if (Is2K10PE())
    {
        Log("Use M: for 2K10 PE");
        return 'M';
    }

    //fixed use Z as mountpoint for Lenovo Product Recovery
    if (strcmp(suffix, "VTLRI") == 0)
    {
        return 'Z';
    }

    Drives = GetLogicalDrives();
    Log("Drives=0x%x", Drives);
    
    while (Mask)
    {
        if ((Drives & Mask) == 0)
        {
            break;
        }

        Letter--;
        Mask >>= 1;
    }

    return Letter;
}

UINT64 GetVentoyEfiPartStartSector(HANDLE hDrive)
{
    BOOL bRet;
    DWORD dwSize; 
    MBR_HEAD MBR;   
    VTOY_GPT_INFO *pGpt = NULL;
    UINT64 StartSector = 0;

    SetFilePointer(hDrive, 0, NULL, FILE_BEGIN);

    bRet = ReadFile(hDrive, &MBR, sizeof(MBR), &dwSize, NULL);
    Log("Read MBR Ret:%u Size:%u code:%u", bRet, dwSize, LASTERR);

    if ((!bRet) || (dwSize != sizeof(MBR)))
    {
        0;
    }

    if (MBR.PartTbl[0].FsFlag == 0xEE)
    {
        Log("GPT partition style");

        pGpt = malloc(sizeof(VTOY_GPT_INFO));
        if (!pGpt)
        {
            return 0;
        }

        SetFilePointer(hDrive, 0, NULL, FILE_BEGIN);
        bRet = ReadFile(hDrive, pGpt, sizeof(VTOY_GPT_INFO), &dwSize, NULL);        
        if ((!bRet) || (dwSize != sizeof(VTOY_GPT_INFO)))
        {
            Log("Failed to read gpt info %d %u %d", bRet, dwSize, LASTERR);
            return 0;
        }

        StartSector = pGpt->PartTbl[1].StartLBA;
        free(pGpt);
    }
    else
    {
        Log("MBR partition style");
        StartSector = MBR.PartTbl[1].StartSectorId;
    }

    Log("GetVentoyEfiPart StartSector: %llu", StartSector);
    return StartSector;
}

int VentoyCopyImdisk(DWORD PhyDrive, CHAR *ImPath)
{
    int rc = 1;
    BOOL bRet;
    DWORD dwBytes;
    HANDLE hDrive;
    CHAR PhyPath[MAX_PATH];
    GET_LENGTH_INFORMATION LengthInfo;

    if (IsFileExist("X:\\Windows\\System32\\imdisk.exe"))
    {
        Log("imdisk.exe already exist, no need to copy...");
        strcpy_s(ImPath, MAX_PATH, "imdisk.exe");        
        return 0;
    }

    if (IsFileExist("X:\\Windows\\System32\\ventoy\\imdisk.exe"))
    {
        Log("imdisk.exe already copied, no need to copy...");
        strcpy_s(ImPath, MAX_PATH, "ventoy\\imdisk.exe");
        return 0;
    }

    sprintf_s(PhyPath, sizeof(PhyPath), "\\\\.\\PhysicalDrive%d", PhyDrive);
    hDrive = CreateFileA(PhyPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (hDrive == INVALID_HANDLE_VALUE)
    {
        Log("Could not open the disk<%s>, error:%u", PhyPath, GetLastError());
        goto End;
    }

    bRet = DeviceIoControl(hDrive, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0, &LengthInfo, sizeof(LengthInfo), &dwBytes, NULL);
    if (!bRet)
    {
        Log("Could not get phy disk %s size, error:%u", PhyPath, GetLastError());
        goto End;
    }

    g_FatPhyDrive = hDrive;
    g_Part2StartSec = GetVentoyEfiPartStartSector(hDrive);

    Log("Parse FAT fs...");

    fl_init();

    if (0 == fl_attach_media(VentoyFatDiskRead, NULL))
    {
        if (g_system_bit == 64)
        {
            CopyFileFromFatDisk("/ventoy/imdisk/64/imdisk.sys", "ventoy\\imdisk.sys");
            CopyFileFromFatDisk("/ventoy/imdisk/64/imdisk.exe", "ventoy\\imdisk.exe");
            CopyFileFromFatDisk("/ventoy/imdisk/64/imdisk.cpl", "ventoy\\imdisk.cpl");
        }
        else
        {
            CopyFileFromFatDisk("/ventoy/imdisk/32/imdisk.sys", "ventoy\\imdisk.sys");
            CopyFileFromFatDisk("/ventoy/imdisk/32/imdisk.exe", "ventoy\\imdisk.exe");
            CopyFileFromFatDisk("/ventoy/imdisk/32/imdisk.cpl", "ventoy\\imdisk.cpl");
        }

        GetCurrentDirectoryA(sizeof(PhyPath), PhyPath);
        strcat_s(PhyPath, sizeof(PhyPath), "\\ventoy\\imdisk.sys");

        if (LoadNtDriver(PhyPath) == 0)
        {        
            strcpy_s(ImPath, MAX_PATH, "ventoy\\imdisk.exe");
            rc = 0;
        }
    }
    fl_shutdown();

End:

    SAFE_CLOSE_HANDLE(hDrive);

    return rc;
}

int VentoyRunImdisk(const char *suffix, const char *IsoPath, const char *imdiskexe, const char *opt)
{
    CHAR Letter;
    CHAR Cmdline[512];
    WCHAR CmdlineW[512];
    PROCESS_INFORMATION Pi;

    Log("VentoyRunImdisk <%s> <%s> <%s> <%s>", suffix, IsoPath, imdiskexe, opt);

    Letter = GetIMDiskMountLogicalDrive(suffix);

    sprintf_s(Cmdline, sizeof(Cmdline), "%s -a -o %s -f \"%s\" -m %C:", imdiskexe, opt, IsoPath, Letter);    
    Log("mount iso to %C: use imdisk cmd <%s>", Letter, Cmdline);

    if (IsUTF8Encode(IsoPath))
    {
        STARTUPINFOW Si;
        GetStartupInfoW(&Si);
        Si.dwFlags |= STARTF_USESHOWWINDOW;
        Si.wShowWindow = SW_HIDE;

        Utf8ToUtf16(Cmdline, CmdlineW);
        CreateProcessW(NULL, CmdlineW, NULL, NULL, FALSE, 0, NULL, NULL, &Si, &Pi);

        Log("This is UTF8 encoding");
    }
    else
    {
        STARTUPINFOA Si;
        GetStartupInfoA(&Si);
        Si.dwFlags |= STARTF_USESHOWWINDOW;
        Si.wShowWindow = SW_HIDE;

        CreateProcessA(NULL, Cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &Si, &Pi);

        Log("This is ANSI encoding");
    }

    Log("Wait for imdisk process ...");
    WaitForSingleObject(Pi.hProcess, INFINITE);
    Log("imdisk process finished");

    return 0;
}

int VentoyMountISOByImdisk(const char *IsoPath, DWORD PhyDrive)
{
    int rc = 1;
    CHAR ImPath[MAX_PATH];

    Log("VentoyMountISOByImdisk %s", IsoPath);

    if (0 == VentoyCopyImdisk(PhyDrive, ImPath))
    {
        VentoyRunImdisk("iso", IsoPath, ImPath, "ro");
        rc = 0;
    }

    return rc;
}

int GetIsoId(CONST CHAR *IsoPath, IsoId *ids)
{
    int i;
    int n = 0;
    HANDLE hFile;
    DWORD dwSize = 0;
    BOOL bRet[8];

    hFile = CreateFileA(IsoPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return 1;
    }

    SetFilePointer(hFile, 2048 * 16 + 8, NULL, FILE_BEGIN);
    bRet[n++] = ReadFile(hFile, ids->SystemId, 32, &dwSize, NULL);
    
    SetFilePointer(hFile, 2048 * 16 + 40, NULL, FILE_BEGIN);
    bRet[n++] = ReadFile(hFile, ids->VolumeId, 32, &dwSize, NULL);

    SetFilePointer(hFile, 2048 * 16 + 318, NULL, FILE_BEGIN);
    bRet[n++] = ReadFile(hFile, ids->PulisherId, 128, &dwSize, NULL);

    SetFilePointer(hFile, 2048 * 16 + 446, NULL, FILE_BEGIN);
    bRet[n++] = ReadFile(hFile, ids->PreparerId, 128, &dwSize, NULL);

    CloseHandle(hFile);


    for (i = 0; i < n; i++)
    {
        if (bRet[i] == FALSE)
        {
            return 1;
        }
    }


    TrimString(ids->SystemId, FALSE);
    TrimString(ids->VolumeId, FALSE);
    TrimString(ids->PulisherId, FALSE);
    TrimString(ids->PreparerId, FALSE);

    Log("ISO ID: System<%s> Volume<%s> Pulisher<%s> Preparer<%s>", 
        ids->SystemId, ids->VolumeId, ids->PulisherId, ids->PreparerId);

    return 0;
}

int CheckSkipMountIso(CONST CHAR *IsoPath)
{
    BOOL InRoot = FALSE;
    int slashcnt = 0;
    CONST CHAR *p = NULL;
    IsoId ID;

    // C:\\xxx
    for (p = IsoPath; *p; p++)
    {
        if (*p == '\\' || *p == '/')
        {
            slashcnt++;
        }
    }

    if (slashcnt == 2)
    {
        InRoot = TRUE;
    }

    memset(&ID, 0, sizeof(ID));
    if (GetIsoId(IsoPath, &ID))
    {
        return 0;
    }

    //Bob.Ombs.Modified.Win10PEx64.iso will auto find ISO file in root, so we can skip the mount
    if (InRoot && strcmp(ID.VolumeId, "Modified-Win10PEx64") == 0)
    {
        return 1;
    }

    return 0;
}

int MountIsoFile(CONST CHAR *IsoPath, DWORD PhyDrive)
{
    if (CheckSkipMountIso(IsoPath))
    {
        Log("Skip mount ISO file for <%s>", IsoPath);
        return 0;
    }

    if (IsWindows8OrGreater())
    {
        Log("This is Windows 8 or latter...");
        if (VentoyMountISOByAPI(IsoPath) == 0)
        {
            Log("Mount iso by API success");
            return 0;
        }
        else
        {
            Log("Mount iso by API failed, maybe not supported, try imdisk");
            return VentoyMountISOByImdisk(IsoPath, PhyDrive);
        }
    }
    else
    {
        Log("This is before Windows 8 ...");
        if (VentoyMountISOByImdisk(IsoPath, PhyDrive) == 0)
        {
            Log("Mount iso by imdisk success");
            return 0;
        }
        else
        {
            return VentoyMountISOByAPI(IsoPath);
        }
    }
}

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
