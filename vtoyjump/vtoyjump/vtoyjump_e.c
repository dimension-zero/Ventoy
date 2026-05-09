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

int VentoyGetFileVersion(const CHAR *FilePath, UINT16 *pMajor, UINT16 *pMinor, UINT16 *pBuild, UINT16 *pRevision)
{
    int ret = 1;
    DWORD dwHandle;
    DWORD dwSize;
    UINT VerLen = 0;
    CHAR *Buffer = NULL;
    VS_FIXEDFILEINFO* VerInfo = NULL;
    UINT16 Major, Minor, Build, Revision;

    Log("Get file version for <%s>", FilePath);

    dwSize = GetFileVersionInfoSizeA(FilePath, &dwHandle);
    if (0 == dwSize)
    {
        Log("Failed to get file version info size: %u", LASTERR);
        goto End;
    }

    Buffer = malloc(dwSize);
    if (!Buffer)
    {
        Log("malloc failed %u", dwSize);
        goto End;
    }

    if (FALSE == GetFileVersionInfoA(FilePath, dwHandle, dwSize, Buffer))
    {
        Log("Failed to get file version info : %u", LASTERR);
        goto End;
    }

    if (VerQueryValueA(Buffer, "\\", (LPVOID)&VerInfo, &VerLen) && VerLen != 0)
    {
        if (VerInfo->dwSignature == VS_FFI_SIGNATURE)
        {
            Major = HIWORD(VerInfo->dwFileVersionMS);
            Minor = LOWORD(VerInfo->dwFileVersionMS);
            Build = HIWORD(VerInfo->dwFileVersionLS);
            Revision = LOWORD(VerInfo->dwFileVersionLS);

            Log("FileVersionze: <%u %u %u %u>", Major, Minor, Build, Revision);

            if (Major == 10 && Build > 20000)
            {
                Major = 11;
            }

            if (pMajor)
            {
                *pMajor = Major;
            }
            if (pMinor)
            {
                *pMinor = Minor;
            }
            if (pBuild)
            {
                *pBuild = Build;
            }
            if (pRevision)
            {
                *pRevision = Revision;
            }

            ret = 0;
        }
        else
        {
            Log("Invalid verinfo signature 0x%x", VerInfo->dwSignature);
        }
    }
    else
    {
        Log("VerQueryValueA failed %u", LASTERR);
    }

End:
    if (Buffer)
    {
        free(Buffer);
    }

    return ret;
}

BOOL VentoyIsNeedBypass(const char *isofile, const char MntLetter)
{
    UINT16 Major; 
    BOOL bRet = FALSE;
    CHAR CheckFile[MAX_PATH];

    if (FALSE == IsFileExist("%C:\\sources\\install.wim", MntLetter) &&
        FALSE == IsFileExist("%C:\\sources\\install.esd", MntLetter))
    {
        Log("install.wim/install.esd not exist, this is not a windows install media.");
        goto End;
    }

    if (FALSE == IsFileExist("%C:\\sources\\boot.wim", MntLetter))
    {
        Log("boot.wim not exist, this is not a windows install media.");
        goto End;
    }

    if (IsFileExist("%C:\\sources\\compatresources.dll", MntLetter))
    {
        sprintf_s(CheckFile, sizeof(CheckFile), "%C:\\sources\\compatresources.dll", MntLetter);
    }
    else if (IsFileExist("%C:\\setup.exe", MntLetter))
    {
        sprintf_s(CheckFile, sizeof(CheckFile), "%C:\\setup.exe", MntLetter);
    }
    else if (IsFileExist("X:\\setup.exe"))
    {
        sprintf_s(CheckFile, sizeof(CheckFile), "X:\\setup.exe");
    }
    else
    {
        Log("No Check file found");
        goto End;
    }

    if (VentoyGetFileVersion(CheckFile, &Major, NULL, NULL, NULL))
    {
        goto End;
    }

    if (Major >= 11)
    {
        Log("Enable for Windows 11 %u", Major);
        bRet = TRUE;
    }
    else
    {
        Log("This is not Windows 11, not need to bypass %u.", Major);
    }

End:
    return bRet;
}

int Windows11Bypass(const char *isofile, const char MntLetter, UINT8 Check, UINT8 NRO)
{
    int Ret = 1;        
    HKEY hKey = NULL;
    HKEY hSubKey = NULL;
    LSTATUS Status;
    DWORD dwValue = 1;
    DWORD dwSize;

    Log("Windows11Bypass for <%s> %C: Check:%u NRO:%u", isofile, MntLetter, Check, NRO);

    if (!VentoyIsNeedBypass(isofile, MntLetter))
    {
        goto End;
    }

    //bugfix: change VTOYEFI partition attribute
    


    //Now we really need to bypass windows 11 check. create registry

    if (Check)
    {
        Status = RegCreateKeyExA(HKEY_LOCAL_MACHINE, "System\\Setup", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &dwSize);
        if (ERROR_SUCCESS != Status)
        {
            Log("Failed to create reg key System\\Setup %u %u", LASTERR, Status);
            goto End;
        }

        Status = RegCreateKeyExA(hKey, "LabConfig", 0, NULL, 0, KEY_SET_VALUE | KEY_QUERY_VALUE | KEY_CREATE_SUB_KEY, NULL, &hSubKey, &dwSize);
        if (ERROR_SUCCESS != Status)
        {
            Log("Failed to create LabConfig reg  %u %u", LASTERR, Status);
            goto End;
        }

        //set reg value
        Status += RegSetValueExA(hSubKey, "BypassRAMCheck", 0, REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));
        Status += RegSetValueExA(hSubKey, "BypassTPMCheck", 0, REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));
        Status += RegSetValueExA(hSubKey, "BypassSecureBootCheck", 0, REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));
        Status += RegSetValueExA(hSubKey, "BypassCPUCheck", 0, REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));

        Log("Create bypass check registry %s %u", (Status == ERROR_SUCCESS) ? "SUCCESS" : "FAILED", Status);
    }


    if (NRO)
    {
        Status = RegCreateKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &dwSize);
        if (ERROR_SUCCESS != Status)
        {
            Log("Failed to create reg key SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\OOBE %u %u", LASTERR, Status);
            goto End;
        }

        Status = RegCreateKeyExA(hKey, "OOBE", 0, NULL, 0, KEY_SET_VALUE | KEY_QUERY_VALUE | KEY_CREATE_SUB_KEY, NULL, &hSubKey, &dwSize);
        if (ERROR_SUCCESS != Status)
        {
            Log("Failed to create OOBE reg  %u %u", LASTERR, Status);
            goto End;
        }

        Status += RegSetValueExA(hSubKey, "BypassNRO", 0, REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));
        Log("Create BypassNRO registry %s %u", (Status == ERROR_SUCCESS) ? "SUCCESS" : "FAILED", Status);

        SetupMonNroStart(isofile);
    }
    

    Ret = 0;

End:
    
    return Ret; 
}

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

