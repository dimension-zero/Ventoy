/******************************************************************************
 * ventoy_util_windows_priv.h — shared static helpers for ventoy_util_windows_*
 *
 * Copyright (c) 2021, longpanda <admin@ventoy.net>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef __VENTOY_UTIL_WINDOWS_PRIV_H__
#define __VENTOY_UTIL_WINDOWS_PRIV_H__

#include <Windows.h>
#include <string.h>

static __inline int VtoyWin_IsUTF8Encode(const char *src)
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

static __inline int VtoyWin_Utf8ToUtf16(const char* src, WCHAR *dst)
{
    return MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, MAX_PATH * sizeof(WCHAR));
}

static __inline int VtoyWin_Utf16ToUtf8(const WCHAR* src, CHAR *dst)
{
    int size = WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, MAX_PATH, NULL, 0);
    dst[size] = 0;
    return size;
}

static __inline void VtoyWin_TrimString(CHAR *String)
{
    CHAR *Pos1 = String;
    CHAR *Pos2 = String;
    size_t Len = strlen(String);

    while (Len > 0)
    {
        if (String[Len - 1] != ' ' && String[Len - 1] != '\t')
        {
            break;
        }
        String[Len - 1] = 0;
        Len--;
    }

    while (*Pos1 == ' ' || *Pos1 == '\t')
    {
        Pos1++;
    }

    while (*Pos1)
    {
        *Pos2++ = *Pos1++;
    }
    *Pos2++ = 0;
}

#endif
