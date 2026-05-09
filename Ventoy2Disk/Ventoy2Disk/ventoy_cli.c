#include <Windows.h>
#include <stdio.h>
#include "Ventoy2Disk.h"
#include "DiskService.h"
#include "ventoy_cli.h"

BOOL g_CLI_Mode = FALSE;
PHY_DRIVE_INFO* g_CLI_PhyDrvInfo = NULL;

PHY_DRIVE_INFO* CLI_PhyDrvInfo(void)
{
    return g_CLI_PhyDrvInfo;
}

/*
 * Ventoy2Disk.exe VTOYCLI CMD [OPTION] { /Drive:F: | /PhyDrive:N }
 *
 * CMD: /I /SI /U /L /V /HELP /?
 * OPTION: /GPT /NoSB /SB /R:MB /FS:TYPE /Label:NAME /NoUSBCheck /NonDest /Y
 *
 * Run with /HELP for full usage.
 */
int VentoyCLIMain(int argc, char** argv)
{
    int ret = 1;
    PHY_DRIVE_INFO* pDrvInfo = NULL;
    CLI_CFG CliCfg;

    DeleteFileA(VENTOY_CLI_PERCENT);
    DeleteFileA(VENTOY_CLI_DONE);

    CLI_AttachConsoleIO();

    g_CLI_PhyDrvInfo = pDrvInfo = (PHY_DRIVE_INFO*)malloc(sizeof(PHY_DRIVE_INFO));
    if (!pDrvInfo)
    {
        goto end;
    }
    memset(pDrvInfo, 0, sizeof(PHY_DRIVE_INFO));

    if (CLI_CheckParam(argc, argv, pDrvInfo, &CliCfg))
    {
        goto end;
    }

    if (CliCfg.op == CLI_OP_HELP)
    {
        CLI_PrintHelp();
        ret = 0;
        goto end;
    }

    if (CliCfg.op == CLI_OP_VERSION)
    {
        CLIPrint("%s", GetLocalVentoyVersion());
        ret = 0;
        goto end;
    }

    if (CliCfg.op == CLI_OP_LIST)
    {
        if (pDrvInfo->VentoyVersion[0] == 0 || pDrvInfo->VentoyVersion[0] == '?')
        {
            CLIPrint("Ventoy Version in Disk : N/A");
            ret = 1;
        }
        else
        {
            CLIPrint("Ventoy Version in Disk : %s", pDrvInfo->VentoyVersion);
            CLIPrint("Disk Partition Style   : %s", pDrvInfo->PartStyle ? "GPT" : "MBR");
            CLIPrint("Secure Boot Support    : %s", pDrvInfo->SecureBootSupport ? "YES" : "NO");
            CLIPrint("Filesystem Type        : %s", pDrvInfo->VentoyFsType);
            CLIPrint("Disk Size              : %d GB", GetHumanReadableGBSize(pDrvInfo->SizeInBytes));
            CLIPrint("Bus Type               : %s", GetBusTypeString(pDrvInfo->BusType));
            ret = 0;
        }
        goto end;
    }

    if (CliCfg.op == CLI_OP_SAFEINSTALL)
    {
        if (pDrvInfo->VentoyVersion[0] != 0 && pDrvInfo->VentoyVersion[0] != '?')
        {
            CLIPrint("[ERROR] PhyDrive %d already has Ventoy %s.",
                     pDrvInfo->PhyDrive, pDrvInfo->VentoyVersion);
            CLIPrint("        Use /U to update, or /I to force-reinstall.");
            ret = 1;
            goto end;
        }
        if (!CLI_ConfirmDestructiveOperation(pDrvInfo))
        {
            ret = 0;
            goto end;
        }
        CliCfg.op = CLI_OP_INSTALL;
    }

    //Check USB type for install
    if (CliCfg.op == CLI_OP_INSTALL && CliCfg.USBCheck)
    {
        if (pDrvInfo->BusType != BusTypeUsb)
        {
            Log("[ERROR] PhyDrive %d is NOT USB type",  pDrvInfo->PhyDrive);
            goto end;
        }
    }

    if (CliCfg.op == CLI_OP_INSTALL)
    {
        if (CliCfg.NonDest)
        {
            ret = Ventoy_CLI_NonDestInstall(pDrvInfo, &CliCfg);
        }
        else
        {
            AlertSuppressInit();
            SetAlertPromptHookEnable(TRUE);
            ret = Ventoy_CLI_Install(pDrvInfo, &CliCfg);
        }
    }
    else
    {
        if (pDrvInfo->VentoyVersion[0] == 0)
        {
            Log("[ERROR] No Ventoy information detected in PhyDrive %d, so can not do update", pDrvInfo->PhyDrive);
            goto end;
        }

        ret = Ventoy_CLI_Update(pDrvInfo, &CliCfg);
    }

end:
    CHECK_FREE(pDrvInfo);

    CLI_UpdatePercent(PT_FINISH);
    CLI_WriteDoneFile(ret);

    return ret;
}
