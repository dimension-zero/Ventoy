#include <Windows.h>
#include <io.h>
#include <fcntl.h>
#include <stdio.h>
#include "Ventoy2Disk.h"
#include "ventoy_cli.h"

void CLIPrint(const char *Fmt, ...)
{
    va_list Arg;
    char szBuf[1024];
    va_start(Arg, Fmt);
    vsnprintf_s(szBuf, sizeof(szBuf), sizeof(szBuf) - 1, Fmt, Arg);
    va_end(Arg);
    printf("%s\n", szBuf);
}

void CLI_AttachConsoleIO(void)
{
    HANDLE hOut   = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hIn    = GetStdHandle(STD_INPUT_HANDLE);
    DWORD  outType = (hOut && hOut != INVALID_HANDLE_VALUE) ? GetFileType(hOut) : FILE_TYPE_UNKNOWN;
    DWORD  inType  = (hIn  && hIn  != INVALID_HANDLE_VALUE) ? GetFileType(hIn)  : FILE_TYPE_UNKNOWN;

    if (outType == FILE_TYPE_PIPE || outType == FILE_TYPE_DISK)
    {
        /* Launched with redirected stdout (pipe capture or "> file") —
         * hook C runtime stdout to the inherited Win32 handle. */
        int fd = _open_osfhandle((intptr_t)hOut, _O_WRONLY | _O_TEXT);
        if (fd >= 0) { FILE *fp = _fdopen(fd, "w"); if (fp) { *stdout = *fp; setvbuf(stdout, NULL, _IONBF, 0); } }
    }
    else if (AttachConsole(ATTACH_PARENT_PROCESS))
    {
        /* Launched from an interactive console (e.g. cmd.exe). */
        FILE *fp;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        if (inType != FILE_TYPE_PIPE && inType != FILE_TYPE_DISK)
            freopen_s(&fp, "CONIN$", "r", stdin);
    }

    if (inType == FILE_TYPE_PIPE || inType == FILE_TYPE_DISK)
    {
        int fd = _open_osfhandle((intptr_t)hIn, _O_RDONLY | _O_TEXT);
        if (fd >= 0) { FILE *fp = _fdopen(fd, "r"); if (fp) { *stdin = *fp; } }
    }
}

void CLI_UpdatePercent(int Pos)
{
    int Len;
    FILE* File = NULL;
    CHAR szBuf[128];

    Len = (int)sprintf_s(szBuf, sizeof(szBuf), "%d", Pos * 100 / PT_FINISH);
    fopen_s(&File, VENTOY_CLI_PERCENT, "w+");
    if (File)
    {
        fwrite(szBuf, 1, Len, File);
        fwrite("\n", 1, 1, File);
        fclose(File);
    }
}

void CLI_WriteDoneFile(int ret)
{
    FILE* File = NULL;

    fopen_s(&File, VENTOY_CLI_DONE, "w+");
    if (File)
    {
        if (ret == 0)
        {
            fwrite("0\n", 1, 2, File);
        }
        else
        {
            fwrite("1\n", 1, 2, File);
        }
        fclose(File);
    }
}
