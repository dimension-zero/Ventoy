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

void TraceOut(const char *Fmt, ...)
{
    va_list Arg;
    int Len = 0;
    FILE *File = NULL;
    char szBuf[1024];

    va_start(Arg, Fmt);
    Len += vsnprintf_s(szBuf + Len, sizeof(szBuf)-Len, sizeof(szBuf)-Len, Fmt, Arg);
    va_end(Arg);

    fopen_s(&File, VENTOY_FILE_LOG, "a+");
    if (File)
    {
        fwrite(szBuf, 1, Len, File);
        fclose(File);
    }
}

typedef struct LogBuf
{
    int Len; 
    char szBuf[1024];    
    struct LogBuf* next;
}LogBuf;

static BOOL g_LogCache = FALSE;
static LogBuf* g_LogHead = NULL;
static LogBuf* g_LogTail = NULL;

void LogCache(BOOL cache)
{
    g_LogCache = cache;
}

void LogFlush(void)
{
    FILE* File = NULL;
    LogBuf* Node = NULL;
    LogBuf* Next = NULL;

    if (g_CLI_Mode)
    {
        fopen_s(&File, VENTOY_CLI_LOG, "a+");
    }
    else
    {
        fopen_s(&File, VENTOY_FILE_LOG, "a+");
    }

    if (File)
    {
        for (Node = g_LogHead; Node; Node = Node->next)
        {
            fwrite(Node->szBuf, 1, Node->Len, File);
            fwrite("\n", 1, 1, File);
        }
        fclose(File);
    }

    for (Node = g_LogHead; Node; Node = Next)
    {
        Next = Node->next;
        free(Node);
    }

    g_LogHead = g_LogTail = NULL;
}

void Log(const char *Fmt, ...)
{
    va_list Arg;
    int Len = 0;
    FILE *File = NULL;
    SYSTEMTIME Sys;
    char szBuf[1024];

    GetLocalTime(&Sys);
    Len += safe_sprintf(szBuf,
        "[%4d/%02d/%02d %02d:%02d:%02d.%03d] ",
        Sys.wYear, Sys.wMonth, Sys.wDay,
        Sys.wHour, Sys.wMinute, Sys.wSecond,
        Sys.wMilliseconds);

    va_start(Arg, Fmt);
    Len += vsnprintf_s(szBuf + Len, sizeof(szBuf)-Len - 1, sizeof(szBuf)-Len-1, Fmt, Arg);
    va_end(Arg);

    if (g_LogCache)
    {
        LogBuf* Node = NULL;
        Node = malloc(sizeof(LogBuf));
        if (Node)
        {
            memcpy(Node->szBuf, szBuf, Len);
            Node->next = NULL;
            Node->Len = Len;

            if (g_LogTail)
            {
                g_LogTail->next = Node;
                g_LogTail = Node;
            }
            else
            {
                g_LogHead = g_LogTail = Node;
            }
        }

        return;
    }

    if (g_CLI_Mode)
    {
        fopen_s(&File, VENTOY_CLI_LOG, "a+");
    }
    else
    {
        fopen_s(&File, VENTOY_FILE_LOG, "a+");
    }
    if (File)
    {
        fwrite(szBuf, 1, Len, File);
        fwrite("\n", 1, 1, File);
        fclose(File);
    }
}
