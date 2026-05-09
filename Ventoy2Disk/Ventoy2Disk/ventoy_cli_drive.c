#include <Windows.h>
#include <stdio.h>
#include "Ventoy2Disk.h"
#include "DiskService.h"
#include "ventoy_cli.h"

int CLI_GetPhyDriveInfo(int PhyDrive, PHY_DRIVE_INFO* pInfo)
{
    BOOL bRet;
    DWORD dwBytes;
    HANDLE Handle = INVALID_HANDLE_VALUE;
    CHAR PhyDrivePath[128];
    GET_LENGTH_INFORMATION LengthInfo;
    STORAGE_PROPERTY_QUERY Query;
    STORAGE_DESCRIPTOR_HEADER DevDescHeader;
    STORAGE_DEVICE_DESCRIPTOR* pDevDesc;
    STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR diskAlignment;

    safe_sprintf(PhyDrivePath, "\\\\.\\PhysicalDrive%d", PhyDrive);
    Handle = CreateFileA(PhyDrivePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    Log("Create file Handle:%p %s status:%u", Handle, PhyDrivePath, LASTERR);

    if (Handle == INVALID_HANDLE_VALUE)
    {
        return 1;
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
        return 1;
    }

    Log("PHYSICALDRIVE%d size %llu bytes", PhyDrive, (ULONGLONG)LengthInfo.Length.QuadPart);

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
        return 1;
    }

    if (DevDescHeader.Size < sizeof(STORAGE_DEVICE_DESCRIPTOR))
    {
        Log("Invalid DevDescHeader.Size:%u", DevDescHeader.Size);
        return 1;
    }

    pDevDesc = (STORAGE_DEVICE_DESCRIPTOR*)malloc(DevDescHeader.Size);
    if (!pDevDesc)
    {
        Log("failed to malloc error:%u len:%u", LASTERR, DevDescHeader.Size);
        return 1;
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
        return 1;
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


    pInfo->PhyDrive = PhyDrive;
    pInfo->SizeInBytes = LengthInfo.Length.QuadPart;
    pInfo->DeviceType = pDevDesc->DeviceType;
    pInfo->RemovableMedia = pDevDesc->RemovableMedia;
    pInfo->BusType = pDevDesc->BusType;

    pInfo->BytesPerLogicalSector = diskAlignment.BytesPerLogicalSector;
    pInfo->BytesPerPhysicalSector = diskAlignment.BytesPerPhysicalSector;

    if (pDevDesc->VendorIdOffset)
    {
        safe_strcpy(pInfo->VendorId, (char*)pDevDesc + pDevDesc->VendorIdOffset);
        TrimString(pInfo->VendorId);
    }

    if (pDevDesc->ProductIdOffset)
    {
        safe_strcpy(pInfo->ProductId, (char*)pDevDesc + pDevDesc->ProductIdOffset);
        TrimString(pInfo->ProductId);
    }

    if (pDevDesc->ProductRevisionOffset)
    {
        safe_strcpy(pInfo->ProductRev, (char*)pDevDesc + pDevDesc->ProductRevisionOffset);
        TrimString(pInfo->ProductRev);
    }

    if (pDevDesc->SerialNumberOffset)
    {
        safe_strcpy(pInfo->SerialNumber, (char*)pDevDesc + pDevDesc->SerialNumberOffset);
        TrimString(pInfo->SerialNumber);
    }

    free(pDevDesc);

    CHECK_CLOSE_HANDLE(Handle);

    return 0;
}
