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

DWORD WINAPI InstallVentoyThread(void* Param)
{
    int rc;
    int TryId = 1;
    PHY_DRIVE_INFO *pPhyDrive = (PHY_DRIVE_INFO *)Param;

    SetAlertPromptHookEnable(TRUE);

    if (g_WriteImage)
    {
        rc = InstallVentoy2FileImage(pPhyDrive, g_cur_part_style);
    }
    else
    {
        if (!VentoyPhydriveMatch(pPhyDrive))
        {
            rc = 1;
            goto out;
        }

        rc = InstallVentoy2PhyDrive(pPhyDrive, g_cur_part_style, TryId++);
        if (rc)
        {
            if (!VentoyPhydriveMatch(pPhyDrive))
            {
                goto out;
            }

            Log("This time install failed, clean disk by disk, wait 5s and retry...");
			DISK_CleanDisk(pPhyDrive->PhyDrive);

            Sleep(5000);

            Log("Now retry to install...");
            rc = InstallVentoy2PhyDrive(pPhyDrive, g_cur_part_style, TryId++);

            if (rc)
            {
                if (!VentoyPhydriveMatch(pPhyDrive))
                {
                    goto out;
                }

                Log("This time install failed, clean disk by diskpart, wait 10s and retry...");
                DSPT_CleanDisk(pPhyDrive->PhyDrive);

                Sleep(10000);

                Log("Now retry to install...");
                rc = InstallVentoy2PhyDrive(pPhyDrive, g_cur_part_style, TryId++);
            }
        }
    }

out:
    if (rc == 0)
    {
		PROGRESS_BAR_SET_POS(PT_FINISH);
        MessageBox(g_DialogHwnd, g_WriteImage ? _G(STR_VTSI_CREATE_SUCCESS) : _G(STR_INSTALL_SUCCESS), _G(STR_INFO), MB_OK | MB_ICONINFORMATION);

        if (g_WriteImage == 0)
        {
            safe_strcpy(pPhyDrive->VentoyVersion, GetLocalVentoyVersion());
            safe_strcpy(pPhyDrive->VentoyFsType, GetVentoyFsName());
            pPhyDrive->PartStyle = g_cur_part_style;
            pPhyDrive->SecureBootSupport = g_SecureBoot;
        }
    }
    else
    {
		PROGRESS_BAR_SET_POS(PT_FINISH);
        MessageBox(g_DialogHwnd, g_WriteImage ? _G(STR_VTSI_CREATE_FAILED) : _G(STR_INSTALL_FAILED), _G(STR_ERROR), MB_OK | MB_ICONERROR);
    }
    
	PROGRESS_BAR_SET_POS(PT_START);
    g_ThreadHandle = NULL;
    SetWindowText(g_StaticStatusHwnd, _G(STR_STATUS));
    OnComboxSelChange(g_ComboxHwnd);

    SetWindowText(GetDlgItem(g_DialogHwnd, IDC_STATIC_LOCAL), _G(STR_LOCAL_VER));
    SetWindowText(GetDlgItem(g_DialogHwnd, IDC_STATIC_DISK), _G(STR_DISK_VER));

    SetAlertPromptHookEnable(FALSE);

    return 0;
}

DWORD WINAPI ClearVentoyThread(void* Param)
{
    int rc;
    UINT Drive = 0;
    CHAR DrvLetter = 0;
    PHY_DRIVE_INFO *pPhyDrive = (PHY_DRIVE_INFO *)Param;

    rc = ClearVentoyFromPhyDrive(g_DialogHwnd, pPhyDrive, &DrvLetter);
    if (rc)
    {
        Log("This time clear failed, now wait and retry...");
        Sleep(10000);

        Log("Now retry to clear...");

        rc = ClearVentoyFromPhyDrive(g_DialogHwnd, pPhyDrive, &DrvLetter);
    }

    if (rc == 0)
    {
        PROGRESS_BAR_SET_POS(PT_FINISH);
        MessageBox(g_DialogHwnd, _G(STR_CLEAR_SUCCESS), _G(STR_INFO), MB_OK | MB_ICONINFORMATION);
        safe_strcpy(pPhyDrive->VentoyVersion, "");
        safe_strcpy(pPhyDrive->VentoyFsType, "");
        pPhyDrive->VentoyFsClusterSize = 0;
    }
    else
    {
        PROGRESS_BAR_SET_POS(PT_FINISH);
        MessageBox(g_DialogHwnd, _G(STR_CLEAR_FAILED), _G(STR_ERROR), MB_OK | MB_ICONERROR);
    }

    PROGRESS_BAR_SET_POS(PT_START);
    g_ThreadHandle = NULL;
    SetWindowText(g_StaticStatusHwnd, _G(STR_STATUS));
    OnComboxSelChange(g_ComboxHwnd);

    SetWindowText(GetDlgItem(g_DialogHwnd, IDC_STATIC_LOCAL), _G(STR_LOCAL_VER));
    SetWindowText(GetDlgItem(g_DialogHwnd, IDC_STATIC_DISK), _G(STR_DISK_VER));

    if (rc == 0 && DrvLetter > 0)
    {
        if (DrvLetter >= 'A' && DrvLetter <= 'Z')
        {
            Drive = DrvLetter - 'A';
        }
        else if (DrvLetter >= 'a' && DrvLetter <= 'z')
        {
            Drive = DrvLetter - 'a';
        }

        if (Drive > 0)
        {
            //SHFormatDrive(g_DialogHwnd, Drive, SHFMT_ID_DEFAULT, SHFMT_OPT_FULL);
        }
    }

    return 0;
}


DWORD WINAPI UpdateVentoyThread(void* Param)
{
    int rc;
    int TryId = 1;
    PHY_DRIVE_INFO *pPhyDrive = (PHY_DRIVE_INFO *)Param;

    rc = UpdateVentoy2PhyDrive(pPhyDrive, TryId++);
	if (rc)
	{
		Log("This time update failed, now wait and retry...");
		Sleep(4000);

		//Try2
		Log("Now retry to update...");
        rc = UpdateVentoy2PhyDrive(pPhyDrive, TryId++);
		if (rc)
		{
			//Try3
			Sleep(1000);
			Log("Now retry to update...");
			rc = UpdateVentoy2PhyDrive(pPhyDrive, TryId++);
			if (rc)
			{
				//Try4 is dangerous ...
				Sleep(3000);
				Log("Now retry to update...");
				rc = UpdateVentoy2PhyDrive(pPhyDrive, TryId++);
			}
		}
	}

    if (rc == 0)
    {
		PROGRESS_BAR_SET_POS(PT_FINISH);
        MessageBox(g_DialogHwnd, _G(STR_UPDATE_SUCCESS), _G(STR_INFO), MB_OK | MB_ICONINFORMATION);
        safe_strcpy(pPhyDrive->VentoyVersion, GetLocalVentoyVersion());
        pPhyDrive->SecureBootSupport = g_SecureBoot;
    }
    else
    {
		PROGRESS_BAR_SET_POS(PT_FINISH);
        MessageBox(g_DialogHwnd, _G(STR_UPDATE_FAILED), _G(STR_ERROR), MB_OK | MB_ICONERROR);
    }
    
	PROGRESS_BAR_SET_POS(PT_START);
    g_ThreadHandle = NULL;
    SetWindowText(g_StaticStatusHwnd, _G(STR_STATUS));
    OnComboxSelChange(g_ComboxHwnd);

    SetWindowText(GetDlgItem(g_DialogHwnd, IDC_STATIC_LOCAL), _G(STR_LOCAL_VER));
    SetWindowText(GetDlgItem(g_DialogHwnd, IDC_STATIC_DISK), _G(STR_DISK_VER));

    return 0;
}


DWORD WINAPI PartResizeThread(void* Param)
{
	int rc;
    PHY_DRIVE_INFO* pPhyDrive = (PHY_DRIVE_INFO*)Param;

	rc = PartitionResizeForVentoy(pPhyDrive);
	if (rc == 0)
    {
        PROGRESS_BAR_SET_POS(PT_FINISH);
        MessageBox(g_DialogHwnd, _G(STR_PART_RESIZE_SUCCESS), _G(STR_INFO), MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        PROGRESS_BAR_SET_POS(PT_FINISH);
        MessageBox(g_DialogHwnd, _G(STR_PART_RESIZE_FAILED), _G(STR_ERROR), MB_OK | MB_ICONERROR);
    }

    PROGRESS_BAR_SET_POS(PT_START);
    g_ThreadHandle = NULL;
    SetWindowText(g_StaticStatusHwnd, _G(STR_STATUS));

    OnComboxSelChange(g_ComboxHwnd);

    return 0;
}

void OnInstallBtnClick(void)
{
    int nCurSel;
	int SpaceMB = 0;
	int SizeInMB = 0;
    PHY_DRIVE_INFO *pPhyDrive = NULL;

    if (g_WriteImage)
    {
        if (MessageBox(g_DialogHwnd, _G(STR_VTSI_CREATE_TIP), _G(STR_INFO), MB_YESNO | MB_ICONINFORMATION | MB_DEFBUTTON2) != IDYES)
        {
            return;
        }
    }
    else
    {
        if ((g_NoNeedInputYes == 0) && IsWindowEnabled(g_BtnUpdateHwnd))
        {
            DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG3), NULL, YesDialogProc);
            if (!g_InputYes)
            {
                return;
            }
        }

        if (MessageBox(g_DialogHwnd, _G(STR_INSTALL_TIP), _G(STR_WARNING), MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
        {
            return;
        }

        if (MessageBox(g_DialogHwnd, _G(STR_INSTALL_TIP2), _G(STR_WARNING), MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
        {
            return;
        }
    }

    if (g_ThreadHandle)
    {
        Log("Another thread is runing");
        return;
    }

    nCurSel = (int)SendMessage(g_ComboxHwnd, CB_GETCURSEL, 0, 0);
    if (CB_ERR == nCurSel)
    {
        Log("Failed to get combox sel");
        return;;
    }

    pPhyDrive = GetPhyDriveInfoById(nCurSel);
    if (!pPhyDrive)
    {
        return;
    }

	if (g_cur_part_style == 0 && pPhyDrive->SizeInBytes > 2199023255552ULL)
	{
		MessageBox(g_DialogHwnd, _G(STR_DISK_2TB_MBR_ERROR), _G(STR_ERROR), MB_OK | MB_ICONERROR);
		return;
	}

    if (pPhyDrive->BytesPerLogicalSector == 4096 && pPhyDrive->BytesPerPhysicalSector == 4096)
    {
        MessageBox(g_DialogHwnd, _G(STR_4KN_UNSUPPORTED), _G(STR_ERROR), MB_OK | MB_ICONERROR);
        return;
    }


	SpaceMB = GetReservedSpaceInMB();
	SizeInMB = (int)(pPhyDrive->SizeInBytes / 1024 / 1024);
	Log("SpaceMB:%d SizeInMB:%d", SpaceMB, SizeInMB);

	if (SizeInMB <= SpaceMB || (SizeInMB - SpaceMB) <= (VENTOY_EFI_PART_SIZE / SIZE_1MB))
	{
		MessageBox(g_DialogHwnd, _G(STR_SPACE_VAL_INVALID), _G(STR_ERROR), MB_OK | MB_ICONERROR);
		Log("Invalid space value ...");
		return;
	}

    EnableWindow(g_BtnInstallHwnd, FALSE);
    EnableWindow(g_BtnUpdateHwnd, FALSE);

    g_ThreadHandle = CreateThread(NULL, 0, InstallVentoyThread, (LPVOID)pPhyDrive, 0, NULL);
}

void OnRefreshBtnClick(HWND hWnd)
{
	int nCurSel;
	int PhyDrive = -1;
	PHY_DRIVE_INFO *pPhyDrive = NULL;

    Log("#### Now Refresh PhyDrive ####");

	nCurSel = (int)SendMessage(g_ComboxHwnd, CB_GETCURSEL, 0, 0);
	if (CB_ERR != nCurSel)
	{
		pPhyDrive = GetPhyDriveInfoById(nCurSel);
		if (pPhyDrive)
		{
			PhyDrive = pPhyDrive->PhyDrive;
			Log("Current combox selection is PhyDrive%d", PhyDrive);
		}
	}

    Ventoy2DiskDestroy();
    Ventoy2DiskInit();
	InitComboxCtrl(hWnd, PhyDrive);
}

void OnUpdateBtnClick(void)
{
    int nCurSel;
    PHY_DRIVE_INFO *pPhyDrive = NULL;

    if (MessageBox(g_DialogHwnd, _G(STR_UPDATE_TIP), _G(STR_INFO), MB_YESNO | MB_ICONQUESTION) != IDYES)
    {
        return;
    }

    if (g_ThreadHandle)
    {
        Log("Another thread is runing");
        return;
    }

    nCurSel = (int)SendMessage(g_ComboxHwnd, CB_GETCURSEL, 0, 0);
    if (CB_ERR == nCurSel)
    {
        Log("Failed to get combox sel");
        return;;
    }

    pPhyDrive = GetPhyDriveInfoById(nCurSel);
    if (!pPhyDrive)
    {
        return;
    }

    EnableWindow(g_BtnInstallHwnd, FALSE);
    EnableWindow(g_BtnUpdateHwnd, FALSE);

    g_ThreadHandle = CreateThread(NULL, 0, UpdateVentoyThread, (LPVOID)pPhyDrive, 0, NULL);
}

