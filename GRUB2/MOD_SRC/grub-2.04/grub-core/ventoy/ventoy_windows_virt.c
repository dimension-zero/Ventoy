/******************************************************************************
 * ventoy_windows.c 
 *
 * Copyright (c) 2020, longpanda <admin@ventoy.net>
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
#include <grub/time.h>
#include <grub/crypto.h>
#include <grub/ventoy.h>
#include "ventoy_def.h"
#include "ventoy_windows_priv.h"
grub_uint32_t ventoy_windows_get_virt_data_size(void)
{
    grub_uint32_t size = 0;
    wim_tail *wim_data = NULL;
    wim_patch *node = g_wim_patch_head;
    
    while (node)
    {
        if (node->valid)
        {
            wim_data = &node->wim_data;
            size += sizeof(ventoy_virt_chunk) + wim_data->bin_align_len + 
                    wim_data->new_meta_align_len + wim_data->new_lookup_align_len;
        }
        node = node->next;
    }
    
    return size;
}

void ventoy_windows_fill_virt_data(    grub_uint64_t isosize, ventoy_chain_head *chain)
{
    grub_uint64_t sector;
    grub_uint32_t offset;
    grub_uint32_t wim_secs;
    grub_uint32_t mem_secs;
    char *override = NULL;
    ventoy_virt_chunk *cur = NULL;
    wim_tail *wim_data = NULL;
    wim_patch *node = NULL;    

    sector = (isosize + 2047) / 2048;
    offset = sizeof(ventoy_virt_chunk) * g_wim_valid_patch_count;

    override = (char *)chain + chain->virt_chunk_offset;
    cur = (ventoy_virt_chunk *)override;

    for (node = g_wim_patch_head; node; node = node->next)
    {
        if (0 == node->valid)
        {
            continue;
        }

        wim_data = &node->wim_data;

        wim_secs = wim_data->wim_align_size / 2048;
        mem_secs = (wim_data->bin_align_len + wim_data->new_meta_align_len + wim_data->new_lookup_align_len) / 2048;

        cur->remap_sector_start = sector;
        cur->remap_sector_end   = cur->remap_sector_start + wim_secs;
        cur->org_sector_start   = (grub_uint32_t)(wim_data->file_offset / 2048);
        
        cur->mem_sector_start   = cur->remap_sector_end;
        cur->mem_sector_end     = cur->mem_sector_start + mem_secs;
        cur->mem_sector_offset  = offset;

        sector += wim_secs + mem_secs;
        cur++;

        grub_memcpy(override + offset, wim_data->jump_bin_data, wim_data->bin_raw_len);
        offset += wim_data->bin_align_len;

        grub_memcpy(override + offset, wim_data->new_meta_data, wim_data->new_meta_len);
        offset += wim_data->new_meta_align_len;
        
        grub_memcpy(override + offset, wim_data->new_lookup_data, wim_data->new_lookup_len);
        offset += wim_data->new_lookup_align_len;

        chain->virt_img_size_in_bytes += wim_data->wim_align_size + 
                                         wim_data->bin_align_len + 
                                         wim_data->new_meta_align_len + 
                                         wim_data->new_lookup_align_len;
    }

    return;
}

int ventoy_windows_drive_map(ventoy_chain_head *chain, int vlnk)
{
    int hd1 = 0;
    grub_disk_t disk;
        
    debug("drive map begin <%p> <%d> ...\n", chain, vlnk);

    disk = grub_disk_open("hd1");
    if (disk)
    {
        grub_disk_close(disk);
        hd1 = 1;
        debug("BIOS hd1 exist\n");
    }
    else
    {
        debug("failed to open disk %s\n", "hd1");
    }

    if (vlnk)
    {
        if (g_ventoy_disk_bios_id == 0x80 && hd1)
        {
            debug("drive map needed vlnk %p\n", disk);
            chain->drive_map = 0x81;
        }
    }
    else if (chain->disk_drive == 0x80)
    {
        if (hd1)
        {
            debug("drive map needed normal %p\n", disk);
            chain->drive_map = 0x81;
        }
    }
    else
    {
        debug("no need to map 0x%x\n", chain->disk_drive);
    }

    return 0;
}

int ventoy_suppress_windows_cd_prompt(void)
{
    int rc = 1;
    const char  *cdprompt = NULL;
    grub_uint64_t readpos = 0;
    grub_file_t file = NULL;
    grub_uint8_t data[32];

    cdprompt = ventoy_get_env("VTOY_WINDOWS_CD_PROMPT");
    if (cdprompt && cdprompt[0] == '1' && cdprompt[1] == 0)
    {
        debug("VTOY_WINDOWS_CD_PROMPT:<%s>\n", cdprompt);
        return 0;
    }

    g_ventoy_case_insensitive = 1;
    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s/boot/bootfix.bin", "(loop)");
    g_ventoy_case_insensitive = 0;

    if (!file)
    {
        debug("Failed to open %s\n", "bootfix.bin");
        goto end;
    }

    grub_file_read(file, data, 32);

    if (file->fs && file->fs->name && grub_strcmp(file->fs->name, "udf") == 0)
    {
        readpos = grub_udf_get_file_offset(file);
    }
    else
    {
        readpos = grub_iso9660_get_last_read_pos(file);
    }

    debug("bootfix.bin readpos:%lu (sector:%lu)  data: %02x %02x %02x %02x\n", 
        (ulong)readpos, (ulong)readpos / 2048, data[24], data[25], data[26], data[27]);

    if (*(grub_uint32_t *)(data + 24) == 0x13cd0080)
    {
        g_suppress_wincd_override_offset = readpos + 24;
        g_suppress_wincd_override_data = 0x13cd00fd;

        rc = 0;
    }

    debug("g_suppress_wincd_override_offset:%lu\n", (ulong)g_suppress_wincd_override_offset);

end:
    check_free(file, grub_file_close);

    return rc;
}

