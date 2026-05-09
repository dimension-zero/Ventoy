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
void MenuProc(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	WORD CtrlID;
    HMENU SubMenu;
	HMENU hMenu = GetMenu(hWnd);

	CtrlID = LOWORD(wParam);

	if (CtrlID == 0)
	{
		g_SecureBoot = !g_SecureBoot;

		if (g_SecureBoot)
		{
            ShowWindow(g_LocalIconSecureHwnd, SW_NORMAL);
			CheckMenuItem(hMenu, 0, MF_BYCOMMAND | MF_STRING | MF_CHECKED);
		}
		else
		{
            ShowWindow(g_LocalIconSecureHwnd, SW_HIDE);
			CheckMenuItem(hMenu, 0, MF_BYCOMMAND | MF_STRING | MF_UNCHECKED);
		}
	}
    else if (CtrlID == VTOY_MENU_PART_CFG)
    {
        DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG2), hWnd, PartDialogProc);
		UpdateReservedPostfix();
        SetWindowTextA(g_StaticLocalFsHwnd, GetVentoyFsName());
        UpdateClusterTipMsg(IDC_STATIC_LOCAL_FS, hWnd, g_LocalClusterTipHwnd, GetClusterSizeTip());
    }
    else if (CtrlID == VTOY_MENU_CLEAN)
    {
        OnClearVentoy();
    }
    else if (CtrlID == VTOY_MENU_PART_RESIZE)
    {
        OnPartResize();
    }
#if VTSI_SUPPORT  
    else if (CtrlID == VTOY_MENU_VTSI)
    {
        SubMenu = GetSubMenu(hMenu, 0);

        g_WriteImage = 1 - g_WriteImage;
        if (g_WriteImage == 1)
        {
            ModifyMenu(SubMenu, OPT_SUBMENU_VTSI, MF_STRING | MF_BYPOSITION | MF_CHECKED, VTOY_MENU_VTSI, _G(STR_MENU_VTSI_CREATE));
        }
        else
        {
            ModifyMenu(SubMenu, OPT_SUBMENU_VTSI, MF_STRING | MF_BYPOSITION | MF_UNCHECKED, VTOY_MENU_VTSI, _G(STR_MENU_VTSI_CREATE));
        }
    }
#endif
    else if (CtrlID == VTOY_MENU_ALL_DEV)
    {
        SubMenu = GetSubMenu(hMenu, 0);

        g_FilterUSB = 1 - g_FilterUSB;
        if (g_FilterUSB == 0)
        {
            ModifyMenu(SubMenu, OPT_SUBMENU_ALL_DEV, MF_STRING | MF_BYPOSITION | MF_CHECKED, VTOY_MENU_ALL_DEV, _G(STR_SHOW_ALL_DEV));
        }
        else
        {
            ModifyMenu(SubMenu, OPT_SUBMENU_ALL_DEV, MF_STRING | MF_BYPOSITION | MF_UNCHECKED, VTOY_MENU_ALL_DEV, _G(STR_SHOW_ALL_DEV));
        }

        OnRefreshBtnClick(hWnd);
    }
    else if (CtrlID == ID_PARTSTYLE_MBR)
    {
        CheckMenuItem(hMenu, (UINT)ID_PARTSTYLE_MBR, MF_BYCOMMAND | MF_CHECKED);
        CheckMenuItem(hMenu, (UINT)ID_PARTSTYLE_GPT, MF_BYCOMMAND | MF_UNCHECKED);
        g_cur_part_style = 0;
        UpdateLocalVentoyVersion();
        ShowWindow(g_DialogHwnd, SW_HIDE);
        ShowWindow(g_DialogHwnd, SW_NORMAL);
    }
    else if (CtrlID == ID_PARTSTYLE_GPT)
    {
        CheckMenuItem(hMenu, (UINT)ID_PARTSTYLE_MBR, MF_BYCOMMAND | MF_UNCHECKED);
        CheckMenuItem(hMenu, (UINT)ID_PARTSTYLE_GPT, MF_BYCOMMAND | MF_CHECKED);
        g_cur_part_style = 1;
        UpdateLocalVentoyVersion();
        ShowWindow(g_DialogHwnd, SW_HIDE);
        ShowWindow(g_DialogHwnd, SW_NORMAL);
    }
	else if (CtrlID >= VTOY_MENU_LANGUAGE_BEGIN && CtrlID < VTOY_MENU_LANGUAGE_BEGIN + g_language_count)
	{
		UpdateItemString(CtrlID - VTOY_MENU_LANGUAGE_BEGIN);
	}
}

int ExpandDlg(HWND hParent, UINT uiID, int WidthDelta)
{
    HWND hWnd; 
    RECT ret;
    POINT pt1, pt2;

    if (hParent)
    {
        hWnd = GetDlgItem(hParent, uiID);
        GetWindowRect(hWnd, &ret);

        pt1.x = ret.left;
        pt1.y = ret.top;
        pt2.x = ret.right;
        pt2.y = ret.bottom;

        ScreenToClient(hParent, &pt1);
        ScreenToClient(hParent, &pt2);

        MoveWindow(hWnd, pt1.x, pt1.y, pt2.x - pt1.x + WidthDelta, pt2.y - pt1.y, TRUE);
    }

    return 0;
}


int MoveDlg(HWND hParent, UINT uiID, int WidthDelta)
{
    HWND hWnd;
    RECT ret;
    POINT pt1, pt2;

    if (hParent)
    {
        hWnd = GetDlgItem(hParent, uiID);
        GetWindowRect(hWnd, &ret);

        pt1.x = ret.left;
        pt1.y = ret.top;
        pt2.x = ret.right;
        pt2.y = ret.bottom;

        ScreenToClient(hParent, &pt1);
        ScreenToClient(hParent, &pt2);

        MoveWindow(hWnd, pt1.x + WidthDelta, pt1.y, pt2.x - pt1.x, pt2.y - pt1.y, TRUE);
    }

    return 0;
}

INT_PTR CALLBACK DialogProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
    WORD NotifyCode;
    WORD CtrlID;

    switch (Message)
    {
        case WM_SIZE:
        {
            //判断窗口是不是最小化了，因为窗口最小化之后 ，窗口的长和宽会变成0，当前一次变化的时就会出现除以0的错误操作
            if (wParam != SIZE_MINIMIZED && m_MinWidth > 0)
            {
                int WidthDelta;
                RECT CurRet;

                GetWindowRect(g_DialogHwnd, &CurRet);
                WidthDelta = (CurRet.right - CurRet.left) - (m_WindowRect.right - m_WindowRect.left);

                ExpandDlg(g_DialogHwnd, IDC_BUTTON3, WidthDelta / 2);
                ExpandDlg(g_DialogHwnd, IDC_BUTTON4, WidthDelta / 2);
                MoveDlg(g_DialogHwnd, IDC_BUTTON3, WidthDelta / 2);
                MoveDlg(g_DialogHwnd, IDC_SYSLINK1, WidthDelta);
                ExpandDlg(g_DialogHwnd, IDC_STATIC_STATUS, WidthDelta);
                ExpandDlg(g_DialogHwnd, IDC_PROGRESS1, WidthDelta);
                
                ExpandDlg(g_DialogHwnd, IDC_STATIC_LOCAL, WidthDelta / 2);
                ExpandDlg(g_DialogHwnd, IDC_STATIC_DISK, WidthDelta / 2);
                MoveDlg(g_DialogHwnd, IDC_STATIC_DISK, WidthDelta / 2);
                MoveDlg(g_DialogHwnd, IDC_ICON_DISK_SECURE, WidthDelta / 2);
                MoveDlg(g_DialogHwnd, IDC_STATIC_DISK_VER, WidthDelta / 2);

                MoveDlg(g_DialogHwnd, IDC_STATIC_LOCAL_FS, WidthDelta / 2);
                MoveDlg(g_DialogHwnd, IDC_STATIC_LOCAL_STYLE, WidthDelta / 2);
                MoveDlg(g_DialogHwnd, IDC_STATIC_DEV_FS, WidthDelta);
                MoveDlg(g_DialogHwnd, IDC_STATIC_DEV_STYLE, WidthDelta);
                
                ExpandDlg(g_DialogHwnd, IDC_STATIC_DEV, WidthDelta);
                ExpandDlg(g_DialogHwnd, IDC_COMBO1, WidthDelta);
                MoveDlg(g_DialogHwnd, IDC_COMMAND1, WidthDelta);
                

                GetWindowRect(g_DialogHwnd, &m_WindowRect);   //最后要更新对话框的大小，当做下一次变化的旧坐标；
                InvalidateRect(g_DialogHwnd, &m_WindowRect, TRUE);
            }
            break;
        }
        case WM_GETMINMAXINFO:
        {
            MINMAXINFO* lpMMI = (MINMAXINFO*)lParam;
            if (m_MinWidth > 0)
            {
                lpMMI->ptMinTrackSize.x = m_MinWidth;   //x宽度   

                //高度最大和最小设置成一样，即禁止上下拖动大小
                lpMMI->ptMinTrackSize.y = m_MinHeight;   //y高度   
                lpMMI->ptMaxTrackSize.y = m_MinHeight;   //y高度   
            }
            break;
        }
		case WM_NOTIFY:
		{
			UINT code = 0;
			UINT_PTR idFrom = 0;

			if (lParam)
			{
				code = ((LPNMHDR)lParam)->code;
				idFrom = ((LPNMHDR)lParam)->idFrom;
			}
			
			if (idFrom == IDC_SYSLINK1 && (NM_CLICK == code || NM_RETURN == code))
			{
				ShellExecute(NULL, L"open", L"https://www.ventoy.net", NULL, NULL, SW_SHOW);
			}

            if (idFrom == IDC_SYSLINK2 && (NM_CLICK == code || NM_RETURN == code))
            {
                if (g_cur_lang_id == 0)
                {
                    ShellExecute(NULL, L"open", L"https://www.ventoy.net/cn/donation.html", NULL, NULL, SW_SHOW);
                }
                else
                {
                    ShellExecute(NULL, L"open", L"https://www.ventoy.net/en/donation.html", NULL, NULL, SW_SHOW);
                }
            }
			break;
		}
        case WM_COMMAND:
        {
            NotifyCode = HIWORD(wParam);
            CtrlID = LOWORD(wParam);

            if (CtrlID == IDC_COMBO1 && NotifyCode == CBN_SELCHANGE)
            {
                OnComboxSelChange((HWND)lParam);
            }

            if (CtrlID == IDC_BUTTON4 && NotifyCode == BN_CLICKED)
            {
                OnInstallBtnClick();
            }
            else if (CtrlID == IDC_BUTTON3 && NotifyCode == BN_CLICKED)
            {
                OnUpdateBtnClick();
            }
            else if (CtrlID == IDC_COMMAND1 && NotifyCode == BN_CLICKED)
            {
                OnRefreshBtnClick(hWnd);
            }

			if (lParam == 0 && NotifyCode == 0)
			{
				MenuProc(hWnd, wParam, lParam);
			}

            break;
        }
        case WM_INITDIALOG:
        {
            InitDialog(hWnd, wParam, lParam);
            break;
        }
        case WM_CTLCOLORSTATIC:
        {
            if (GetDlgItem(hWnd, IDC_STATIC_LOCAL_VER) == (HANDLE)lParam)
            {
                SetBkMode((HDC)wParam, TRANSPARENT);
                SetTextColor((HDC)wParam, RGB(255, 0, 0));
                return (LRESULT)(HBRUSH)(GetStockObject(HOLLOW_BRUSH));
            }
            else if (GetDlgItem(hWnd, IDC_STATIC_DISK_VER) == (HANDLE)lParam)
            {
                SetBkMode((HDC)wParam, TRANSPARENT);
                SetTextColor((HDC)wParam, g_InvalidClusterSize ? RGB(0, 0, 255) : RGB(255, 0, 0));
                return (LRESULT)(HBRUSH)(GetStockObject(HOLLOW_BRUSH));
            }
            
#if 0
            else if (GetDlgItem(hWnd, IDC_STATIC_LOCAL_SECURE) == (HANDLE)lParam ||
                GetDlgItem(hWnd, IDC_STATIC_DEV_SECURE) == (HANDLE)lParam)
			{
				SetBkMode((HDC)wParam, TRANSPARENT);
                SetTextColor((HDC)wParam, RGB(0xea, 0x99, 0x1f));
				return (LRESULT)(HBRUSH)(GetStockObject(HOLLOW_BRUSH));
			}
#endif
            else
            {
                break;
            }
        }
        case WM_CLOSE:
        {
            if (g_ThreadHandle)
            {
                MessageBox(g_DialogHwnd, _G(STR_WAIT_PROCESS), _G(STR_INFO), MB_OK | MB_ICONINFORMATION);
            }
            else
            {
                EndDialog(hWnd, 0);
            }
			WriteCfgIni();
            break;
        }
    }

    return 0;
}

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
