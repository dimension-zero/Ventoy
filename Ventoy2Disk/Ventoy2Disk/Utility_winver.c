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

BOOL IsWow64(void)
{
    typedef BOOL(WINAPI *LPFN_ISWOW64PROCESS)(HANDLE, PBOOL);
    LPFN_ISWOW64PROCESS fnIsWow64Process;
    BOOL bIsWow64 = FALSE;
	CHAR Wow64Dir[MAX_PATH];

    fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandleA("kernel32"), "IsWow64Process");
    if (NULL != fnIsWow64Process)
    {
        fnIsWow64Process(GetCurrentProcess(), &bIsWow64);
    }

	if (!bIsWow64)
	{
		if (GetSystemWow64DirectoryA(Wow64Dir, sizeof(Wow64Dir)))
		{
			Log("GetSystemWow64DirectoryA=<%s>", Wow64Dir);
			bIsWow64 = TRUE;
		}
	}

    return bIsWow64;
}

/*
* Some code and functions in the file are copied from rufus.
* https://github.com/pbatard/rufus
*/

/* Windows versions */
enum WindowsVersion {
    WINDOWS_UNDEFINED = -1,
    WINDOWS_UNSUPPORTED = 0,
    WINDOWS_XP = 0x51,
    WINDOWS_2003 = 0x52,	// Also XP_64
    WINDOWS_VISTA = 0x60,	// Also Server 2008
    WINDOWS_7 = 0x61,		// Also Server 2008_R2
    WINDOWS_8 = 0x62,		// Also Server 2012
    WINDOWS_8_1 = 0x63,		// Also Server 2012_R2
    WINDOWS_10_PREVIEW1 = 0x64,
    WINDOWS_10 = 0xA0,		// Also Server 2016, also Server 2019
    WINDOWS_11 = 0xB0,		// Also Server 2022
    WINDOWS_MAX
};

static const char* GetEdition(DWORD ProductType)
{
    // From: https://docs.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getproductinfo
    // These values can be found in the winnt.h header.
    switch (ProductType) {
		case 0x00000000: return "";	//  Undefined
		case 0x00000001: return "Ultimate";
		case 0x00000002: return "Home Basic";
		case 0x00000003: return "Home Premium";
		case 0x00000004: return "Enterprise";
		case 0x00000005: return "Home Basic N";
		case 0x00000006: return "Business";
		case 0x00000007: return "Server Standard";
		case 0x00000008: return "Server Datacenter";
		case 0x00000009: return "Smallbusiness Server";
		case 0x0000000A: return "Server Enterprise";
		case 0x0000000B: return "Starter";
		case 0x0000000C: return "Server Datacenter (Core)";
		case 0x0000000D: return "Server Standard (Core)";
		case 0x0000000E: return "Server Enterprise (Core)";
		case 0x00000010: return "Business N";
		case 0x00000011: return "Web Server";
		case 0x00000012: return "HPC Edition";
		case 0x00000013: return "Storage Server (Essentials)";
		case 0x0000001A: return "Home Premium N";
		case 0x0000001B: return "Enterprise N";
		case 0x0000001C: return "Ultimate N";
		case 0x00000022: return "Home Server";
		case 0x00000024: return "Server Standard without Hyper-V";
		case 0x00000025: return "Server Datacenter without Hyper-V";
		case 0x00000026: return "Server Enterprise without Hyper-V";
		case 0x00000027: return "Server Datacenter without Hyper-V (Core)";
		case 0x00000028: return "Server Standard without Hyper-V (Core)";
		case 0x00000029: return "Server Enterprise without Hyper-V (Core)";
		case 0x0000002A: return "Hyper-V Server";
		case 0x0000002F: return "Starter N";
		case 0x00000030: return "Pro";
		case 0x00000031: return "Pro N";
		case 0x00000034: return "Server Solutions Premium";
		case 0x00000035: return "Server Solutions Premium (Core)";
		case 0x00000040: return "Server Hyper Core V";
		case 0x00000042: return "Starter E";
		case 0x00000043: return "Home Basic E";
		case 0x00000044: return "Premium E";
		case 0x00000045: return "Pro E";
		case 0x00000046: return "Enterprise E";
		case 0x00000047: return "Ultimate E";
		case 0x00000048: return "Enterprise (Eval)";
		case 0x0000004F: return "Server Standard (Eval)";
		case 0x00000050: return "Server Datacenter (Eval)";
		case 0x00000054: return "Enterprise N (Eval)";
		case 0x00000057: return "Thin PC";
		case 0x00000058: case 0x00000059: case 0x0000005A: case 0x0000005B: case 0x0000005C: return "Embedded";
		case 0x00000062: return "Home N";
		case 0x00000063: return "Home China";
		case 0x00000064: return "Home Single Language";
		case 0x00000065: return "Home";
		case 0x00000067: return "Pro with Media Center";
		case 0x00000069: case 0x0000006A: case 0x0000006B: case 0x0000006C: return "Embedded";
		case 0x0000006F: return "Home Connected";
		case 0x00000070: return "Pro Student";
		case 0x00000071: return "Home Connected N";
		case 0x00000072: return "Pro Student N";
		case 0x00000073: return "Home Connected Single Language";
		case 0x00000074: return "Home Connected China";
		case 0x00000079: return "Education";
		case 0x0000007A: return "Education N";
		case 0x0000007D: return "Enterprise LTSB";
		case 0x0000007E: return "Enterprise LTSB N";
		case 0x0000007F: return "Pro S";
		case 0x00000080: return "Pro S N";
		case 0x00000081: return "Enterprise LTSB (Eval)";
		case 0x00000082: return "Enterprise LTSB N (Eval)";
		case 0x0000008A: return "Pro Single Language";
		case 0x0000008B: return "Pro China";
		case 0x0000008C: return "Enterprise Subscription";
		case 0x0000008D: return "Enterprise Subscription N";
		case 0x00000091: return "Server Datacenter SA (Core)";
		case 0x00000092: return "Server Standard SA (Core)";
		case 0x00000095: return "Utility VM";
		case 0x000000A1: return "Pro for Workstations";
		case 0x000000A2: return "Pro for Workstations N";
		case 0x000000A4: return "Pro for Education";
		case 0x000000A5: return "Pro for Education N";
		case 0x000000AB: return "Enterprise G";	// I swear Microsoft are just making up editions...
		case 0x000000AC: return "Enterprise G N";
		case 0x000000B6: return "Home OS";
		case 0x000000B7: return "Cloud E";
		case 0x000000B8: return "Cloud E N";
		case 0x000000BD: return "Lite";
		case 0xABCDABCD: return "(Unlicensed)";
		default: return "(Unknown Edition)";
    }
}

#define is_x64 IsWow64
#define static_strcpy safe_strcpy 
#define REGKEY_HKCU HKEY_CURRENT_USER
#define REGKEY_HKLM HKEY_LOCAL_MACHINE
static int  nWindowsVersion = WINDOWS_UNDEFINED;
static int  nWindowsBuildNumber = -1;
static char WindowsVersionStr[128] = "";

/* Helpers for 32 bit registry operations */

/*
* Read a generic registry key value. If a short key_name is used, assume that
* it belongs to the application and create the app subkey if required
*/
static __inline BOOL _GetRegistryKey(HKEY key_root, const char* key_name, DWORD reg_type,
    LPBYTE dest, DWORD dest_size)
{
    const char software_prefix[] = "SOFTWARE\\";
    char long_key_name[MAX_PATH] = { 0 };
    BOOL r = FALSE;
    size_t i;
    LONG s;
    HKEY hSoftware = NULL, hApp = NULL;
    DWORD dwType = -1, dwSize = dest_size;

    memset(dest, 0, dest_size);

    if (key_name == NULL)
        return FALSE;

    for (i = strlen(key_name); i>0; i--) {
        if (key_name[i] == '\\')
            break;
    }

    if (i > 0) {
        // Prefix with "SOFTWARE" if needed
        if (_strnicmp(key_name, software_prefix, sizeof(software_prefix)-1) != 0) {
            if (i + sizeof(software_prefix) >= sizeof(long_key_name))
                return FALSE;
            strcpy_s(long_key_name, sizeof(long_key_name), software_prefix);
            strcat_s(long_key_name, sizeof(long_key_name), key_name);
            long_key_name[sizeof(software_prefix)+i - 1] = 0;
        }
        else {
            if (i >= sizeof(long_key_name))
                return FALSE;
            static_strcpy(long_key_name, key_name);
            long_key_name[i] = 0;
        }
        i++;
        if (RegOpenKeyExA(key_root, long_key_name, 0, KEY_READ, &hApp) != ERROR_SUCCESS) {
            hApp = NULL;
            goto out;
        }
    }
    else {
        if (RegOpenKeyExA(key_root, "SOFTWARE", 0, KEY_READ | KEY_CREATE_SUB_KEY, &hSoftware) != ERROR_SUCCESS) {
            hSoftware = NULL;
            goto out;
        }        
    }

    s = RegQueryValueExA(hApp, &key_name[i], NULL, &dwType, (LPBYTE)dest, &dwSize);
    // No key means default value of 0 or empty string
    if ((s == ERROR_FILE_NOT_FOUND) || ((s == ERROR_SUCCESS) && (dwType == reg_type) && (dwSize > 0))) {
        r = TRUE;
    }
out:
    if (hSoftware != NULL)
        RegCloseKey(hSoftware);
    if (hApp != NULL)
        RegCloseKey(hApp);
    return r;
}

#define GetRegistryKey32(root, key, pval) _GetRegistryKey(root, key, REG_DWORD, (LPBYTE)pval, sizeof(DWORD))
static __inline INT32 ReadRegistryKey32(HKEY root, const char* key) {
    DWORD val;
    GetRegistryKey32(root, key, &val);
    return (INT32)val;
}

/*
* Modified from smartmontools' os_win32.cpp
*/
void GetWindowsVersion(void)
{   
    OSVERSIONINFOEXA vi, vi2;
    DWORD dwProductType;
    const char* w = 0;
    const char* w64 = "32 bit";
    char *vptr;
    size_t vlen;
    unsigned major, minor;
    ULONGLONG major_equal, minor_equal;
    BOOL ws;

    nWindowsVersion = WINDOWS_UNDEFINED;
    static_strcpy(WindowsVersionStr, "Windows Undefined");

    // suppress the C4996 warning for GetVersionExA
    #pragma warning(push)
    #pragma warning(disable:4996)

    memset(&vi, 0, sizeof(vi));
    vi.dwOSVersionInfoSize = sizeof(vi);
    if (!GetVersionExA((OSVERSIONINFOA *)&vi)) {
        memset(&vi, 0, sizeof(vi));
        vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
        if (!GetVersionExA((OSVERSIONINFOA *)&vi))
            return;
    }

    #pragma warning(pop)

    if (vi.dwPlatformId == VER_PLATFORM_WIN32_NT) {

        if (vi.dwMajorVersion > 6 || (vi.dwMajorVersion == 6 && vi.dwMinorVersion >= 2)) {
            // Starting with Windows 8.1 Preview, GetVersionEx() does no longer report the actual OS version
            // See: http://msdn.microsoft.com/en-us/library/windows/desktop/dn302074.aspx
            // And starting with Windows 10 Preview 2, Windows enforces the use of the application/supportedOS
            // manifest in order for VerSetConditionMask() to report the ACTUAL OS major and minor...

            major_equal = VerSetConditionMask(0, VER_MAJORVERSION, VER_EQUAL);
            for (major = vi.dwMajorVersion; major <= 9; major++) {
                memset(&vi2, 0, sizeof(vi2));
                vi2.dwOSVersionInfoSize = sizeof(vi2); vi2.dwMajorVersion = major;
                if (!VerifyVersionInfoA(&vi2, VER_MAJORVERSION, major_equal))
                    continue;
                if (vi.dwMajorVersion < major) {
                    vi.dwMajorVersion = major; vi.dwMinorVersion = 0;
                }

                minor_equal = VerSetConditionMask(0, VER_MINORVERSION, VER_EQUAL);
                for (minor = vi.dwMinorVersion; minor <= 9; minor++) {
                    memset(&vi2, 0, sizeof(vi2)); vi2.dwOSVersionInfoSize = sizeof(vi2);
                    vi2.dwMinorVersion = minor;
                    if (!VerifyVersionInfoA(&vi2, VER_MINORVERSION, minor_equal))
                        continue;
                    vi.dwMinorVersion = minor;
                    break;
                }

                break;
            }
        }

        if (vi.dwMajorVersion <= 0xf && vi.dwMinorVersion <= 0xf) {
            ws = (vi.wProductType <= VER_NT_WORKSTATION);
            nWindowsVersion = vi.dwMajorVersion << 4 | vi.dwMinorVersion;
            switch (nWindowsVersion) {
            case WINDOWS_XP: w = "XP";
                break;
            case WINDOWS_2003: w = (ws ? "XP_64" : (!GetSystemMetrics(89) ? "Server 2003" : "Server 2003_R2"));
                break;
            case WINDOWS_VISTA: w = (ws ? "Vista" : "Server 2008");
                break;
            case WINDOWS_7: w = (ws ? "7" : "Server 2008_R2");
                break;
            case WINDOWS_8: w = (ws ? "8" : "Server 2012");
                break;
            case WINDOWS_8_1: w = (ws ? "8.1" : "Server 2012_R2");
                break;
            case WINDOWS_10_PREVIEW1: w = (ws ? "10 (Preview 1)" : "Server 10 (Preview 1)");
                break;
                // Starting with Windows 10 Preview 2, the major is the same as the public-facing version
            case WINDOWS_10:
                if (vi.dwBuildNumber < 20000) {
                    w = (ws ? "10" : ((vi.dwBuildNumber < 17763) ? "Server 2016" : "Server 2019"));
                    break;
                }
                nWindowsVersion = WINDOWS_11;
                // Fall through
            case WINDOWS_11: w = (ws ? "11" : "Server 2022");
                break;
            default:
                if (nWindowsVersion < WINDOWS_XP)
                    nWindowsVersion = WINDOWS_UNSUPPORTED;
                else
                    w = "12 or later";
                break;
            }
        }
    }

    if (is_x64())
        w64 = "64-bit";

    GetProductInfo(vi.dwMajorVersion, vi.dwMinorVersion, vi.wServicePackMajor, vi.wServicePackMinor, &dwProductType);
    vptr = WindowsVersionStr;
    vlen = sizeof(WindowsVersionStr) - 1;

    if (!w)
        sprintf_s(vptr, vlen, "%s %u.%u %s", (vi.dwPlatformId == VER_PLATFORM_WIN32_NT ? "NT" : "??"),
        (unsigned)vi.dwMajorVersion, (unsigned)vi.dwMinorVersion, w64);
    else if (vi.wServicePackMinor)
        sprintf_s(vptr, vlen, "%s SP%u.%u %s", w, vi.wServicePackMajor, vi.wServicePackMinor, w64);
    else if (vi.wServicePackMajor)
        sprintf_s(vptr, vlen, "%s SP%u %s", w, vi.wServicePackMajor, w64);
    else
        sprintf_s(vptr, vlen, "%s%s%s, %s",
        w, (dwProductType != PRODUCT_UNDEFINED) ? " " : "", GetEdition(dwProductType), w64);

    // Add the build number (including UBR if available) for Windows 8.0 and later
    nWindowsBuildNumber = vi.dwBuildNumber;
    if (nWindowsVersion >= 0x62) {
        int nUbr = ReadRegistryKey32(REGKEY_HKLM, "Software\\Microsoft\\Windows NT\\CurrentVersion\\UBR");
        vptr = WindowsVersionStr + strlen(WindowsVersionStr);
        vlen = sizeof(WindowsVersionStr) - strlen(WindowsVersionStr) - 1;
        if (nUbr > 0)
            sprintf_s(vptr, vlen, " (Build %d.%d)", nWindowsBuildNumber, nUbr);
        else
            sprintf_s(vptr, vlen, " (Build %d)", nWindowsBuildNumber);
    }
}



void DumpWindowsVersion(void)
{
    GetWindowsVersion();
    Log("Windows Version: <<Windows %s>>", WindowsVersionStr);
    return;
}

