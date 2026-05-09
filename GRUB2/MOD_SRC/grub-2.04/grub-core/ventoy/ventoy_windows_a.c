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

        file_offset = grub_udf_get_file_offset(file);

        debug("UDF wim size:%llu override_offset:%llu file_offset:%llu start_block=%u\n", 
            (ulonglong)file->size, (ulonglong)override_offset, (ulonglong)file_offset, start_block);
    }

    wim_data->file_offset = file_offset;
    wim_data->udf_start_block = start_block;
    wim_data->fe_entry_size_offset = fe_entry_size_offset;
    wim_data->override_offset = override_offset;
    wim_data->override_len = override_len;

    return 0;
}

int ventoy_read_resource(grub_file_t fp, wim_header *wimhdr, wim_resource_header *head, void **buffer)
{
    int decompress_len = 0;
    int total_decompress = 0;
    grub_uint32_t i = 0;
    grub_uint32_t chunk_num = 0;
    grub_uint32_t chunk_size = 0;
    grub_uint32_t last_chunk_size = 0;
    grub_uint32_t last_decompress_size = 0;
    grub_uint32_t cur_offset = 0;
    grub_uint8_t *cur_dst = NULL;
    grub_uint8_t *buffer_compress = NULL;
    grub_uint8_t *buffer_decompress = NULL;
    grub_uint32_t *chunk_offset = NULL;

    buffer_decompress = (grub_uint8_t *)grub_malloc(head->raw_size + head->size_in_wim);
    if (NULL == buffer_decompress)
    {
        return 0;
    }

    grub_file_seek(fp, head->offset);

    if (head->size_in_wim == head->raw_size)
    {
        grub_file_read(fp, buffer_decompress, head->size_in_wim);
        *buffer = buffer_decompress;
        return 0;
    }

    buffer_compress = buffer_decompress + head->raw_size;
    grub_file_read(fp, buffer_compress, head->size_in_wim);

    chunk_num = (head->raw_size + WIM_CHUNK_LEN - 1) / WIM_CHUNK_LEN;
    cur_offset = (chunk_num - 1) * 4;
    chunk_offset = (grub_uint32_t *)buffer_compress;

    //debug("%llu %llu chunk_num=%lu", (ulonglong)head->size_in_wim, (ulonglong)head->raw_size, chunk_num);
    
    cur_dst = buffer_decompress;
    
    for (i = 0; i < chunk_num - 1; i++)
    {
        chunk_size = (i == 0) ? chunk_offset[i] : chunk_offset[i] - chunk_offset[i - 1];

        if (WIM_CHUNK_LEN == chunk_size)
        {
            grub_memcpy(cur_dst, buffer_compress + cur_offset, chunk_size);
            decompress_len = (int)chunk_size; 
        }
        else
        {
            if (wimhdr->flags & FLAG_HEADER_COMPRESS_XPRESS)
            {
                decompress_len = (int)xca_decompress(buffer_compress + cur_offset, chunk_size, cur_dst);
            }
            else
            {
                decompress_len = (int)lzx_decompress(buffer_compress + cur_offset, chunk_size, cur_dst);                
            }
        }

        //debug("chunk_size:%u decompresslen:%d\n", chunk_size, decompress_len);
        
        total_decompress += decompress_len;
        cur_dst += decompress_len;
        cur_offset += chunk_size;
    }

    /* last chunk */
    last_chunk_size = (grub_uint32_t)(head->size_in_wim - cur_offset);
    last_decompress_size = head->raw_size - total_decompress;
    
    if (last_chunk_size < WIM_CHUNK_LEN && last_chunk_size == last_decompress_size)
    {
        debug("Last chunk %u uncompressed\n", last_chunk_size);
        grub_memcpy(cur_dst, buffer_compress + cur_offset, last_chunk_size);
        decompress_len = (int)last_chunk_size; 
    }
    else
    {
        if (wimhdr->flags & FLAG_HEADER_COMPRESS_XPRESS)
        {
            decompress_len = (int)xca_decompress(buffer_compress + cur_offset, head->size_in_wim - cur_offset, cur_dst);
        }
        else
        {
            decompress_len = (int)lzx_decompress(buffer_compress + cur_offset, head->size_in_wim - cur_offset, cur_dst);
        }
    }

    cur_dst += decompress_len;
    total_decompress += decompress_len;
    
    //debug("last chunk_size:%u decompresslen:%d tot:%d\n", last_chunk_size, decompress_len, total_decompress);

    if (cur_dst != buffer_decompress + head->raw_size)
    {
        debug("head->size_in_wim:%llu head->raw_size:%llu cur_dst:%p buffer_decompress:%p total_decompress:%d\n", 
            (ulonglong)head->size_in_wim, (ulonglong)head->raw_size, cur_dst, buffer_decompress, total_decompress);
        grub_free(buffer_decompress);
        return 1;
    }
    
    *buffer = buffer_decompress;
    return 0;
}


wim_directory_entry * search_wim_dirent(wim_directory_entry *dir, const char *search_name)
{
    do 
    {
        if (dir->len && dir->name_len)
        {
            if (wim_name_cmp(search_name, (grub_uint16_t *)(dir + 1), dir->name_len / 2) == 0)
            {
                return dir;
            }
        }
        dir = (wim_directory_entry *)((grub_uint8_t *)dir + dir->len);
    } while(dir->len);
        
    return NULL;
}

wim_directory_entry * search_full_wim_dirent
(
    void *meta_data, 
    wim_directory_entry *dir,
    const char **path    
)
{
    wim_directory_entry *subdir = NULL;
    wim_directory_entry *search = dir;

    while (*path)
    {
        subdir = (wim_directory_entry *)((char *)meta_data + search->subdir);
        search = search_wim_dirent(subdir, *path);
        path++;
    }
    
    return search;
}



wim_lookup_entry * ventoy_find_look_entry(wim_header *header, wim_lookup_entry *lookup, wim_hash *hash)
{
    grub_uint32_t i = 0;
    
    for (i = 0; i < (grub_uint32_t)header->lookup.raw_size / sizeof(wim_lookup_entry); i++)
    {
        if (grub_memcmp(&lookup[i].hash, hash, sizeof(wim_hash)) == 0)
        {
            return lookup + i;
        }
    }

    return NULL;
}

int parse_registry_setup_cmdline
(
    grub_file_t file, 
    wim_header *head, 
    wim_lookup_entry *lookup, 
    void *meta_data, 
    wim_directory_entry *dir, 
    char *buf, 
    grub_uint32_t buflen
)
{
    char c;
    int ret = 0;
    grub_uint32_t i = 0;    
    grub_uint32_t reglen = 0;
    wim_hash zerohash;
    reg_vk *regvk = NULL;
    wim_lookup_entry *look = NULL;
    wim_directory_entry *wim_dirent = NULL;
    char *decompress_data = NULL;
    const char *reg_path[] = { "Windows", "System32", "config", "SYSTEM", NULL };

    wim_dirent = search_full_wim_dirent(meta_data, dir, reg_path);
    debug("search reg SYSTEM %p\n", wim_dirent);
    if (!wim_dirent)
    {
        return 1;
    }

    grub_memset(&zerohash, 0, sizeof(zerohash));
    if (grub_memcmp(&zerohash, wim_dirent->hash.sha1, sizeof(wim_hash)) == 0)
    {
        return 2;
    }

    look = ventoy_find_look_entry(head, lookup, &wim_dirent->hash);
    if (!look)
    {
        return 3;
    }

    reglen = (grub_uint32_t)look->resource.raw_size;
    debug("find system lookup entry_id:%ld raw_size:%u\n", 
        ((long)look - (long)lookup) / sizeof(wim_lookup_entry), reglen);

    if (0 != ventoy_read_resource(file, head, &(look->resource), (void **)&(decompress_data)))
    {
        return 4;
    }

    if (grub_strncmp(decompress_data + 0x1000, "hbin", 4))
    {
        ret_goto_end(5);
    }

    for (i = 0x1000; i + sizeof(reg_vk) < reglen; i += 8)
    {
        regvk = (reg_vk *)(decompress_data + i);
        if (regvk->sig == 0x6B76 && regvk->namesize == 7 &&
            regvk->datatype == 1 && regvk->flag == 1)
        {
            if (grub_strncasecmp((char *)(regvk + 1), "cmdline", 7) == 0)
            {
                debug("find registry cmdline i:%u offset:(0x%x)%u size:(0x%x)%u\n", 
                        i, regvk->dataoffset, regvk->dataoffset, regvk->datasize, regvk->datasize);
                break;
            }
        }
    }

    if (i + sizeof(reg_vk) >= reglen || regvk == NULL)
    {
        ret_goto_end(6);
    }

    if (regvk->datasize == 0 || (regvk->datasize & 0x80000000) > 0 ||
        regvk->dataoffset == 0 || regvk->dataoffset == 0xFFFFFFFF)
    {
        ret_goto_end(7);
    }

    if (regvk->datasize / 2 >= buflen)
    {
        ret_goto_end(8);
    }

    debug("start offset is 0x%x(%u)\n", 0x1000 + regvk->dataoffset + 4, 0x1000 + regvk->dataoffset + 4);

    for (i = 0; i < regvk->datasize; i+=2)
    {
        c = (char)(*(grub_uint16_t *)(decompress_data + 0x1000 + regvk->dataoffset + 4 + i));
        *buf++ = c;
    }

    ret = 0;

end:
    grub_check_free(decompress_data);
    return ret;
}

int parse_custom_setup_path(char *cmdline, const char **path, char *exefile)
{
    int i = 0;
    int len = 0;
    char *pos1 = NULL;
    char *pos2 = NULL;
    
    if ((cmdline[0] == 'x' || cmdline[0] == 'X') && cmdline[1] == ':')
    {
        pos1 = pos2 = cmdline + 3;

        while (i < VTOY_MAX_DIR_DEPTH && *pos2)
        {
            while (*pos2 && *pos2 != '\\' && *pos2 != '/')
            {
                pos2++;
            }

            path[i++] = pos1;
            
            if (*pos2 == 0)
            {                
                break;
            }

            *pos2 = 0;
            pos1 = pos2 + 1;
            pos2 = pos1;
        }

        if (i == 0 || i >= VTOY_MAX_DIR_DEPTH)
        {
            return 1;
        }
    }
    else
    {
        path[i++] = "Windows";
        path[i++] = "System32";
        path[i++] = cmdline;
    }

    pos1 = (char *)path[i - 1];
    while (*pos1 != ' ' && *pos1 != '\t' && *pos1)
    {
        pos1++;
    }
    *pos1 = 0;

    len = (int)grub_strlen(path[i - 1]);
    if (len < 4 || grub_strcasecmp(path[i - 1] + len - 4, ".exe") != 0)
    {
        grub_snprintf(exefile, 256, "%s.exe", path[i - 1]);
        path[i - 1] = exefile;            
    }


    debug("custom setup: %d <%s>\n", i, path[i - 1]);
    return 0;
}

wim_directory_entry * search_replace_wim_dirent
(
    grub_file_t file, 
    wim_header *head, 
    wim_lookup_entry *lookup, 
    void *meta_data, 
    wim_directory_entry *dir
)
{
    int ret;
    char exefile[256] = {0};
    char cmdline[256] = {0};
    wim_directory_entry *wim_dirent = NULL;
    wim_directory_entry *pecmd_dirent = NULL;
    const char *peset_path[] = { "Windows", "System32", "peset.exe", NULL };
    const char *pecmd_path[] = { "Windows", "System32", "pecmd.exe", NULL };
    const char *winpeshl_path[] = { "Windows", "System32", "winpeshl.exe", NULL };
    const char *custom_path[VTOY_MAX_DIR_DEPTH + 1] = { NULL };

    pecmd_dirent = search_full_wim_dirent(meta_data, dir, pecmd_path);
    debug("search pecmd.exe %p\n", pecmd_dirent);

    if (pecmd_dirent)
    {
        ret = parse_registry_setup_cmdline(file, head, lookup, meta_data, dir, cmdline, sizeof(cmdline) - 1);
        if (0 == ret)
        {
            debug("registry setup cmdline:<%s>\n", cmdline);
            
            if (grub_strncasecmp(cmdline, "PECMD", 5) == 0)
            {
                wim_dirent = pecmd_dirent;
            }
            else if (grub_strncasecmp(cmdline, "PESET", 5) == 0)
            {
                wim_dirent = search_full_wim_dirent(meta_data, dir, peset_path);
                debug("search peset.exe %p\n", wim_dirent);
            }
            else if (grub_strncasecmp(cmdline, "WINPESHL", 8) == 0)
            {
                wim_dirent = search_full_wim_dirent(meta_data, dir, winpeshl_path);
                debug("search winpeshl.exe %p\n", wim_dirent);
            }
            else if (0 == parse_custom_setup_path(cmdline, custom_path, exefile))
            {
                wim_dirent = search_full_wim_dirent(meta_data, dir, custom_path);
                debug("search custom path %p\n", wim_dirent);
            }

            if (wim_dirent)
            {
                return wim_dirent;
            }
        }
        else
        {
            debug("registry setup cmdline failed : %d\n", ret);
        }
    }

    wim_dirent = pecmd_dirent;
    if (wim_dirent)
    {
        return wim_dirent;
    }

    wim_dirent = search_full_wim_dirent(meta_data, dir, winpeshl_path);
    debug("search winpeshl.exe %p\n", wim_dirent);
    if (wim_dirent)
    {
        return wim_dirent;
    }

    return NULL;
}


wim_lookup_entry * ventoy_find_meta_entry(wim_header *header, wim_lookup_entry *lookup)
{
    grub_uint32_t i = 0;
    grub_uint32_t index = 0;;

    if ((header == NULL) || (lookup == NULL))
    {
        return NULL;
    }
    
    for (i = 0; i < (grub_uint32_t)header->lookup.raw_size / sizeof(wim_lookup_entry); i++)
    {
        if (lookup[i].resource.flags & RESHDR_FLAG_METADATA)
        {
            index++;
            if (index == header->boot_index)
            {
                return lookup + i;
            }
        }
    }

    return NULL;
}

grub_uint64_t ventoy_get_stream_len(wim_directory_entry *dir)
{
    grub_uint16_t i;
    grub_uint64_t offset = 0;
    wim_stream_entry *stream = (wim_stream_entry *)((char *)dir + dir->len);

    for (i = 0; i < dir->streams; i++)
    {
        offset += stream->len;
        stream = (wim_stream_entry *)((char *)stream + stream->len);
    }

    return offset;
}

int ventoy_update_stream_hash(wim_patch *patch, wim_directory_entry *dir)
{
    grub_uint16_t i;
    grub_uint64_t offset = 0;
    wim_stream_entry *stream = (wim_stream_entry *)((char *)dir + dir->len);

    for (i = 0; i < dir->streams; i++)
    {
        if (grub_memcmp(stream->hash.sha1, patch->old_hash.sha1, sizeof(wim_hash)) == 0)
        {
            debug("find target stream %u, name_len:%u upadte hash\n", i, stream->name_len);
            grub_memcpy(stream->hash.sha1, &(patch->wim_data.bin_hash), sizeof(wim_hash));
        }

        offset += stream->len;
        stream = (wim_stream_entry *)((char *)stream + stream->len);
    }

    return offset;
}

int ventoy_update_all_hash(wim_patch *patch, void *meta_data, wim_directory_entry *dir)
{
    if ((meta_data == NULL) || (dir == NULL))
    {
        return 0;
    }

    if (dir->len < sizeof(wim_directory_entry))
    {
        return 0;
    }

    do
    {
        if (dir->subdir == 0 && grub_memcmp(dir->hash.sha1, patch->old_hash.sha1, sizeof(wim_hash)) == 0)
        {
            debug("find target file, name_len:%u upadte hash\n", dir->name_len);
            grub_memcpy(dir->hash.sha1, &(patch->wim_data.bin_hash), sizeof(wim_hash));
        }
        
        if (dir->subdir)
