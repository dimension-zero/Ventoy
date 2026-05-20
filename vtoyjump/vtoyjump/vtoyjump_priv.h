/******************************************************************************
 * vtoyjump_priv.h — cross-module declarations for vtoyjump
 *****************************************************************************/
#ifndef __VTOYJUMP_PRIV_H__
#define __VTOYJUMP_PRIV_H__

#include "vtoyjump.h"

/* ---- shared macros ---- */
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

/* ---- shared globals (defined in vtoyjump.c) ---- */
extern ventoy_os_param g_os_param;
extern ventoy_windows_data g_windows_data;
extern UINT8 g_os_param_reserved[32];
extern INT g_system_bit;
extern ventoy_guid g_ventoy_guid;
extern HANDLE g_vtoylog_mutex;
extern HANDLE g_vtoyins_mutex;
extern BOOL g_wimboot_mode;
extern DWORD g_vtoy_disk_drive;
extern CHAR g_prog_full_path[MAX_PATH];
extern CHAR g_prog_dir[MAX_PATH];
extern CHAR g_prog_name[MAX_PATH];
extern HANDLE g_FatPhyDrive;
extern UINT64 g_Part2StartSec;

/* ---- cross-module function prototypes ---- */
void BreakAndLaunchCmd(int line);
const char * GetFileNameInPath(const char *fullpath);
int split_path_name(char *fullpath, char *dir, char *name);
void TrimString(CHAR *String, BOOL TrimLeft);
int VentoyProcessRunCmd(const char *Fmt, ...);
CHAR VentoyGetFirstFreeDriveLetter(BOOL Reverse);
void Log(const char *Fmt, ...);
int LoadNtDriver(const char *DrvBinPath);
int ReadWholeFile2Buf(const char *Fullpath, void **Data, DWORD *Size);
BOOL CheckPeHead(BYTE *Buffer, DWORD Size, DWORD Offset);
BOOL CheckOsParam(ventoy_os_param *param);
int SaveBuffer2File(const char *Fullpath, void *Buffer, DWORD Length);
int IsUTF8Encode(const char *src);
int Utf16ToUtf8(const WCHAR *src, char *dst);
int Utf8ToUtf16(const char* src, WCHAR * dst);
BOOL IsDirExist(const char *Fmt, ...);
BOOL IsFileExist(const char *Fmt, ...);
int GetPhyDiskUUID(const char LogicalDrive, UINT8 *UUID, UINT32 *DiskSig, DISK_EXTENT *DiskExtent);
int VentoyMountAnywhere(HANDLE Handle);
int VentoyMountY(HANDLE Handle);
BOOL VentoyAPINeedMountY(const char *IsoPath);
int VentoyAttachVirtualDisk(HANDLE Handle, const char *IsoPath);
int VentoyMountISOByAPI(const char *IsoPath);
int CopyFileFromFatDisk(const CHAR* SrcFile, const CHAR *DstFile);
int VentoyFatDiskRead(uint32 Sector, uint8 *Buffer, uint32 SectorCount);
BOOL Is2K10PE(void);
CHAR GetIMDiskMountLogicalDrive(const char *suffix);
UINT64 GetVentoyEfiPartStartSector(HANDLE hDrive);
int VentoyCopyImdisk(DWORD PhyDrive, CHAR *ImPath);
int VentoyRunImdisk(const char *suffix, const char *IsoPath, const char *imdiskexe, const char *opt);
int VentoyMountISOByImdisk(const char *IsoPath, DWORD PhyDrive);
int GetIsoId(CONST CHAR *IsoPath, IsoId *ids);
int CheckSkipMountIso(CONST CHAR *IsoPath);
int MountIsoFile(CONST CHAR *IsoPath, DWORD PhyDrive);
int GetPhyDriveByLogicalDrive(int DriveLetter);
int DeleteVentoyPart2MountPoint(DWORD PhyDrive);
BOOL check_tar_archive(const char *archive, CHAR *tarName);
void unxz_error(char *x);
int unxz_flush(void *src, unsigned int size);
int DecompressInjectionArchive(const char *archive, DWORD PhyDrive);
int UnattendNeedVarExpand(const char *script);
int ExpandSingleVar(VarDiskInfo *pDiskInfo, int DiskNum, const char *var, char *value, int len);
int GetRegDwordValue(HKEY Key, LPCSTR SubKey, LPCSTR ValueName, DWORD *pValue);
const CHAR * GetBusTypeString(int Type);
int GetHumanReadableGBSize(UINT64 SizeBytes);
int EnumerateAllDisk(VarDiskInfo **ppDiskInfo, int *pDiskNum);
int UnattendVarExpand(const char *script, const char *tmpfile);
int CreateUnattendRegKey(const char *file);
int ProcessUnattendedInstallation(const char *script, DWORD PhyDrive);
int VentoyGetFileVersion(const CHAR *FilePath, UINT16 *pMajor, UINT16 *pMinor, UINT16 *pBuild, UINT16 *pRevision);
BOOL VentoyIsNeedBypass(const char *isofile, const char MntLetter);
int Windows11Bypass(const char *isofile, const char MntLetter, UINT8 Check, UINT8 NRO);
BOOL CheckVentoyDisk(DWORD DiskNum);
BOOL VentoyIsLenovoRecovery(CHAR *IsoPath, CHAR *VTLRIPath);
int MountVTLRI(CHAR *ImgPath, DWORD PhyDrive);
BOOL FindVentoyDiskBySig(UINT32 VtoySig, DWORD* pDiskNum);
int VentoyHook(ventoy_os_param *param);
int ExtractWindowsDataFile(char *databuf);
int ventoy_check_create_directory(void);
int VentoyJump(INT argc, CHAR **argv, CHAR *LunchFile);
void VentoyToUpper(CHAR *str);
int vtoy_remove_duplicate_file(char *File);
int VTLRI_ServiceMain(int argc, char **argv);

#endif /* __VTOYJUMP_PRIV_H__ */
