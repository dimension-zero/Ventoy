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

unsigned int g_disk_unxz_len = 0;
BYTE *g_part_img_pos = NULL;
BYTE *g_part_img_buf[VENTOY_EFI_PART_SIZE / SIZE_1MB];


static int VentoyFatMemRead(uint32 Sector, uint8 *Buffer, uint32 SectorCount)
{
	uint32 i;
	uint32 offset;
	BYTE *MbBuf = NULL;

	for (i = 0; i < SectorCount; i++)
	{
		offset = (Sector + i) * 512;

		if (g_part_img_buf[1] == NULL)
		{
			MbBuf = g_part_img_buf[0] + offset;
			memcpy(Buffer + i * 512, MbBuf, 512);
		}
		else
		{
			MbBuf = g_part_img_buf[offset / SIZE_1MB];
			memcpy(Buffer + i * 512, MbBuf + (offset % SIZE_1MB), 512);
		}
	}

	return 1;
}


static int VentoyFatMemWrite(uint32 Sector, uint8 *Buffer, uint32 SectorCount)
{
	uint32 i;
	uint32 offset;
	BYTE *MbBuf = NULL;

	for (i = 0; i < SectorCount; i++)
	{
		offset = (Sector + i) * 512;

		if (g_part_img_buf[1] == NULL)
		{
			MbBuf = g_part_img_buf[0] + offset;
			memcpy(MbBuf, Buffer + i * 512, 512);
		}
		else
		{
			MbBuf = g_part_img_buf[offset / SIZE_1MB];
			memcpy(MbBuf + (offset % SIZE_1MB), Buffer + i * 512, 512);
		}
	}

	return 1;
}

int VentoyProcSecureBoot(BOOL SecureBoot)
{
	int rc = 0;
	int size;
	char *filebuf = NULL;
	void *file = NULL;

	Log("VentoyProcSecureBoot %d ...", SecureBoot);
	
	if (SecureBoot)
	{
		Log("Secure boot is enabled ...");
		return 0;
	}

	fl_init();

	if (0 == fl_attach_media(VentoyFatMemRead, VentoyFatMemWrite))
	{
		file = fl_fopen("/EFI/BOOT/grubx64_real.efi", "rb");
		Log("Open ventoy efi file %p ", file);
		if (file)
		{
			fl_fseek(file, 0, SEEK_END);
			size = (int)fl_ftell(file);
			fl_fseek(file, 0, SEEK_SET);

			Log("ventoy efi file size %d ...", size);

			filebuf = (char *)malloc(size);
			if (filebuf)
			{
				fl_fread(filebuf, 1, size, file);
			}

			fl_fclose(file);

			Log("Now delete all efi files ...");
            fl_remove("/EFI/BOOT/BOOTX64.EFI");            
            fl_remove("/EFI/BOOT/grubx64.efi");            
			fl_remove("/EFI/BOOT/grubx64_real.efi");
			fl_remove("/EFI/BOOT/MokManager.efi");
			fl_remove("/EFI/BOOT/mmx64.efi");
            fl_remove("/ENROLL_THIS_KEY_IN_MOKMANAGER.cer");
            fl_remove("/EFI/BOOT/grub.efi");

			file = fl_fopen("/EFI/BOOT/BOOTX64.EFI", "wb");
			Log("Open bootx64 efi file %p ", file);
			if (file)
			{
				if (filebuf)
				{
					fl_fwrite(filebuf, 1, size, file);
				}
				
				fl_fflush(file);
				fl_fclose(file);
			}

			if (filebuf)
			{
				free(filebuf);
			}
		}

        file = fl_fopen("/EFI/BOOT/grubia32_real.efi", "rb");
        Log("Open ventoy efi file %p ", file);
        if (file)
        {
            fl_fseek(file, 0, SEEK_END);
            size = (int)fl_ftell(file);
            fl_fseek(file, 0, SEEK_SET);

            Log("ventoy efi file size %d ...", size);

            filebuf = (char *)malloc(size);
            if (filebuf)
            {
                fl_fread(filebuf, 1, size, file);
            }

            fl_fclose(file);

            Log("Now delete all efi files ...");
            fl_remove("/EFI/BOOT/BOOTIA32.EFI");
            fl_remove("/EFI/BOOT/grubia32.efi");
            fl_remove("/EFI/BOOT/grubia32_real.efi");
            fl_remove("/EFI/BOOT/mmia32.efi");            

            file = fl_fopen("/EFI/BOOT/BOOTIA32.EFI", "wb");
            Log("Open bootia32 efi file %p ", file);
            if (file)
            {
                if (filebuf)
                {
                    fl_fwrite(filebuf, 1, size, file);
                }

                fl_fflush(file);
                fl_fclose(file);
            }

            if (filebuf)
            {
                free(filebuf);
            }
        }

	}
	else
	{
		rc = 1;
	}

	fl_shutdown();

	return rc;
}


