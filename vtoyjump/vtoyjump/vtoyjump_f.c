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

int VentoyHook(ventoy_os_param *param)
{
    int i;
    int rc;
    BOOL find = FALSE;
    BOOL vtoyfind = FALSE;
    CHAR Letter;
    CHAR MntLetter;
    CHAR VtoyLetter;
    DWORD Drives;
    DWORD NewDrives;
    DWORD VtoyDiskNum;
    UINT32 DiskSig;
    UINT32 VtoySig;
    DISK_EXTENT DiskExtent;
    DISK_EXTENT VtoyDiskExtent;
    UINT8 UUID[16];
    CHAR IsoPath[MAX_PATH];
    CHAR VTLRIPath[MAX_PATH];

    Log("VentoyHook Path:<%s>", param->vtoy_img_path);

    if (IsUTF8Encode(param->vtoy_img_path))
    {
        Log("This file is UTF8 encoding");
    }

    for (i = 0; i < 5; i++)
    {
        Letter = 'A';
        Drives = GetLogicalDrives();
        Log("Logic Drives: 0x%x", Drives);

        while (Drives)
        {
            if (Drives & 0x01)
            {
                sprintf_s(IsoPath, sizeof(IsoPath), "%C:\\%s", Letter, param->vtoy_img_path);
                if (IsFileExist("%s", IsoPath))
                {
                    Log("File exist under %C:", Letter);
                    memset(UUID, 0, sizeof(UUID));
                    memset(&DiskExtent, 0, sizeof(DiskExtent));
                    if (GetPhyDiskUUID(Letter, UUID, NULL, &DiskExtent) == 0)
                    {
                        if (memcmp(UUID, param->vtoy_disk_guid, 16) == 0)
                        {
                            Log("Disk UUID match");
                            find = TRUE;
                            break;
                        }
                    }
                }
                else
                {
                    Log("File NOT exist under %C:", Letter);
                }
            }

            Drives >>= 1;
            Letter++;
        }

        if (find)
        {
            break;
        }
        else
        {
            Log("Now wait and retry ...");
            Sleep(1000);
        }
    }

    if (find == FALSE)
    {
        Log("Failed to find ISO file");
        return 1;
    }

    Log("Find ISO file <%s>", IsoPath);
    
    //Find VtoyLetter in Vlnk Mode
    if (g_os_param_reserved[6] == 1)
    {
        memcpy(&VtoySig, g_os_param_reserved + 7, 4);
        for (i = 0; i < 3; i++)
        {
            VtoyLetter = 'A';
            Drives = GetLogicalDrives();
            Log("[%d] Logic Drives: 0x%x  VentoySig:%08X", i, Drives, VtoySig);

            while (Drives)
            {
                if (Drives & 0x01)
                {
                    memset(UUID, 0, sizeof(UUID));
                    memset(&VtoyDiskExtent, 0, sizeof(VtoyDiskExtent));
                    DiskSig = 0;
					
                    if (GetPhyDiskUUID(VtoyLetter, UUID, &DiskSig, &VtoyDiskExtent) == 0)
                    {
                        Log("[%d] DiskSig=%08X PartStart=%lld", i, DiskSig, VtoyDiskExtent.StartingOffset.QuadPart);
                        if (DiskSig == VtoySig && VtoyDiskExtent.StartingOffset.QuadPart == SIZE_1MB)
                        {
                            Log("Ventoy Disk Sig and offset match");
                            vtoyfind = TRUE;
                            break;
                        }
                    }
                }

                Drives >>= 1;
                VtoyLetter++;
            }

            if (vtoyfind)
            {
                Log("Find Ventoy Letter: %C", VtoyLetter);
                break;
            }
            else
            {
                Log("Now wait and retry ...");
                Sleep(1000);
            }
        }

        if (vtoyfind == FALSE) // vlnk mode Ventoy partition has no letter
        {
            Log("Warning: Ventoy partition has no drive letter, assume C: and find by sig");
            VtoyLetter = 'C';            
            vtoyfind = FindVentoyDiskBySig(VtoySig, &VtoyDiskExtent.DiskNumber);
        }

        if (vtoyfind == FALSE)
        {
            Log("Failed to find ventoy disk");
            return 1;
        }

        VtoyDiskNum = VtoyDiskExtent.DiskNumber;
    }
    else
    {
        VtoyLetter = Letter;
        Log("No vlnk mode %C", Letter);

        VtoyDiskNum = DiskExtent.DiskNumber;
    }

    if (CheckVentoyDisk(VtoyDiskNum))
    {
        Log("Disk check OK %C: %u", VtoyLetter, VtoyDiskNum);
    }
    else
    {
        Log("Failed to check ventoy disk %u", VtoyDiskNum);
        return 1;
    }

    g_vtoy_disk_drive = VtoyDiskNum;

    Drives = GetLogicalDrives();
    Log("Drives before mount: 0x%x", Drives);
    
    if (VentoyIsLenovoRecovery(IsoPath, VTLRIPath))
    {
        Log("This is lenovo recovery image, mount VTLRI file.");
        rc = MountVTLRI(VTLRIPath, VtoyDiskNum);
    }
    else
    {
        Log("This is normal image, mount ISO file.");
        rc = MountIsoFile(IsoPath, VtoyDiskNum);
    }

    NewDrives = GetLogicalDrives();
    Log("Drives after mount: 0x%x (0x%x)", NewDrives, (NewDrives ^ Drives));

    MntLetter = 'A';
    NewDrives = (NewDrives ^ Drives);
    while (NewDrives)
    {
        if (NewDrives & 0x01)
        {
            if ((NewDrives >> 1) == 0)
            {
                Log("The ISO file is mounted at %C:", MntLetter);
            }
            else
            {
                Log("Maybe the ISO file is mounted at %C:", MntLetter);
            }
            break;
        }

        NewDrives >>= 1;
        MntLetter++;
    }

    Log("Mount ISO FILE: %s", rc == 0 ? "SUCCESS" : "FAILED");

    //Windows 11 bypass check
    if (g_windows_data.windows11_bypass_check == 1 || g_windows_data.windows11_bypass_nro == 1)
    {
        Windows11Bypass(IsoPath, MntLetter, g_windows_data.windows11_bypass_check, g_windows_data.windows11_bypass_nro);
    }

    // for protect
    rc = DeleteVentoyPart2MountPoint(VtoyDiskNum);
    Log("Delete ventoy mountpoint: %s", rc == 0 ? "SUCCESS" : "NO NEED");
    
    if (g_windows_data.auto_install_script[0])
    {
        if (IsFileExist("%s", VTOY_AUTO_FILE))
        {
            Log("use auto install script %s...", VTOY_AUTO_FILE);
            ProcessUnattendedInstallation(VTOY_AUTO_FILE, VtoyDiskNum);
        }
        else
        {
            Log("auto install script %s not exist", IsoPath);
        }
    }
    else
    {
        Log("auto install no need");
    }

    if (g_windows_data.injection_archive[0])
    {
        sprintf_s(IsoPath, sizeof(IsoPath), "%C:%s", VtoyLetter, g_windows_data.injection_archive);
