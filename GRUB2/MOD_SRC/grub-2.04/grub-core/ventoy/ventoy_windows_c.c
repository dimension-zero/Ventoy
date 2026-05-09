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

GRUB_MOD_LICENSE ("GPLv3+");

int g_iso_fs_type = 0;
int g_wim_total_patch_count = 0;
int g_wim_valid_patch_count = 0;
wim_patch *g_wim_patch_head = NULL;

grub_uint64_t g_suppress_wincd_override_offset = 0;
grub_uint32_t g_suppress_wincd_override_data = 0;

grub_uint8_t g_temp_buf[512];

grub_ssize_t lzx_decompress ( const void *data, grub_size_t len, void *buf );
grub_ssize_t xca_decompress ( const void *data, grub_size_t len, void *buf );

        node = node->next;
    }

    return 0;
}

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

int ventoy_windows_fill_udf_short_ad(grub_file_t isofile, grub_uint32_t curpos, 
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

int ventoy_extract_init_exe(char *wimfile, grub_uint8_t **pexe_data, grub_uint32_t *pexe_len, char *exe_name)
{
    int rc;
    int ret = 1;
    grub_uint16_t i;
    grub_file_t file = NULL;
    grub_uint32_t exe_len = 0;
    wim_header *head = NULL;
    grub_uint16_t *uname = NULL;
    grub_uint8_t *exe_data = NULL;
    grub_uint8_t *decompress_data = NULL;
    wim_lookup_entry *lookup = NULL;
    wim_security_header *security = NULL;
    wim_directory_entry *rootdir = NULL;
    wim_directory_entry *search = NULL;
    wim_stream_entry *stream = NULL;
    wim_lookup_entry *replace_look = NULL;
    wim_header wimhdr;
    wim_hash hashdata;

    head = &wimhdr;

    file = grub_file_open(wimfile, VENTOY_FILE_TYPE);
    if (!file)
    {
        goto out;
    }

    grub_file_read(file, head, sizeof(wim_header));
    rc = ventoy_read_resource(file, head, &head->metadata, (void **)&decompress_data);
    if (rc)
    {
        grub_printf("failed to read meta data %d\n", rc);
        goto out;
    }

    security = (wim_security_header *)decompress_data;
    if (security->len > 0)
    {
        rootdir = (wim_directory_entry *)(decompress_data + ((security->len + 7) & 0xFFFFFFF8U));
    }
    else
    {
        rootdir = (wim_directory_entry *)(decompress_data + 8);
    }

    debug("read lookup offset:%llu size:%llu\n", (ulonglong)head->lookup.offset, (ulonglong)head->lookup.raw_size);
    lookup = grub_malloc(head->lookup.raw_size);
    grub_file_seek(file, head->lookup.offset);
    grub_file_read(file, lookup, head->lookup.raw_size);

    /* search winpeshl.exe dirent entry */
    search = search_replace_wim_dirent(file, head, lookup, decompress_data, rootdir);
    if (!search)
    {
        debug("Failed to find replace file %p\n", search);
        goto out;
    }
    
    uname = (grub_uint16_t *)(search + 1);
    for (i = 0; i < search->name_len / 2 && i < 200; i++)
    {
        exe_name[i] = (char)uname[i];
    }
    exe_name[i] = 0;
    debug("find replace file at %p <%s>\n", search, exe_name);

    grub_memset(&hashdata, 0, sizeof(wim_hash));
    if (grub_memcmp(&hashdata, search->hash.sha1, sizeof(wim_hash)) == 0)
    {
        debug("search hash all 0, now do deep search\n");
        stream = (wim_stream_entry *)((char *)search + search->len);
        for (i = 0; i < search->streams; i++)
        {
            if (stream->name_len == 0)
            {
                grub_memcpy(&hashdata, stream->hash.sha1, sizeof(wim_hash));
                debug("new search hash: %02x %02x %02x %02x %02x %02x %02x %02x\n", 
                    ventoy_varg_8(hashdata.sha1));
                break;
            }
            stream = (wim_stream_entry *)((char *)stream + stream->len);
        }
    }
    else
    {
        grub_memcpy(&hashdata, search->hash.sha1, sizeof(wim_hash));        
    }

    /* find and extact winpeshl.exe */
    replace_look = ventoy_find_look_entry(head, lookup, &hashdata);
    if (replace_look)
    {
        exe_len = (grub_uint32_t)replace_look->resource.raw_size;
        debug("find replace lookup entry_id:%ld raw_size:%u\n", 
