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

static HANDLE g_FatPhyDrive;
static UINT64 g_Part2StartSec;
static int GetVentoyVersionFromFatFile(CHAR *VerBuf, size_t BufLen)
{
    int rc = 1;
    int size = 0;
    char *buf = NULL;
    void *flfile = NULL;

    flfile = fl_fopen("/grub/grub.cfg", "rb");
    if (flfile)
    {
        fl_fseek(flfile, 0, SEEK_END);
        size = (int)fl_ftell(flfile);

        fl_fseek(flfile, 0, SEEK_SET);

        buf = (char *)malloc(size + 1);
        if (buf)
        {
            fl_fread(buf, 1, size, flfile);
            buf[size] = 0;

            rc = 0;
            sprintf_s(VerBuf, BufLen, "%s", ParseVentoyVersionFromString(buf));
            free(buf);
        }

        fl_fclose(flfile);
    }

    return rc;
}

static int VentoyFatDiskRead(uint32 Sector, uint8 *Buffer, uint32 SectorCount)
{
    DWORD dwSize;
    BOOL bRet;
    DWORD ReadSize;
    LARGE_INTEGER liCurrentPosition;

    liCurrentPosition.QuadPart = Sector + g_Part2StartSec;
    liCurrentPosition.QuadPart *= 512;
    SetFilePointerEx(g_FatPhyDrive, liCurrentPosition, &liCurrentPosition, FILE_BEGIN);

    ReadSize = (DWORD)(SectorCount * 512);

    bRet = ReadFile(g_FatPhyDrive, Buffer, ReadSize, &dwSize, NULL);
    if (bRet == FALSE || dwSize != ReadSize)
    {
        Log("ReadFile error bRet:%u WriteSize:%u dwSize:%u ErrCode:%u\n", bRet, ReadSize, dwSize, LASTERR);
    }

    return 1;
}


int GetVentoyVerInPhyDrive(const PHY_DRIVE_INFO *pDriveInfo, UINT64 Part2StartSector, CHAR *VerBuf, size_t BufLen, BOOL *pSecureBoot)
{
    int rc = 0;
    HANDLE hDrive;
    void *flfile;

    hDrive = GetPhysicalHandle(pDriveInfo->PhyDrive, FALSE, FALSE, FALSE);
    if (hDrive == INVALID_HANDLE_VALUE)
    {
        return 1;
    }
    
    g_FatPhyDrive = hDrive;
	g_Part2StartSec = Part2StartSector;

    Log("Parse FAT fs...");

    fl_init();

    if (0 == fl_attach_media(VentoyFatDiskRead, NULL))
    {
        Log("attach media success...");
        rc = GetVentoyVersionFromFatFile(VerBuf, BufLen);
    }
    else
    {
        Log("attach media failed...");
        rc = 1;
    }

    Log("GetVentoyVerInPhyDrive rc=%d...", rc);
    if (rc == 0)
    {
        Log("VentoyVerInPhyDrive %d is <%s>...", pDriveInfo->PhyDrive, VerBuf);

        flfile = fl_fopen("/EFI/BOOT/grubx64_real.efi", "rb");
        if (flfile)
        {
            *pSecureBoot = TRUE;
            fl_fclose(flfile);
        }
    }

    fl_shutdown();

    CHECK_CLOSE_HANDLE(hDrive);

    return rc;
}




