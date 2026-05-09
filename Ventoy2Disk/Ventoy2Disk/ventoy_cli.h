#ifndef __VENTOY_CLI_H__
#define __VENTOY_CLI_H__

#define CLI_OP_INSTALL      0
#define CLI_OP_UPDATE       1
#define CLI_OP_LIST         2
#define CLI_OP_HELP         3
#define CLI_OP_VERSION      4
#define CLI_OP_SAFEINSTALL  5

typedef struct CLI_CFG
{
    int op;
    int PartStyle;
    int ReserveMB;
    BOOL USBCheck;
    BOOL NonDest;
    int fstype;
}CLI_CFG;

extern PHY_DRIVE_INFO* g_CLI_PhyDrvInfo;

void CLIPrint(const char *Fmt, ...);
void CLI_AttachConsoleIO(void);
void CLI_WriteDoneFile(int ret);
void CLI_PrintHelp(void);

int CLI_GetPhyDriveInfo(int PhyDrive, PHY_DRIVE_INFO* pInfo);
int CLI_CheckParam(int argc, char** argv, PHY_DRIVE_INFO* pDrvInfo, CLI_CFG *pCfg);
BOOL CLI_ConfirmDestructiveOperation(PHY_DRIVE_INFO *pDrvInfo);

int Ventoy_CLI_Install(PHY_DRIVE_INFO* pDrvInfo, CLI_CFG *pCfg);
int Ventoy_CLI_NonDestInstall(PHY_DRIVE_INFO* pDrvInfo, CLI_CFG* pCfg);
int Ventoy_CLI_Update(PHY_DRIVE_INFO* pDrvInfo, CLI_CFG* pCfg);

extern void CLISetReserveSpace(int MB);

#endif
