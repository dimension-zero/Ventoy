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


    return 1;
}

BOOL check_tar_archive(const char *archive, CHAR *tarName)
{
    int len;
    int nameLen;
    const char *pos = archive;
    const char *slash = archive;

    while (*pos)
    {
        if (*pos == '\\' || *pos == '/')
        {
            slash = pos;
        }
        pos++;
    }

    len = (int)strlen(slash);

    if (len > 7 && (strncmp(slash + len - 7, ".tar.gz", 7) == 0 || strncmp(slash + len - 7, ".tar.xz", 7) == 0))
    {
        nameLen = (int)sprintf_s(tarName, MAX_PATH, "X:%s", slash);
        tarName[nameLen - 3] = 0;
        return TRUE;
    }
    else if (len > 8 && strncmp(slash + len - 8, ".tar.bz2", 8) == 0)
    {
        nameLen = (int)sprintf_s(tarName, MAX_PATH, "X:%s", slash);
        tarName[nameLen - 4] = 0;
        return TRUE;
    }
    else if (len > 9 && strncmp(slash + len - 9, ".tar.lzma", 9) == 0)
    {
        nameLen = (int)sprintf_s(tarName, MAX_PATH, "X:%s", slash);
        tarName[nameLen - 5] = 0;
        return TRUE;
    }

    return FALSE;
}

UCHAR *g_unxz_buffer = NULL;
int g_unxz_len = 0;

void unxz_error(char *x)
{
    Log("%s", x);
}

int unxz_flush(void *src, unsigned int size)
{
    memcpy(g_unxz_buffer + g_unxz_len, src, size);
    g_unxz_len += (int)size;

    return (int)size;
}

int DecompressInjectionArchive(const char *archive, DWORD PhyDrive)
{
    int rc = 1;
    int writelen = 0;
    UCHAR *Buffer = NULL;
    UCHAR *RawBuffer = NULL;
    BOOL bRet;
    DWORD dwBytes;
    DWORD dwSize;
    HANDLE hDrive;
    HANDLE hOut;
    DWORD flags = CREATE_NO_WINDOW;
    CHAR StrBuf[MAX_PATH];
    CHAR tarName[MAX_PATH];
    STARTUPINFOA Si;
    PROCESS_INFORMATION Pi;
    PROCESS_INFORMATION NewPi;
    GET_LENGTH_INFORMATION LengthInfo;
    SECURITY_ATTRIBUTES Sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    Log("DecompressInjectionArchive %s", archive);

    sprintf_s(StrBuf, sizeof(StrBuf), "\\\\.\\PhysicalDrive%d", PhyDrive);
    hDrive = CreateFileA(StrBuf, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (hDrive == INVALID_HANDLE_VALUE)
    {
        Log("Could not open the disk<%s>, error:%u", StrBuf, GetLastError());
        goto End;
    }

    bRet = DeviceIoControl(hDrive, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0, &LengthInfo, sizeof(LengthInfo), &dwBytes, NULL);
    if (!bRet)
    {
        Log("Could not get phy disk %s size, error:%u", StrBuf, GetLastError());
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
            CopyFileFromFatDisk("/ventoy/7z/64/7za.xz", "ventoy\\7za.xz");
        }
        else
        {
            CopyFileFromFatDisk("/ventoy/7z/32/7za.xz", "ventoy\\7za.xz");
        }

        ReadWholeFile2Buf("ventoy\\7za.xz", &Buffer, &dwSize);
        Log("7za.xz file size:%u", dwSize);

        RawBuffer = malloc(SIZE_1MB * 4);
        if (RawBuffer)
        {
            g_unxz_buffer = RawBuffer;
            g_unxz_len = 0;
            unxz(Buffer, (int)dwSize, NULL, unxz_flush, NULL, &writelen, unxz_error);
            if (writelen == (int)dwSize)
            {
                Log("Decompress success 7za.xz(%u) ---> 7za.exe(%d)", dwSize, g_unxz_len);
            }
            else
            {
                Log("Decompress failed 7za.xz(%u) ---> 7za.exe(%u)", dwSize, dwSize);
            }

            SaveBuffer2File("ventoy\\7za.exe", RawBuffer, (DWORD)g_unxz_len);

            g_unxz_buffer = NULL;
            g_unxz_len = 0;
            free(RawBuffer);
        }
        else
        {
            Log("Failed to alloc 4MB memory");
        }

        sprintf_s(StrBuf, sizeof(StrBuf), "ventoy\\7za.exe x -y -aoa -oX:\\ %s", archive);

        Log("extract inject to X:");
        Log("cmdline:<%s>", StrBuf);

        GetStartupInfoA(&Si);

        hOut = CreateFileA("ventoy\\7z.log",
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

        CreateProcessA(NULL, StrBuf, NULL, NULL, TRUE, flags, NULL, NULL, &Si, &Pi);
        WaitForSingleObject(Pi.hProcess, INFINITE);

        //
        // decompress tar archive, for tar.gz/tar.xz/tar.bz2
        //
        if (check_tar_archive(archive, tarName))
        {
            Log("Decompress tar archive...<%s>", tarName);

            sprintf_s(StrBuf, sizeof(StrBuf), "ventoy\\7za.exe x -y -aoa -oX:\\ %s", tarName);

            CreateProcessA(NULL, StrBuf, NULL, NULL, TRUE, flags, NULL, NULL, &Si, &NewPi);
            WaitForSingleObject(NewPi.hProcess, INFINITE);

            Log("Now delete %s", tarName);
            DeleteFileA(tarName);
        }

        SAFE_CLOSE_HANDLE(hOut);
    }
    fl_shutdown();

End:

    SAFE_CLOSE_HANDLE(hDrive);

    return rc;
}

int UnattendNeedVarExpand(const char *script)
{
    FILE *fp = NULL;
    char szLine[4096];

    fopen_s(&fp, script, "r");
    if (!fp)
    {
        return 0;
    }

    szLine[0] = szLine[4095] = 0;
    
    while (fgets(szLine, sizeof(szLine) - 1, fp))
    {
        if (strstr(szLine, "$$VT_"))
        {
            fclose(fp);
            return 1;
        }
    
        szLine[0] = szLine[4095] = 0;
    }
    
    fclose(fp);
    return 0;
}

int ExpandSingleVar(VarDiskInfo *pDiskInfo, int DiskNum, const char *var, char *value, int len)
{
    int i;
    int index = -1;
    UINT64 uiDst = 0;
    UINT64 uiDelta = 0;
    UINT64 uiMaxSize = 0;
    UINT64 uiMaxDelta = ULLONG_MAX;

    value[0] = 0;
    
    if (strcmp(var, "VT_WINDOWS_DISK_1ST_NONVTOY") == 0)
    {
        for (i = 0; i < DiskNum; i++)
        {
            if (pDiskInfo[i].Capacity > 0 && i != g_vtoy_disk_drive)
            {
                Log("%s=<PhyDrive%d>", var, i);
                sprintf_s(value, len, "%d", i);
                return 0;
            }
        }
    }
    else if (strcmp(var, "VT_WINDOWS_DISK_1ST_NONUSB") == 0)
    {
        for (i = 0; i < DiskNum; i++)
        {
            if (pDiskInfo[i].Capacity > 0 && pDiskInfo[i].BusType != BusTypeUsb)
            {
                Log("%s=<PhyDrive%d>", var, i);
                sprintf_s(value, len, "%d", i);
                return 0;
            }
        }
    }
    else if (strcmp(var, "VT_WINDOWS_DISK_MAX_SIZE") == 0)
    {
        for (i = 0; i < DiskNum; i++)
        {
            if (pDiskInfo[i].Capacity > 0 && pDiskInfo[i].Capacity > uiMaxSize)
            {
                index = i;
                uiMaxSize = pDiskInfo[i].Capacity;
            }
        }

        Log("%s=<PhyDrive%d>", var, index);
        sprintf_s(value, len, "%d", index);
    }
    else if (strncmp(var, "VT_WINDOWS_DISK_CLOSEST_", 24) == 0)
    {
        uiDst = strtoul(var + 24, NULL, 10);
        uiDst = uiDst * (1024ULL * 1024ULL * 1024ULL);
    
        for (i = 0; i < DiskNum; i++)
        {
            if (pDiskInfo[i].Capacity == 0)
            {
                continue;
            }
        
            if (pDiskInfo[i].Capacity > uiDst)
            {
                uiDelta = pDiskInfo[i].Capacity - uiDst;
            }
            else
            {
                uiDelta = uiDst - pDiskInfo[i].Capacity;
            }
            
            if (uiDelta < uiMaxDelta)
            {
                uiMaxDelta = uiDelta;
                index = i;
            }
        }

        Log("%s=<PhyDrive%d>", var, index);
        sprintf_s(value, len, "%d", index);
    }
    else if (strncmp(var, "VT_WINDOWS_DISK_NONVTOY_CLOSEST_", 32) == 0)
    {
        uiDst = strtoul(var + 32, NULL, 10);
        uiDst = uiDst * (1024ULL * 1024ULL * 1024ULL);

        for (i = 0; i < DiskNum; i++)
        {
            if (pDiskInfo[i].Capacity == 0 || i == g_vtoy_disk_drive)
            {
                continue;
            }

            if (pDiskInfo[i].Capacity > uiDst)
            {
                uiDelta = pDiskInfo[i].Capacity - uiDst;
            }
            else
            {
                uiDelta = uiDst - pDiskInfo[i].Capacity;
            }

            if (uiDelta < uiMaxDelta)
            {
                uiMaxDelta = uiDelta;
                index = i;
            }
        }

        Log("%s=<PhyDrive%d>", var, index);
        sprintf_s(value, len, "%d", index);
    }
    else
    {
        Log("Invalid var name <%s>", var);
        sprintf_s(value, len, "$$%s$$", var);
    }
    
    if (value[0] == 0)
    {
        sprintf_s(value, len, "$$%s$$", var);
    }

    return 0;
}

