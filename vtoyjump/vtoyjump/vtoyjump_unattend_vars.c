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
#include "vtoyjump_priv.h"
int UnattendNeedVarExpand(const char *script)
{
    FILE *fp = NULL;
    char szLine[4096];

    fopen_s(&fp, script, "r");
    if (!fp)
    {
        return 0;
    }

    szLine[0] = szLine[4095] = 0;
    
    while (fgets(szLine, sizeof(szLine) - 1, fp))
    {
        if (strstr(szLine, "$$VT_"))
        {
            fclose(fp);
            return 1;
        }
    
        szLine[0] = szLine[4095] = 0;
    }
    
    fclose(fp);
    return 0;
}

int ExpandSingleVar(VarDiskInfo *pDiskInfo, int DiskNum, const char *var, char *value, int len)
{
    int i;
    int index = -1;
    UINT64 uiDst = 0;
    UINT64 uiDelta = 0;
    UINT64 uiMaxSize = 0;
    UINT64 uiMaxDelta = ULLONG_MAX;

    value[0] = 0;
    
    if (strcmp(var, "VT_WINDOWS_DISK_1ST_NONVTOY") == 0)
    {
        for (i = 0; i < DiskNum; i++)
        {
            if (pDiskInfo[i].Capacity > 0 && i != g_vtoy_disk_drive)
            {
                Log("%s=<PhyDrive%d>", var, i);
                sprintf_s(value, len, "%d", i);
                return 0;
            }
        }
    }
    else if (strcmp(var, "VT_WINDOWS_DISK_1ST_NONUSB") == 0)
    {
        for (i = 0; i < DiskNum; i++)
        {
            if (pDiskInfo[i].Capacity > 0 && pDiskInfo[i].BusType != BusTypeUsb)
            {
                Log("%s=<PhyDrive%d>", var, i);
                sprintf_s(value, len, "%d", i);
                return 0;
            }
        }
    }
    else if (strcmp(var, "VT_WINDOWS_DISK_MAX_SIZE") == 0)
    {
        for (i = 0; i < DiskNum; i++)
        {
            if (pDiskInfo[i].Capacity > 0 && pDiskInfo[i].Capacity > uiMaxSize)
            {
                index = i;
                uiMaxSize = pDiskInfo[i].Capacity;
            }
        }

        Log("%s=<PhyDrive%d>", var, index);
        sprintf_s(value, len, "%d", index);
    }
    else if (strncmp(var, "VT_WINDOWS_DISK_CLOSEST_", 24) == 0)
    {
        uiDst = strtoul(var + 24, NULL, 10);
        uiDst = uiDst * (1024ULL * 1024ULL * 1024ULL);
    
        for (i = 0; i < DiskNum; i++)
        {
            if (pDiskInfo[i].Capacity == 0)
            {
                continue;
            }
        
            if (pDiskInfo[i].Capacity > uiDst)
            {
                uiDelta = pDiskInfo[i].Capacity - uiDst;
            }
            else
            {
                uiDelta = uiDst - pDiskInfo[i].Capacity;
            }
            
            if (uiDelta < uiMaxDelta)
            {
                uiMaxDelta = uiDelta;
                index = i;
            }
        }

        Log("%s=<PhyDrive%d>", var, index);
        sprintf_s(value, len, "%d", index);
    }
    else if (strncmp(var, "VT_WINDOWS_DISK_NONVTOY_CLOSEST_", 32) == 0)
    {
        uiDst = strtoul(var + 32, NULL, 10);
        uiDst = uiDst * (1024ULL * 1024ULL * 1024ULL);

        for (i = 0; i < DiskNum; i++)
        {
            if (pDiskInfo[i].Capacity == 0 || i == g_vtoy_disk_drive)
            {
                continue;
            }

            if (pDiskInfo[i].Capacity > uiDst)
            {
                uiDelta = pDiskInfo[i].Capacity - uiDst;
            }
            else
            {
                uiDelta = uiDst - pDiskInfo[i].Capacity;
            }

            if (uiDelta < uiMaxDelta)
            {
                uiMaxDelta = uiDelta;
                index = i;
            }
        }

        Log("%s=<PhyDrive%d>", var, index);
        sprintf_s(value, len, "%d", index);
    }
    else
    {
        Log("Invalid var name <%s>", var);
        sprintf_s(value, len, "$$%s$$", var);
    }
    
    if (value[0] == 0)
    {
        sprintf_s(value, len, "$$%s$$", var);
    }

    return 0;
}

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

