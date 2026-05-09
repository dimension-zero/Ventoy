/******************************************************************************
 * PhyDrive.c
 *
 * Copyright (c) 2020, longpanda <admin@ventoy.net>
 * Copyright (c) 2011-2020, Pete Batard <pete@akeo.ie>
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
 
#include <Windows.h>
#include <time.h>
#include <winternl.h>
#include <commctrl.h>
#include <initguid.h>
#include "resource.h"
#include "Language.h"
#include "Ventoy2Disk.h"
#include "fat_filelib.h"
#include "ff.h"
#include "DiskService.h"
#include "PhyDrive_priv.h"

BOOL VentoyPhydriveMatch(PHY_DRIVE_INFO* pPhyDrive)
{
    BOOL  bRet = FALSE;
    DWORD dwBytes;
    HANDLE Handle = INVALID_HANDLE_VALUE;
    CHAR PhyDrive[128];
    GET_LENGTH_INFORMATION LengthInfo;
    STORAGE_PROPERTY_QUERY Query;
    STORAGE_DESCRIPTOR_HEADER DevDescHeader;
    STORAGE_DEVICE_DESCRIPTOR* pDevDesc = NULL;
    STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR diskAlignment;
    CHAR VendorId[128] = { 0 };
    CHAR ProductId[128] = { 0 };
    CHAR ProductRev[128] = { 0 };
    CHAR SerialNumber[128] = { 0 };


    safe_sprintf(PhyDrive, "\\\\.\\PhysicalDrive%d", pPhyDrive->PhyDrive);
    Handle = CreateFileA(PhyDrive, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (Handle == INVALID_HANDLE_VALUE)
    {
        Log("Create file Handle:%p %s status:%u", Handle, PhyDrive, LASTERR);
        return FALSE;
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
        return FALSE;
    }

    if (pPhyDrive->SizeInBytes != (ULONGLONG)LengthInfo.Length.QuadPart)
    {
        Log("PHYSICALDRIVE%d size not match %llu %llu", pPhyDrive->PhyDrive, (ULONGLONG)LengthInfo.Length.QuadPart,
            (ULONGLONG)pPhyDrive->SizeInBytes);
        CHECK_CLOSE_HANDLE(Handle);
        return FALSE;
    }

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
        CHECK_CLOSE_HANDLE(Handle);
        return FALSE;
    }

    if (DevDescHeader.Size < sizeof(STORAGE_DEVICE_DESCRIPTOR))
    {
        Log("Invalid DevDescHeader.Size:%u", DevDescHeader.Size);
        CHECK_CLOSE_HANDLE(Handle);
        return FALSE;
    }

    pDevDesc = (STORAGE_DEVICE_DESCRIPTOR*)malloc(DevDescHeader.Size);
    if (!pDevDesc)
    {
        Log("failed to malloc error:%u len:%u", LASTERR, DevDescHeader.Size);
        CHECK_CLOSE_HANDLE(Handle);
        return FALSE;
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
        goto out;
    }



    memset(&Query, 0, sizeof(STORAGE_PROPERTY_QUERY));
    Query.PropertyId = StorageAccessAlignmentProperty;
    Query.QueryType = PropertyStandardQuery;
    memset(&diskAlignment, 0, sizeof(STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR));

    bRet = DeviceIoControl(Handle,
        IOCTL_STORAGE_QUERY_PROPERTY,
        &Query,
        sizeof(STORAGE_PROPERTY_QUERY),
        &diskAlignment,
        sizeof(STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR),
        &dwBytes,
        NULL);
    if (!bRet)
    {
        Log("DeviceIoControl3 error:%u dwBytes:%u", LASTERR, dwBytes);
    }

    if (pPhyDrive->DeviceType != pDevDesc->DeviceType ||
        pPhyDrive->RemovableMedia != pDevDesc->RemovableMedia ||
        pPhyDrive->BusType != pDevDesc->BusType ||
        pPhyDrive->BytesPerLogicalSector != diskAlignment.BytesPerLogicalSector ||
        pPhyDrive->BytesPerPhysicalSector != diskAlignment.BytesPerPhysicalSector
        )
    {
        Log("Some properties not match DeviceType[%u %u] Removable[%u %u] BusType[%u %u] LogSec[%u %u] PhySec[%u %u]", 
            pPhyDrive->DeviceType, pDevDesc->DeviceType,
            pPhyDrive->RemovableMedia, pDevDesc->RemovableMedia,
            pPhyDrive->BusType, pDevDesc->BusType,
            pPhyDrive->BytesPerLogicalSector, diskAlignment.BytesPerLogicalSector,
            pPhyDrive->BytesPerPhysicalSector, diskAlignment.BytesPerPhysicalSector
            );
        goto out;
    }

    if (pDevDesc->VendorIdOffset)
    {
        safe_strcpy(VendorId, (char*)pDevDesc + pDevDesc->VendorIdOffset);
        TrimString(VendorId);

        if (strcmp(pPhyDrive->VendorId, VendorId))
        {
            Log("VendorId not match <%s %s>", pPhyDrive->VendorId, VendorId);
            goto out;
        }
    }

    if (pDevDesc->ProductIdOffset)
    {
        safe_strcpy(ProductId, (char*)pDevDesc + pDevDesc->ProductIdOffset);
        TrimString(ProductId);

        if (strcmp(pPhyDrive->ProductId, ProductId))
        {
            Log("ProductId not match <%s %s>", pPhyDrive->ProductId, ProductId);
            goto out;
        }
    }

    if (pDevDesc->ProductRevisionOffset)
    {
        safe_strcpy(ProductRev, (char*)pDevDesc + pDevDesc->ProductRevisionOffset);
        TrimString(ProductRev);

        if (strcmp(pPhyDrive->ProductRev, ProductRev))
        {
            Log("ProductRev not match <%s %s>", pPhyDrive->ProductRev, ProductRev);
            goto out;
        }
    }

    if (pDevDesc->SerialNumberOffset)
    {
        safe_strcpy(SerialNumber, (char*)pDevDesc + pDevDesc->SerialNumberOffset);
        TrimString(SerialNumber);

        if (strcmp(pPhyDrive->SerialNumber, SerialNumber))
        {
            Log("ProductRev not match <%s %s>", pPhyDrive->SerialNumber, SerialNumber);
            goto out;
        }
    }

    Log("PhyDrive%d ALL match, now continue", pPhyDrive->PhyDrive);

    bRet = TRUE;

out:
    if (pDevDesc)
    {
        free(pDevDesc);
    }

    CHECK_CLOSE_HANDLE(Handle);

    return bRet;
}
