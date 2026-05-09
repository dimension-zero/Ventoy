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

int VentoyJump(INT argc, CHAR **argv, CHAR *LunchFile)
{
    int rc = 1;
    int stat = 0;
    int exlen = 0;
    DWORD Pos;
    DWORD PeStart;
    DWORD FileSize;
    DWORD LockStatus = 0;
    BYTE *Buffer = NULL; 
    CHAR ExeFileName[MAX_PATH];

    sprintf_s(ExeFileName, sizeof(ExeFileName), "%s", argv[0]);
    if (!IsFileExist("%s", ExeFileName))
    {
        Log("File %s NOT exist, now try %s.exe", ExeFileName, ExeFileName);
        sprintf_s(ExeFileName, sizeof(ExeFileName), "%s.exe", argv[0]);

        Log("File %s exist ? %s", ExeFileName, IsFileExist("%s", ExeFileName) ? "YES" : "NO");
    }

    if (ReadWholeFile2Buf(ExeFileName, (void **)&Buffer, &FileSize))
    {
        goto End;
    }
    
    Log("VentoyJump %dbit", g_system_bit);

    MUTEX_LOCK(g_vtoyins_mutex);
    stat = ventoy_check_create_directory();
    MUTEX_UNLOCK(g_vtoyins_mutex);

    if (stat != 0)
    {
        goto End;
    }

    for (PeStart = 0; PeStart < FileSize; PeStart += 16)
    {
        if (CheckOsParam((ventoy_os_param *)(Buffer + PeStart)) && 
            CheckPeHead(Buffer, FileSize, PeStart + sizeof(ventoy_os_param)))
        {
            Log("Find os pararm at %u", PeStart);

            memcpy(&g_os_param, Buffer + PeStart, sizeof(ventoy_os_param));
            memcpy(&g_windows_data, Buffer + PeStart + sizeof(ventoy_os_param), sizeof(ventoy_windows_data));  
            exlen = ExtractWindowsDataFile(Buffer + PeStart + sizeof(ventoy_os_param));
            memcpy(g_os_param_reserved, g_os_param.vtoy_reserved, sizeof(g_os_param_reserved));

            if (g_os_param_reserved[0] == 1)
            {
                Log("break here for debug .....");
                goto End;
            }

            // convert / to \\   
            for (Pos = 0; Pos < sizeof(g_os_param.vtoy_img_path) && g_os_param.vtoy_img_path[Pos]; Pos++)
            {
                if (g_os_param.vtoy_img_path[Pos] == '/')
                {
                    g_os_param.vtoy_img_path[Pos] = '\\';
                }
            }

            PeStart += sizeof(ventoy_os_param) + sizeof(ventoy_windows_data) + exlen;
            sprintf_s(LunchFile, MAX_PATH, "ventoy\\%s", GetFileNameInPath(ExeFileName));

            MUTEX_LOCK(g_vtoyins_mutex);
            if (IsFileExist("%s", LunchFile))
            {
                Log("vtoyjump multiple call ...");
                rc = 0;
                MUTEX_UNLOCK(g_vtoyins_mutex);
                goto End;
            }

            SaveBuffer2File(LunchFile, Buffer + PeStart, FileSize - PeStart);
            MUTEX_UNLOCK(g_vtoyins_mutex);

            break;
        }
    }

    if (PeStart >= FileSize)
    {
        Log("OS param not found");
        goto End;
    }

    if (g_os_param_reserved[0] == 2)
    {
        Log("skip hook for debug .....");
        rc = 0;
        goto End;
    }

    rc = VentoyHook(&g_os_param);

End:

    if (Buffer)
    {
        free(Buffer);
    }

    return rc;
}


int real_main(int argc, char **argv)
{
    int i = 0;
    int rc = 0;
    CHAR NewFile[MAX_PATH];
    CHAR LunchFile[MAX_PATH];
    CHAR CallParam[1024] = { 0 };
    STARTUPINFOA Si;
    PROCESS_INFORMATION Pi;

    Log("#### real_main #### argc = %d", argc);
    Log("program full path: <%s>", g_prog_full_path);
    Log("program dir: <%s>", g_prog_dir);
    Log("program name: <%s>", g_prog_name);

    Log("argc = %d", argc);
    for (i = 0; i < argc; i++)
    {
        Log("argv[%d]=<%s>", i, argv[i]);
        if (i > 0)
        {
            strcat_s(CallParam, sizeof(CallParam), " ");
            strcat_s(CallParam, sizeof(CallParam), argv[i]);
        }
    }

    GetStartupInfoA(&Si);
    memset(LunchFile, 0, sizeof(LunchFile));

    rc = VentoyJump(argc, argv, LunchFile);

    Log("LunchFile=<%s> CallParam=<%s>", LunchFile, CallParam);

    if (_stricmp(g_prog_name, "winpeshl.exe") != 0 && IsFileExist("ventoy\\%s", g_prog_name))
    {
        sprintf_s(NewFile, sizeof(NewFile), "%s\\VTOYJUMP.EXE", g_prog_dir);
        MoveFileA(g_prog_full_path, NewFile);
        Log("Move <%s> to <%s>", g_prog_full_path, NewFile);

        sprintf_s(NewFile, sizeof(NewFile), "ventoy\\%s", g_prog_name);
        CopyFileA(NewFile, g_prog_full_path, TRUE);
        Log("Copy <%s> to <%s>", NewFile, g_prog_full_path);

        sprintf_s(LunchFile, sizeof(LunchFile), "%s", g_prog_full_path);
        Log("Final lunchFile is <%s>", LunchFile);
    }
    else
    {
        Log("We don't need to recover original <%s>", g_prog_name);
    }

    if (g_os_param_reserved[0] == 3)
    {
        Log("Open log for debug ...");
        sprintf_s(LunchFile, sizeof(LunchFile), "%s", "notepad.exe ventoy.log");
    }
    else
    {
        if (CallParam[0])
        {
            strcat_s(LunchFile, sizeof(LunchFile), CallParam);
        }
        else if (NULL == strstr(LunchFile, "setup.exe"))
        {
            Log("Not setup.exe, hide windows.");
            Si.dwFlags |= STARTF_USESHOWWINDOW;
            Si.wShowWindow = SW_HIDE;
        }

        Log("Ventoy jump %s ...", rc == 0 ? "success" : "failed");
    }

    Log("Now launch <%s> ...", LunchFile);

    if (g_os_param_reserved[0] == 4)
    {
        Log("Open cmd for debug ...");
        Si.dwFlags |= STARTF_USESHOWWINDOW;
        Si.wShowWindow = SW_NORMAL;
        sprintf_s(LunchFile, sizeof(LunchFile), "%s", "cmd.exe");
    }

    Log("Backup log at this point");
    CopyFileA(LOG_FILE, "X:\\Windows\\ventoy.backup", TRUE);

    CreateProcessA(NULL, LunchFile, NULL, NULL, FALSE, 0, NULL, NULL, &Si, &Pi);

    for (i = 0; rc && i < 1800; i++)
    {
        Log("Ventoy hook failed, now wait and retry ...");
        Sleep(1000);
        rc = VentoyHook(&g_os_param);
    }

    Log("Wait process...");
    WaitForSingleObject(Pi.hProcess, INFINITE);

    Log("vtoyjump finished");
    return 0;
}

void VentoyToUpper(CHAR *str)
{
    int i;
    for (i = 0; str[i]; i++)
    {
        str[i] = (CHAR)toupper(str[i]);
    }
}

int vtoy_remove_duplicate_file(char *File)
{
    CHAR szCmd[MAX_PATH];
    CHAR NewFile[MAX_PATH];
    STARTUPINFOA Si;
    PROCESS_INFORMATION Pi;

    Log("<1> Copy New file", File);
    sprintf_s(NewFile, sizeof(NewFile), "%s_NEW", File);
    CopyFileA(File, NewFile, FALSE);

    Log("<2> Remove file <%s>", File);
    GetStartupInfoA(&Si);
    Si.dwFlags |= STARTF_USESHOWWINDOW;
    Si.wShowWindow = SW_HIDE;
    sprintf_s(szCmd, sizeof(szCmd), "cmd.exe /c del /F /Q %s", File);
    CreateProcessA(NULL, szCmd, NULL, NULL, FALSE, 0, NULL, NULL, &Si, &Pi);
    WaitForSingleObject(Pi.hProcess, INFINITE);

    Log("<3> Copy back file <%s>", File);
    MoveFileA(NewFile, File);

    return 0;
}

int VTLRI_ServiceMain(int argc, char **argv)
{
    int n = 0;
    DWORD Err;
    BOOL bRet = TRUE;
    CHAR Drive[16];
    CHAR FsType[64];
    CHAR Cmdline[256];
    PROCESS_INFORMATION Pi;
    STARTUPINFOA Si;

    //XXX.exe VTLRI_SRV D Z
    Log("VTLRI_ServiceMain start %s %s %s ...", argv[0], argv[1], argv[2]);

    sprintf_s(Drive, sizeof(Drive), "%C:\\", argv[2][0]);

    while (n < 3)
    {
        bRet = GetVolumeInformationA(Drive, NULL, 0, NULL, NULL, NULL, FsType, sizeof(FsType));
        if (bRet)
        {
            Sleep(400);
        }
        else
        {
            Err = LASTERR;
            if (Err == ERROR_PATH_NOT_FOUND)
            {
                Log("%s not found", Drive);
                n++;
            }
        }
    }

    sprintf_s(Cmdline, sizeof(Cmdline), "ventoy\\imdisk.exe -d -m %C:", argv[3][0]);
    Log("Remove disk by <%s>", Cmdline);

    GetStartupInfoA(&Si);
    Si.dwFlags |= STARTF_USESHOWWINDOW;
    Si.wShowWindow = SW_HIDE;

    CreateProcessA(NULL, Cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &Si, &Pi);
    WaitForSingleObject(Pi.hProcess, INFINITE);
    return 0;
}

int main(int argc, char **argv)
{
    int i;
    STARTUPINFOA Si;
    PROCESS_INFORMATION Pi;
    CHAR CurDir[MAX_PATH];
    CHAR NewArgv0[MAX_PATH];
    CHAR CallParam[1024] = { 0 };

    g_vtoylog_mutex = CreateMutexA(NULL, FALSE, "VTOYLOG_LOCK");
    g_vtoyins_mutex = CreateMutexA(NULL, FALSE, "VTOYINS_LOCK");

    Log("######## VentoyJump %dbit ##########", g_system_bit);

    if (argc > 1 && strcmp(argv[1], "VTLRI_SRV") == 0)
    {
        return VTLRI_ServiceMain(argc, argv);
    }

    GetCurrentDirectoryA(sizeof(CurDir), CurDir);
    Log("Current directory is <%s>", CurDir);
    
    GetModuleFileNameA(NULL, g_prog_full_path, MAX_PATH);
    split_path_name(g_prog_full_path, g_prog_dir, g_prog_name);

    Log("EXE path: <%s> dir:<%s> name:<%s>", g_prog_full_path, g_prog_dir, g_prog_name);

    if (IsFileExist(WIMBOOT_FILE))
    {
        Log("This is wimboot mode ...");
        g_wimboot_mode = TRUE;

        if (!IsFileExist(WIMBOOT_DONE))
        {
            vtoy_remove_duplicate_file(g_prog_full_path);
            SaveBuffer2File(WIMBOOT_DONE, g_prog_full_path, 1);
        }
    }
    else
    {
        Log("This is normal mode ...");
    }

    if (_stricmp(g_prog_name, "WinLogon.exe") == 0)
    {
        Log("This time is rejump back ...");
        
        strcpy_s(g_prog_full_path, sizeof(g_prog_full_path), argv[1]);
        split_path_name(g_prog_full_path, g_prog_dir, g_prog_name);

        return real_main(argc - 1, argv + 1);
    }
    else if (_stricmp(g_prog_name, "PECMD.exe") == 0)
    {
        strcpy_s(NewArgv0, sizeof(NewArgv0), g_prog_dir);
        VentoyToUpper(NewArgv0);
        
        if (NULL == strstr(NewArgv0, "SYSTEM32") && IsFileExist(ORG_PECMD_BK_PATH))
        {
            Log("Just call original pecmd.exe");
            strcpy_s(CallParam, sizeof(CallParam), ORG_PECMD_PATH);
        }
        else
        {
            Log("We need to rejump for pecmd ...");

            ventoy_check_create_directory();
            CopyFileA(g_prog_full_path, "ventoy\\WinLogon.exe", TRUE);

            sprintf_s(CallParam, sizeof(CallParam), "ventoy\\WinLogon.exe %s", g_prog_full_path);
        }
        
        for (i = 1; i < argc; i++)
        {
            strcat_s(CallParam, sizeof(CallParam), " ");
            strcat_s(CallParam, sizeof(CallParam), argv[i]);
        }

        Log("Now rejump to <%s> ...", CallParam);
        GetStartupInfoA(&Si);
        CreateProcessA(NULL, CallParam, NULL, NULL, FALSE, 0, NULL, NULL, &Si, &Pi);

        Log("Wait rejump process...");
        WaitForSingleObject(Pi.hProcess, INFINITE);
        Log("rejump finished");
        return 0;
    }
    else
    {
        Log("We don't need to rejump ...");

        ventoy_check_create_directory();
        strcpy_s(NewArgv0, sizeof(NewArgv0), g_prog_full_path);
        argv[0] = NewArgv0;

        return real_main(argc, argv);
    }
}
