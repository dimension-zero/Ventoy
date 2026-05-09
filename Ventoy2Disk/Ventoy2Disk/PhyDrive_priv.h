/*
 * PhyDrive_priv.h — internal cross-TU declarations shared between
 * PhyDrive.c (drive query), PhyDrive_match.c, PhyDrive_fatver.c,
 * PhyDrive_secboot.c, PhyDrive_format.c, PhyDrive_clear.c,
 * PhyDrive_install.c, PhyDrive_resize.c, PhyDrive_update.c.
 */
#ifndef __PHYDRIVE_PRIV_H__
#define __PHYDRIVE_PRIV_H__

#include <Windows.h>
#include "Ventoy2Disk.h"

extern int g_backup_bin_index;
extern unsigned int g_disk_unxz_len;
extern BYTE *g_part_img_pos;
extern BYTE *g_part_img_buf[VENTOY_EFI_PART_SIZE / SIZE_1MB];

BOOL WriteDataToPhyDisk(HANDLE hDrive, UINT64 Offset, VOID *buffer, DWORD len);
DWORD GetVentoyVolumeName(int PhyDrive, UINT64 StartSectorId, CHAR *NameBuf, UINT32 BufLen, BOOL DelSlash);
void unxz_error(char *x);
BOOL TryWritePart2(HANDLE hDrive, UINT64 StartSectorId);
int FormatPart2Fat(HANDLE hDrive, UINT64 StartSectorId);
int WriteGrubStage1ToPhyDrive(HANDLE hDrive, int PartStyle);
int FormatPart1LargeFAT32(UINT64 DiskSizeBytes, int CluserSize);
int FormatPart1exFAT(UINT64 DiskSizeBytes);
int ZeroPart1FileSystem(HANDLE hDrive, UINT64 Part2StartSector);

#endif
