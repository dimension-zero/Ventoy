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
int LoadNtDriver(const char *DrvBinPath)
{
    int i;
    int rc = 0;
    BOOL Ret;
    DWORD Status;
    SC_HANDLE hServiceMgr;
    SC_HANDLE hService;
    char name[256] = { 0 };

    for (i = (int)strlen(DrvBinPath) - 1; i >= 0; i--)
    {
        if (DrvBinPath[i] == '\\' || DrvBinPath[i] == '/')
        {
            sprintf_s(name, sizeof(name), "%s", DrvBinPath + i + 1);
            break;
        }
    }

    Log("Load NT driver: %s %s", DrvBinPath, name);

    hServiceMgr = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (hServiceMgr == NULL)
    {
        Log("OpenSCManager failed Error:%u", GetLastError());
        return 1;
    }

    Log("OpenSCManager OK");

    hService = CreateServiceA(hServiceMgr,
        name,
        name,
        SERVICE_ALL_ACCESS,
        SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL,
        DrvBinPath,
        NULL, NULL, NULL, NULL, NULL);
    if (hService == NULL)
    {
        Status = GetLastError();
        if (Status != ERROR_IO_PENDING && Status != ERROR_SERVICE_EXISTS)
        {
            Log("CreateService failed v %u", Status);
            CloseServiceHandle(hServiceMgr);
            return 1;
        }

        hService = OpenServiceA(hServiceMgr, name, SERVICE_ALL_ACCESS);
        if (hService == NULL)
        {
            Log("OpenService failed %u", Status);
            CloseServiceHandle(hServiceMgr);
            return 1;
        }
    }

    Log("CreateService imdisk OK");

    Ret = StartServiceA(hService, 0, NULL);
    if (Ret)
    {
        Log("StartService OK");
    }
    else
    {
        Status = GetLastError();
        if (Status == ERROR_SERVICE_ALREADY_RUNNING)
        {
            rc = 0;
        }
        else
        {
            Log("StartService error  %u", Status);
            rc = 1;
        }
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hServiceMgr);

    Log("Load NT driver %s", rc ? "failed" : "success");

    return rc;
}

int ReadWholeFile2Buf(const char *Fullpath, void **Data, DWORD *Size)
{
    int rc = 1;
    DWORD FileSize;
    DWORD dwSize;
    HANDLE Handle;
    BYTE *Buffer = NULL;

    Log("ReadWholeFile2Buf <%s>", Fullpath);

    Handle = CreateFileA(Fullpath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (Handle == INVALID_HANDLE_VALUE)
    {
        Log("Could not open the file<%s>, error:%u", Fullpath, GetLastError());
        goto End;
    }

    FileSize = SetFilePointer(Handle, 0, NULL, FILE_END);

    Buffer = malloc(FileSize);
    if (!Buffer)
    {
        Log("Failed to alloc memory size:%u", FileSize);
        goto End;
    }

    SetFilePointer(Handle, 0, NULL, FILE_BEGIN);
    if (!ReadFile(Handle, Buffer, FileSize, &dwSize, NULL))
    {
        Log("ReadFile failed, dwSize:%u  error:%u", dwSize, GetLastError());
        goto End;
    }

    *Data = Buffer;
    *Size = FileSize;

    Log("Success read file size:%u", FileSize);

    rc = 0;

End:
    SAFE_CLOSE_HANDLE(Handle);

    return rc;
}

BOOL CheckPeHead(BYTE *Buffer, DWORD Size, DWORD Offset)
{
    UINT32 PeOffset;
    BYTE *Head = NULL;
    DWORD End;
    ventoy_windows_data *pdata = NULL;

    Head = Buffer + Offset;
    pdata = (ventoy_windows_data *)Head;
    Head += sizeof(ventoy_windows_data);

    if (pdata->auto_install_script[0] && pdata->auto_install_len > 0)
    {
        End = Offset + sizeof(ventoy_windows_data) + pdata->auto_install_len + 60;
        if (End < Size)
        {
            Head += pdata->auto_install_len;
        }
    }

    if (Head[0] != 'M' || Head[1] != 'Z')
    {
        return FALSE;
    }

    PeOffset = *(UINT32 *)(Head + 60);
    if (*(UINT32 *)(Head + PeOffset) != 0x00004550)
    {
        return FALSE;
    }

    return TRUE;
}


BOOL CheckOsParam(ventoy_os_param *param)
{
    UINT32 i;
    BYTE Sum = 0;

    if (memcmp(&param->guid, &g_ventoy_guid, sizeof(ventoy_guid)))
    {
        return FALSE;
    }

    for (i = 0; i < sizeof(ventoy_os_param); i++)
    {
        Sum += *((BYTE *)param + i);
    }
    
    if (Sum)
    {
        return FALSE;
    }

    if (param->vtoy_img_location_addr % 4096)
    {
        return FALSE;
    }

    return TRUE;
}

int SaveBuffer2File(const char *Fullpath, void *Buffer, DWORD Length)
{
    int rc = 1;
    DWORD dwSize;
    HANDLE Handle;

    Log("SaveBuffer2File <%s> len:%u", Fullpath, Length);

    Handle = CreateFileA(Fullpath, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, 0, 0);
    if (Handle == INVALID_HANDLE_VALUE)
    {
        Log("Could not create new file, error:%u", GetLastError());
        goto End;
    }

    WriteFile(Handle, Buffer, Length, &dwSize, NULL);

    rc = 0;

End:
    SAFE_CLOSE_HANDLE(Handle);

    return rc;
}

int IsUTF8Encode(const char *src)
{
    int i;
    const UCHAR *Byte = (const UCHAR *)src;

    for (i = 0; i < MAX_PATH && Byte[i]; i++)
    {
        if (Byte[i] > 127)
        {
            return 1;
        }
    }
    
    return 0;
}

int Utf16ToUtf8(const WCHAR *src, char *dst)
{
    int len;
    int size;

    len = (int)wcslen(src) + 1;
    size = WideCharToMultiByte(CP_UTF8, 0, src, len, NULL, 0, NULL, NULL);
    return WideCharToMultiByte(CP_UTF8, 0, src, len, dst, size, NULL, NULL);
}

int Utf8ToUtf16(const char* src, WCHAR * dst)
{
    int size = MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, 0);
    return MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, size + 1);
}

