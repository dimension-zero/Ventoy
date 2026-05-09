/******************************************************************************
 * Utility.c
 *
 * Copyright (c) 2021, longpanda <admin@ventoy.net>
 * Copyright (c) 2011-2020, Pete Batard <pete@akeo.ie>
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
#include <Windows.h>
#include "Ventoy2Disk.h"

BOOL IsPathExist(BOOL Dir, const char *Fmt, ...)
{
    va_list Arg;
    HANDLE hFile;
    DWORD Attr;
    CHAR FilePath[MAX_PATH];

    va_start(Arg, Fmt);
    vsnprintf_s(FilePath, sizeof(FilePath), sizeof(FilePath), Fmt, Arg);
    va_end(Arg);

    hFile = CreateFileA(FilePath, FILE_READ_EA, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (INVALID_HANDLE_VALUE == hFile)
    {
        return FALSE;
    }

    CloseHandle(hFile);

    Attr = GetFileAttributesA(FilePath);

    if (Dir)
    {
        if ((Attr & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            return FALSE;
        }
    }
    else
    {
        if (Attr & FILE_ATTRIBUTE_DIRECTORY)
        {
            return FALSE;
        }
    }

    return TRUE;
}

int SaveBufToFile(const CHAR *FileName, const void *Buffer, int BufLen)
{
    FILE *File = NULL;
    void *Data = NULL;

    fopen_s(&File, FileName, "wb");
    if (File == NULL)
    {
        Log("Failed to open file %s", FileName);
        return 1;
    }

    fwrite(Buffer, 1, BufLen, File);
    fclose(File);
    return 0;
}

int ReadWholeFileToBuf(const CHAR *FileName, int ExtLen, void **Bufer, int *BufLen)
{
    int FileSize;
    FILE *File = NULL;
    void *Data = NULL;

    fopen_s(&File, FileName, "rb");
    if (File == NULL)
    {
        Log("Failed to open file %s", FileName);
        return 1;
    }

    fseek(File, 0, SEEK_END);
    FileSize = (int)ftell(File);

    Data = malloc(FileSize + ExtLen);
    if (!Data)
    {
        fclose(File);
        return 1;
    }

    fseek(File, 0, SEEK_SET);
    fread(Data, 1, FileSize, File);

    fclose(File);

    *Bufer = Data;
    *BufLen = FileSize;

    return 0;
}

const CHAR* GetLocalVentoyVersion(void)
{
    int rc;
    int FileSize;
    CHAR *Pos = NULL;
    CHAR *Buf = NULL;
    static CHAR LocalVersion[64] = { 0 };

    if (LocalVersion[0] == 0)
    {
        rc = ReadWholeFileToBuf(VENTOY_FILE_VERSION, 1, (void **)&Buf, &FileSize);
        if (rc)
        {
            return "";
        }
        Buf[FileSize] = 0;

        for (Pos = Buf; *Pos; Pos++)
        {
            if (*Pos == '\r' || *Pos == '\n')
            {
                *Pos = 0;
                break;
            }
        }

        safe_sprintf(LocalVersion, "%s", Buf);
        free(Buf);
    }
    
    return LocalVersion;
}

const CHAR* ParseVentoyVersionFromString(CHAR *Buf)
{
    CHAR *Pos = NULL;
    CHAR *End = NULL;
    static CHAR LocalVersion[64] = { 0 };

    Pos = strstr(Buf, "VENTOY_VERSION=");
    if (Pos)
    {
        Pos += strlen("VENTOY_VERSION=");
        if (*Pos == '"')
        {
            Pos++;
        }

        End = Pos;
        while (*End != 0 && *End != '"' && *End != '\r' && *End != '\n')
        {
            End++;
        }

        *End = 0;

        safe_sprintf(LocalVersion, "%s", Pos);
        return LocalVersion;
    }

    return "";
}
