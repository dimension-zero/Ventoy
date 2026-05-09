#include <Windows.h>
#include <stdio.h>
#include "Ventoy2Disk.h"
#include "DiskService.h"
#include "ventoy_cli.h"

void CLI_PrintHelp(void)
{
    CLIPrint("Usage: Ventoy2Disk.exe VTOYCLI [CMD] [OPTION] { /Drive:F: | /PhyDrive:N }");
    CLIPrint("");
    CLIPrint("CMD (one required):");
    CLIPrint("  /I          Force install (overwrites existing; no prompts)");
    CLIPrint("  /SI         Safe install (fails if Ventoy present; prompts for confirmation)");
    CLIPrint("  /U          Update Ventoy on disk");
    CLIPrint("  /L          List Ventoy info on disk (read-only)");
    CLIPrint("  /V          Print local Ventoy version and exit");
    CLIPrint("  /HELP /?    Show this help");
    CLIPrint("");
    CLIPrint("OPTION (optional):");
    CLIPrint("  /GPT           GPT partition style (default: MBR; install only)");
    CLIPrint("  /NoSB          Disable secure boot support (default: enabled)");
    CLIPrint("  /SB            Enable secure boot support explicitly");
    CLIPrint("  /R:SIZE_MB     Reserve SIZE_MB megabytes at end of disk");
    CLIPrint("  /FS:TYPE       Filesystem: exFAT (default), NTFS, FAT32, UDF");
    CLIPrint("  /Label:NAME    Volume label for partition 1 (default: Ventoy)");
    CLIPrint("  /NoUSBCheck    Allow installation on non-USB drives");
    CLIPrint("  /NonDest       Non-destructive install (resize existing partition)");
    CLIPrint("  /Y             Auto-confirm prompts (for scripted/silent use)");
    CLIPrint("");
    CLIPrint("Drive (required for all CMDs except /V):");
    CLIPrint("  /Drive:F:      Select by logical drive letter");
    CLIPrint("  /PhyDrive:N    Select by physical drive number");
}

int CLI_CheckParam(int argc, char** argv, PHY_DRIVE_INFO* pDrvInfo, CLI_CFG *pCfg)
{
    int i;
    int fstype = VTOY_FS_EXFAT;
    int op = -1;
    char* opt = NULL;
    int PhyDrive = -1;
    int PartStyle = 0;
    int ReserveMB = 0;
    BOOL USBCheck = TRUE;
    BOOL NonDest = FALSE;
    MBR_HEAD MBR;
    UINT64 Part2GPTAttr = 0;
    UINT64 Part2StartSector = 0;

    for (i = 0; i < argc; i++)
    {
        opt = argv[i];
        if (_stricmp(opt, "/HELP") == 0 || _stricmp(opt, "/?") == 0)
        {
            op = CLI_OP_HELP;
        }
        else if (_stricmp(opt, "/V") == 0)
        {
            op = CLI_OP_VERSION;
        }
        else if (_stricmp(opt, "/L") == 0)
        {
            op = CLI_OP_LIST;
        }
        else if (_stricmp(opt, "/SI") == 0)
        {
            op = CLI_OP_SAFEINSTALL;
        }
        else if (_stricmp(opt, "/I") == 0)
        {
            op = CLI_OP_INSTALL;
        }
        else if (_stricmp(opt, "/U") == 0)
        {
            op = CLI_OP_UPDATE;
        }
        else if (_stricmp(opt, "/GPT") == 0)
        {
            PartStyle = 1;
        }
        else if (_stricmp(opt, "/NoSB") == 0)
        {
            g_SecureBoot = FALSE;
        }
        else if (_stricmp(opt, "/SB") == 0)
        {
            g_SecureBoot = TRUE;
        }
        else if (_stricmp(opt, "/Y") == 0)
        {
            g_NoNeedInputYes = 1;
        }
        else if (_stricmp(opt, "/NoUSBCheck") == 0)
        {
            USBCheck = FALSE;
        }
        else if (_stricmp(opt, "/NonDest") == 0)
        {
            NonDest = TRUE;
        }
        else if (_strnicmp(opt, "/Drive:", 7) == 0)
        {
            Log("Get PhyDrive by logical drive %C:", opt[7]);
            PhyDrive = GetPhyDriveByLogicalDrive(opt[7], NULL);
        }
        else if (_strnicmp(opt, "/PhyDrive:", 10) == 0)
        {
            PhyDrive = (int)strtol(opt + 10, NULL, 10);
        }
        else if (_strnicmp(opt, "/R:", 3) == 0)
        {
            ReserveMB = (int)strtol(opt + 3, NULL, 10);
        }
        else if (_strnicmp(opt, "/FS:", 4) == 0)
        {
            if (_stricmp(opt + 4, "NTFS") == 0)
            {
                fstype = VTOY_FS_NTFS;
            }
            else if (_stricmp(opt + 4, "FAT32") == 0)
            {
                fstype = VTOY_FS_FAT32;
            }
            else if (_stricmp(opt + 4, "UDF") == 0)
            {
                fstype = VTOY_FS_UDF;
            }
        }
        else if (_strnicmp(opt, "/Label:", 7) == 0)
        {
            if (strlen(opt + 7) > 32)
            {
                CLIPrint("[ERROR] /Label: value too long (max 32 chars)");
                return 1;
            }
            safe_strcpy(g_VolumeLabel, opt + 7);
        }
    }

    if (op == CLI_OP_HELP || op == CLI_OP_VERSION)
    {
        pCfg->op = op;
        return 0;
    }

    if (op < 0 || PhyDrive < 0)
    {
        Log("[ERROR] Invalid parameters %d %d", op, PhyDrive);
        return 1;
    }

    Log("Ventoy CLI op:%d PhyDrive:%d %s SecureBoot:%d ReserveSpace:%dMB USBCheck:%u FS:%s NonDest:%d",
        op, PhyDrive, PartStyle ? "GPT" : "MBR",
        g_SecureBoot, ReserveMB, USBCheck, GetVentoyFsFmtNameByTypeA(fstype), NonDest
        );

    if (CLI_GetPhyDriveInfo(PhyDrive, pDrvInfo))
    {
        Log("[ERROR] Failed to get phydrive%d info", PhyDrive);
        return 1;
    }

    Log("PhyDrive:%d BusType:%-4s Removable:%u Size:%dGB(%llu) Name:%s %s",
        pDrvInfo->PhyDrive, GetBusTypeString(pDrvInfo->BusType), pDrvInfo->RemovableMedia,
        GetHumanReadableGBSize(pDrvInfo->SizeInBytes), pDrvInfo->SizeInBytes,
        pDrvInfo->VendorId, pDrvInfo->ProductId);

    if (op == CLI_OP_LIST)
    {
        GetLettersBelongPhyDrive(PhyDrive, pDrvInfo->DriveLetters, sizeof(pDrvInfo->DriveLetters));
    }

    if (IsVentoyPhyDrive(PhyDrive, pDrvInfo->SizeInBytes, &MBR, &Part2StartSector, &Part2GPTAttr))
    {
        memcpy(&(pDrvInfo->MBR), &MBR, sizeof(MBR));
        pDrvInfo->PartStyle = (MBR.PartTbl[0].FsFlag == 0xEE) ? 1 : 0;
        pDrvInfo->Part2GPTAttr = Part2GPTAttr;
        GetVentoyVerInPhyDrive(pDrvInfo, Part2StartSector, pDrvInfo->VentoyVersion, sizeof(pDrvInfo->VentoyVersion), &(pDrvInfo->SecureBootSupport));
        Log("PhyDrive %d is Ventoy Disk ver:%s SecureBoot:%u", pDrvInfo->PhyDrive, pDrvInfo->VentoyVersion, pDrvInfo->SecureBootSupport);

        GetVentoyFsNameInPhyDrive(pDrvInfo);

        if (pDrvInfo->VentoyVersion[0] == 0)
        {
            pDrvInfo->VentoyVersion[0] = '?';
            Log("Unknown Ventoy Version");
        }
    }

    if (op == CLI_OP_INSTALL && NonDest)
    {
        GetLettersBelongPhyDrive(PhyDrive, pDrvInfo->DriveLetters, sizeof(pDrvInfo->DriveLetters));
    }

    pCfg->op = op;
    pCfg->PartStyle = PartStyle;
    pCfg->ReserveMB = ReserveMB;
    pCfg->USBCheck = USBCheck;
    pCfg->NonDest = NonDest;
    pCfg->fstype = fstype;

    return 0;
}
