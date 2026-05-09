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
	for (i = 0; i < g_language_count; i++)
	{
		CheckMenuItem(hMenu, VTOY_MENU_LANGUAGE_BEGIN | i, MF_BYCOMMAND | MF_STRING | MF_UNCHECKED);
	}
	CheckMenuItem(hMenu, VTOY_MENU_LANGUAGE_BEGIN | defaultLangId, MF_BYCOMMAND | MF_STRING | MF_CHECKED);
}

int ventoy_compare_language(VENTOY_LANGUAGE *lang1, VENTOY_LANGUAGE *lang2)
{
	if (lstrcmp(lang1->Name, TEXT("Chinese Simplified (简体中文)")) == 0)
	{
		return -1;
	}
	else if (lstrcmp(lang2->Name, TEXT("Chinese Simplified (简体中文)")) == 0)
	{
		return 1;
	}

	return lstrcmp(lang1->Name, lang2->Name);
}

void ventoy_sort_language(VENTOY_LANGUAGE *LangData, int LangCount)
{
	int i, j;
	VENTOY_LANGUAGE *tmpdata = NULL;

	tmpdata = (VENTOY_LANGUAGE *)malloc(sizeof(VENTOY_LANGUAGE));
    if (tmpdata == NULL)
    {
        return;
    }

	for (i = 0; i < LangCount; i++)
	{
		for (j = i + 1; j < LangCount; j++)
		{
			if (ventoy_compare_language(LangData + j, LangData + i) < 0)
			{
				memcpy(tmpdata, LangData + i, sizeof(VENTOY_LANGUAGE));
				memcpy(LangData + i, LangData + j, sizeof(VENTOY_LANGUAGE));
				memcpy(LangData + j, tmpdata, sizeof(VENTOY_LANGUAGE));
			}
		}
	}

	free(tmpdata);
}

void LoadLanguageFromIni(void)
{
    int i, j, k;
    WCHAR *SectionName = NULL;
    WCHAR *SectionNameBuf = NULL;
    VENTOY_LANGUAGE *cur_lang = NULL;
    WCHAR Language[64];
    WCHAR TmpBuf[64];

    swprintf_s(Language, 64, L"StringDefine");
    for (i = 0; i < STR_ID_MAX; i++)
    {
        swprintf_s(TmpBuf, 64, L"%d", i);
        GET_INI_STRING(Language, TmpBuf, g_language_data[0].StrId[i]);
    }

    SectionNameBuf = (WCHAR *)malloc(SIZE_1MB);
    if (SectionNameBuf == NULL)
    {
        return;
    }

    GetPrivateProfileSectionNames(SectionNameBuf, SIZE_1MB / sizeof(WCHAR), VENTOY_LANGUAGE_INI);

    cur_lang = g_language_data;
    for (SectionName = SectionNameBuf; *SectionName && g_language_count < VENTOY_MAX_LANGUAGE; SectionName += (lstrlen(SectionName) + 1))
    {
        if (lstrlen(SectionName) < 9 || memcmp(L"Language-", SectionName, 9 * sizeof(WCHAR)))
        {
            continue;
        }

        // "Language-"
        lstrcpy(cur_lang->Name, SectionName + 9);

        GET_INI_STRING(SectionName, TEXT("FontFamily"), cur_lang->FontFamily);
        cur_lang->FontSize = GetPrivateProfileInt(SectionName, TEXT("FontSize"), 10, VENTOY_LANGUAGE_INI);

        for (j = 0; j < STR_ID_MAX; j++)
        {
            GET_INI_STRING(SectionName, g_language_data[0].StrId[j], cur_lang->MsgString[j]);

            for (k = 0; cur_lang->MsgString[j][k] && cur_lang->MsgString[j][k + 1]; k++)
            {
                if (cur_lang->MsgString[j][k] == '#' && cur_lang->MsgString[j][k + 1] == '@')
                {
                    cur_lang->MsgString[j][k] = '\r';
                    cur_lang->MsgString[j][k + 1] = '\n';
                }
            }
        }

        g_language_count++;
        cur_lang++;
    }
    free(SectionNameBuf);

    Log("Total %d languages ...", g_language_count);
}

void UTF8ToWString(const char *str, WCHAR *buf)
{
    int wcsLen;
    int len = (int)strlen(str);

    wcsLen = MultiByteToWideChar(CP_UTF8, 0, str, len, NULL, 0);
    MultiByteToWideChar(CP_UTF8, 0, str, len, buf, wcsLen);
}

void LoadLanguageFromJson(void)
{
    int k;
    int ret;
    int index = 0;
    int len = 0;
    char *buf = NULL;
    VTOY_JSON *json = NULL;
    VTOY_JSON *node = NULL;
    VTOY_JSON *cur = NULL;
    VENTOY_LANGUAGE *cur_lang = NULL;

    ReadWholeFileToBuf(VENTOY_LANGUAGE_JSON_A, 4, &buf, &len);
    buf[len] = 0;

    json = vtoy_json_create();

    ret = vtoy_json_parse(json, buf);

    Log("language json file len:%d json parse:%d", len, ret);

    cur_lang = g_language_data;
    for (node = json->pstChild; node; node = node->pstNext)
    {
        cur = node->pstChild;
        index = 0;
        while (cur)
        {
            if (strncmp(cur->pcName, "name", 4) == 0)
            {
                UTF8ToWString(cur->unData.pcStrVal, cur_lang->Name);
            }
            else if (strcmp(cur->pcName, "FontFamily") == 0)
            {
                UTF8ToWString(cur->unData.pcStrVal, cur_lang->FontFamily);
            }
            else if (strcmp(cur->pcName, "FontSize") == 0)
            {
                cur_lang->FontSize = (int)cur->unData.lValue;
            }
            else if (strncmp(cur->pcName, "STR_", 4) == 0)
            {
                UTF8ToWString(cur->unData.pcStrVal, cur_lang->MsgString[index]);

                for (k = 0; cur_lang->MsgString[index][k] && cur_lang->MsgString[index][k + 1]; k++)
                {
                    if (cur_lang->MsgString[index][k] == '#' && cur_lang->MsgString[index][k + 1] == '@')
                    {
                        cur_lang->MsgString[index][k] = '\r';
                        cur_lang->MsgString[index][k + 1] = '\n';
                    }
                }

                index++;
            }
            cur = cur->pstNext;
        }

        cur_lang++;
        g_language_count++;
    }

    vtoy_json_destroy(json);
    free(buf);

    Log("Total %d languages ...", g_language_count);
}


void LanguageInit(void)
{
    int i;
	int id = -1, DefaultId = -1;
	WCHAR TmpBuf[256];
	LANGID LangId = GetSystemDefaultUILanguage();
	HMENU SubMenu;
	HMENU hMenu = GetMenu(g_DialogHwnd);

    SubMenu = GetSubMenu(hMenu, 0);
    AppendMenu(SubMenu, MF_STRING | MF_BYCOMMAND, VTOY_MENU_PART_CFG, TEXT("yyy"));
    AppendMenu(SubMenu, MF_STRING | MF_BYCOMMAND, VTOY_MENU_ALL_DEV, TEXT("USB Device Only")); 

#if VTSI_SUPPORT    
    AppendMenu(SubMenu, MF_STRING | MF_BYCOMMAND, VTOY_MENU_VTSI, TEXT("Generate VTSI File"));    
#endif

    AppendMenu(SubMenu, MF_STRING | MF_BYCOMMAND, VTOY_MENU_CLEAN, TEXT("yyy"));       
    AppendMenu(SubMenu, MF_STRING | MF_BYCOMMAND, VTOY_MENU_PART_RESIZE, TEXT("yyy"));
    
    if (g_cur_part_style)
    {
        CheckMenuItem(hMenu, (UINT)ID_PARTSTYLE_MBR, MF_BYCOMMAND | MF_UNCHECKED);
        CheckMenuItem(hMenu, (UINT)ID_PARTSTYLE_GPT, MF_BYCOMMAND | MF_CHECKED);
    }
    else
    {
        CheckMenuItem(hMenu, (UINT)ID_PARTSTYLE_MBR, MF_BYCOMMAND | MF_CHECKED);
        CheckMenuItem(hMenu, (UINT)ID_PARTSTYLE_GPT, MF_BYCOMMAND | MF_UNCHECKED);
    }

	SubMenu = GetSubMenu(hMenu, 1);
	DeleteMenu(SubMenu, 0, MF_BYPOSITION);

	g_language_data = (VENTOY_LANGUAGE *)malloc(sizeof(VENTOY_LANGUAGE)* VENTOY_MAX_LANGUAGE);
    if (g_language_data == NULL)
    {
        return;
    }

	memset(g_language_data, 0, sizeof(VENTOY_LANGUAGE)* VENTOY_MAX_LANGUAGE);

    if (IsFileExist(VENTOY_LANGUAGE_JSON_A))
    {
        Log("Load languages from json file ...");
        LoadLanguageFromJson(); 
    }
    else
    {
        Log("Load languages from ini file ...");
        LoadLanguageFromIni();
    }

	ventoy_sort_language(g_language_data, g_language_count);

	if (MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED) == LangId)
	{
		DefaultId = 0;
	}

	memset(TmpBuf, 0, sizeof(TmpBuf));
	GetPrivateProfileString(TEXT("Ventoy"), TEXT("Language"), TEXT("#"), TmpBuf, 256, VENTOY_CFG_INI);

	for (i = 0; i < g_language_count; i++)
	{
		AppendMenu(SubMenu, MF_STRING | MF_BYCOMMAND, VTOY_MENU_LANGUAGE_BEGIN | i, g_language_data[i].Name);
		
		if (id < 0 && lstrcmp(g_language_data[i].Name, TmpBuf) == 0)
		{
			id = i;
		}

		if (DefaultId < 0 && lstrcmp(g_language_data[i].Name, TEXT("English (English)")) == 0)
		{
			DefaultId = i;
		}
	}

	if (id < 0)
	{
		id = DefaultId;
	}



	UpdateItemString(id);
}

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
