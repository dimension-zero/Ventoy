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
void InitComboxCtrl(HWND hWnd, int PhyDrive)
{
    int SizeGB = 0;
	WPARAM n = 0;
	WPARAM nIndex = 0;
    DWORD i, j;
    HANDLE hCombox;
    CHAR Drive[16];
    CHAR Letter[128];
    CHAR DeviceName[256];

    hCombox = GetDlgItem(hWnd, IDC_COMBO1);

    // delete all items
    SendMessage(hCombox, CB_RESETCONTENT, 0, 0);
    
    //Fill device combox
    for (i = 0; i < g_PhyDriveCount; i++)
    {
        if (g_PhyDriveList[i].Id < 0)
        {
            continue;
        }

        if (g_PhyDriveList[i].DriveLetters[0])
        {
            safe_sprintf(Letter, "%C: ", g_PhyDriveList[i].DriveLetters[0]);
            for (j = 1; j < sizeof(g_PhyDriveList[i].DriveLetters) / sizeof(CHAR); j++)
            {
                if (g_PhyDriveList[i].DriveLetters[j] == 0)
                {
                    break;
                }
                safe_sprintf(Drive, "%C: ", g_PhyDriveList[i].DriveLetters[j]);
                strcat_s(Letter, sizeof(Letter), Drive);
            }
        }
        else
        {
            Letter[0] = 0;
        }

        SizeGB = GetHumanReadableGBSize(g_PhyDriveList[i].SizeInBytes);

        if ((SizeGB % 1024) == 0)
        {
            safe_sprintf(DeviceName, "%s[%dTB] %s %s",
                Letter,
                SizeGB / 1024,
                g_PhyDriveList[i].VendorId,
                g_PhyDriveList[i].ProductId
            );
        }
        else
        {
            safe_sprintf(DeviceName, "%s[%dGB] %s %s",
                Letter,
                SizeGB,
                g_PhyDriveList[i].VendorId,
                g_PhyDriveList[i].ProductId
            );
        }
        
        SendMessageA(hCombox, CB_ADDSTRING, 0, (LPARAM)DeviceName);

		if (g_PhyDriveList[i].PhyDrive == PhyDrive)
		{
			nIndex = n;
		}
		n++;
    }

	SendMessage(hCombox, CB_SETCURSEL, nIndex, 0);
    OnComboxSelChange(g_ComboxHwnd);
}

HWND CreateClusterToolTip(int toolID, HWND hDlg, PTSTR pszText)
{
    if (!toolID || !hDlg || !pszText)
    {
        return NULL;
    }

    // Get the window of the tool.
    HWND hwndTool = GetDlgItem(hDlg, toolID);

    // Create the tooltip. g_hInst is the global instance handle.
    HWND hwndTip = CreateWindowEx(0, TOOLTIPS_CLASS, NULL,
        WS_POPUP | TTS_ALWAYSTIP | TTS_BALLOON,
        CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT,
        hDlg, NULL,
        g_hInst, NULL);

    if (!hwndTool || !hwndTip)
    {
        return (HWND)NULL;
    }

    // Associate the tooltip with the tool.
    TOOLINFO toolInfo = { 0 };
    toolInfo.cbSize = sizeof(toolInfo);
    toolInfo.hwnd = hDlg;
    toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    toolInfo.uId = (UINT_PTR)hwndTool;
    toolInfo.lpszText = pszText;

    SendMessage(hwndTip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);
    //SendMessage(hwndTip, TTM_ACTIVATE, TRUE, NULL);

    return hwndTip;
}

BOOL InitDialog(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	//HFONT hStyleFont;
    HFONT hDlgFont;
    HFONT hDlgBoldFont;
    HFONT hStaticFont;	
    HICON hIcon;
    HICON hSecureIcon;
    CHAR WinText[128];
    CHAR WinPath[MAX_PATH] = { 0 };

    g_DialogHwnd = hWnd;
    g_ComboxHwnd = GetDlgItem(hWnd, IDC_COMBO1);
    g_StaticLocalVerHwnd = GetDlgItem(hWnd, IDC_STATIC_LOCAL_VER);
    g_StaticDiskVerHwnd = GetDlgItem(hWnd, IDC_STATIC_DISK_VER);
	g_StaticLocalStyleHwnd = GetDlgItem(hWnd, IDC_STATIC_LOCAL_STYLE);
	g_StaticDiskStyleHwnd = GetDlgItem(hWnd, IDC_STATIC_DEV_STYLE);
    g_StaticLocalFsHwnd = GetDlgItem(hWnd, IDC_STATIC_LOCAL_FS);
    g_StaticDevFsHwnd = GetDlgItem(hWnd, IDC_STATIC_DEV_FS);

    g_BtnInstallHwnd = GetDlgItem(hWnd, IDC_BUTTON4);

	g_StaticDevHwnd = GetDlgItem(hWnd, IDC_STATIC_DEV);
	g_StaticLocalHwnd = GetDlgItem(hWnd, IDC_STATIC_LOCAL);
	g_StaticDiskHwnd = GetDlgItem(hWnd, IDC_STATIC_DISK);

    g_LocalClusterTipHwnd = CreateClusterToolTip(IDC_STATIC_LOCAL_FS, hWnd, L"");
    g_DiskClusterTipHwnd = CreateClusterToolTip(IDC_STATIC_DEV_FS, hWnd, L"");

    g_LocalIconSecureHwnd = GetDlgItem(hWnd, IDC_ICON_LOCAL_SECURE);
    g_DiskIconSecureHwnd = GetDlgItem(hWnd, IDC_ICON_DISK_SECURE);

    hSecureIcon = (HICON)LoadImage(g_hInst, MAKEINTRESOURCE(IDI_ICON4), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    SendMessage(g_LocalIconSecureHwnd, STM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hSecureIcon);
    SendMessage(g_DiskIconSecureHwnd, STM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hSecureIcon);
    ShowWindow(g_LocalIconSecureHwnd, SW_HIDE);
    ShowWindow(g_DiskIconSecureHwnd, SW_HIDE);


    g_BtnUpdateHwnd = GetDlgItem(hWnd, IDC_BUTTON3);
    g_ProgressBarHwnd = GetDlgItem(hWnd, IDC_PROGRESS1);
    g_StaticStatusHwnd = GetDlgItem(hWnd, IDC_STATIC_STATUS);

    hIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_ICON1));
    SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);


    SendDlgItemMessage(hWnd, IDC_COMMAND1, BM_SETIMAGE, IMAGE_ICON, (LPARAM)LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_ICON2)));

    SendMessage(g_ProgressBarHwnd, PBM_SETRANGE, (WPARAM)0, (LPARAM)(MAKELPARAM(0, PT_FINISH)));
    PROGRESS_BAR_SET_POS(PT_START);

	SetMenu(hWnd, LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_MENU1)));

    LanguageInit();

    sprintf_s(WinText, sizeof(WinText), "Ventoy2Disk  %s", current_arch_string());
    SetWindowTextA(hWnd, WinText);

    // Set static text & font 
    hStaticFont = CreateFont(26, 0, 0, 0, FW_BOLD, FALSE, FALSE, 0,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH&FF_SWISS, TEXT("Courier New"));
	SendMessage(g_StaticLocalVerHwnd, WM_SETFONT, (WPARAM)hStaticFont, TRUE);
	SendMessage(g_StaticDiskVerHwnd, WM_SETFONT, (WPARAM)hStaticFont, TRUE);

#if 0
	hStyleFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, 0,
		ANSI_CHARSET, OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
		DEFAULT_PITCH&FF_SWISS, TEXT("Courier New"));
	SendMessage(g_StaticLocalStyleHwnd, WM_SETFONT, (WPARAM)hStyleFont, TRUE);
	SendMessage(g_StaticDiskStyleHwnd, WM_SETFONT, (WPARAM)hStyleFont, TRUE);
#endif

    SetWindowTextA(g_StaticLocalFsHwnd, GetVentoyFsName());
    UpdateClusterTipMsg(IDC_STATIC_LOCAL_FS, hWnd, g_LocalClusterTipHwnd, GetClusterSizeTip());

    InitComboxCtrl(hWnd, -1);

    SetFocus(g_ProgressBarHwnd);


    GetEnvironmentVariableA("SystemRoot", WinPath, MAX_PATH);
    strcat_s(WinPath, MAX_PATH, "\\Fonts\\couri.ttf");

    if (IsFileExist(WinPath))
    {
        Log("Courier New font <%s> exist OK.", WinPath);
    }
    else
    {
        Log("Courier New font <%s> does NOT exist.", WinPath);

        hDlgFont = CreateFont(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, 0,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
            DEFAULT_PITCH & FF_SWISS, TEXT("Microsoft Yahe"));

        hDlgBoldFont = CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, 0,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
            DEFAULT_PITCH & FF_SWISS, TEXT("Microsoft Yahe"));

        SendMessage(hWnd, WM_SETFONT, (WPARAM)hDlgFont, TRUE);
        SendMessage(g_StaticLocalStyleHwnd, WM_SETFONT, (WPARAM)hDlgFont, TRUE);
        SendMessage(g_StaticDiskStyleHwnd, WM_SETFONT, (WPARAM)hDlgFont, TRUE);
        SendMessage(g_StaticLocalFsHwnd, WM_SETFONT, (WPARAM)hDlgFont, TRUE);
        SendMessage(g_StaticDevFsHwnd, WM_SETFONT, (WPARAM)hDlgFont, TRUE);
        SendMessage(g_ComboxHwnd, WM_SETFONT, (WPARAM)hDlgFont, TRUE);

        SendMessage(g_BtnInstallHwnd, WM_SETFONT, (WPARAM)hDlgBoldFont, TRUE);
        SendMessage(g_BtnUpdateHwnd, WM_SETFONT, (WPARAM)hDlgBoldFont, TRUE);

        SendMessage(GetDlgItem(hWnd, IDC_SYSLINK1), WM_SETFONT, (WPARAM)hDlgFont, TRUE);
        SendMessage(GetDlgItem(hWnd, IDC_SYSLINK2), WM_SETFONT, (WPARAM)hDlgFont, TRUE);
    }

    AlertSuppressInit();

    GetWindowRect(hWnd, &m_WindowRect);
    m_MinWidth = m_WindowRect.right - m_WindowRect.left;
    m_MinHeight = m_WindowRect.bottom - m_WindowRect.top;

    return TRUE;
}

