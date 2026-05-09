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

int ventoy_wimdows_locate_wim(const char *disk, wim_patch *patch, int windatalen)
{
    int rc;
    grub_uint16_t i;
    grub_file_t file;
    grub_uint32_t exe_len;
    grub_uint8_t *exe_data = NULL;
    grub_uint8_t *decompress_data = NULL;
    wim_lookup_entry *lookup = NULL;
    wim_security_header *security = NULL;
    wim_directory_entry *rootdir = NULL;
    wim_directory_entry *search = NULL;
    wim_stream_entry *stream = NULL;
    wim_header *head = &(patch->wim_data.wim_header);    
    wim_tail *wim_data = &patch->wim_data;
    
    debug("windows locate wim start %s\n", patch->path);

    g_ventoy_case_insensitive = 1;
    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s", disk, patch->path);
    g_ventoy_case_insensitive = 0;
    
    if (!file)
    {
        debug("File %s%s NOT exist\n", disk, patch->path);
        return 1;
    }

    ventoy_get_override_info(file, &patch->wim_data);

    grub_file_seek(file, 0);
    grub_file_read(file, head, sizeof(wim_header));

    if (grub_memcmp(head->signature, WIM_HEAD_SIGNATURE, sizeof(head->signature)))
    {
        debug("Not a valid wim file %s\n", (char *)head->signature);
        grub_file_close(file);
        return 1;
    }

    if (head->flags & FLAG_HEADER_COMPRESS_LZMS)
    {
        debug("LZMS compress is not supported 0x%x\n", head->flags);
        grub_file_close(file);
        return 1;
    }

    rc = ventoy_read_resource(file, head, &head->metadata, (void **)&decompress_data);
    if (rc)
    {
        grub_printf("failed to read meta data %d\n", rc);
        grub_file_close(file);
        return 1;
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
        grub_file_close(file);
        return 1;
    }
    
    debug("find replace file at %p\n", search);

    grub_memset(&patch->old_hash, 0, sizeof(wim_hash));
    if (grub_memcmp(&patch->old_hash, search->hash.sha1, sizeof(wim_hash)) == 0)
    {
        debug("search hash all 0, now do deep search\n");
        stream = (wim_stream_entry *)((char *)search + search->len);
        for (i = 0; i < search->streams; i++)
        {
            if (stream->name_len == 0)
            {
                grub_memcpy(&patch->old_hash, stream->hash.sha1, sizeof(wim_hash));
                debug("new search hash: %02x %02x %02x %02x %02x %02x %02x %02x\n", 
                    ventoy_varg_8(patch->old_hash.sha1));
                break;
            }
            stream = (wim_stream_entry *)((char *)stream + stream->len);
        }
    }
    else
    {
        grub_memcpy(&patch->old_hash, search->hash.sha1, sizeof(wim_hash));        
    }

    
    /* find and extact winpeshl.exe */
    patch->replace_look = ventoy_find_look_entry(head, lookup, &patch->old_hash);
    if (patch->replace_look)
    {
        exe_len = (grub_uint32_t)patch->replace_look->resource.raw_size;
        debug("find replace lookup entry_id:%ld raw_size:%u\n", 
            ((long)patch->replace_look - (long)lookup) / sizeof(wim_lookup_entry), exe_len);

        if (0 == ventoy_read_resource(file, head, &(patch->replace_look->resource), (void **)&(exe_data)))
        {
            ventoy_cat_exe_file_data(wim_data, exe_len, exe_data, windatalen);
            grub_free(exe_data);
        }
        else
        {
            debug("failed to read replace file meta data %u\n", exe_len);
        }
    }
    else
    {
        debug("failed to find lookup entry for replace file %02x %02x %02x %02x\n", 
            ventoy_varg_4(patch->old_hash.sha1));
    }

    wim_data->wim_raw_size = (grub_uint32_t)file->size;
    wim_data->wim_align_size = ventoy_align(wim_data->wim_raw_size, 2048);
    
    grub_check_free(wim_data->new_meta_data);
    wim_data->new_meta_data = decompress_data;
    wim_data->new_meta_len = head->metadata.raw_size;
    wim_data->new_meta_align_len = ventoy_align(wim_data->new_meta_len, 2048);
    
    grub_check_free(wim_data->new_lookup_data);
    wim_data->new_lookup_data = (grub_uint8_t *)lookup;
    wim_data->new_lookup_len = (grub_uint32_t)head->lookup.raw_size;
    wim_data->new_lookup_align_len = ventoy_align(wim_data->new_lookup_len, 2048);

    head->metadata.flags = RESHDR_FLAG_METADATA;
    head->metadata.offset = wim_data->wim_align_size + wim_data->bin_align_len;
    head->metadata.size_in_wim = wim_data->new_meta_len;
    head->metadata.raw_size = wim_data->new_meta_len;

    head->lookup.flags = 0;
    head->lookup.offset = head->metadata.offset + wim_data->new_meta_align_len;
    head->lookup.size_in_wim = wim_data->new_lookup_len;
    head->lookup.raw_size = wim_data->new_lookup_len;

    grub_file_close(file);

    debug("%s", "windows locate wim finish\n");
    return 0;
}

grub_err_t ventoy_cmd_sel_winpe_wim(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i = 0;
    int pos = 0;
    int len = 0;
    int find = 0;
    char *cmd = NULL;
    wim_patch *node = NULL;
    wim_patch *tmp = NULL;
    grub_file_t file = NULL;
    wim_header *head = NULL;
    char cfgfile[128];

    (void)ctxt;
    (void)argc;

    len = 8 * VTOY_SIZE_1KB;
    cmd = (char *)grub_malloc(len + sizeof(wim_header));
    if (!cmd)
    {
        return 1;
    }

    head = (wim_header *)(cmd + len);
    grub_env_unset("vtoy_pe_wim_path");

    for (node = g_wim_patch_head; node; node = node->next)
    {
        find = 0;
        for (tmp = g_wim_patch_head; tmp != node; tmp = tmp->next)
        {
            if (tmp->valid && grub_strcasecmp(tmp->path, node->path) == 0)
            {
                find = 1;
                break;
            }
        }

        if (find)
        {
            continue;
        }
        
        g_ventoy_case_insensitive = 1;
        file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s", args[0], node->path);
        g_ventoy_case_insensitive = 0;
        if (!file)
        {
            debug("File %s%s NOT exist\n", args[0], node->path);
            continue;
        }

        grub_file_read(file, head, sizeof(wim_header));
        if (grub_memcmp(head->signature, WIM_HEAD_SIGNATURE, sizeof(head->signature)))
        {
            debug("Not a valid wim file %s\n", (char *)head->signature);
            grub_file_close(file);
            continue;
        }

        if (head->flags & FLAG_HEADER_COMPRESS_LZMS)
        {
            debug("LZMS compress is not supported 0x%x\n", head->flags);
            grub_file_close(file);
            continue;
        }

        grub_file_close(file);
        node->valid = 1;

        vtoy_len_ssprintf(cmd, pos, len, "menuentry \"%s\" --class=\"sel_wim\"  {\n  echo \"\"\n}\n", node->path);
    }

    if (pos > 0)
    {
        g_ventoy_menu_esc = 1;
        g_ventoy_suppress_esc = 1;
        g_ventoy_suppress_esc_default = 0;
        g_ventoy_secondary_menu_on = 1;

        grub_snprintf(cfgfile, sizeof(cfgfile), "configfile mem:0x%llx:size:%d", (ulonglong)(ulong)cmd, pos);
        grub_script_execute_sourcecode(cfgfile);   

        g_ventoy_menu_esc = 0;
        g_ventoy_suppress_esc = 0;
        g_ventoy_suppress_esc_default = 1;
        g_ventoy_secondary_menu_on = 0;

        for (node = g_wim_patch_head; node; node = node->next)
        {
            if (node->valid)
            {
                if (i == g_ventoy_last_entry)
                {
                    grub_env_set("vtoy_pe_wim_path", node->path);
                    break;
                }
                i++;
            }
        }
    }

    grub_free(cmd);
    return 0;
}

grub_err_t ventoy_cmd_locate_wim_patch(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int datalen = 0;
    int dataflag = 0;
    wim_patch *node = g_wim_patch_head;

    (void)ctxt;
    (void)argc;
    (void)args;

    datalen = ventoy_get_windows_rtdata_len(args[1], &dataflag);

    while (node)
    {
        node->wim_data.windata_flag = dataflag;
        if (0 == ventoy_wimdows_locate_wim(args[0], node, datalen))
        {
            node->valid = 1;
            g_wim_valid_patch_count++;
        }

