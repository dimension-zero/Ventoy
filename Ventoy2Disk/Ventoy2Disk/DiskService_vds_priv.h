/*
 * DiskService_vds_priv.h — internal prototypes shared between
 * DiskService_vds.c (main), DiskService_vds_errors.c (error→string tables),
 * and DiskService_vds_volume.c (CHKDSK + shrink + format paths).
 */
#ifndef __DISKSERVICE_VDS_PRIV_H__
#define __DISKSERVICE_VDS_PRIV_H__

#include <vds.h>

const char *GetVdsError(DWORD error_code);
const char *WindowsErrorString(DWORD error_code);
IVdsService * VDS_InitService(void);

#endif
