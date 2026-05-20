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
