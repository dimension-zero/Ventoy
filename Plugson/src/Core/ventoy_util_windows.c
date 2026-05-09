/******************************************************************************
 * ventoy_util_windows.c — general file/dir/buffer helpers (Windows)
 *
 * Copyright (c) 2021, longpanda <admin@ventoy.net>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Path-case lookup lives in ventoy_util_windows_path.c.
 * Drive query and version detection live in ventoy_util_windows_disk.c.
 * Writeback thread lives in ventoy_util_windows_thread.c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ventoy_define.h>
#include <ventoy_util.h>
#include <ventoy_disk.h>
#include "ventoy_util_windows_priv.h"

void ventoy_gen_preudo_uuid(void *uuid)
{
    CoCreateGuid((GUID *)uuid);
}

int ventoy_is_directory_exist(const char *Fmt, ...)
{
    va_list Arg;
    DWORD Attr;
    int UTF8 = 0;
    CHAR FilePathA[MAX_PATH];
    WCHAR FilePathW[MAX_PATH];

    va_start(Arg, Fmt);
    vsnprintf_s(FilePathA, sizeof(FilePathA), sizeof(FilePathA), Fmt, Arg);
    va_end(Arg);

    UTF8 = VtoyWin_IsUTF8Encode(FilePathA);

    if (UTF8)
    {
        VtoyWin_Utf8ToUtf16(FilePathA, FilePathW);
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

int ventoy_is_file_exist(const char *Fmt, ...)
{
    va_list Arg;
    HANDLE hFile;
    DWORD Attr;
    int UTF8 = 0;
    CHAR FilePathA[MAX_PATH];
    WCHAR FilePathW[MAX_PATH];

    va_start(Arg, Fmt);
    vsnprintf_s(FilePathA, sizeof(FilePathA), sizeof(FilePathA), Fmt, Arg);
    va_end(Arg);

    UTF8 = VtoyWin_IsUTF8Encode(FilePathA);
    if (UTF8)
    {
        VtoyWin_Utf8ToUtf16(FilePathA, FilePathW);
        hFile = CreateFileW(FilePathW, FILE_READ_EA, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
        Attr = GetFileAttributesW(FilePathW);
    }
    else
    {
        hFile = CreateFileA(FilePathA, FILE_READ_EA, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
        Attr = GetFileAttributesA(FilePathA);
    }

    if (INVALID_HANDLE_VALUE == hFile)
    {
        return 0;
    }
    CloseHandle(hFile);

    if (Attr & FILE_ATTRIBUTE_DIRECTORY)
    {
        return 0;
    }

    return 1;
}

const char * ventoy_get_os_language(void)
{
    if (GetUserDefaultUILanguage() == 0x0804)
    {
        return "cn";
    }
    else
    {
        return "en";
    }
}

int ventoy_get_file_size(const char *file)
{
	int Size = -1;
	HANDLE hFile;

	hFile = CreateFileA(file, 0, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		Size = (int)GetFileSize(hFile, NULL);
		CHECK_CLOSE_HANDLE(hFile);
	}

	return Size;
}

int ventoy_read_file_to_buf(const char *FileName, int ExtLen, void **Bufer, int *BufLen)
{
    int UTF8 = 0;
    int Size = 0;
    BOOL bRet;
    DWORD dwBytes;
    HANDLE hFile;
    char *buffer = NULL;
    WCHAR FilePathW[MAX_PATH];

    UTF8 = VtoyWin_IsUTF8Encode(FileName);
    if (UTF8)
    {
        VtoyWin_Utf8ToUtf16(FileName, FilePathW);
        hFile = CreateFileW(FilePathW, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    }
    else
    {
        hFile = CreateFileA(FileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    }

    if (hFile == INVALID_HANDLE_VALUE)
    {
		vlog("Failed to open %s %u\n", FileName, LASTERR);
        return 1;
    }

    Size = (int)GetFileSize(hFile, NULL);
    buffer = malloc(Size + ExtLen);
    if (!buffer)
    {
        vlog("Failed to alloc file buffer\n");
        CloseHandle(hFile);
        return 1;
    }

    bRet = ReadFile(hFile, buffer, (DWORD)Size, &dwBytes, NULL);
    if ((!bRet) || ((DWORD)Size != dwBytes))
    {
        vlog("Failed to read file <%s> %u err:%u", FileName, dwBytes, LASTERR);
        CloseHandle(hFile);
        free(buffer);
        return 1;
    }

    *Bufer = buffer;
    *BufLen = Size;

    CloseHandle(hFile);
    return 0;
}

int ventoy_write_buf_to_file(const char *FileName, void *Bufer, int BufLen)
{
    BOOL bRet;
    DWORD dwBytes;
    HANDLE hFile;

    hFile = CreateFileA(FileName, GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (hFile == INVALID_HANDLE_VALUE)
    {
		vlog("CreateFile %s failed %u\n", FileName, LASTERR);
        return 1;
    }

    bRet = WriteFile(hFile, Bufer, (DWORD)BufLen, &dwBytes, NULL);

    if ((!bRet) || ((DWORD)BufLen != dwBytes))
    {
        vlog("Failed to write file <%s> %u err:%u", FileName, dwBytes, LASTERR);
        CloseHandle(hFile);
        return 1;
    }

    FlushFileBuffers(hFile);
    CloseHandle(hFile);

    return 0;
}

int ventoy_copy_file(const char *a, const char *b)
{
    CopyFileA(a, b, FALSE);
    return 0;
}
