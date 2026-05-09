/******************************************************************************
* vtoyjump.c
*
* Copyright (c) 2021, longpanda <admin@ventoy.net>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>
#include <virtdisk.h>
#include <winioctl.h>
#include <VersionHelpers.h>
#include "vtoyjump.h"
#include "fat_filelib.h"

ventoy_os_param g_os_param;
ventoy_windows_data g_windows_data;
UINT8 g_os_param_reserved[32];
INT g_system_bit = VTOY_BIT;
ventoy_guid g_ventoy_guid = VENTOY_GUID;
HANDLE g_vtoylog_mutex = NULL;
HANDLE g_vtoyins_mutex = NULL;

BOOL g_wimboot_mode = FALSE;

DWORD g_vtoy_disk_drive;

CHAR g_prog_full_path[MAX_PATH];
CHAR g_prog_dir[MAX_PATH];
CHAR g_prog_name[MAX_PATH];

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

int GetRegDwordValue(HKEY Key, LPCSTR SubKey, LPCSTR ValueName, DWORD *pValue)
{
    HKEY hKey;
    DWORD Type;
    DWORD Size;
    LSTATUS lRet;
    DWORD Value;

    lRet = RegOpenKeyExA(Key, SubKey, 0, KEY_QUERY_VALUE, &hKey);
    Log("RegOpenKeyExA <%s> Ret:%ld", SubKey, lRet);

    if (ERROR_SUCCESS == lRet)
    {
        Size = sizeof(Value);
        lRet = RegQueryValueExA(hKey, ValueName, NULL, &Type, (LPBYTE)&Value, &Size);
        Log("RegQueryValueExA <%s> ret:%u  Size:%u Value:%u", ValueName, lRet, Size, Value);

        *pValue = Value;
        RegCloseKey(hKey);

        return 0;
    }
    else
    {
        return 1;
    }
}

const CHAR * GetBusTypeString(int Type)
{
    switch (Type)
    {
        case BusTypeUnknown: return "unknown";
        case BusTypeScsi: return "SCSI";
        case BusTypeAtapi: return "Atapi";
        case BusTypeAta: return "ATA";
        case BusType1394: return "1394";
        case BusTypeSsa: return "SSA";
        case BusTypeFibre: return "Fibre";
        case BusTypeUsb: return "USB";
        case BusTypeRAID: return "RAID";
        case BusTypeiScsi: return "iSCSI";
        case BusTypeSas: return "SAS";
        case BusTypeSata: return "SATA";
        case BusTypeSd: return "SD";
        case BusTypeMmc: return "MMC";
        case BusTypeVirtual: return "Virtual";
        case BusTypeFileBackedVirtual: return "FileBackedVirtual";
        case BusTypeSpaces: return "Spaces";
        case BusTypeNvme: return "Nvme";
    }
    return "unknown";
}

int GetHumanReadableGBSize(UINT64 SizeBytes)
{
    int i;
    int Pow2 = 1;
    double Delta;
    double GB = SizeBytes * 1.0 / 1000 / 1000 / 1000;

    if ((SizeBytes % 1073741824) == 0)
    {
        return (int)(SizeBytes / 1073741824);
    }

    for (i = 0; i < 12; i++)
    {
        if (Pow2 > GB)
        {
            Delta = (Pow2 - GB) / Pow2;
        }
        else
        {
            Delta = (GB - Pow2) / Pow2;
        }

        if (Delta < 0.05)
        {
            return Pow2;
        }

        Pow2 <<= 1;
    }

    return (int)GB;
}

int EnumerateAllDisk(VarDiskInfo **ppDiskInfo, int *pDiskNum)
{
    int i;
    DWORD Value;
    int DiskNum = 0;
    BOOL  bRet;
    DWORD dwBytes;
    VarDiskInfo *pDiskInfo = NULL;
    HANDLE Handle = INVALID_HANDLE_VALUE;
    CHAR PhyDrive[128];    
    GET_LENGTH_INFORMATION LengthInfo;
    STORAGE_PROPERTY_QUERY Query;
    STORAGE_DESCRIPTOR_HEADER DevDescHeader;
    STORAGE_DEVICE_DESCRIPTOR *pDevDesc;
    
    if (GetRegDwordValue(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\disk\\Enum", "Count", &Value) == 0)
    {
        DiskNum = (int)Value;
    }
    else
    {
        Log("Failed to read disk count");
        return 1;
    }

    Log("Current phy disk count:%d", DiskNum);
    if (DiskNum <= 0)
    {
        return 1;
    }

    pDiskInfo = malloc(DiskNum * sizeof(VarDiskInfo));
    if (!pDiskInfo)
    {
        Log("Failed to alloc");
        return 1;
    }
    memset(pDiskInfo, 0, DiskNum * sizeof(VarDiskInfo));

    for (i = 0; i < DiskNum; i++)
    {
        SAFE_CLOSE_HANDLE(Handle);

        safe_sprintf(PhyDrive, "\\\\.\\PhysicalDrive%d", i);
        Handle = CreateFileA(PhyDrive, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);        
        Log("Create file Handle:%p %s status:%u", Handle, PhyDrive, LASTERR);

        if (Handle == INVALID_HANDLE_VALUE)
        {
            continue;
        }

        bRet = DeviceIoControl(Handle,
                               IOCTL_DISK_GET_LENGTH_INFO, NULL,
                               0,
                               &LengthInfo,
                               sizeof(LengthInfo),
                               &dwBytes,
                               NULL);
        if (!bRet)
        {
            Log("DeviceIoControl IOCTL_DISK_GET_LENGTH_INFO failed error:%u", LASTERR);
            continue;
        }

        Log("PHYSICALDRIVE%d size %llu bytes", i, (ULONGLONG)LengthInfo.Length.QuadPart);

        Query.PropertyId = StorageDeviceProperty;
        Query.QueryType = PropertyStandardQuery;

        bRet = DeviceIoControl(Handle,
                               IOCTL_STORAGE_QUERY_PROPERTY,
                               &Query,
                               sizeof(Query),
                               &DevDescHeader,
                               sizeof(STORAGE_DESCRIPTOR_HEADER),
                               &dwBytes,
                               NULL);
        if (!bRet)
        {
            Log("DeviceIoControl1 error:%u dwBytes:%u", LASTERR, dwBytes);
            continue;
        }

        if (DevDescHeader.Size < sizeof(STORAGE_DEVICE_DESCRIPTOR))
        {
            Log("Invalid DevDescHeader.Size:%u", DevDescHeader.Size);
            continue;
        }

        pDevDesc = (STORAGE_DEVICE_DESCRIPTOR *)malloc(DevDescHeader.Size);
        if (!pDevDesc)
        {
            Log("failed to malloc error:%u len:%u", LASTERR, DevDescHeader.Size);
            continue;
        }

        bRet = DeviceIoControl(Handle,
                               IOCTL_STORAGE_QUERY_PROPERTY,
                               &Query,
                               sizeof(Query),
                               pDevDesc,
                               DevDescHeader.Size,
                               &dwBytes,
                               NULL);
        if (!bRet)
        {
            Log("DeviceIoControl2 error:%u dwBytes:%u", LASTERR, dwBytes);
            free(pDevDesc);
            continue;
        }

        pDiskInfo[i].RemovableMedia = pDevDesc->RemovableMedia;
        pDiskInfo[i].BusType = pDevDesc->BusType;
        pDiskInfo[i].DeviceType = pDevDesc->DeviceType;
        pDiskInfo[i].Capacity = LengthInfo.Length.QuadPart;

        if (pDevDesc->VendorIdOffset)
        {
            safe_strcpy(pDiskInfo[i].VendorId, (char *)pDevDesc + pDevDesc->VendorIdOffset);
            TrimString(pDiskInfo[i].VendorId, TRUE);
        }

        if (pDevDesc->ProductIdOffset)
        {
            safe_strcpy(pDiskInfo[i].ProductId, (char *)pDevDesc + pDevDesc->ProductIdOffset);
            TrimString(pDiskInfo[i].ProductId, TRUE);
        }

        if (pDevDesc->ProductRevisionOffset)
        {
            safe_strcpy(pDiskInfo[i].ProductRev, (char *)pDevDesc + pDevDesc->ProductRevisionOffset);
            TrimString(pDiskInfo[i].ProductRev, TRUE);
        }

        if (pDevDesc->SerialNumberOffset)
        {
            safe_strcpy(pDiskInfo[i].SerialNumber, (char *)pDevDesc + pDevDesc->SerialNumberOffset);
            TrimString(pDiskInfo[i].SerialNumber, TRUE);
        }

        free(pDevDesc);
        SAFE_CLOSE_HANDLE(Handle);
    }

    Log("########## DUMP DISK BEGIN ##########");
    for (i = 0; i < DiskNum; i++)
    {
        Log("PhyDrv:%d BusType:%-4s Removable:%u Size:%dGB(%llu) Name:%s %s",
            i, GetBusTypeString(pDiskInfo[i].BusType), pDiskInfo[i].RemovableMedia,
            GetHumanReadableGBSize(pDiskInfo[i].Capacity), pDiskInfo[i].Capacity,
            pDiskInfo[i].VendorId, pDiskInfo[i].ProductId);
    }
    Log("Ventoy disk is PhyDvr%d", g_vtoy_disk_drive);
    Log("########## DUMP DISK END ##########");

    *ppDiskInfo = pDiskInfo;
    *pDiskNum = DiskNum;
    return 0;
}

int UnattendVarExpand(const char *script, const char *tmpfile)
{
    FILE *fp = NULL;
    FILE *fout = NULL;
    char *start = NULL;
    char *end = NULL;
    char szLine[4096];
    char szValue[256];
    int DiskNum = 0;
    VarDiskInfo *pDiskInfo = NULL;

    Log("UnattendVarExpand ...");

    if (EnumerateAllDisk(&pDiskInfo, &DiskNum))
    {
        Log("Failed to EnumerateAllDisk");
        return 1;
    }
    
    fopen_s(&fp, script, "r");
    if (!fp)
    {
        free(pDiskInfo);
        return 0;
    }

    fopen_s(&fout, tmpfile, "w+");
    if (!fout)
    {
        fclose(fp);
        free(pDiskInfo);
        return 0;
    }

    szLine[0] = szLine[4095] = 0;
    
    while (fgets(szLine, sizeof(szLine) - 1, fp))
    {
        start = strstr(szLine, "$$VT_");
        if (start)
        {
            end = strstr(start + 5, "$$");
        }

        if (start && end)
        {
            *start = 0;
            fprintf(fout, "%s", szLine);

            *end = 0;
            ExpandSingleVar(pDiskInfo, DiskNum, start + 2, szValue, sizeof(szValue) - 1);
            fprintf(fout, "%s", szValue);
            
            fprintf(fout, "%s", end + 2);
        }
        else
        {
            fprintf(fout, "%s", szLine);
        }
        
        szLine[0] = szLine[4095] = 0;
    }

    fclose(fp);
    fclose(fout);
    free(pDiskInfo);
    return 0;
}

//#define VAR_DEBUG 1

int CreateUnattendRegKey(const char *file)
{
    DWORD dw;
    HKEY hKey;
    LSTATUS Ret;

#ifndef VAR_DEBUG
    Ret = RegCreateKeyEx(HKEY_LOCAL_MACHINE, "System\\Setup", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &dw);
    if (ERROR_SUCCESS == Ret)
    {
        Ret = RegSetValueEx(hKey, "UnattendFile", 0, REG_SZ, file, (DWORD)(strlen(file) + 1));
    }
#endif

    return 0;
}

int ProcessUnattendedInstallation(const char *script, DWORD PhyDrive)
{
    CHAR Letter;
    CHAR DrvLetter;
    CHAR TmpFile[MAX_PATH];
    CHAR CurDir[MAX_PATH];
    CHAR ImPath[MAX_PATH];

    Log("Copy unattended XML ...");
    
    GetCurrentDirectory(sizeof(CurDir), CurDir);
    Letter = CurDir[0];
    if ((Letter >= 'A' && Letter <= 'Z') || (Letter >= 'a' && Letter <= 'z'))
    {
        Log("Current Drive Letter: %C", Letter);
    }
    else
    {
        Letter = 'X';
    }

#ifdef VAR_DEBUG
    sprintf_s(CurDir, sizeof(CurDir), "%C:\\AutounattendXXX.xml", Letter);
#else
    sprintf_s(CurDir, sizeof(CurDir), "%C:\\Unattend.xml", Letter);
#endif

    if (UnattendNeedVarExpand(script))
    {
        sprintf_s(TmpFile, sizeof(TmpFile), "%C:\\__Autounattend", Letter);
        UnattendVarExpand(script, TmpFile);
        
        Log("Expand Copy file <%s> --> <%s>", script, CurDir);
        CopyFileA(TmpFile, CurDir, FALSE);
    }
    else
    {
        Log("No var expand copy file <%s> --> <%s>", script, CurDir);
        CopyFileA(script, CurDir, FALSE);
    }

    VentoyCopyImdisk(PhyDrive, ImPath);
    DrvLetter = VentoyGetFirstFreeDriveLetter(FALSE);
    VentoyProcessRunCmd("%s -a -s 64M -m %C: -p \"/fs:FAT32 /q /y\"", ImPath, DrvLetter);

    Sleep(300);

    sprintf_s(TmpFile, sizeof(TmpFile), "%C:\\Unattend.xml", DrvLetter);
    if (CopyFileA(CurDir, TmpFile, FALSE))
    {
        DeleteFileA(CurDir);
        Log("Move file <%s> ==> <%s>, use the later as unattend XML", CurDir, TmpFile);
        CreateUnattendRegKey(TmpFile);
    }
    else
    {
        Log("Failed to copy file <%s> ==> <%s>, use OLD", CurDir, TmpFile);
        CreateUnattendRegKey(CurDir);
    }

    return 0;
}

