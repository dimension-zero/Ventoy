/******************************************************************************
 * WinDialog.c
 *
 * Copyright © 2017-2019 Pete Batard <pete@akeo.ie>
 * Copyright (c) 2020, longpanda <admin@ventoy.net>
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
#include <Shlobj.h>
#include <tlhelp32.h>
#include <Psapi.h>
#include <commctrl.h>
#include "resource.h"
#include "Language.h"
#include "Ventoy2Disk.h"
#include "DiskService.h"
#include "VentoyJson.h"
#include "WinDialog_priv.h"

HINSTANCE g_hInst;

BOOL g_InvalidClusterSize = FALSE;
BOOL g_SecureBoot = TRUE;
CHAR g_VolumeLabel[64] = "Ventoy";
HWND g_DialogHwnd;
HWND g_ComboxHwnd;
HWND g_LocalClusterTipHwnd;
HWND g_DiskClusterTipHwnd;
HWND g_StaticLocalVerHwnd;
HWND g_StaticDiskVerHwnd;
HWND g_StaticLocalStyleHwnd;
HWND g_StaticDiskStyleHwnd;
HWND g_StaticLocalFsHwnd;
HWND g_StaticDevFsHwnd;
HWND g_BtnInstallHwnd;
HWND g_StaticDevHwnd;
HWND g_StaticLocalHwnd;
HWND g_StaticDiskHwnd;
HWND g_BtnUpdateHwnd;
HWND g_ProgressBarHwnd;
HWND g_StaticStatusHwnd;
HWND g_LocalIconSecureHwnd;
HWND g_DiskIconSecureHwnd;
HANDLE g_ThreadHandle = NULL;

HFONT g_language_normal_font = NULL;
HFONT g_language_bold_font = NULL;

int g_cur_part_style = 0; // 0:MBR  1:GPT
int g_language_count = 0;
int g_cur_lang_id = 0;
VENTOY_LANGUAGE *g_language_data = NULL;
VENTOY_LANGUAGE *g_cur_lang_data = NULL;

int m_MinWidth;
int m_MinHeight;
RECT m_WindowRect;

DWORD VentoyGetParentProcessId(DWORD pid)
{
    HANDLE h = NULL;
    PROCESSENTRY32 pe = { 0 };
    DWORD ppid = 0;

    pe.dwSize = sizeof(PROCESSENTRY32);
    h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (INVALID_HANDLE_VALUE == h)
    {
        return 0;
    }

    if (Process32First(h, &pe))
    {
        do
        {
            if (pe.th32ProcessID == pid)
            {
                ppid = pe.th32ParentProcessID;
                break;
            }
        } while (Process32Next(h, &pe));
    }

    CloseHandle(h);
    return ppid;
}

int VentoyCheckParentProcess(void)
{
    int i, j;
    int ret = 0;
    HANDLE h;
    DWORD pid, ppid;
    DWORD len = MAX_PATH;
    BYTE* buffer = NULL;
    UINT32* pData = NULL;
    BYTE Magic[] = { 0x4E, 0x75, 0x6C, 0x6C, 0x73, 0x6F, 0x66, 0x74 };
    WCHAR ParentPath[MAX_PATH];    

    pid = GetCurrentProcessId();
    ppid = VentoyGetParentProcessId(pid);

    if (ppid == 0)
    {
        Log("Failed to get parent process id for %u %u", pid, LASTERR);
        return 0;
    }

    Log("id=%u/%u", pid, ppid);
    
    h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, ppid);
    if (h == INVALID_HANDLE_VALUE)
    {
        Log("Failed to OpenProcess for %u %u", ppid, LASTERR);
        return 0;
    }

    if (0 == QueryFullProcessImageName(h, 0, ParentPath, &len))
    {
        Log("Failed to QueryFullProcessImageName for %u %u", ppid, LASTERR);
        return 0;
    }

    CHECK_CLOSE_HANDLE(h);

    Log("PPath:<%ls>", ParentPath);

    h = CreateFile(ParentPath, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
    {
        Log("Failed to create file %u", LASTERR);
        return 0;
    }

    len = GetFileSize(h, NULL);
    Log("PSize:<%u %uKB>", len, len / 1024);

    if (len < 8 * SIZE_1MB)
    {
        goto out;
    }

    buffer = malloc(SIZE_1MB);
    if (buffer == NULL)
    {
        goto out;
    }

    if (FALSE == ReadFile(h, buffer, SIZE_1MB, &len, NULL))
    {
        Log("Failed to readfile %u", LASTERR);
        goto out;
    }

    for (i = 0; i + 16 < SIZE_1MB && ret == 0; i += 16)
    {
        pData = (UINT32*)(buffer + i);
        if (pData[0] == 0x6D783F3C && pData[1] == 0x6576206C)
        {
            for (j = 0; j < 1024 && (i + j + 16) < SIZE_1MB; j++)
            {
                if (0 == memcmp(buffer + i + j, Magic, sizeof(Magic)))
                {
                    ret = 1;
                    break;
                }
            }
        }
    }

out:
    Log("Lunch main process %d", ret);
    CHECK_CLOSE_HANDLE(h);
    if (buffer) free(buffer);
    return ret;
}

//
//copy from Rufus
//
#include <delayimp.h>
// For delay-loaded DLLs, use LOAD_LIBRARY_SEARCH_SYSTEM32 to avoid DLL search order hijacking.
FARPROC WINAPI dllDelayLoadHook(unsigned dliNotify, PDelayLoadInfo pdli)
{
    if (dliNotify == dliNotePreLoadLibrary) {
        // Windows 7 without KB2533623 does not support the LOAD_LIBRARY_SEARCH_SYSTEM32 flag.
        // That is is OK, because the delay load handler will interrupt the NULL return value
        // to mean that it should perform a normal LoadLibrary.
        return (FARPROC)LoadLibraryExA(pdli->szDll, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    }
    return NULL;
}

#if defined(_MSC_VER)
// By default the Windows SDK headers have a `const` while MinGW does not.
const
#endif
PfnDliHook __pfnDliNotifyHook2 = dllDelayLoadHook;

typedef BOOL(WINAPI *SetDefaultDllDirectories_t)(DWORD);
void DllProtect(void)
{
    SetDefaultDllDirectories_t pfSetDefaultDllDirectories = NULL;

    // Disable loading system DLLs from the current directory (sideloading mitigation)
    // PS: You know that official MSDN documentation for SetDllDirectory() that explicitly
    // indicates that "If the parameter is an empty string (""), the call removes the current
    // directory from the default DLL search order"? Yeah, that doesn't work. At all.
    // Still, we invoke it, for platforms where the following call might actually work...
    SetDllDirectoryA("");

    // For libraries on the KnownDLLs list, the system will always load them from System32.
    // For other DLLs we link directly to, we can delay load the DLL and use a delay load
    // hook to load them from System32. Note that, for this to work, something like:
    // 'somelib.dll;%(DelayLoadDLLs)' must be added to the 'Delay Loaded Dlls' option of
    // the linker properties in Visual Studio (which means this won't work with MinGW).
    // For all other DLLs, use SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32).
    // Finally, we need to perform the whole gymkhana below, where we can't call on
    // SetDefaultDllDirectories() directly, because Windows 7 doesn't have the API exposed.
    // Also, no, Coverity, we never need to care about freeing kernel32 as a library.
    // coverity[leaked_storage]

    pfSetDefaultDllDirectories = (SetDefaultDllDirectories_t)
        GetProcAddress(LoadLibraryW(L"kernel32.dll"), "SetDefaultDllDirectories");
    if (pfSetDefaultDllDirectories != NULL)
        pfSetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow)
{
    int ret;
    int i, j;
    WCHAR *Pos = NULL;
    WCHAR CurDir[MAX_PATH];
    WCHAR ExePath[MAX_PATH];
    CHAR TmpPathA[MAX_PATH];
    const char *checkfile[] =
    {
        "boot\\boot.img",
        "boot\\core.img.xz",
        "ventoy\\ventoy.disk.img.xz",
        "ventoy\\version",
        NULL
    };

    LogCache(TRUE);
    

    UNREFERENCED_PARAMETER(hPrevInstance);

    if (__argc > 1 && __argv[1] && _stricmp(__argv[1], "VTOYCLI") == 0)
    {
        g_CLI_Mode = TRUE;
    }

    DllProtect();


    Log("\n##################################################################################\n"
        "############################## Ventoy2Disk%s %s #############################\n"
        "##################################################################################",
        current_arch_string(), g_CLI_Mode ? "CLI MODE" : "");

    GetCurrentDirectoryW(MAX_PATH, CurDir);
    GetModuleFileNameW(NULL, ExePath, MAX_PATH);
    UTF8_Log("Current Directory <%s>", CurDir);
    UTF8_Log("Exe file path <%s>", ExePath);

    for (Pos = NULL, i = 0; i < MAX_PATH && ExePath[i]; i++)
    {
        if (ExePath[i] == '\\' || ExePath[i] == '/')
        {
            Pos = ExePath + i;
        }
    }

#ifdef NDEBUG
    if (Pos)
    {
        *Pos = 0;
        if (wcscmp(CurDir, ExePath))
        {
            UTF8_Log("Change current dir to exe <%s>", ExePath);
            SetCurrentDirectory(ExePath);
            GetCurrentDirectory(MAX_PATH, CurDir);
        }
        else
        {
            Log("Current directory check OK.");
        }
    }
#endif

    Pos = wcsstr(CurDir, L"\\altexe");
    if (Pos)
    {
        *Pos = 0;
        UTF8_Log("altexe detected, change current dir to <%s>", CurDir);
        SetCurrentDirectory(CurDir);
    }
    

    LogCache(FALSE);
    LogFlush();

    /* /HELP /? /V /L are read-only and need no local boot files.
     * /DEV (or VENTOY_DEV=1) skips the check entirely for contributors who want
     * to launch the GUI without first running bootstrap-data.ps1. */
    {
        BOOL needBootFiles = TRUE;
        int k;
        for (k = 1; k < __argc; k++)
        {
            if (_stricmp(__argv[k], "/DEV") == 0)
            {
                Log("/DEV flag set, skipping boot-file presence check");
                needBootFiles = FALSE;
                break;
            }
        }
        if (needBootFiles)
        {
            CHAR envBuf[8] = { 0 };
            if (GetEnvironmentVariableA("VENTOY_DEV", envBuf, sizeof(envBuf)) > 0 && envBuf[0] == '1')
            {
                Log("VENTOY_DEV=1, skipping boot-file presence check");
                needBootFiles = FALSE;
            }
        }
        if (g_CLI_Mode && needBootFiles)
        {
            for (k = 2; k < __argc; k++)
            {
                if (_stricmp(__argv[k], "/HELP") == 0 || _stricmp(__argv[k], "/?") == 0 ||
                    _stricmp(__argv[k], "/V") == 0    || _stricmp(__argv[k], "/L") == 0)
                {
                    needBootFiles = FALSE;
                    break;
                }
            }
        }
        if (needBootFiles)
        {
            for (i = 0; checkfile[i]; i++)
            {
                if (!IsFileExist("%s", checkfile[i]))
                {
                    for (j = 0; j < 50; j++)
                    {
                        Log("####### File <%s> not found, did you download it from official website ? ######", checkfile[i]);
                    }

                    if (g_CLI_Mode)
                    {
                        Log("[ERROR] Boot file not found: %s. Run from the Ventoy installation directory.", checkfile[i]);
                        return ERROR_NOT_FOUND;
                    }
                    else if (IsFileExist("grub\\grub.cfg"))
                    {
                        MessageBox(NULL, TEXT("Don't run me here, please use the released install package."), TEXT("Error"), MB_OK | MB_ICONERROR);
                    }
                    else
                    {
                        MessageBox(NULL, TEXT("Please run under the correct directory!"), TEXT("Error"), MB_OK | MB_ICONERROR);
                    }
                    return ERROR_NOT_FOUND;
                }
            }
        }
    }
    UTF8_Log("Current directory:<%s>", CurDir);
    Log("######### Current Ventoy Version: Ventoy2Disk%s %s ########", current_arch_string(), GetLocalVentoyVersion());

    if (g_CLI_Mode)
    {
        DumpWindowsVersion();

        if (VentoyCLIMain(__argc, __argv))
        {
            Log("[ERROR] ######### Ventoy CLI FAILED #########");
            ret = 1;
        }
        else
        {
            Log("######### Ventoy CLI SUCCESS #########");
            ret = 0;
        }

        return ret;
    }

    if (VentoyCheckParentProcess())
    {
        return ERROR_NOT_SUPPORTED;
    }

    ParseCmdLineOption(lpCmdLine);
    LoadCfgIni();

    DumpWindowsVersion();

    Ventoy2DiskInit();

    g_hInst = hInstance;
    DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DialogProc);

    Ventoy2DiskDestroy();

    return 0;
}
