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

int VentoyFillMBRLocation(UINT64 DiskSizeInBytes, UINT32 StartSectorId, UINT32 SectorCount, PART_TABLE *Table)
{
    BYTE Head;
    BYTE Sector;
    BYTE nSector = 63;
    BYTE nHead = 8;    
    UINT32 Cylinder;
    UINT32 EndSectorId;

    while (nHead != 0 && (DiskSizeInBytes / 512 / nSector / nHead) > 1024)
    {
        nHead = (BYTE)nHead * 2;
    }

    if (nHead == 0)
    {
        nHead = 255;
    }

    Cylinder = StartSectorId / nSector / nHead;
    Head = StartSectorId / nSector % nHead;
    Sector = StartSectorId % nSector + 1;

    Table->StartHead = Head;
    Table->StartSector = Sector;
    Table->StartCylinder = Cylinder;

    EndSectorId = StartSectorId + SectorCount - 1;
    Cylinder = EndSectorId / nSector / nHead;
    Head = EndSectorId / nSector % nHead;
    Sector = EndSectorId % nSector + 1;

    Table->EndHead = Head;
    Table->EndSector = Sector;
    Table->EndCylinder = Cylinder;

    Table->StartSectorId = StartSectorId;
    Table->SectorCount = SectorCount;

    return 0;
}

int VentoyFillMBR(UINT64 DiskSizeBytes, MBR_HEAD *pMBR, int PartStyle, UINT8 FsFlag)
{
    GUID Guid;
	int ReservedValue;
    UINT32 DiskSignature;
    UINT32 DiskSectorCount;
    UINT32 PartSectorCount;
    UINT32 PartStartSector;
	UINT32 ReservedSector;

    VentoyGetLocalBootImg(pMBR);

    CoCreateGuid(&Guid);

    memcpy(&DiskSignature, &Guid, sizeof(UINT32));

    Log("Disk signature: 0x%08x", DiskSignature);

    *((UINT32 *)(pMBR->BootCode + 0x1B8)) = DiskSignature;
	memcpy(pMBR->BootCode + 0x180, &Guid, 16);

    if (DiskSizeBytes / 512 > 0xFFFFFFFF)
    {
        DiskSectorCount = 0xFFFFFFFF;
    }
    else
    {
        DiskSectorCount = (UINT32)(DiskSizeBytes / 512);
    }

	ReservedValue = GetReservedSpaceInMB();
	if (ReservedValue <= 0)
	{
		ReservedSector = 0;
	}
	else
	{
		ReservedSector = (UINT32)(ReservedValue * 2048);
	}

    if (PartStyle)
    {
        ReservedSector += 33; // backup GPT part table
    }

    // check aligned with 4KB
    if (IsPartNeed4KBAlign())
    {
        UINT64 sectors = DiskSizeBytes / 512;
        if (sectors % 8)
        {
            Log("Disk need to align with 4KB %u", (UINT32)(sectors % 8));
            ReservedSector += (UINT32)(sectors % 8);
        }
    }

	Log("ReservedSector: %u", ReservedSector);

    //Part1
    PartStartSector = VENTOY_PART1_START_SECTOR;
	PartSectorCount = DiskSectorCount - ReservedSector - VENTOY_EFI_PART_SIZE / 512 - PartStartSector;
    VentoyFillMBRLocation(DiskSizeBytes, PartStartSector, PartSectorCount, pMBR->PartTbl);

    pMBR->PartTbl[0].Active = 0x80; // bootable
    pMBR->PartTbl[0].FsFlag = FsFlag; // File system flag  07:exFAT/NTFS/HPFS  0C:FAT32

    //Part2
    PartStartSector += PartSectorCount;
    PartSectorCount = VENTOY_EFI_PART_SIZE / 512;
    VentoyFillMBRLocation(DiskSizeBytes, PartStartSector, PartSectorCount, pMBR->PartTbl + 1);

    pMBR->PartTbl[1].Active = 0x00; 
    pMBR->PartTbl[1].FsFlag = 0xEF; // EFI System Partition

    pMBR->Byte55 = 0x55;
    pMBR->ByteAA = 0xAA;

    return 0;
}


static int VentoyFillProtectMBR(UINT64 DiskSizeBytes, MBR_HEAD *pMBR)
{
    GUID Guid;
    UINT32 DiskSignature;
    UINT64 DiskSectorCount;

    VentoyGetLocalBootImg(pMBR);

    CoCreateGuid(&Guid);

    memcpy(&DiskSignature, &Guid, sizeof(UINT32));

    Log("Disk signature: 0x%08x", DiskSignature);

    *((UINT32 *)(pMBR->BootCode + 0x1B8)) = DiskSignature;
	memcpy(pMBR->BootCode + 0x180, &Guid, 16);

    DiskSectorCount = DiskSizeBytes / 512 - 1;
    if (DiskSectorCount > 0xFFFFFFFF)
    {
        DiskSectorCount = 0xFFFFFFFF;
    }

    memset(pMBR->PartTbl, 0, sizeof(pMBR->PartTbl));

    pMBR->PartTbl[0].Active = 0x00;
    pMBR->PartTbl[0].FsFlag = 0xee; // EE

    pMBR->PartTbl[0].StartHead = 0;
    pMBR->PartTbl[0].StartSector = 1;
    pMBR->PartTbl[0].StartCylinder = 0;
    pMBR->PartTbl[0].EndHead = 254;
    pMBR->PartTbl[0].EndSector = 63;
    pMBR->PartTbl[0].EndCylinder = 1023;

    pMBR->PartTbl[0].StartSectorId = 1;
    pMBR->PartTbl[0].SectorCount = (UINT32)DiskSectorCount;

    pMBR->Byte55 = 0x55;
    pMBR->ByteAA = 0xAA;

    pMBR->BootCode[92] = 0x22;

    return 0;
}

int VentoyFillWholeGpt(UINT64 DiskSizeBytes, VTOY_GPT_INFO *pInfo)
{
    UINT64 Part1SectorCount = 0;
    UINT64 DiskSectorCount = DiskSizeBytes / 512;
    VTOY_GPT_HDR *Head = &pInfo->Head;
    VTOY_GPT_PART_TBL *Table = pInfo->PartTbl;
    static GUID WindowsDataPartType = { 0xebd0a0a2, 0xb9e5, 0x4433, { 0x87, 0xc0, 0x68, 0xb6, 0xb7, 0x26, 0x99, 0xc7 } };

    VentoyFillProtectMBR(DiskSizeBytes, &pInfo->MBR);

    Part1SectorCount = DiskSectorCount - 33 - 2048;

    memcpy(Head->Signature, "EFI PART", 8);
    Head->Version[2] = 0x01;
    Head->Length = 92;
    Head->Crc = 0;
    Head->EfiStartLBA = 1;
    Head->EfiBackupLBA = DiskSectorCount - 1;
    Head->PartAreaStartLBA = 34;
    Head->PartAreaEndLBA = DiskSectorCount - 34;
    CoCreateGuid(&Head->DiskGuid);
    Head->PartTblStartLBA = 2;
    Head->PartTblTotNum = 128;
    Head->PartTblEntryLen = 128;


    memcpy(&(Table[0].PartType), &WindowsDataPartType, sizeof(GUID));
    CoCreateGuid(&(Table[0].PartGuid));
    Table[0].StartLBA = 2048;
    Table[0].LastLBA = 2048 + Part1SectorCount - 1;
    Table[0].Attr = 0;
    memcpy(Table[0].Name, L"Data", 4 * 2);

    //Update CRC
    Head->PartTblCrc = VentoyCrc32(Table, sizeof(pInfo->PartTbl));
    Head->Crc = VentoyCrc32(Head, Head->Length);

    return 0;
}

int VentoyFillGpt(UINT64 DiskSizeBytes, VTOY_GPT_INFO *pInfo)
{
    INT64 ReservedValue = 0;
    UINT64 ModSectorCount = 0;
    UINT64 ReservedSector = 33;
    UINT64 Part1SectorCount = 0;
    UINT64 DiskSectorCount = DiskSizeBytes / 512;
    VTOY_GPT_HDR *Head = &pInfo->Head;
    VTOY_GPT_PART_TBL *Table = pInfo->PartTbl;
    static GUID WindowsDataPartType = { 0xebd0a0a2, 0xb9e5, 0x4433, { 0x87, 0xc0, 0x68, 0xb6, 0xb7, 0x26, 0x99, 0xc7 } };
    static GUID EspPartType = { 0xc12a7328, 0xf81f, 0x11d2, { 0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b } };
	static GUID BiosGrubPartType = { 0x21686148, 0x6449, 0x6e6f, { 0x74, 0x4e, 0x65, 0x65, 0x64, 0x45, 0x46, 0x49 } };

    VentoyFillProtectMBR(DiskSizeBytes, &pInfo->MBR);

    ReservedValue = GetReservedSpaceInMB();
    if (ReservedValue > 0)
    {
        ReservedSector += ReservedValue * 2048;
    }

    Part1SectorCount = DiskSectorCount - ReservedSector - (VENTOY_EFI_PART_SIZE / 512) - 2048;

    ModSectorCount = (Part1SectorCount % 8);
    if (ModSectorCount)
    {
        Log("Part1SectorCount:%llu is not aligned by 4KB (%llu)", (ULONGLONG)Part1SectorCount, (ULONGLONG)ModSectorCount);
    }

    // check aligned with 4KB
    if (IsPartNeed4KBAlign())
    {
        if (ModSectorCount)
        {
            Log("Disk need to align with 4KB %u", (UINT32)ModSectorCount);
            Part1SectorCount -= ModSectorCount;
        }
        else
        {
            Log("no need to align with 4KB");
        }
    }

    memcpy(Head->Signature, "EFI PART", 8);
    Head->Version[2] = 0x01;
    Head->Length = 92;
    Head->Crc = 0;
    Head->EfiStartLBA = 1;
    Head->EfiBackupLBA = DiskSectorCount - 1;
    Head->PartAreaStartLBA = 34;
    Head->PartAreaEndLBA = DiskSectorCount - 34;
    CoCreateGuid(&Head->DiskGuid);
    Head->PartTblStartLBA = 2;
    Head->PartTblTotNum = 128;
    Head->PartTblEntryLen = 128;


    memcpy(&(Table[0].PartType), &WindowsDataPartType, sizeof(GUID));
    CoCreateGuid(&(Table[0].PartGuid));
    Table[0].StartLBA = 2048;
    Table[0].LastLBA = 2048 + Part1SectorCount - 1;
    Table[0].Attr = 0;
    memcpy(Table[0].Name, L"Ventoy", 6 * 2);

    // to fix windows issue
    //memcpy(&(Table[1].PartType), &EspPartType, sizeof(GUID));
    memcpy(&(Table[1].PartType), &WindowsDataPartType, sizeof(GUID));
    CoCreateGuid(&(Table[1].PartGuid));
    Table[1].StartLBA = Table[0].LastLBA + 1;
    Table[1].LastLBA = Table[1].StartLBA + VENTOY_EFI_PART_SIZE / 512 - 1;
    Table[1].Attr = VENTOY_EFI_PART_ATTR;
    memcpy(Table[1].Name, L"VTOYEFI", 7 * 2);

#if 0
	memcpy(&(Table[2].PartType), &BiosGrubPartType, sizeof(GUID));
	CoCreateGuid(&(Table[2].PartGuid));
	Table[2].StartLBA = 34;
	Table[2].LastLBA = 2047;
	Table[2].Attr = 0;
#endif

    //Update CRC
    Head->PartTblCrc = VentoyCrc32(Table, sizeof(pInfo->PartTbl));
    Head->Crc = VentoyCrc32(Head, Head->Length);

    return 0;
}

int VentoyFillBackupGptHead(VTOY_GPT_INFO *pInfo, VTOY_GPT_HDR *pHead)
{
    UINT64 LBA;
    UINT64 BackupLBA;

    memcpy(pHead, &pInfo->Head, sizeof(VTOY_GPT_HDR));

    LBA = pHead->EfiStartLBA;
    BackupLBA = pHead->EfiBackupLBA;
    
    pHead->EfiStartLBA = BackupLBA;
    pHead->EfiBackupLBA = LBA;
    pHead->PartTblStartLBA = BackupLBA + 1 - 33;

    pHead->Crc = 0;
    pHead->Crc = VentoyCrc32(pHead, pHead->Length);

    return 0;
}

