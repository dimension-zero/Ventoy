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

static UCHAR *g_unxz_buffer = NULL;
static int g_unxz_len = 0;

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

