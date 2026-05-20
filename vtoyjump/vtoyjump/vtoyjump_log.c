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
void BreakAndLaunchCmd(int line)
{
    STARTUPINFOA Si;
    PROCESS_INFORMATION Pi;

    Log("Break at line:%d", line);
    
    GetStartupInfoA(&Si);
    Si.dwFlags |= STARTF_USESHOWWINDOW;
    Si.wShowWindow = SW_NORMAL;

    CreateProcessA(NULL, "cmd.exe", NULL, NULL, FALSE, 0, NULL, NULL, &Si, &Pi);
    WaitForSingleObject(Pi.hProcess, INFINITE);
}

const char * GetFileNameInPath(const char *fullpath)
{
    int i;

    if (strstr(fullpath, ":"))
    {
        for (i = (int)strlen(fullpath); i > 0; i--)
        {
            if (fullpath[i - 1] == '/' || fullpath[i - 1] == '\\')
            {
                return fullpath + i;
            }
        }
    }

    return fullpath;
}

int split_path_name(char *fullpath, char *dir, char *name)
{
    CHAR ch;
    CHAR *Pos = NULL;

    Pos = (CHAR *)GetFileNameInPath(fullpath);

    strcpy_s(name, MAX_PATH, Pos);

    ch = *(Pos - 1);
    *(Pos - 1) = 0;
    strcpy_s(dir, MAX_PATH, fullpath);
    *(Pos - 1) = ch;

    return 0;
}

void TrimString(CHAR *String, BOOL TrimLeft)
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

    if (TrimLeft)
    {        
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

    return;
}

int VentoyProcessRunCmd(const char *Fmt, ...)
{
    int Len = 0;
    va_list Arg;
    STARTUPINFOA Si;
    PROCESS_INFORMATION Pi;
    char szBuf[1024] = { 0 };

    va_start(Arg, Fmt);
    Len += vsnprintf_s(szBuf + Len, sizeof(szBuf)-Len, sizeof(szBuf)-Len, Fmt, Arg);
    va_end(Arg);

    GetStartupInfoA(&Si);
    Si.dwFlags |= STARTF_USESHOWWINDOW;
    Si.wShowWindow = SW_HIDE;

    Log("Process Run: <%s>", szBuf);
    CreateProcessA(NULL, szBuf, NULL, NULL, FALSE, 0, NULL, NULL, &Si, &Pi);
    WaitForSingleObject(Pi.hProcess, INFINITE);

    return 0;
}

CHAR VentoyGetFirstFreeDriveLetter(BOOL Reverse)
{
    int i;
    CHAR Letter = 'T';
    DWORD Drives;

    Drives = GetLogicalDrives();

    if (Reverse)
    {
        for (i = 25; i >= 2; i--)
        {
            if (0 == (Drives & (1 << i)))
            {
                Letter = 'A' + i;
                break;
            }
        }
    }
    else
    {
        for (i = 2; i < 26; i++)
        {
            if (0 == (Drives & (1 << i)))
            {
                Letter = 'A' + i;
                break;
            }
        }
    }

    Log("FirstFreeDriveLetter %u %C:", Reverse, Letter);
    return Letter;
}

void Log(const char *Fmt, ...)
{
    va_list Arg;
    int Len = 0;
    FILE *File = NULL;
    SYSTEMTIME Sys;
    char szBuf[1024];
    DWORD LockStatus = 0;
    DWORD PID = GetCurrentProcessId();

    GetLocalTime(&Sys);
    Len += sprintf_s(szBuf, sizeof(szBuf),
        "[%4d/%02d/%02d %02d:%02d:%02d.%03d] [%u] ",
        Sys.wYear, Sys.wMonth, Sys.wDay,
        Sys.wHour, Sys.wMinute, Sys.wSecond,
        Sys.wMilliseconds, PID);

    va_start(Arg, Fmt);
    Len += vsnprintf_s(szBuf + Len, sizeof(szBuf)-Len, sizeof(szBuf)-Len, Fmt, Arg);
    va_end(Arg);

    MUTEX_LOCK(g_vtoylog_mutex);

    fopen_s(&File, LOG_FILE, "a+");
    if (File)
    {
        fwrite(szBuf, 1, Len, File);
        fwrite("\n", 1, 1, File);
        fclose(File);
    }

    MUTEX_UNLOCK(g_vtoylog_mutex);
}


