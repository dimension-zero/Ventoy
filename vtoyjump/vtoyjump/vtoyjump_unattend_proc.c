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

