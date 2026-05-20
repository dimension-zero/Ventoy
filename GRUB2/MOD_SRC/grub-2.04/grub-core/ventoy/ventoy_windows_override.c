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
grub_uint32_t ventoy_get_override_chunk_num(void)
{
    grub_uint32_t chunk_num = 0;
    
    if (g_iso_fs_type == 0)
    {
        /* ISO9660: */
        /* per wim */
        /* 1: file_size and file_offset */
        /* 2: new wim file header */
        chunk_num = g_wim_valid_patch_count * 2;
    }
    else
    {
        /* UDF: */
        /* global: */
        /* 1: block count in Partition Descriptor */

        /* per wim */
        /* 1: file_size in file_entry or extend_file_entry */
        /* 2: data_size and position in extend data short ad */
        /* 3: new wim file header */
        chunk_num = g_wim_valid_patch_count * 3 + 1;
    }

    if (g_suppress_wincd_override_offset > 0)
    {
        chunk_num++;
    }

    return chunk_num;
}

void ventoy_fill_suppress_wincd_override_data(void *override)
{
    ventoy_override_chunk *cur = (ventoy_override_chunk *)override;

    cur->override_size = 4;
    cur->img_offset = g_suppress_wincd_override_offset;
    grub_memcpy(cur->override_data, &g_suppress_wincd_override_data, cur->override_size);
}

void ventoy_windows_fill_override_data_iso9660(    grub_uint64_t isosize, void *override)
{
    grub_uint64_t sector;
    grub_uint32_t new_wim_size;
    ventoy_override_chunk *cur;
    wim_patch *node = NULL;
    wim_tail *wim_data = NULL;
    ventoy_iso9660_override *dirent = NULL;

    sector = (isosize + 2047) / 2048;

    cur = (ventoy_override_chunk *)override;

    if (g_suppress_wincd_override_offset > 0)
    {
        ventoy_fill_suppress_wincd_override_data(cur);
        cur++;
    }

    debug("ventoy_windows_fill_override_data_iso9660 %lu\n", (ulong)isosize);

    for (node = g_wim_patch_head; node; node = node->next)
    {
        wim_data = &node->wim_data;
        if (0 == node->valid)
        {
            continue;
        }
        
        new_wim_size = wim_data->wim_align_size + wim_data->bin_align_len + 
                wim_data->new_meta_align_len + wim_data->new_lookup_align_len;

        dirent = (ventoy_iso9660_override *)wim_data->override_data;

        dirent->first_sector    = (grub_uint32_t)sector;
        dirent->size            = new_wim_size;
        dirent->first_sector_be = grub_swap_bytes32(dirent->first_sector);
        dirent->size_be         = grub_swap_bytes32(dirent->size);

        sector += (new_wim_size / 2048);

        /* override 1: position and length in dirent */
        cur->img_offset = wim_data->override_offset;
        cur->override_size = wim_data->override_len;
        grub_memcpy(cur->override_data, wim_data->override_data, cur->override_size);
        cur++;

        /* override 2: new wim file header */
        cur->img_offset = wim_data->file_offset;
        cur->override_size = sizeof(wim_header);
        grub_memcpy(cur->override_data, &(wim_data->wim_header), cur->override_size);
        cur++;
    }

    return;
}

static int ventoy_windows_fill_udf_short_ad(grub_file_t isofile, grub_uint32_t curpos, 
    wim_tail *wim_data, grub_uint32_t new_wim_size)
{
    int i;
    grub_uint32_t total = 0;
    grub_uint32_t left_size = 0;
    ventoy_udf_override *udf = NULL;
    ventoy_udf_override tmp[4];
    
    grub_memset(tmp, 0, sizeof(tmp));
    grub_file_seek(isofile, wim_data->override_offset);
    grub_file_read(isofile, tmp, sizeof(tmp));

    left_size = new_wim_size;
    udf = (ventoy_udf_override *)wim_data->override_data;

    for (i = 0; i < 4; i++)
    {
        total += tmp[i].length;
        if (total >= wim_data->wim_raw_size)
        {
            udf->length   = left_size;
            udf->position = curpos;
            return 0;
        }
        else
        {
            udf->length   = tmp[i].length;
            udf->position = curpos;
        }

        left_size -= tmp[i].length;
        curpos += udf->length / 2048;
        udf++;
        wim_data->override_len += sizeof(ventoy_udf_override);
    }

    debug("######## Too many udf ad ######\n");
    return 1;
}

void ventoy_windows_fill_override_data_udf(grub_file_t isofile, void *override)
{
    grub_uint32_t data32;
    grub_uint64_t data64;
    grub_uint64_t sector;
    grub_uint32_t new_wim_size;
    grub_uint64_t total_wim_size = 0;
    grub_uint32_t udf_start_block = 0;
    ventoy_override_chunk *cur;
    wim_patch *node = NULL;
    wim_tail *wim_data = NULL;

    sector = (isofile->size + 2047) / 2048;

    cur = (ventoy_override_chunk *)override;
    
    if (g_suppress_wincd_override_offset > 0)
    {
        ventoy_fill_suppress_wincd_override_data(cur);
        cur++;
    }

    debug("ventoy_windows_fill_override_data_udf %lu\n", (ulong)isofile->size);

    for (node = g_wim_patch_head; node; node = node->next)
    {
        wim_data = &node->wim_data;
        if (node->valid)
        {
            if (udf_start_block == 0)
            {
                udf_start_block = wim_data->udf_start_block;
            }
            new_wim_size = wim_data->wim_align_size + wim_data->bin_align_len + 
                wim_data->new_meta_align_len + wim_data->new_lookup_align_len;
            total_wim_size += new_wim_size;
        }
    }

    //override 1: sector number in pd data 
    cur->img_offset = grub_udf_get_last_pd_size_offset();
    cur->override_size = 4;
    data32 = sector - udf_start_block + (total_wim_size / 2048);
    grub_memcpy(cur->override_data, &(data32), 4);

    for (node = g_wim_patch_head; node; node = node->next)
    {
        wim_data = &node->wim_data;
        if (0 == node->valid)
        {
            continue;
        }
        
        new_wim_size = wim_data->wim_align_size + wim_data->bin_align_len + 
                wim_data->new_meta_align_len + wim_data->new_lookup_align_len;

        //override 2: filesize in file_entry
        cur++;
        cur->img_offset = wim_data->fe_entry_size_offset;
        cur->override_size = 8;
        data64 = new_wim_size;
        grub_memcpy(cur->override_data, &(data64), 8);

        /* override 3: position and length in extend data */
        ventoy_windows_fill_udf_short_ad(isofile, (grub_uint32_t)sector - udf_start_block, wim_data, new_wim_size);

        sector += (new_wim_size / 2048);
        
        cur++;
        cur->img_offset = wim_data->override_offset;
        cur->override_size = wim_data->override_len;
        grub_memcpy(cur->override_data, wim_data->override_data, cur->override_size);

        /* override 4: new wim file header */
        cur++;
        cur->img_offset = wim_data->file_offset;
        cur->override_size = sizeof(wim_header);
        grub_memcpy(cur->override_data, &(wim_data->wim_header), cur->override_size);
    }

    return;
}

