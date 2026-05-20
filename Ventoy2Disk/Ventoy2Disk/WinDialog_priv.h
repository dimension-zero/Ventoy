/* WinDialog_priv.h — cross-module declarations for WinDialog */
#ifndef __WINDIALOG_PRIV_H__
#define __WINDIALOG_PRIV_H__

#include <Windows.h>
#include <Shlobj.h>
#include <tlhelp32.h>
#include <Psapi.h>
#include <commctrl.h>
#include <delayimp.h>
#include "resource.h"
#include "Language.h"
#include "Ventoy2Disk.h"
#include "DiskService.h"
#include "VentoyJson.h"

/* ---- file-scope globals shared across modules ---- */
extern HINSTANCE g_hInst;
extern BOOL g_InvalidClusterSize;
extern HWND g_ComboxHwnd;
extern HWND g_LocalClusterTipHwnd;
extern HWND g_DiskClusterTipHwnd;
extern HWND g_StaticLocalVerHwnd;
extern HWND g_StaticDiskVerHwnd;
extern HWND g_StaticLocalStyleHwnd;
extern HWND g_StaticDiskStyleHwnd;
extern HWND g_StaticLocalFsHwnd;
extern HWND g_StaticDevFsHwnd;
extern HWND g_BtnInstallHwnd;
extern HWND g_StaticDevHwnd;
extern HWND g_StaticLocalHwnd;
extern HWND g_StaticDiskHwnd;
extern HWND g_BtnUpdateHwnd;
extern HWND g_ProgressBarHwnd;
extern HWND g_StaticStatusHwnd;
extern HWND g_LocalIconSecureHwnd;
extern HWND g_DiskIconSecureHwnd;
extern HANDLE g_ThreadHandle;
extern HFONT g_language_normal_font;
extern HFONT g_language_bold_font;
extern int g_cur_part_style;
extern int g_language_count;
extern int g_cur_lang_id;
extern VENTOY_LANGUAGE *g_language_data;
extern VENTOY_LANGUAGE *g_cur_lang_data;
extern int m_MinWidth;
extern int m_MinHeight;
extern RECT m_WindowRect;

/* ---- function prototypes ---- */
const char* current_arch_string(void);
int LoadCfgIni(void);
int WriteCfgIni(void);
void SetProgressBarPos(int Pos);
void UpdateLocalVentoyVersion();
int UpdateClusterTipMsg(int toolID, HWND hDlg, HWND hWndTip, WCHAR* Msg);
void OnComboxSelChange(HWND hCombox);
void UpdateReservedPostfix(void);
void UpdateItemString(int defaultLangId);
int ventoy_compare_language(VENTOY_LANGUAGE *lang1, VENTOY_LANGUAGE *lang2);
void ventoy_sort_language(VENTOY_LANGUAGE *LangData, int LangCount);
void LoadLanguageFromIni(void);
void UTF8ToWString(const char *str, WCHAR *buf);
void LoadLanguageFromJson(void);
void LanguageInit(void);
void InitComboxCtrl(HWND hWnd, int PhyDrive);
HWND CreateClusterToolTip(int toolID, HWND hDlg, PTSTR pszText);
BOOL InitDialog(HWND hWnd, WPARAM wParam, LPARAM lParam);
DWORD WINAPI InstallVentoyThread(void* Param);
DWORD WINAPI ClearVentoyThread(void* Param);
DWORD WINAPI UpdateVentoyThread(void* Param);
DWORD WINAPI PartResizeThread(void* Param);
void OnInstallBtnClick(void);
void OnRefreshBtnClick(HWND hWnd);
void OnUpdateBtnClick(void);
BOOL PartResizePreCheck(PHY_DRIVE_INFO** ppPhyDrive);
void OnPartResize(void);
void OnClearVentoy(void);
void MenuProc(HWND hWnd, WPARAM wParam, LPARAM lParam);
int ExpandDlg(HWND hParent, UINT uiID, int WidthDelta);
int MoveDlg(HWND hParent, UINT uiID, int WidthDelta);
INT_PTR CALLBACK DialogProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);
DWORD VentoyGetParentProcessId(DWORD pid);
int VentoyCheckParentProcess(void);
FARPROC WINAPI dllDelayLoadHook(unsigned dliNotify, PDelayLoadInfo pdli);
void DllProtect(void);
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow);

#endif /* __WINDIALOG_PRIV_H__ */
