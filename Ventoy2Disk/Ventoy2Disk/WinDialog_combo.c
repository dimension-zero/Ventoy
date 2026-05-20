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
void OnComboxSelChange(HWND hCombox)
{
    int nCurSelected;
    PHY_DRIVE_INFO *CurDrive = NULL;
    HMENU SubMenu;    
    HMENU hMenu = GetMenu(g_DialogHwnd);
    WCHAR Tip[256];

    UpdateLocalVentoyVersion();
    SetWindowTextA(g_StaticDiskVerHwnd, ""); g_InvalidClusterSize = FALSE;
	SetWindowTextA(g_StaticDiskStyleHwnd, "");
    SetWindowTextA(g_StaticDevFsHwnd, "");    
    ShowWindow(g_DiskIconSecureHwnd, SW_HIDE);
    EnableWindow(g_BtnInstallHwnd, FALSE);
    EnableWindow(g_BtnUpdateHwnd, FALSE);
    UpdateClusterTipMsg(IDC_STATIC_DEV_FS, g_DialogHwnd, g_DiskClusterTipHwnd, L"");

    SubMenu = GetSubMenu(hMenu, 0);
    ModifyMenu(SubMenu, OPT_SUBMENU_CLEAR, MF_BYPOSITION | MF_STRING | MF_DISABLED, VTOY_MENU_CLEAN, _G(STR_MENU_CLEAR));    
    ModifyMenu(SubMenu, OPT_SUBMENU_PART_RESIZE, MF_BYPOSITION | MF_STRING | MF_DISABLED, VTOY_MENU_PART_RESIZE, _G(STR_MENU_PART_RESIZE));

    if (g_PhyDriveCount > 0)
    {
        nCurSelected = (int)SendMessage(hCombox, CB_GETCURSEL, 0, 0);
        if (CB_ERR != nCurSelected)
        {
            CurDrive = GetPhyDriveInfoById(nCurSelected);
        }
    }
    
    if (CurDrive)
    {
        ModifyMenu(SubMenu, OPT_SUBMENU_CLEAR, MF_BYPOSITION | MF_STRING | MF_ENABLED, VTOY_MENU_CLEAN, _G(STR_MENU_CLEAR));
        SetWindowTextA(g_StaticDiskVerHwnd, CurDrive->VentoyVersion); g_InvalidClusterSize = FALSE;
        SetWindowTextA(g_StaticDevFsHwnd, CurDrive->VentoyFsType);

        FormatClusterSizeTip(CurDrive->VentoyFsClusterSize, Tip, 256);
        UpdateClusterTipMsg(IDC_STATIC_DEV_FS, g_DialogHwnd, g_DiskClusterTipHwnd, Tip);

		if (CurDrive->VentoyVersion[0])
		{
            if (_stricmp(CurDrive->VentoyFsType, "EXFAT") == 0 ||
                _stricmp(CurDrive->VentoyFsType, "FAT32") == 0 ||
                _stricmp(CurDrive->VentoyFsType, "NTFS") == 0)
            {
                if (CurDrive->VentoyFsClusterSize < 2048)
                {
                    g_InvalidClusterSize = TRUE;
                }
            }

			SetWindowTextA(g_StaticDiskStyleHwnd, CurDrive->PartStyle ? "GPT" : "MBR");

            Log("Combox select change, update secure boot option: %u %u", g_SecureBoot, CurDrive->SecureBootSupport);
            g_SecureBoot = CurDrive->SecureBootSupport;

            if (g_SecureBoot)
            {
                ShowWindow(g_DiskIconSecureHwnd, SW_NORMAL);
                ShowWindow(g_LocalIconSecureHwnd, SW_NORMAL);

                CheckMenuItem(hMenu, 0, MF_BYCOMMAND | MF_STRING | MF_CHECKED);
            }
            else
            {
                ShowWindow(g_DiskIconSecureHwnd, SW_HIDE);
                ShowWindow(g_LocalIconSecureHwnd, SW_HIDE);

                CheckMenuItem(hMenu, 0, MF_BYCOMMAND | MF_STRING | MF_UNCHECKED);
            }
		}
		else
		{
            ModifyMenu(SubMenu, OPT_SUBMENU_PART_RESIZE, MF_BYPOSITION | MF_STRING | MF_ENABLED, VTOY_MENU_PART_RESIZE, _G(STR_MENU_PART_RESIZE));
			
            SetWindowTextA(g_StaticDiskStyleHwnd, "");
            SetWindowTextA(g_StaticDevFsHwnd, "");

            Log("Not ventoy disk, set secure boot option");
            g_SecureBoot = TRUE;            
            ShowWindow(g_DiskIconSecureHwnd, SW_HIDE);
            ShowWindow(g_LocalIconSecureHwnd, SW_NORMAL);
            CheckMenuItem(hMenu, 0, MF_BYCOMMAND | MF_STRING | MF_CHECKED);
		}
		
		
        if (g_ForceOperation == 0)
        {
            if (CurDrive->VentoyVersion[0])
            {
                //only can update
                EnableWindow(g_BtnInstallHwnd, FALSE);
                EnableWindow(g_BtnUpdateHwnd, TRUE);
            }
            else
            {
                //only can install
                EnableWindow(g_BtnInstallHwnd, TRUE);
                EnableWindow(g_BtnUpdateHwnd, FALSE);
            }
        }
        else
        {
            EnableWindow(g_BtnInstallHwnd, TRUE);
			if (CurDrive->VentoyVersion[0])
			{
				EnableWindow(g_BtnUpdateHwnd, TRUE);
			}
        }
    }
    
    InvalidateRect(g_DialogHwnd, NULL, TRUE);
    UpdateWindow(g_DialogHwnd);
}

void UpdateReservedPostfix(void)
{
	int Space = 0;
	WCHAR Buf[128] = { 0 };
	
	Space = GetReservedSpaceInMB();

	if (Space <= 0)
	{
		SetWindowText(GetDlgItem(g_DialogHwnd, IDC_STATIC_DEV), _G(STR_DEVICE));
	}
	else
	{
		if (Space % 1024 == 0)
		{
			wsprintf(Buf, L"%s  [ -%dGB ]", _G(STR_DEVICE), Space / 1024);
		}
		else
		{
			wsprintf(Buf, L"%s  [ -%dMB ]", _G(STR_DEVICE), Space);
		}
		SetWindowText(GetDlgItem(g_DialogHwnd, IDC_STATIC_DEV), Buf);
	}
}

void UpdateItemString(int defaultLangId)
{
	int i;
    UINT State;
	HMENU SubMenu;
	HFONT hLangFont, hBoldFont;
    WCHAR Str[256];
	HMENU hMenu = GetMenu(g_DialogHwnd);

	g_cur_lang_id = defaultLangId;
	g_cur_lang_data = g_language_data + defaultLangId;

	hBoldFont = hLangFont = CreateFont(g_language_data[defaultLangId].FontSize, 0, 0, 0, 700, FALSE, FALSE, 0,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
		DEFAULT_PITCH, g_language_data[defaultLangId].FontFamily);

	hLangFont = CreateFont(g_language_data[defaultLangId].FontSize, 0, 0, 0, 400, FALSE, FALSE, 0,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
		DEFAULT_PITCH, g_language_data[defaultLangId].FontFamily);

	SendMessage(g_BtnInstallHwnd, WM_SETFONT, (WPARAM)hBoldFont, TRUE);
	SendMessage(g_BtnUpdateHwnd, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

	SendMessage(g_StaticStatusHwnd, WM_SETFONT, (WPARAM)hLangFont, TRUE);
	SendMessage(g_StaticLocalHwnd, WM_SETFONT, (WPARAM)hLangFont, TRUE);
	SendMessage(g_StaticDiskHwnd, WM_SETFONT, (WPARAM)hLangFont, TRUE);
	SendMessage(g_StaticDevHwnd, WM_SETFONT, (WPARAM)hLangFont, TRUE);
	SendMessage(g_DialogHwnd, WM_SETFONT, (WPARAM)hLangFont, TRUE);

    SendMessage(GetDlgItem(g_DialogHwnd, IDC_SYSLINK2), WM_SETFONT, (WPARAM)hLangFont, TRUE);

    g_language_normal_font = hLangFont;
    g_language_bold_font = hBoldFont;

	ModifyMenu(hMenu, 0, MF_BYPOSITION | MF_STRING, 0, _G(STR_MENU_OPTION));

	UpdateReservedPostfix();

	SetWindowText(GetDlgItem(g_DialogHwnd, IDC_STATIC_LOCAL), _G(STR_LOCAL_VER));
	SetWindowText(GetDlgItem(g_DialogHwnd, IDC_STATIC_DISK), _G(STR_DISK_VER));
	SetWindowText(g_StaticStatusHwnd, _G(STR_STATUS));

	SetWindowText(g_BtnInstallHwnd, _G(STR_INSTALL));
	SetWindowText(g_BtnUpdateHwnd, _G(STR_UPDATE));

    swprintf_s(Str, 200, L"<a>%ls</a>", _G(STR_DONATE));
    SetWindowText(GetDlgItem(g_DialogHwnd, IDC_SYSLINK2), Str);

	SubMenu = GetSubMenu(hMenu, 0);
	if (g_SecureBoot)
	{
        ShowWindow(g_LocalIconSecureHwnd, SW_NORMAL);
		ModifyMenu(SubMenu, OPT_SUBMENU_SECURE_BOOT, MF_BYPOSITION | MF_STRING | MF_CHECKED, 0, _G(STR_MENU_SECURE_BOOT));
	}
	else
	{
        ShowWindow(g_LocalIconSecureHwnd, SW_HIDE);
        ModifyMenu(SubMenu, OPT_SUBMENU_SECURE_BOOT, MF_BYPOSITION | MF_STRING | MF_UNCHECKED, 0, _G(STR_MENU_SECURE_BOOT));
	}
    
    ModifyMenu(SubMenu, OPT_SUBMENU_PART_STYLE, MF_STRING | MF_BYPOSITION, VTOY_MENU_PART_STYLE, _G(STR_MENU_PART_STYLE));
    ModifyMenu(SubMenu, OPT_SUBMENU_PART_CFG, MF_STRING | MF_BYPOSITION, VTOY_MENU_PART_CFG, _G(STR_MENU_PART_CFG));

    State = GetMenuState(SubMenu, VTOY_MENU_CLEAN, MF_BYCOMMAND);
    if (State & MF_DISABLED)
    {
        ModifyMenu(SubMenu, OPT_SUBMENU_CLEAR, MF_STRING | MF_BYPOSITION | MF_DISABLED, VTOY_MENU_CLEAN, _G(STR_MENU_CLEAR));
    }
    else
    {
        ModifyMenu(SubMenu, OPT_SUBMENU_CLEAR, MF_STRING | MF_BYPOSITION, VTOY_MENU_CLEAN, _G(STR_MENU_CLEAR));
    }

    State = GetMenuState(SubMenu, VTOY_MENU_PART_RESIZE, MF_BYCOMMAND);
    if (State & MF_DISABLED)
    {
        ModifyMenu(SubMenu, OPT_SUBMENU_PART_RESIZE, MF_STRING | MF_BYPOSITION | MF_DISABLED, VTOY_MENU_PART_RESIZE, _G(STR_MENU_PART_RESIZE));
    }
    else
    {
        ModifyMenu(SubMenu, OPT_SUBMENU_PART_RESIZE, MF_STRING | MF_BYPOSITION, VTOY_MENU_PART_RESIZE, _G(STR_MENU_PART_RESIZE));
    }

    if (g_FilterUSB == 0)
    {
        ModifyMenu(SubMenu, OPT_SUBMENU_ALL_DEV, MF_STRING | MF_BYPOSITION | MF_CHECKED, VTOY_MENU_ALL_DEV, _G(STR_SHOW_ALL_DEV));
    }
    else
    {
        ModifyMenu(SubMenu, OPT_SUBMENU_ALL_DEV, MF_STRING | MF_BYPOSITION | MF_UNCHECKED, VTOY_MENU_ALL_DEV, _G(STR_SHOW_ALL_DEV));
    }

#if VTSI_SUPPORT
    if (g_WriteImage == 1)
    {
        ModifyMenu(SubMenu, OPT_SUBMENU_VTSI, MF_STRING | MF_BYPOSITION | MF_CHECKED, VTOY_MENU_VTSI, _G(STR_MENU_VTSI_CREATE));
    }
    else
    {
        ModifyMenu(SubMenu, OPT_SUBMENU_VTSI, MF_STRING | MF_BYPOSITION | MF_UNCHECKED, VTOY_MENU_VTSI, _G(STR_MENU_VTSI_CREATE));
    }
#endif

	ShowWindow(g_DialogHwnd, SW_HIDE);
	ShowWindow(g_DialogHwnd, SW_NORMAL);

	//Update check
	for (i = 0; i < g_language_count; i++)
	{
		CheckMenuItem(hMenu, VTOY_MENU_LANGUAGE_BEGIN | i, MF_BYCOMMAND | MF_STRING | MF_UNCHECKED);
	}
	CheckMenuItem(hMenu, VTOY_MENU_LANGUAGE_BEGIN | defaultLangId, MF_BYCOMMAND | MF_STRING | MF_CHECKED);
}

