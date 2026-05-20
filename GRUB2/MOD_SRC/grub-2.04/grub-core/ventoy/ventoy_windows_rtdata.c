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
int ventoy_cat_exe_file_data(wim_tail *wim_data, grub_uint32_t exe_len, grub_uint8_t *exe_data, int windatalen)
{
    int pe64 = 0;
    char file[256];
    grub_uint32_t jump_len = 0;
    grub_uint32_t jump_align = 0;
    grub_uint8_t *jump_data = NULL;

    pe64 = ventoy_is_pe64(exe_data);
    
    grub_snprintf(file, sizeof(file), "%s/vtoyjump%d.exe", grub_env_get("vtoy_path"), pe64 ? 64 : 32);
    ventoy_load_jump_exe(file, &jump_data, &jump_len, NULL);
    jump_align = ventoy_align(jump_len, 16);
    
    wim_data->jump_exe_len = jump_len;
    wim_data->bin_raw_len = jump_align + sizeof(ventoy_os_param) + windatalen + exe_len;
    wim_data->bin_align_len = ventoy_align(wim_data->bin_raw_len, 2048);
    
    wim_data->jump_bin_data = grub_malloc(wim_data->bin_align_len);
    if (wim_data->jump_bin_data)
    {
        grub_memcpy(wim_data->jump_bin_data, jump_data, jump_len);
        grub_memcpy(wim_data->jump_bin_data + jump_align + sizeof(ventoy_os_param) + windatalen, exe_data, exe_len);
    }

    debug("jump_exe_len:%u bin_raw_len:%u bin_align_len:%u\n", 
        wim_data->jump_exe_len, wim_data->bin_raw_len, wim_data->bin_align_len);
    
    return 0;
}

int ventoy_get_windows_rtdata_len(const char *iso, int *flag)
{
    int size = 0;
    int template_file_len = 0;
    char *pos = NULL;
    char *script = NULL;
    install_template *template_node = NULL;

    *flag = 0;
    size = (int)sizeof(ventoy_windows_data);
    
    pos = grub_strstr(iso, "/");
    if (!pos)
    {
        return size;
    }
    
    script = ventoy_plugin_get_cur_install_template(pos, &template_node);
    if (script)
    {
        (*flag) |= WINDATA_FLAG_TEMPLATE;
        template_file_len = template_node->filelen;
    }

    return size + template_file_len;
}

int ventoy_fill_windows_rtdata(void *buf, char *isopath, int dataflag)
{
    int template_len = 0;
    char *pos = NULL;
    char *end = NULL;
    char *script = NULL;
    const char *env = NULL;
    install_template *template_node = NULL;
    ventoy_windows_data *data = (ventoy_windows_data *)buf;

    grub_memset(data, 0, sizeof(ventoy_windows_data));

    env = grub_env_get("VTOY_WIN11_BYPASS_CHECK");
    if (env && env[0] == '1' && env[1] == 0)
    {
        data->windows11_bypass_check = 1;
    }
    
    env = grub_env_get("VTOY_WIN11_BYPASS_NRO");
    if (env && env[0] == '1' && env[1] == 0)
    {
        data->windows11_bypass_nro = 1;
    }

    pos = grub_strstr(isopath, "/");
    if (!pos)
    {
        return 1;
    }

    if (dataflag & WINDATA_FLAG_TEMPLATE)
    {
        script = ventoy_plugin_get_cur_install_template(pos, &template_node);
        if (script)
        {
            data->auto_install_len = template_len = template_node->filelen;
            debug("auto install script OK <%s> <len:%d>\n", script, template_len);
            end = ventoy_str_last(script, '/');
            grub_snprintf(data->auto_install_script, sizeof(data->auto_install_script) - 1, "%s", end ? end + 1 : script);
            grub_memcpy(data + 1, template_node->filebuf, template_len);
        }
    }
    else
    {
        debug("auto install script skipped or not configed %s\n", pos);
    }

    script = (char *)ventoy_plugin_get_injection(pos);
    if (script)
    {
        if (ventoy_check_file_exist("%s%s", ventoy_get_env("vtoy_iso_part"), script))
        {
            debug("injection archive <%s> OK\n", script);
            grub_snprintf(data->injection_archive, sizeof(data->injection_archive) - 1, "%s", script);
        }
        else
        {
            debug("injection archive <%s> NOT exist\n", script);
        }
    }
    else
    {
        debug("injection archive not configed %s\n", pos);
    }

    return 0;
}

int ventoy_update_before_chain(ventoy_os_param *param, char *isopath)
{
    grub_uint32_t jump_align = 0;
    wim_lookup_entry *meta_look = NULL;
    wim_security_header *security = NULL;
    wim_directory_entry *rootdir = NULL;
    wim_header *head = NULL;
    wim_lookup_entry *lookup = NULL;
    wim_patch *node = NULL;
    wim_tail *wim_data = NULL;

    for (node = g_wim_patch_head; node; node = node->next)
    {
        if (0 == node->valid)
        {
            continue;
        }

        wim_data = &node->wim_data;
        head = &wim_data->wim_header;
        lookup = (wim_lookup_entry *)wim_data->new_lookup_data;

        jump_align = ventoy_align(wim_data->jump_exe_len, 16);
        if (wim_data->jump_bin_data)
        {
            grub_memcpy(wim_data->jump_bin_data + jump_align, param, sizeof(ventoy_os_param));        
            ventoy_fill_windows_rtdata(wim_data->jump_bin_data + jump_align + sizeof(ventoy_os_param), isopath, wim_data->windata_flag);
        }

        grub_crypto_hash(GRUB_MD_SHA1, wim_data->bin_hash.sha1, wim_data->jump_bin_data, wim_data->bin_raw_len);

        security = (wim_security_header *)wim_data->new_meta_data;
        if (security->len > 0)
        {
            rootdir = (wim_directory_entry *)(wim_data->new_meta_data + ((security->len + 7) & 0xFFFFFFF8U));
        }
        else
        {
            rootdir = (wim_directory_entry *)(wim_data->new_meta_data + 8);
        }

        /* update all winpeshl.exe dirent entry's hash */
        ventoy_update_all_hash(node, wim_data->new_meta_data, rootdir);

        /* update winpeshl.exe lookup entry data (hash/offset/length) */
        if (node->replace_look)
        {
            debug("update replace lookup entry_id:%ld\n", ((long)node->replace_look - (long)lookup) / sizeof(wim_lookup_entry));
            node->replace_look->resource.raw_size = wim_data->bin_raw_len;
            node->replace_look->resource.size_in_wim = wim_data->bin_raw_len;
            node->replace_look->resource.flags = 0;
            node->replace_look->resource.offset = wim_data->wim_align_size;

            grub_memcpy(node->replace_look->hash.sha1, wim_data->bin_hash.sha1, sizeof(wim_hash));
        }

        /* update metadata's hash */
        meta_look = ventoy_find_meta_entry(head, lookup);
        if (meta_look)
        {
            debug("find meta lookup entry_id:%ld\n", ((long)meta_look - (long)lookup) / sizeof(wim_lookup_entry));
            grub_memcpy(&meta_look->resource, &head->metadata, sizeof(wim_resource_header));
            grub_crypto_hash(GRUB_MD_SHA1, meta_look->hash.sha1, wim_data->new_meta_data, wim_data->new_meta_len);
        }
    }

    return 0;
}

