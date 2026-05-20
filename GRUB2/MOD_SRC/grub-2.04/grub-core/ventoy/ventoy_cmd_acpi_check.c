/******************************************************************************
 * ventoy_cmd.c
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

#include <grub/types.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/err.h>
#include <grub/dl.h>
#include <grub/disk.h>
#include <grub/device.h>
#include <grub/term.h>
#include <grub/partition.h>
#include <grub/file.h>
#include <grub/normal.h>
#include <grub/extcmd.h>
#include <grub/datetime.h>
#include <grub/i18n.h>
#include <grub/net.h>
#include <grub/misc.h>
#include <grub/kernel.h>
#ifdef GRUB_MACHINE_EFI
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#endif
#include <grub/time.h>
#include <grub/video.h>
#include <grub/acpi.h>
#include <grub/charset.h>
#include <grub/crypto.h>
#include <grub/lib/crc.h>
#include <grub/random.h>
#include <grub/ventoy.h>
#include "ventoy_def.h"
#include "miniz.h"
#include "ventoy_cmd_priv.h"
grub_err_t ventoy_cmd_img_unhook_root(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    ventoy_env_hook_root(0);

    return 0;
}

#ifdef GRUB_MACHINE_EFI
grub_err_t ventoy_cmd_check_secureboot_var(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int ret = 1;
    grub_uint8_t *var;
    grub_size_t size;
    grub_efi_guid_t global = GRUB_EFI_GLOBAL_VARIABLE_GUID;

    (void)ctxt;
    (void)argc;
    (void)args;

    var = grub_efi_get_variable("SecureBoot", &global, &size);
    if (var && *var == 1)
    {
        return 0;
    }

    return ret;
}
#else
grub_err_t ventoy_cmd_check_secureboot_var(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;
    return 1;
}
#endif

grub_err_t ventoy_cmd_img_check_range(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    int ret = 1;
    grub_file_t file;
    grub_uint64_t FileSectors = 0;
    ventoy_gpt_info *gpt = NULL;
    ventoy_part_table *pt = NULL;
    grub_uint8_t zeroguid[16] = {0};

    (void)ctxt;
    (void)argc;

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (!file)
    {
        debug("failed to open file %s\n", args[0]);
        return 1;
    }

    if (file->size % 512)
    {
        debug("unaligned file size: %llu\n", (ulonglong)file->size);
        goto out;
    }

    gpt = grub_zalloc(sizeof(ventoy_gpt_info));
    if (!gpt)
    {
        goto out;
    }

    FileSectors = file->size / 512;

    grub_file_read(file, gpt, sizeof(ventoy_gpt_info));
    if (grub_strncmp(gpt->Head.Signature, "EFI PART", 8) == 0)
    {
        debug("This is EFI partition table\n");

        for (i = 0; i < 128; i++)
        {
            if (grub_memcmp(gpt->PartTbl[i].PartGuid, zeroguid, 16))
            {
                if (FileSectors < gpt->PartTbl[i].LastLBA)
                {
                    debug("out of range: part[%d] LastLBA:%llu FileSectors:%llu\n", i,
                        (ulonglong)gpt->PartTbl[i].LastLBA, (ulonglong)FileSectors);
                    goto out;
                }
            }
        }
    }
    else
    {
        debug("This is MBR partition table\n");

        for (i = 0; i < 4; i++)
        {
            pt = gpt->MBR.PartTbl + i;
            if (FileSectors < pt->StartSectorId + pt->SectorCount)
            {
                debug("out of range: part[%d] LastLBA:%llu FileSectors:%llu\n", i,
                       (ulonglong)(pt->StartSectorId + pt->SectorCount),
                       (ulonglong)FileSectors);
                goto out;
            }
        }
    }

    ret = 0;

out:
    grub_file_close(file);
    grub_check_free(gpt);
    grub_errno = GRUB_ERR_NONE;
    return ret;
}

grub_err_t ventoy_cmd_clear_key(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    int ret;

    (void)ctxt;
    (void)argc;
    (void)args;

    for (i = 0; i < 500; i++)
    {
        ret = grub_getkey_noblock();
        if (ret == GRUB_TERM_NO_KEY)
        {
            break;
        }
    }

    if (i >= 500)
    {
        grub_cls();
        grub_printf("\n\n Still have key input after clear.\n");
        grub_refresh();
        grub_sleep(5);
    }

    return 0;
}

grub_err_t ventoy_cmd_acpi_param(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    int buflen;
    int datalen;
    int loclen;
    int img_chunk_num;
    int image_sector_size;
    char cmd[64];
    ventoy_chain_head *chain;
    ventoy_img_chunk *chunk;
    ventoy_os_param *osparam;
    ventoy_image_location *location;
    ventoy_image_disk_region *region;
    struct grub_acpi_table_header *acpi;

    (void)ctxt;

    if (argc != 2)
    {
        return 1;
    }

    debug("ventoy_cmd_acpi_param %s %s\n", args[0], args[1]);

    chain = (ventoy_chain_head *)(ulong)grub_strtoul(args[0], NULL, 16);
    if (!chain)
    {
        return 1;
    }

    image_sector_size = (int)grub_strtol(args[1], NULL, 10);

    if (grub_memcmp(&g_ventoy_guid, &(chain->os_param.guid), 16))
    {
        debug("Invalid ventoy guid 0x%x\n", chain->os_param.guid.data1);
        return 1;
    }

    img_chunk_num = chain->img_chunk_num;

    loclen = sizeof(ventoy_image_location) + (img_chunk_num - 1) * sizeof(ventoy_image_disk_region);
    datalen = sizeof(ventoy_os_param) + loclen;

    buflen = sizeof(struct grub_acpi_table_header) + datalen;
    acpi = grub_zalloc(buflen);
    if (!acpi)
    {
        return 1;
    }

    /* Step1: Fill acpi table header */
    grub_memcpy(acpi->signature, "VTOY", 4);
    acpi->length = buflen;
    acpi->revision = 1;
    grub_memcpy(acpi->oemid, "VENTOY", 6);
    grub_memcpy(acpi->oemtable, "OSPARAMS", 8);
    acpi->oemrev = 1;
    acpi->creator_id[0] = 1;
    acpi->creator_rev = 1;

    /* Step2: Fill data */
    osparam = (ventoy_os_param *)(acpi + 1);
    grub_memcpy(osparam, &chain->os_param, sizeof(ventoy_os_param));
    osparam->vtoy_img_location_addr = 0;
    osparam->vtoy_img_location_len  = loclen;
    osparam->chksum = 0;
    osparam->chksum = 0x100 - grub_byte_checksum(osparam, sizeof(ventoy_os_param));

    location = (ventoy_image_location *)(osparam + 1);
    grub_memcpy(&location->guid, &osparam->guid, sizeof(ventoy_guid));
    location->image_sector_size = image_sector_size;
    location->disk_sector_size  = chain->disk_sector_size;
    location->region_count = img_chunk_num;

    region = location->regions;
    chunk = (ventoy_img_chunk *)((char *)chain + chain->img_chunk_offset);
    if (512 == image_sector_size)
    {
        for (i = 0; i < img_chunk_num; i++)
        {
            region->image_sector_count = chunk->disk_end_sector - chunk->disk_start_sector + 1;
            region->image_start_sector = chunk->img_start_sector * 4;
            region->disk_start_sector  = chunk->disk_start_sector;
            region++;
            chunk++;
        }
    }
    else
    {
        for (i = 0; i < img_chunk_num; i++)
        {
            region->image_sector_count = chunk->img_end_sector - chunk->img_start_sector + 1;
            region->image_start_sector = chunk->img_start_sector;
            region->disk_start_sector  = chunk->disk_start_sector;
            region++;
            chunk++;
        }
    }

    /* Step3: Fill acpi checksum */
    acpi->checksum = 0;
    acpi->checksum = 0x100 - grub_byte_checksum(acpi, acpi->length);

    /* load acpi table */
    grub_snprintf(cmd, sizeof(cmd), "acpi mem:0x%lx:size:%d", (ulong)acpi, acpi->length);
    grub_script_execute_sourcecode(cmd);

    grub_free(acpi);

    VENTOY_CMD_RETURN(0);
}

grub_err_t ventoy_cmd_push_last_entry(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    g_ventoy_last_entry_back = g_ventoy_last_entry;
    g_ventoy_last_entry = -1;

    return 0;
}

grub_err_t ventoy_cmd_pop_last_entry(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    g_ventoy_last_entry = g_ventoy_last_entry_back;

    return 0;
}

