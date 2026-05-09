#include <Windows.h>
#include <stdio.h>
#include "Ventoy2Disk.h"
#include "DiskService.h"
#include "ventoy_cli.h"

BOOL CLI_ConfirmDestructiveOperation(PHY_DRIVE_INFO *pDrvInfo)
{
    char Answer[16];

    if (g_NoNeedInputYes)
        return TRUE;

    CLIPrint("WARNING: ALL DATA on PhyDrive %d (%s %s, %d GB) will be LOST.",
             pDrvInfo->PhyDrive, pDrvInfo->VendorId, pDrvInfo->ProductId,
             GetHumanReadableGBSize(pDrvInfo->SizeInBytes));
    printf("Continue? (y/n): ");
    fflush(stdout);
    if (!fgets(Answer, sizeof(Answer), stdin) || (Answer[0] != 'y' && Answer[0] != 'Y'))
    { CLIPrint("Aborted."); return FALSE; }

    CLIPrint("WARNING: ALL DATA on PhyDrive %d will be LOST. Double-check.", pDrvInfo->PhyDrive);
    printf("Continue? (y/n): ");
    fflush(stdout);
    if (!fgets(Answer, sizeof(Answer), stdin) || (Answer[0] != 'y' && Answer[0] != 'Y'))
    { CLIPrint("Aborted."); return FALSE; }

    return TRUE;
}

int Ventoy_CLI_NonDestInstall(PHY_DRIVE_INFO* pDrvInfo, CLI_CFG* pCfg)
{
    int rc;
    int TryId = 1;

    Log("Ventoy_CLI_NonDestInstall start ...");

    if (pDrvInfo->BytesPerLogicalSector == 4096 && pDrvInfo->BytesPerPhysicalSector == 4096)
    {
        Log("Ventoy does not support 4k native disk.");
        rc = 1;
        goto out;
    }

    if (!PartResizePreCheck(NULL))
    {
        Log("#### Part Resize PreCheck Failed ####");
        rc = 1;
        goto out;
    }

    rc = PartitionResizeForVentoy(pDrvInfo);

out:
    Log("Ventoy_CLI_NonDestInstall [%s]", rc == 0 ? "SUCCESS" : "FAILED");

    return rc;
}


int Ventoy_CLI_Install(PHY_DRIVE_INFO* pDrvInfo, CLI_CFG *pCfg)
{
    int rc;
    int TryId = 1;

    Log("Ventoy_CLI_Install start ...");

    if (pDrvInfo->BytesPerLogicalSector == 4096 && pDrvInfo->BytesPerPhysicalSector == 4096)
    {
        Log("Ventoy does not support 4k native disk.");
        rc = 1;
        goto out;
    }

    if (pCfg->ReserveMB > 0)
    {
        CLISetReserveSpace(pCfg->ReserveMB);
    }

    SetVentoyFsType(pCfg->fstype);

    rc = InstallVentoy2PhyDrive(pDrvInfo, pCfg->PartStyle, TryId++);
    if (rc)
    {
        Log("This time install failed, clean disk by disk, wait 3s and retry...");
        DISK_CleanDisk(pDrvInfo->PhyDrive);

        Sleep(3000);

        Log("Now retry to install...");
        rc = InstallVentoy2PhyDrive(pDrvInfo, pCfg->PartStyle, TryId++);

        if (rc)
        {
            Log("This time install failed, clean disk by diskpart, wait 5s and retry...");
            DSPT_CleanDisk(pDrvInfo->PhyDrive);

            Sleep(5000);

            Log("Now retry to install...");
            rc = InstallVentoy2PhyDrive(pDrvInfo, pCfg->PartStyle, TryId++);
        }
    }

    SetVentoyFsType(VTOY_FS_EXFAT);

out:
    Log("Ventoy_CLI_Install [%s]", rc == 0 ? "SUCCESS" : "FAILED");

    return rc;
}

int Ventoy_CLI_Update(PHY_DRIVE_INFO* pDrvInfo, CLI_CFG* pCfg)
{
    int rc;
    int TryId = 1;

    Log("Ventoy_CLI_Update start ...");

    rc = UpdateVentoy2PhyDrive(pDrvInfo, TryId++);
    if (rc)
    {
        Log("This time update failed, now wait and retry...");
        Sleep(4000);

        //Try2
        Log("Now retry to update...");
        rc = UpdateVentoy2PhyDrive(pDrvInfo, TryId++);
        if (rc)
        {
            //Try3
            Sleep(1000);
            Log("Now retry to update...");
            rc = UpdateVentoy2PhyDrive(pDrvInfo, TryId++);
            if (rc)
            {
                //Try4 is dangerous ...
                Sleep(3000);
                Log("Now retry to update...");
                rc = UpdateVentoy2PhyDrive(pDrvInfo, TryId++);
            }
        }
    }

    Log("Ventoy_CLI_Update [%s]", rc == 0 ? "SUCCESS" : "FAILED");

    return rc;
}
