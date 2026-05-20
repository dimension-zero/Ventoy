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

static wim_directory_entry * search_replace_wim_dirent
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
        {
            ventoy_update_all_hash(patch, meta_data, (wim_directory_entry *)((char *)meta_data + dir->subdir));
        }

        if (dir->streams)
        {
            ventoy_update_stream_hash(patch, dir);
            dir = (wim_directory_entry *)((char *)dir + dir->len + ventoy_get_stream_len(dir));
        }
        else
        {
            dir = (wim_directory_entry *)((char *)dir + dir->len);            
        }
    } while (dir->len >= sizeof(wim_directory_entry));

    return 0;
}

