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
BOOL CheckVentoyDisk(DWORD DiskNum)
{
    DWORD dwSize = 0;
    CHAR PhyPath[128];
    UINT8 SectorBuf[512];
    HANDLE Handle;
    UINT8 check[8] = { 0x56, 0x54, 0x00, 0x47, 0x65, 0x00, 0x48, 0x44 };

    sprintf_s(PhyPath, sizeof(PhyPath), "\\\\.\\PhysicalDrive%d", DiskNum);
    Handle = CreateFileA(PhyPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (Handle == INVALID_HANDLE_VALUE)
    {
        Log("Could not open the disk<%s>, error:%u", PhyPath, GetLastError());
        return FALSE;
    }

    if (!ReadFile(Handle, SectorBuf, sizeof(SectorBuf), &dwSize, NULL))
    {
        Log("ReadFile failed, dwSize:%u  error:%u", dwSize, GetLastError());
        CloseHandle(Handle);
        return FALSE;
    }

    CloseHandle(Handle);

    if (memcmp(SectorBuf + 0x190, check, 8) == 0)
    {
        return TRUE;
    }

    return FALSE;
}

BOOL VentoyIsLenovoRecovery(CHAR *IsoPath, CHAR *VTLRIPath)
{
    int n;
    int UTF8 = 0;
    HANDLE hFile;
    DWORD Attr;
    WCHAR FilePathW[MAX_PATH];

    UTF8 = IsUTF8Encode(IsoPath);

    if (UTF8)
    {
        Utf8ToUtf16(IsoPath, FilePathW);

        n = (int)wcslen(FilePathW);
        if (n > 4 && _wcsicmp(FilePathW + n - 4, L".iso") == 0)
        {
            FilePathW[n - 3] = L'V';
            FilePathW[n - 2] = L'T';
            FilePathW[n - 1] = L'L';
            FilePathW[n - 0] = L'R';
            FilePathW[n + 1] = L'I';
            FilePathW[n + 2] = 0;

            hFile = CreateFileW(FilePathW, FILE_READ_EA, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
            if (hFile != INVALID_HANDLE_VALUE)
            {
                CloseHandle(hFile);
                Attr = GetFileAttributesW(FilePathW);

                if ((Attr & FILE_ATTRIBUTE_DIRECTORY) == 0)
                {
                    Utf16ToUtf8(FilePathW, VTLRIPath);
                    return TRUE;
                }
            }
        }
    }
    else
    {
        n = (int)strlen(IsoPath);
        if (n > 4 && _stricmp(IsoPath + n - 4, ".iso") == 0)
        {
            IsoPath[n - 4] = 0;
            sprintf_s(VTLRIPath, MAX_PATH, "%s.VTLRI", IsoPath);
            IsoPath[n - 4] = '.';

            if (IsFileExist(VTLRIPath))
            {
                return TRUE;
            }
        }
    }
    
    return FALSE;
}

int MountVTLRI(CHAR *ImgPath, DWORD PhyDrive)
{
    STARTUPINFOA Si;
    PROCESS_INFORMATION Pi;
    CHAR Cmdline[256]; 
    CHAR ImDiskPath[256];    
    
    Log("MountVTLRI <%s> %u", ImgPath, PhyDrive);

    VentoyCopyImdisk(PhyDrive, ImDiskPath);

    VentoyRunImdisk("VTLRI", ImgPath, ImDiskPath, "ro,rem");

    CopyFileA(g_prog_full_path, "ventoy\\VTLRISRV.exe", FALSE);

    sprintf_s(Cmdline, sizeof(Cmdline), "ventoy\\VTLRISRV.exe VTLRI_SRV %C Z", ImgPath[0]);


    GetStartupInfoA(&Si);
    Si.dwFlags |= STARTF_USESHOWWINDOW;
    Si.wShowWindow = SW_HIDE;
    CreateProcessA(NULL, Cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &Si, &Pi);

    Log("Process cmdline <%s>", Cmdline);

    return 0;
}

BOOL FindVentoyDiskBySig(UINT32 VtoySig, DWORD* pDiskNum)
{
    HANDLE Handle;
    DWORD dwSize = 0;
    CHAR PhyPath[128];
    UINT8 SectorBuf[512];

    Log("Find Ventoy Disk by Sig %08x ...", VtoySig);

    for (int DiskNum = 0; DiskNum < 32; DiskNum++)
    {
        sprintf_s(PhyPath, sizeof(PhyPath), "\\\\.\\PhysicalDrive%d", DiskNum);
        Handle = CreateFileA(PhyPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
        if (Handle == INVALID_HANDLE_VALUE)
        {
            Log("Could not open the disk<%s>, error:%u", PhyPath, GetLastError());
            continue;
        }

        if (!ReadFile(Handle, SectorBuf, sizeof(SectorBuf), &dwSize, NULL))
        {
            Log("ReadFile failed, dwSize:%u  error:%u", dwSize, GetLastError());
            CloseHandle(Handle);
            continue;
        }

        CloseHandle(Handle);

        if (*(UINT32*)(SectorBuf + 0x1B8) == VtoySig)
        {
            Log("%s sig match ...", PhyPath);
            *pDiskNum = DiskNum;
            return TRUE;
        }
    }

    return FALSE;
}

