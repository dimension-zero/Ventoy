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

int InstallVentoy2FileImage(PHY_DRIVE_INFO *pPhyDrive, int PartStyle)
{
    int i;
    int rc = 1;
    int Len = 0;
    int dataLen = 0;
    UINT size = 0;
    UINT segnum = 0;
    UINT32 chksum = 0;
    UINT64 data_offset = 0;
    UINT64 Part2StartSector = 0;
    UINT64 Part1StartSector = 0;
    UINT64 Part1SectorCount = 0;
    UINT8 *pData = NULL;    
    UINT8 *pBkGptPartTbl = NULL;
    BYTE *ImgBuf = NULL;
    MBR_HEAD *pMBR = NULL;
    VTSI_FOOTER *pImgFooter = NULL;
    VTSI_SEGMENT *pSegment = NULL;
    VTOY_GPT_INFO *pGptInfo = NULL;
    VTOY_GPT_HDR *pBkGptHdr = NULL;
    FILE *fp = NULL;

    Log("InstallVentoy2FileImage %s PhyDrive%d <<%s %s %dGB>>",
        PartStyle ? "GPT" : "MBR", pPhyDrive->PhyDrive, pPhyDrive->VendorId, pPhyDrive->ProductId,
        GetHumanReadableGBSize(pPhyDrive->SizeInBytes));

    PROGRESS_BAR_SET_POS(PT_LOCK_FOR_CLEAN);

    size = SIZE_1MB + VENTOY_EFI_PART_SIZE + 33 * 512 + VTSI_IMG_MAX_SEG * sizeof(VTSI_SEGMENT) + sizeof(VTSI_FOOTER);

    pData = (UINT8 *)malloc(size);
    if (!pData)
    {
        Log("malloc image buffer failed %d.", size);
        goto End;
    }

    pImgFooter = (VTSI_FOOTER *)(pData + size - sizeof(VTSI_FOOTER));
    pSegment = (VTSI_SEGMENT *)((UINT8 *)pImgFooter - VTSI_IMG_MAX_SEG * sizeof(VTSI_SEGMENT));
    memset(pImgFooter, 0, sizeof(VTSI_FOOTER));
    memset(pSegment, 0, VTSI_IMG_MAX_SEG * sizeof(VTSI_SEGMENT));

    PROGRESS_BAR_SET_POS(PT_WRITE_VENTOY_START);

    Log("Writing Boot Image ............................. ");
    if (ReadWholeFileToBuf(VENTOY_FILE_STG1_IMG, 0, (void **)&ImgBuf, &Len))
    {
        Log("Failed to read stage1 img");
        goto End;
    }

    unxz(ImgBuf, Len, NULL, NULL, pData, &dataLen, unxz_error);
    SAFE_FREE(ImgBuf);

    Log("decompress %s len:%d", VENTOY_FILE_STG1_IMG, dataLen);

    if (PartStyle)
    {
        pData[500] = 35;//update blocklist
        memmove(pData + 34 * 512, pData, SIZE_1MB - 512 * 34);
        memset(pData, 0, 34 * 512);

        pGptInfo = (VTOY_GPT_INFO *)pData;
        memset(pGptInfo, 0, sizeof(VTOY_GPT_INFO));
        VentoyFillGpt(pPhyDrive->SizeInBytes, pGptInfo);

        pBkGptPartTbl = pData + SIZE_1MB + VENTOY_EFI_PART_SIZE;
        memset(pBkGptPartTbl, 0, 33 * 512);

        memcpy(pBkGptPartTbl, pGptInfo->PartTbl, 32 * 512);
        pBkGptHdr = (VTOY_GPT_HDR *)(pBkGptPartTbl + 32 * 512);
        VentoyFillBackupGptHead(pGptInfo, pBkGptHdr);

        Part1StartSector = pGptInfo->PartTbl[0].StartLBA;
        Part1SectorCount = pGptInfo->PartTbl[0].LastLBA - Part1StartSector + 1;
        Part2StartSector = pGptInfo->PartTbl[1].StartLBA;

        Log("Write GPT Info OK ...");
    }
    else
    {
        memmove(pData + 512, pData, SIZE_1MB - 512);
        memset(pData, 0, 512);

        pMBR = (MBR_HEAD *)pData;
        VentoyFillMBR(pPhyDrive->SizeInBytes, pMBR, PartStyle, 0x07);
        Part1StartSector = pMBR->PartTbl[0].StartSectorId;
        Part1SectorCount = pMBR->PartTbl[0].SectorCount;
        Part2StartSector = pMBR->PartTbl[1].StartSectorId;

        Log("Write MBR OK ...");
    }

    Log("Writing EFI part Image ............................. ");
    rc = ReadWholeFileToBuf(VENTOY_FILE_DISK_IMG, 0, (void **)&ImgBuf, &Len);
    if (rc)
    {
        Log("Failed to read img file %p %u", ImgBuf, Len);
        goto End;
    }

    PROGRESS_BAR_SET_POS(PT_WRITE_VENTOY_START + 28);
    memset(g_part_img_buf, 0, sizeof(g_part_img_buf));
    unxz(ImgBuf, Len, NULL, NULL, pData + SIZE_1MB, &dataLen, unxz_error);
    if (dataLen == Len)
    {
        Log("decompress finished success");
        g_part_img_buf[0] = pData + SIZE_1MB;

        VentoyProcSecureBoot(g_SecureBoot);
    }
    else
    {
        Log("decompress finished failed");
        goto End;
    }

    fopen_s(&fp, "VentoySparseImg.vtsi", "wb+");
    if (!fp)
    {
        Log("Failed to create Ventoy img file");
        goto End;
    }

    Log("Writing stage1 data ............................. ");

    fwrite(pData, 1, SIZE_1MB, fp);

    pSegment[0].disk_start_sector = 0;
    pSegment[0].sector_num = SIZE_1MB / 512;
    pSegment[0].data_offset = data_offset;
    data_offset += pSegment[0].sector_num * 512;

    disk_io_set_param(INVALID_HANDLE_VALUE, Part1StartSector + Part1SectorCount);// include the 2048 sector gap
    disk_io_set_imghook(fp, pSegment + 1, VTSI_IMG_MAX_SEG - 1, data_offset);

    Log("Formatting part1 exFAT ...");
    if (0 != FormatPart1exFAT(pPhyDrive->SizeInBytes))
    {
        Log("FormatPart1exFAT failed.");
        disk_io_reset_imghook(&segnum, &data_offset);
        goto End;
    }

    disk_io_reset_imghook(&segnum, &data_offset);
    segnum++;

    Log("current segment number:%d dataoff:%ld", segnum, (long)data_offset);

    //write data
    Log("Writing part2 data ............................. ");
    fwrite(pData + SIZE_1MB, 1, VENTOY_EFI_PART_SIZE, fp);
    pSegment[segnum].disk_start_sector = Part2StartSector;
    pSegment[segnum].sector_num = VENTOY_EFI_PART_SIZE / 512;
    pSegment[segnum].data_offset = data_offset;
    data_offset += pSegment[segnum].sector_num * 512;
    segnum++;

    if (PartStyle)
    {
        Log("Writing backup gpt table ............................. ");
        fwrite(pBkGptPartTbl, 1, 33 * 512, fp);
        pSegment[segnum].disk_start_sector = pPhyDrive->SizeInBytes / 512 - 33;
        pSegment[segnum].sector_num = 33;
        pSegment[segnum].data_offset = data_offset;
        data_offset += pSegment[segnum].sector_num * 512;
        segnum++;
    }

    Log("Writing segment metadata ............................. ");

    for (i = 0; i < (int)segnum; i++)
    {
        Log("SEG[%d]:  PhySector:%llu SectorNum:%llu DataOffset:%llu(sector:%llu)", i, pSegment[i].disk_start_sector, pSegment[i].sector_num,
            pSegment[i].data_offset, pSegment[i].data_offset / 512);
    }

    dataLen = segnum * sizeof(VTSI_SEGMENT);
    fwrite(pSegment, 1, dataLen, fp);

    if (dataLen % 512)
    {
        //pData + SIZE_1MB - 8192 is a temp data buffer with zero
        fwrite(pData + SIZE_1MB - 8192, 1, 512 - (dataLen % 512), fp);
    }

    //Fill footer
    pImgFooter->magic = VTSI_IMG_MAGIC;
    pImgFooter->version = 1;
    pImgFooter->disk_size = pPhyDrive->SizeInBytes;
    memcpy(&pImgFooter->disk_signature, pPhyDrive->MBR.BootCode + 0x1b8, 4);
    pImgFooter->segment_num = segnum;
    pImgFooter->segment_offset = data_offset;

    for (i = 0, chksum = 0; i < (int)(segnum * sizeof(VTSI_SEGMENT)); i++)
    {
        chksum += *((UINT8 *)pSegment + i);
    }
    pImgFooter->segment_chksum = ~chksum;

    for (i = 0, chksum = 0; i < sizeof(VTSI_FOOTER); i++)
    {
        chksum += *((UINT8 *)pImgFooter + i);
    }
    pImgFooter->foot_chksum = ~chksum;

    Log("Writing footer segnum(%u)  segoffset(%llu) ......................", segnum, data_offset);
    Log("disk_size=%llu disk_signature=%lx segment_offset=%llu", pImgFooter->disk_size, pImgFooter->disk_signature, pImgFooter->segment_offset);

    fwrite(pImgFooter, 1, sizeof(VTSI_FOOTER), fp);
    fclose(fp);

    Log("Writing Ventoy image file finished, the file size should be %llu .", data_offset + 512 + ((dataLen + 511) / 512 * 512));

    rc = 0;

End:

    PROGRESS_BAR_SET_POS(PT_MOUNT_VOLUME);
    PROGRESS_BAR_SET_POS(PT_REFORMAT_FINISH);

    Log("retcode:%d\n", rc);

    SAFE_FREE(pData);
    SAFE_FREE(ImgBuf);
    
    return rc;
}


int InstallVentoy2PhyDrive(PHY_DRIVE_INFO *pPhyDrive, int PartStyle, int TryId)
{
    int i;
    int rc = 0;
    int state = 0;
    BOOL ReformatOK;
    HANDLE hDrive;
    DWORD dwSize;
    BOOL bRet;
    CHAR MountDrive;
    CHAR DriveName[] = "?:\\";
    CHAR DriveLetters[MAX_PATH] = { 0 };
    MBR_HEAD MBR;
    VTOY_GPT_INFO *pGptInfo = NULL;
    UINT64 Part1StartSector = 0;
    UINT64 Part1SectorCount = 0;
    UINT64 Part2StartSector = 0;
    BOOL LargeFAT32 = FALSE;
    BOOL DefaultExFAT = FALSE;
    UINT8 FsFlag = 0x07;

	Log("#####################################################");
    Log("InstallVentoy2PhyDrive try%d %s PhyDrive%d <<%s %s %dGB>>", TryId,
        PartStyle ? "GPT" : "MBR", pPhyDrive->PhyDrive, pPhyDrive->VendorId, pPhyDrive->ProductId,
        GetHumanReadableGBSize(pPhyDrive->SizeInBytes));
	Log("#####################################################");

    if (PartStyle)
    {
        pGptInfo = malloc(sizeof(VTOY_GPT_INFO));
        memset(pGptInfo, 0, sizeof(VTOY_GPT_INFO));
    }

    PROGRESS_BAR_SET_POS(PT_LOCK_FOR_CLEAN);

    if (PartStyle)
    {
        VentoyFillGpt(pPhyDrive->SizeInBytes, pGptInfo);
        Part1StartSector = pGptInfo->PartTbl[0].StartLBA;
        Part1SectorCount = pGptInfo->PartTbl[0].LastLBA - Part1StartSector + 1;
        Part2StartSector = pGptInfo->PartTbl[1].StartLBA;
    }
    else
    {
        if (GetVentoyFsType() == VTOY_FS_FAT32)
        {
            FsFlag = 0x0C;
        }

        VentoyFillMBR(pPhyDrive->SizeInBytes, &MBR, PartStyle, FsFlag);
        Part1StartSector = MBR.PartTbl[0].StartSectorId;
        Part1SectorCount = MBR.PartTbl[0].SectorCount;
        Part2StartSector = MBR.PartTbl[1].StartSectorId;
    }

    Log("Lock disk for clean ............................. ");

    hDrive = GetPhysicalHandle(pPhyDrive->PhyDrive, TRUE, FALSE, FALSE);
    if (hDrive == INVALID_HANDLE_VALUE)
    {
        Log("Failed to open physical disk");
        free(pGptInfo);
        return 1;
    }

    GetLettersBelongPhyDrive(pPhyDrive->PhyDrive, DriveLetters, sizeof(DriveLetters));

    if (DriveLetters[0] == 0)
    {
        Log("No drive letter was assigned...");
        DriveName[0] = GetFirstUnusedDriveLetter();
        Log("GetFirstUnusedDriveLetter %C: ...", DriveName[0]);
    }
    else
    {
        // Unmount all mounted volumes that belong to this drive
        // Do it in reverse so that we always end on the first volume letter
        for (i = (int)strlen(DriveLetters); i > 0; i--)
        {
            DriveName[0] = DriveLetters[i - 1];
            bRet = DeleteVolumeMountPointA(DriveName);
            Log("Delete mountpoint %s ret:%u code:%u", DriveName, bRet, GetLastError());
        }
    }

    MountDrive = DriveName[0];
    Log("Will use '%C:' as volume mountpoint", DriveName[0]);

    // It kind of blows, but we have to relinquish access to the physical drive
    // for VDS to be able to delete the partitions that reside on it...
    DeviceIoControl(hDrive, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &dwSize, NULL);
    CHECK_CLOSE_HANDLE(hDrive);

    PROGRESS_BAR_SET_POS(PT_DEL_ALL_PART);

    if (!VDS_DeleteAllPartitions(pPhyDrive->PhyDrive))
    {
        Log("Notice: Could not delete partitions: 0x%x, but we continue.", GetLastError());
    }

    Log("Deleting all partitions ......................... OK");

    PROGRESS_BAR_SET_POS(PT_LOCK_FOR_WRITE);

    Log("Lock disk for write ............................. ");
    hDrive = GetPhysicalHandle(pPhyDrive->PhyDrive, TRUE, TRUE, FALSE);
    if (hDrive == INVALID_HANDLE_VALUE)
    {
        Log("Failed to GetPhysicalHandle for write.");
        rc = 1;
        goto End;
    }

    //Refresh Drive Layout
    DeviceIoControl(hDrive, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, &dwSize, NULL);

    disk_io_set_param(hDrive, Part1StartSector + Part1SectorCount);// include the 2048 sector gap

    PROGRESS_BAR_SET_POS(PT_FORMAT_PART1);

    if (PartStyle == 1 && pPhyDrive->PartStyle == 0)
    {
        Log("Wait for format part1 ...");
        Sleep(1000 * 5);
    }

    if (GetVentoyFsType() == VTOY_FS_FAT32 && (Part1SectorCount * 512 >= FAT32_MAX_LIMIT))
    {
        Log("Formatting part1 large FAT32 ...");
        LargeFAT32 = TRUE;
        if (0 != FormatPart1LargeFAT32(pPhyDrive->SizeInBytes, GetClusterSize()))
        {
            Log("FormatPart1LargeFAT32 failed.");
            rc = 1;
            goto End;
        }
    }
    else if (GetVentoyFsType() == VTOY_FS_EXFAT && GetClusterSize() == 0)
    {
        Log("Formatting part1 exFAT ...");
        DefaultExFAT = TRUE;
        if (0 != FormatPart1exFAT(pPhyDrive->SizeInBytes))
        {
            Log("FormatPart1exFAT failed.");
            rc = 1;
            goto End;
        }
    }
    else
    {
        Log("Zero part1 file system ...");
        if (0 != ZeroPart1FileSystem(hDrive, Part2StartSector))
        {
            Log("ZeroPart1FileSystem failed.");
            rc = 1;
            goto End;
        }
    }

    PROGRESS_BAR_SET_POS(PT_FORMAT_PART2);
    Log("Writing part2 FAT img ...");
    
    if (0 != FormatPart2Fat(hDrive, Part2StartSector))
    {
        Log("FormatPart2Fat failed.");
        rc = 1;
        goto End;
    }

    PROGRESS_BAR_SET_POS(PT_WRITE_STG1_IMG);
    Log("Writing Boot Image ............................. ");
    if (WriteGrubStage1ToPhyDrive(hDrive, PartStyle) != 0)
    {
        Log("WriteGrubStage1ToPhyDrive failed.");
        rc = 1;
        goto End;
    }

    PROGRESS_BAR_SET_POS(PT_WRITE_PART_TABLE);
    Log("Writing Partition Table ........................ ");
    SetFilePointer(hDrive, 0, NULL, FILE_BEGIN);

    if (PartStyle)
    {
        VTOY_GPT_HDR BackupHead;
        LARGE_INTEGER liCurrentPosition;

        SET_FILE_POS(pPhyDrive->SizeInBytes - 512);
        VentoyFillBackupGptHead(pGptInfo, &BackupHead);
        if (!WriteFile(hDrive, &BackupHead, sizeof(VTOY_GPT_HDR), &dwSize, NULL))
        {
            rc = 1;
            Log("Write GPT Backup Head Failed, dwSize:%u (%u) ErrCode:%u", dwSize, sizeof(VTOY_GPT_INFO), GetLastError());
            goto End;
        }

        SET_FILE_POS(pPhyDrive->SizeInBytes - 512 * 33);
        if (!WriteFile(hDrive, pGptInfo->PartTbl, sizeof(pGptInfo->PartTbl), &dwSize, NULL))
        {
            rc = 1;
            Log("Write GPT Backup Part Table Failed, dwSize:%u (%u) ErrCode:%u", dwSize, sizeof(VTOY_GPT_INFO), GetLastError());
            goto End;
        }

        SET_FILE_POS(0);
        if (!WriteFile(hDrive, pGptInfo, sizeof(VTOY_GPT_INFO), &dwSize, NULL))
        {
            rc = 1;
            Log("Write GPT Info Failed, dwSize:%u (%u) ErrCode:%u", dwSize, sizeof(VTOY_GPT_INFO), GetLastError());
            goto End;
        }

        Log("Write GPT Info OK ...");
        memcpy(&(pPhyDrive->MBR), &(pGptInfo->MBR), 512);
    }
    else
    {
        if (!WriteFile(hDrive, &MBR, sizeof(MBR), &dwSize, NULL))
        {
            rc = 1;
            Log("Write MBR Failed, dwSize:%u ErrCode:%u", dwSize, GetLastError());
            goto End;
        }
        Log("Write MBR OK ...");
        memcpy(&(pPhyDrive->MBR), &MBR, 512);
    }

    //Refresh Drive Layout
    DeviceIoControl(hDrive, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, &dwSize, NULL);

End:

    PROGRESS_BAR_SET_POS(PT_MOUNT_VOLUME);

    if (rc == 0)
    {
        Log("Mounting Ventoy Partition ....................... ");
        Sleep(1000);

        state = 0;
        memset(DriveLetters, 0, sizeof(DriveLetters));
        GetLettersBelongPhyDrive(pPhyDrive->PhyDrive, DriveLetters, sizeof(DriveLetters));
        Log("Logical drive letter after write ventoy: <%s>", DriveLetters);

        for (i = 0; i < sizeof(DriveLetters) && DriveLetters[i]; i++)
        {
            DriveName[0] = DriveLetters[i];
            if (IsVentoyLogicalDrive(DriveName[0]))
            {
                Log("%s is ventoy part2, delete mountpoint", DriveName);
                DeleteVolumeMountPointA(DriveName);
            }
            else
            {
                Log("%s is ventoy part1, already mounted", DriveName);
                MountDrive = DriveName[0];
                state = 1;
            }
        }

        if (state != 1)
        {
            Log("need to mount ventoy part1...");
            
            if (0 == GetVentoyVolumeName(pPhyDrive->PhyDrive, Part1StartSector, DriveLetters, sizeof(DriveLetters), FALSE))
            {
                DriveName[0] = MountDrive;
                bRet = SetVolumeMountPointA(DriveName, DriveLetters);
                Log("SetVolumeMountPoint <%s> <%s> bRet:%u code:%u", DriveName, DriveLetters, bRet, GetLastError());

                if (bRet)
                {
                    state = 1;
                }
            }
            else
            {
                Log("Failed to find ventoy volume");
            }
        }

        // close handle, or it will deny reformat
        Log("Close handle ...");
        CHECK_CLOSE_HANDLE(hDrive);

        ReformatOK = TRUE;

        if (state)
        {
            if (LargeFAT32)
            {
                Log("No need to reformat for large FAT32");
                pPhyDrive->VentoyFsClusterSize = GetVolumeClusterSize(MountDrive);
            }
            else if (DefaultExFAT)
            {
                Log("No need to reformat for default exfat");
                pPhyDrive->VentoyFsClusterSize = GetVolumeClusterSize(MountDrive);
            }
            else
            {
                bRet = DISK_FormatVolume(MountDrive, GetVentoyFsType(), Part1SectorCount * 512);
                for (i = 0; bRet == FALSE && i < 2; i++)
                {
                    Log("Wait and retry reformat ...");
                    Sleep(1000);
                    bRet = DISK_FormatVolume(MountDrive, GetVentoyFsType(), Part1SectorCount * 512);
                }

                if (bRet)
                {
                    Log("Reformat %C:\\ to %s SUCCESS", MountDrive, GetVentoyFsName());
                    pPhyDrive->VentoyFsClusterSize = GetVolumeClusterSize(MountDrive);

                    if ((GetVentoyFsType() != VTOY_FS_UDF) && (pPhyDrive->VentoyFsClusterSize < 2048))
                    {
                        for (i = 0; i < 10; i++)
                        {
                            Log("### Invalid cluster size %d ###", pPhyDrive->VentoyFsClusterSize);
                        }
                    }
                }
                else
                {
                    ReformatOK = FALSE;
                    Log("Reformat %C:\\ to %s FAILED", MountDrive, GetVentoyFsName());
                }
            }
        }
        else
        {
            Log("Can not reformat %s to %s", DriveName, GetVentoyFsName());
        }

        if (!ReformatOK)
        {
            Log("Format to exfat with built-in algorithm");

            hDrive = GetPhysicalHandle(pPhyDrive->PhyDrive, TRUE, TRUE, FALSE);
            if (hDrive == INVALID_HANDLE_VALUE)
            {
                Log("Failed to GetPhysicalHandle for write.");
            }
            else
            {
                if (0 != FormatPart1exFAT(pPhyDrive->SizeInBytes))
                {
                    Log("FormatPart1exFAT SUCCESS.");
                }
                else
                {
                    Log("FormatPart1exFAT FAILED.");
                }

                CHECK_CLOSE_HANDLE(hDrive);
            }
        }

        Log("OK\n");
    }
    else
    {
		PROGRESS_BAR_SET_POS(PT_LOCK_FOR_CLEAN);

        FindProcessOccupyDisk(hDrive, pPhyDrive);

		if (!VDS_IsLastAvaliable())
		{
			Log("###### [Error:] Virtual Disk Service (VDS) Unavailable ######");
			Log("###### [Error:] Virtual Disk Service (VDS) Unavailable ######");
			Log("###### [Error:] Virtual Disk Service (VDS) Unavailable ######");
			Log("###### [Error:] Virtual Disk Service (VDS) Unavailable ######");
			Log("###### [Error:] Virtual Disk Service (VDS) Unavailable ######");
		}

        CHECK_CLOSE_HANDLE(hDrive);
    }

    if (pGptInfo)
    {
        free(pGptInfo);
    }
    
    return rc;
}


