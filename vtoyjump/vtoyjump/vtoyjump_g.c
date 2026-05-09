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

        if (IsFileExist("%s", IsoPath))
        {
            Log("decompress injection archive %s...", IsoPath);
            DecompressInjectionArchive(IsoPath, VtoyDiskNum);

            if (IsFileExist("%s", AUTO_RUN_BAT))
            {
                HANDLE hOut;
                DWORD flags = CREATE_NO_WINDOW;
                CHAR StrBuf[1024];
                STARTUPINFOA Si;
                PROCESS_INFORMATION Pi;
                SECURITY_ATTRIBUTES Sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

                Log("%s exist, now run it...", AUTO_RUN_BAT);

                GetStartupInfoA(&Si);

                hOut = CreateFileA(AUTO_RUN_LOG,
                    FILE_APPEND_DATA,
                    FILE_SHARE_WRITE | FILE_SHARE_READ,
                    &Sa,
                    OPEN_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL,
                    NULL);

                Si.dwFlags |= STARTF_USESTDHANDLES;
                if (hOut != INVALID_HANDLE_VALUE)
                {
                    Si.hStdError = hOut;
                    Si.hStdOutput = hOut;
                }

                sprintf_s(IsoPath, sizeof(IsoPath), "%C:\\%s", Letter, param->vtoy_img_path);
                sprintf_s(StrBuf, sizeof(StrBuf), "cmd.exe /c %s \"%s\" %C", AUTO_RUN_BAT, IsoPath, MntLetter);
                CreateProcessA(NULL, StrBuf, NULL, NULL, TRUE, flags, NULL, NULL, &Si, &Pi);
                WaitForSingleObject(Pi.hProcess, INFINITE);

                SAFE_CLOSE_HANDLE(hOut);
            }
            else
            {
                Log("%s not exist...", AUTO_RUN_BAT);
            }
        }
        else
        {
            Log("injection archive %s not exist", IsoPath);
        }
    }
    else
    {
        Log("no injection archive found");
    }

    return 0;
}

int ExtractWindowsDataFile(char *databuf)
{
    int len = 0;
    char *filedata = NULL;
    ventoy_windows_data *pdata = (ventoy_windows_data *)databuf;

    Log("ExtractWindowsDataFile: auto install <%s:%d>", pdata->auto_install_script, pdata->auto_install_len);

    filedata = databuf + sizeof(ventoy_windows_data);

    if (pdata->auto_install_script[0] && pdata->auto_install_len > 0)
    {
        SaveBuffer2File(VTOY_AUTO_FILE, filedata, pdata->auto_install_len);
        filedata += pdata->auto_install_len;
        len = pdata->auto_install_len;
    }
    
    return len;
}

int ventoy_check_create_directory(void)
{
    if (IsDirExist("ventoy"))
    {
        Log("ventoy directory already exist");
    }
    else
    {
        Log("ventoy directory not exist, now create it.");
        if (!CreateDirectoryA("ventoy", NULL))
        {
            Log("Failed to create ventoy directory err:%u", GetLastError());
            return 1;
        }
    }

    return 0;
}

