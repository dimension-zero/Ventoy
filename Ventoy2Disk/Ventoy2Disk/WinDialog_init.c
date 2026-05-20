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
const char* current_arch_string(void)
{
#if (defined VTARCH_X86)
    return "X86";
#elif (defined VTARCH_X64)
    return "X64"; 
#elif (defined VTARCH_ARM)
    return "ARM";
#elif (defined VTARCH_ARM64)
    return "ARM64"; 
#else
    return "XXX";
#endif
}

int LoadCfgIni(void)
{
	int value;

    value = GetPrivateProfileInt(TEXT("Ventoy"), TEXT("PartStyle"), 0, VENTOY_CFG_INI);
    if (value == 1)
    {
        g_cur_part_style = 1;
    }

    value = GetPrivateProfileInt(TEXT("Ventoy"), TEXT("ShowAllDevice"), 0, VENTOY_CFG_INI);
    if (value == 1)
    {
        g_FilterUSB = 0;
    }

    value = GetPrivateProfileInt(TEXT("Ventoy"), TEXT("MainPartFs"), 0, VENTOY_CFG_INI);
    if (value == 1 || value == 2 || value == 3)
    {
        SetVentoyFsType(value);
        Log("Set Ventoy FS Type %d", value);
    }

    value = GetPrivateProfileInt(TEXT("Ventoy"), TEXT("ClusterSize"), -1, VENTOY_CFG_INI);
    if (value >= 0)
    {
        SetClusterSize(value);        
        Log("Set Ventoy FS ClusterSize %d", value);
    }

	return 0;
}

int WriteCfgIni(void)
{
    WCHAR *CfgBuf = NULL;
    WORD UTFHdr = 0xFEFF;
    int charcount = 0;
    FILE *fp = NULL;


    fopen_s(&fp, VENTOY_CFG_INI_A, "wb+");
    if (fp == NULL)
    {
        return 1;
    }

    CfgBuf = (WCHAR *)malloc(1024 * 64);
    if (CfgBuf == NULL)
    {
        fclose(fp);
        return 1;
    }

    charcount = swprintf_s(CfgBuf, 1024 * 64 / sizeof(WCHAR),
        L"[Ventoy]\r\n"
        L"Language=%s\r\n"
        L"PartStyle=%d\r\n"
        L"ShowAllDevice=%d\r\n"
        L"MainPartFs=%d\r\n"
        L"ClusterSize=%d\r\n"
        ,
        g_language_data[g_cur_lang_id].Name,
        g_cur_part_style,
        1 - g_FilterUSB,
        GetVentoyFsType(),
        GetClusterSize());

    fwrite(&UTFHdr, 1, sizeof(UTFHdr), fp);
    fwrite(CfgBuf, 1, charcount * sizeof(WCHAR), fp);
    fclose(fp);

    free(CfgBuf);

//	WritePrivateProfileString(TEXT("Ventoy"), TEXT("Language"), g_language_data[g_cur_lang_id].Name, VENTOY_CFG_INI);

//  swprintf_s(TmpBuf, 128, TEXT("%d"), g_SecureBoot);
//  WritePrivateProfileString(TEXT("Ventoy"), TEXT("SecureBoot"), TmpBuf, VENTOY_CFG_INI);

	return 0;
}

void SetProgressBarPos(int Pos)
{
    int i = 0;
    int flag = 0;
    int index = 0;
    const TCHAR* StrPos = NULL;
    const TCHAR* StrStatus = NULL;
    CHAR Ratio[64] = { 0 };
    WCHAR wRatio[256] = { 0 };

    if (g_CLI_Mode)
    {
        CLI_UpdatePercent(Pos);
        return;
    }

    if (Pos >= PT_FINISH)
    {
        Pos = PT_FINISH;
    }

    SendMessage(g_ProgressBarHwnd, PBM_SETPOS, Pos, 0);

    StrStatus = _G(STR_STATUS);
    if (StrStatus)
    {
        for (StrPos = StrStatus; *StrPos; StrPos++)
        {
            if (index < 200)
            {
                wRatio[index] = *StrPos;
            }
            
            index++;            
            if (*StrPos == L'-')
            {
                flag = 1;
                break;
            }
        }
    }
    
    if (flag && index < 200)
    {        
        safe_sprintf(Ratio, " %.0lf%%", Pos * 100.0 / PT_FINISH);
        for (i = 0; index < 200 && Ratio[i]; i++, index++)
        {
            wRatio[index] = Ratio[i];
        }

        SetWindowTextW(g_StaticStatusHwnd, wRatio);
    }
    else
    {
        safe_sprintf(Ratio, "Status - %.0lf%%", Pos * 100.0 / PT_FINISH);
        SetWindowTextA(g_StaticStatusHwnd, Ratio);
    }
}

void UpdateLocalVentoyVersion()
{
    CHAR Ver[128];

	safe_sprintf(Ver, "%s", GetLocalVentoyVersion());
	SetWindowTextA(g_StaticLocalVerHwnd, Ver);

	SetWindowTextA(g_StaticLocalStyleHwnd, g_cur_part_style ? "GPT" : "MBR");
}

int UpdateClusterTipMsg(int toolID, HWND hDlg, HWND hWndTip, WCHAR* Msg)
{
    HWND hwndTool = GetDlgItem(hDlg, toolID);

    TOOLINFO toolInfo = { 0 };
    toolInfo.cbSize = sizeof(toolInfo);
    toolInfo.hwnd = hDlg;
    toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    toolInfo.uId = (UINT_PTR)hwndTool;
    toolInfo.lpszText = Msg;

    SendMessage(hWndTip, TTM_UPDATETIPTEXT, 0, (LPARAM)&toolInfo);
    return 0;
}

