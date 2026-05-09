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

static int disk_xz_flush(void *src, unsigned int size)
{
    unsigned int i;
    BYTE *buf = (BYTE *)src;

    for (i = 0; i < size; i++)
    {
        *g_part_img_pos = *buf++;

        g_disk_unxz_len++;
        if ((g_disk_unxz_len % SIZE_1MB) == 0)
        {
            g_part_img_pos = g_part_img_buf[g_disk_unxz_len / SIZE_1MB];
        }
        else
        {
            g_part_img_pos++;
        }
    }

    return (int)size;
}

void unxz_error(char *x)
{
    Log("%s", x);
}

BOOL TryWritePart2(HANDLE hDrive, UINT64 StartSectorId)
{
    BOOL bRet;
    DWORD TrySize = 16 * 1024;
    DWORD dwSize;
    BYTE *Buffer = NULL;
    unsigned char *data = NULL;
    LARGE_INTEGER liCurrentPosition;

    liCurrentPosition.QuadPart = StartSectorId * 512;
    SetFilePointerEx(hDrive, liCurrentPosition, &liCurrentPosition, FILE_BEGIN);
    
    Buffer = malloc(TrySize);

    bRet = WriteFile(hDrive, Buffer, TrySize, &dwSize, NULL);

    free(Buffer);

    Log("Try write part2 bRet:%u dwSize:%u code:%u", bRet, dwSize, LASTERR);

    if (bRet && dwSize == TrySize)
    {
        return TRUE;
    }

    return FALSE;
}

int FormatPart2Fat(HANDLE hDrive, UINT64 StartSectorId)
{
    int i;
    int rc = 0;
    int len = 0;
    int writelen = 0;
    int partwrite = 0;
    int Pos = PT_WRITE_VENTOY_START;
    DWORD dwSize = 0;
    BOOL bRet;
    unsigned char *data = NULL;
    LARGE_INTEGER liCurrentPosition;
	LARGE_INTEGER liNewPosition;
    BYTE *CheckBuf = NULL;

	Log("FormatPart2Fat %llu...", (ULONGLONG)StartSectorId);

    CheckBuf = malloc(SIZE_1MB);
    if (!CheckBuf)
    {
        Log("Failed to malloc check buf");
        return 1;
    }

    rc = ReadWholeFileToBuf(VENTOY_FILE_DISK_IMG, 0, (void **)&data, &len);
    if (rc)
    {
        Log("Failed to read img file %p %u", data, len);
        free(CheckBuf);
        return 1;
    }

    liCurrentPosition.QuadPart = StartSectorId * 512;
    SetFilePointerEx(hDrive, liCurrentPosition, &liNewPosition, FILE_BEGIN);

    memset(g_part_img_buf, 0, sizeof(g_part_img_buf));

    g_part_img_buf[0] = (BYTE *)malloc(VENTOY_EFI_PART_SIZE);
    if (g_part_img_buf[0])
    {
        Log("Malloc whole img buffer success, now decompress ...");
        unxz(data, len, NULL, NULL, g_part_img_buf[0], &writelen, unxz_error);

        if (len == writelen)
        {
            Log("decompress finished success");

			VentoyProcSecureBoot(g_SecureBoot);

            for (i = 0; i < VENTOY_EFI_PART_SIZE / SIZE_1MB; i++)
            {
                dwSize = 0;
				bRet = WriteFile(hDrive, g_part_img_buf[0] + i * SIZE_1MB, SIZE_1MB, &dwSize, NULL);
                Log("Write part data bRet:%u dwSize:%u code:%u", bRet, dwSize, LASTERR);

                if (!bRet)
                {
                    rc = 1;
                    goto End;
                }

                PROGRESS_BAR_SET_POS(Pos);
                if (i % 2 == 0)
                {
                    Pos++;
                }
            }

            //Read and check the data
            liCurrentPosition.QuadPart = StartSectorId * 512;
            SetFilePointerEx(hDrive, liCurrentPosition, &liNewPosition, FILE_BEGIN);

            for (i = 0; i < VENTOY_EFI_PART_SIZE / SIZE_1MB; i++)
            {
                bRet = ReadFile(hDrive, CheckBuf, SIZE_1MB, &dwSize, NULL);
                Log("Read part data bRet:%u dwSize:%u code:%u", bRet, dwSize, LASTERR);

                if (!bRet || memcmp(CheckBuf, g_part_img_buf[0] + i * SIZE_1MB, SIZE_1MB))
                {
                    Log("### [Check Fail] The data write and read does not match");
                    rc = 1;
                    goto End;
                }

                PROGRESS_BAR_SET_POS(Pos);
                if (i % 2 == 0)
                {
                    Pos++;
                }
            }
        }
        else
        {
            rc = 1;
            Log("decompress finished failed");
            goto End;
        }
    }
    else
    {
        Log("Failed to malloc whole img size %u, now split it", VENTOY_EFI_PART_SIZE);

        partwrite = 1;
        for (i = 0; i < VENTOY_EFI_PART_SIZE / SIZE_1MB; i++)
        {
            g_part_img_buf[i] = (BYTE *)malloc(SIZE_1MB);
            if (g_part_img_buf[i] == NULL)
            {
                rc = 1;
                goto End;
            }
        }

        Log("Malloc part img buffer success, now decompress ...");

        g_part_img_pos = g_part_img_buf[0];

        unxz(data, len, NULL, disk_xz_flush, NULL, NULL, unxz_error);

        if (g_disk_unxz_len == VENTOY_EFI_PART_SIZE)
        {
            Log("decompress finished success");
			
			VentoyProcSecureBoot(g_SecureBoot);

            for (i = 0; i < VENTOY_EFI_PART_SIZE / SIZE_1MB; i++)
            {
                dwSize = 0;
                bRet = WriteFile(hDrive, g_part_img_buf[i], SIZE_1MB, &dwSize, NULL);
                Log("Write part data bRet:%u dwSize:%u code:%u", bRet, dwSize, LASTERR);

                if (!bRet)
                {
                    rc = 1;
                    goto End;
                }
                
                PROGRESS_BAR_SET_POS(Pos);
                if (i % 2 == 0)
                {
                    Pos++;
                }
            }

            //Read and check the data
            liCurrentPosition.QuadPart = StartSectorId * 512;
            SetFilePointerEx(hDrive, liCurrentPosition, &liNewPosition, FILE_BEGIN);

            for (i = 0; i < VENTOY_EFI_PART_SIZE / SIZE_1MB; i++)
            {
                bRet = ReadFile(hDrive, CheckBuf, SIZE_1MB, &dwSize, NULL);
                Log("Read part data bRet:%u dwSize:%u code:%u", bRet, dwSize, LASTERR);

                if (!bRet || memcmp(CheckBuf, g_part_img_buf[i], SIZE_1MB))
                {
                    Log("### [Check Fail] The data write and read does not match");
                    rc = 1;
                    goto End;
                }

                PROGRESS_BAR_SET_POS(Pos);
                if (i % 2 == 0)
                {
                    Pos++;
                }
            }
        }
        else
        {
            rc = 1;
            Log("decompress finished failed");
            goto End;
        }
    }

End:

    if (data) free(data);
    if (CheckBuf)free(CheckBuf);

    if (partwrite)
    {
        for (i = 0; i < VENTOY_EFI_PART_SIZE / SIZE_1MB; i++)
        {
            if (g_part_img_buf[i]) free(g_part_img_buf[i]);
        }
    }
    else
    {
        if (g_part_img_buf[0]) free(g_part_img_buf[0]);
    }

    return rc;
}

int WriteGrubStage1ToPhyDrive(HANDLE hDrive, int PartStyle)
{
    int Len = 0;
    int readLen = 0;
    BOOL bRet;
    DWORD dwSize;
    BYTE *ImgBuf = NULL;
    BYTE *RawBuf = NULL;

    Log("WriteGrubStage1ToPhyDrive ...");

    RawBuf = (BYTE *)malloc(SIZE_1MB);
    if (!RawBuf)
    {
        return 1;
    }

    if (ReadWholeFileToBuf(VENTOY_FILE_STG1_IMG, 0, (void **)&ImgBuf, &Len))
    {
        Log("Failed to read stage1 img");
        free(RawBuf);
        return 1;
    }

    unxz(ImgBuf, Len, NULL, NULL, RawBuf, &readLen, unxz_error);

    if (PartStyle)
    {
        Log("Write GPT stage1 ...");
        RawBuf[500] = 35;//update blocklist
        SetFilePointer(hDrive, 512 * 34, NULL, FILE_BEGIN);        
        bRet = WriteFile(hDrive, RawBuf, SIZE_1MB - 512 * 34, &dwSize, NULL);
    }
    else
    {
        Log("Write MBR stage1 ...");
        SetFilePointer(hDrive, 512, NULL, FILE_BEGIN);
        bRet = WriteFile(hDrive, RawBuf, SIZE_1MB - 512, &dwSize, NULL);
    }

    Log("WriteFile Ret:%u dwSize:%u ErrCode:%u", bRet, dwSize, GetLastError());

    free(RawBuf);
    free(ImgBuf);
    return 0;
}


int FormatPart1LargeFAT32(UINT64 DiskSizeBytes, int CluserSize)
{
    MKFS_PARM Option;
    FRESULT Ret;
    FATFS FS;

    Option.fmt = FM_FAT32;
    Option.n_fat = 1;
    Option.align = 8;
    Option.n_root = 1;

    if (CluserSize == 0)
    {
        // < 32GB select 32KB as cluster size
        // > 32GB select 128KB as cluster size
        if (DiskSizeBytes / 1024 / 1024 / 1024 <= 32)
        {
            Option.au_size = 32768;
        }
        else
        {
            Option.au_size = 131072;
        }
    }
    else
    {
        Option.au_size = CluserSize;
    }

    Log("Formatting Part1 large FAT32 ClusterSize:%u(%uKB) ...", CluserSize, CluserSize / 1024);

    disk_io_reset_write_error();

    Ret = f_mkfs(TEXT("0:"), &Option, 0, 8 * 1024 * 1024);
    if (FR_OK == Ret)
    {
        if (disk_io_is_write_error())
        {
            Log("Formatting Part1 large FAT32 failed, write error.");
            return 1;
        }

        Log("Formatting Part1 large FAT32 success, now set label");
        
        Ret = f_mount(&FS, TEXT("0:"), 1);
        if (FR_OK == Ret)
        {
            Log("f_mount SUCCESS");
            Ret = f_setlabel(TEXT("0:Ventoy"));
            if (FR_OK == Ret)
            {
                Log("f_setlabel SUCCESS");
                Ret = f_unmount(TEXT("0:"));
                Log("f_unmount %d %s", Ret, (FR_OK == Ret) ? "SUCCESS" : "FAILED");
            }
            else
            {
                Log("f_setlabel failed %d", Ret);
            }
        }
        else
        {
            Log("f_mount failed %d", Ret);
        }

        return 0;
    }
    else
    {
        Log("Formatting Part1 large FAT32 failed");
        return 1;
    }
}

int FormatPart1exFAT(UINT64 DiskSizeBytes)
{
    MKFS_PARM Option;
    FRESULT Ret;

    Option.fmt = FM_EXFAT;
    Option.n_fat = 1;
    Option.align = 8;
    Option.n_root = 1;

    // < 32GB select 32KB as cluster size
    // > 32GB select 128KB as cluster size
    if (DiskSizeBytes / 1024 / 1024 / 1024 <= 32)
    {
        Option.au_size = 32768;
    }
    else
    {
        Option.au_size = 131072;
    }

    Log("Formatting Part1 exFAT ...");

	disk_io_reset_write_error();

    Ret = f_mkfs(TEXT("0:"), &Option, 0, 8 * 1024 * 1024);
    if (FR_OK == Ret)
    {
		if (disk_io_is_write_error())
		{
			Log("Formatting Part1 exFAT failed, write error.");
			return 1;
		}

        Log("Formatting Part1 exFAT success");
        return 0;
    }
    else
    {
        Log("Formatting Part1 exFAT failed");
        return 1;
    }
}

int ZeroPart1FileSystem(HANDLE hDrive, UINT64 Part2StartSector)
{
    int i;
    DWORD dwSize = 0;
    LARGE_INTEGER liCurPos;
    LARGE_INTEGER liNewPos;
    CHAR TmpBuffer[1024] = { 0 };

    liCurPos.QuadPart = VENTOY_PART1_START_SECTOR * 512;
    liNewPos.QuadPart = 0;
    if (0 == SetFilePointerEx(hDrive, liCurPos, &liNewPos, FILE_BEGIN) ||
        liNewPos.QuadPart != liCurPos.QuadPart)
    {
        Log("SetFilePointerEx Failed %u %llu %llu", LASTERR, (ULONGLONG)liCurPos.QuadPart, (ULONGLONG)liNewPos.QuadPart);
        return 1;
    }

    for (i = 0; i < 1024; i++)
    {
        WriteFile(hDrive, TmpBuffer, 1024, &dwSize, NULL);
    }

    liCurPos.QuadPart = (Part2StartSector * 512) - (1024 * 1024);
    liNewPos.QuadPart = 0;
    if (0 == SetFilePointerEx(hDrive, liCurPos, &liNewPos, FILE_BEGIN) ||
        liNewPos.QuadPart != liCurPos.QuadPart)
    {
        Log("SetFilePointerEx Failed %u %llu %llu", LASTERR, (ULONGLONG)liCurPos.QuadPart, (ULONGLONG)liNewPos.QuadPart);
        return 1;
    }

    for (i = 0; i < 1024; i++)
    {
        WriteFile(hDrive, TmpBuffer, 1024, &dwSize, NULL);
    }

    Log("Zero Part1 SUCCESS");
    return 0;
}

