/******************************************************************************
 * ventoy_util_windows_path.c — case-correcting path lookups
 *
 * Copyright (c) 2021, longpanda <admin@ventoy.net>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ventoy_define.h>
#include <ventoy_util.h>
#include <ventoy_disk.h>
#include "ventoy_util_windows_priv.h"

int ventoy_path_case(char *path, int slash)
{
	int i;
	int j = 0;
    int count = 0;
	int isUTF8 = 0;
	BOOL bRet;
	HANDLE handle = INVALID_HANDLE_VALUE;
	WCHAR Buffer[MAX_PATH + 16];
	WCHAR FilePathW[MAX_PATH];
	CHAR FilePathA[MAX_PATH];
	FILE_NAME_INFO *pInfo = NULL;

	if (g_sysinfo.pathcase == 0)
	{
		return 0;
	}

    if (path == NULL || path[0] == '/' || path[0] == '\\')
    {
        return 0;
    }

	isUTF8 = VtoyWin_IsUTF8Encode(path);
	if (isUTF8)
	{
		VtoyWin_Utf8ToUtf16(path, FilePathW);
		handle = CreateFileW(FilePathW, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
	}
	else
	{
		handle = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
	}

	if (handle != INVALID_HANDLE_VALUE)
	{
		bRet = GetFileInformationByHandleEx(handle, FileNameInfo, Buffer, sizeof(Buffer));
		if (bRet)
		{
			pInfo = (FILE_NAME_INFO *)Buffer;

			if (slash)
			{
				for (i = 0; i < (int)(pInfo->FileNameLength / sizeof(WCHAR)); i++)
				{
					if (pInfo->FileName[i] == L'\\')
					{
						pInfo->FileName[i] = L'/';
					}
				}
			}

			pInfo->FileName[(pInfo->FileNameLength / sizeof(WCHAR))] = 0;

			memset(FilePathA, 0, sizeof(FilePathA));
			VtoyWin_Utf16ToUtf8(pInfo->FileName, FilePathA);

			if (FilePathA[1] == ':')
			{
				j = 3;
			}
			else
			{
				j = 1;
			}

			for (i = 0; i < MAX_PATH && j < MAX_PATH; i++, j++)
			{
				if (FilePathA[j] == 0)
				{
					break;
				}

				if (path[i] != FilePathA[j])
                {
					path[i] = FilePathA[j];
                    count++;
                }
			}
		}

        CHECK_CLOSE_HANDLE(handle);
	}

    return count;
}
