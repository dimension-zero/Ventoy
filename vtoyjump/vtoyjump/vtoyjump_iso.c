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
int GetIsoId(CONST CHAR *IsoPath, IsoId *ids)
{
    int i;
    int n = 0;
    HANDLE hFile;
    DWORD dwSize = 0;
    BOOL bRet[8];

    hFile = CreateFileA(IsoPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return 1;
    }

    SetFilePointer(hFile, 2048 * 16 + 8, NULL, FILE_BEGIN);
    bRet[n++] = ReadFile(hFile, ids->SystemId, 32, &dwSize, NULL);
    
    SetFilePointer(hFile, 2048 * 16 + 40, NULL, FILE_BEGIN);
    bRet[n++] = ReadFile(hFile, ids->VolumeId, 32, &dwSize, NULL);

    SetFilePointer(hFile, 2048 * 16 + 318, NULL, FILE_BEGIN);
    bRet[n++] = ReadFile(hFile, ids->PulisherId, 128, &dwSize, NULL);

    SetFilePointer(hFile, 2048 * 16 + 446, NULL, FILE_BEGIN);
    bRet[n++] = ReadFile(hFile, ids->PreparerId, 128, &dwSize, NULL);

    CloseHandle(hFile);


    for (i = 0; i < n; i++)
    {
        if (bRet[i] == FALSE)
        {
            return 1;
        }
    }


    TrimString(ids->SystemId, FALSE);
    TrimString(ids->VolumeId, FALSE);
    TrimString(ids->PulisherId, FALSE);
    TrimString(ids->PreparerId, FALSE);

    Log("ISO ID: System<%s> Volume<%s> Pulisher<%s> Preparer<%s>", 
        ids->SystemId, ids->VolumeId, ids->PulisherId, ids->PreparerId);

    return 0;
}

int CheckSkipMountIso(CONST CHAR *IsoPath)
{
    BOOL InRoot = FALSE;
    int slashcnt = 0;
    CONST CHAR *p = NULL;
    IsoId ID;

    // C:\\xxx
    for (p = IsoPath; *p; p++)
    {
        if (*p == '\\' || *p == '/')
        {
            slashcnt++;
        }
    }

    if (slashcnt == 2)
    {
        InRoot = TRUE;
    }

    memset(&ID, 0, sizeof(ID));
    if (GetIsoId(IsoPath, &ID))
    {
        return 0;
    }

    //Bob.Ombs.Modified.Win10PEx64.iso will auto find ISO file in root, so we can skip the mount
    if (InRoot && strcmp(ID.VolumeId, "Modified-Win10PEx64") == 0)
    {
        return 1;
    }

    return 0;
}

int MountIsoFile(CONST CHAR *IsoPath, DWORD PhyDrive)
{
    if (CheckSkipMountIso(IsoPath))
    {
        Log("Skip mount ISO file for <%s>", IsoPath);
        return 0;
    }

    if (IsWindows8OrGreater())
    {
        Log("This is Windows 8 or latter...");
        if (VentoyMountISOByAPI(IsoPath) == 0)
        {
            Log("Mount iso by API success");
            return 0;
        }
        else
        {
            Log("Mount iso by API failed, maybe not supported, try imdisk");
            return VentoyMountISOByImdisk(IsoPath, PhyDrive);
        }
    }
    else
    {
        Log("This is before Windows 8 ...");
        if (VentoyMountISOByImdisk(IsoPath, PhyDrive) == 0)
        {
            Log("Mount iso by imdisk success");
            return 0;
        }
        else
        {
            return VentoyMountISOByAPI(IsoPath);
        }
    }
}

