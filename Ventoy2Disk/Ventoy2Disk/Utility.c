/******************************************************************************
 * Utility.c
 *
 * Copyright (c) 2021, longpanda <admin@ventoy.net>
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
#include "Ventoy2Disk.h"

const char* GUID2String(void *guid, char *buf, int len)
{
    GUID* pGUID = (GUID*)guid;
    sprintf_s(buf, len, "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
        pGUID->Data1, pGUID->Data2, pGUID->Data3,
        pGUID->Data4[0], pGUID->Data4[1],
        pGUID->Data4[2], pGUID->Data4[3], pGUID->Data4[4], pGUID->Data4[5], pGUID->Data4[6], pGUID->Data4[7]
    );
    return buf;
}

BOOL IsVentoyLogicalDrive(CHAR DriveLetter)
{
    int i;
    CONST CHAR *Files[] =
    {
        "EFI\\BOOT\\BOOTX64.EFI",
        "grub\\themes\\ventoy\\theme.txt",
        "ventoy\\ventoy.cpio",
    };

    for (i = 0; i < sizeof(Files) / sizeof(Files[0]); i++)
    {
        if (!IsFileExist("%C:\\%s", DriveLetter, Files[i]))
        {
            return FALSE;
        }
    }

    return TRUE;
}

CHAR GetFirstUnusedDriveLetter(void)
{
    CHAR Letter = 'D';
    DWORD Drives = GetLogicalDrives();

    Drives >>= 3;
    while (Drives & 0x1)
    {
        Letter++;
        Drives >>= 1;
    }

    return Letter;
}

const CHAR * GetBusTypeString(STORAGE_BUS_TYPE Type)
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

int VentoyGetLocalBootImg(MBR_HEAD *pMBR)
{
    int Len = 0;
    BYTE *ImgBuf = NULL;
    static int Loaded = 0;
    static MBR_HEAD MBR;

    if (Loaded)
    {
        memcpy(pMBR, &MBR, 512);
        return 0;
    }

    if (0 == ReadWholeFileToBuf(VENTOY_FILE_BOOT_IMG, 0, (void **)&ImgBuf, &Len))
    {
        Log("Copy boot img success");
        memcpy(pMBR, ImgBuf, 512);
        free(ImgBuf);
        
        CoCreateGuid((GUID *)(pMBR->BootCode + 0x180));

        memcpy(&MBR, pMBR, 512);
        Loaded = 1;

        return 0;
    }
    else
    {
        Log("Copy boot img failed");
        return 1;
    }
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

void TrimString(CHAR *String)
{
    CHAR *Pos1 = String;
    CHAR *Pos2 = String;
    size_t Len = strlen(String);

    while (Len > 0)
    {
        if (String[Len - 1] != ' ' && String[Len - 1] != '\t')
        {
            break;
        }
        String[Len - 1] = 0;
        Len--;
    }

    while (*Pos1 == ' ' || *Pos1 == '\t')
    {
        Pos1++;
    }

    while (*Pos1)
    {
        *Pos2++ = *Pos1++;
    }
    *Pos2++ = 0;

    return;
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

int GetPhysicalDriveCount(void)
{
    DWORD Value;
    int Count = 0;

    if (GetRegDwordValue(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\disk\\Enum", "Count", &Value) == 0)
    {
        Count = (int)Value;
    }

    Log("GetPhysicalDriveCount: %d", Count);
    return Count;
}

void VentoyStringToUpper(CHAR* str)
{
    while (str && *str)
    {
        if (*str >= 'a' && *str <= 'z')
        {
            *str = toupper(*str);
        }        
        str++;
    }
}
