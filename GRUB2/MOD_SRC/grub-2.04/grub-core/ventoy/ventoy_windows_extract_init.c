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
            ((long)replace_look - (long)lookup) / sizeof(wim_lookup_entry), exe_len);

        if (0 != ventoy_read_resource(file, head, &(replace_look->resource), (void **)&(exe_data)))
        {
            exe_len = 0;
            exe_data = NULL;
            debug("failed to read replace file meta data %u\n", exe_len);
        }
    }
    else
    {
        debug("failed to find lookup entry for replace file %02x %02x %02x %02x\n", 
            ventoy_varg_4(hashdata.sha1));
    }

    if (exe_data)
    {
        ret = 0;
        *pexe_data = exe_data;
        *pexe_len = exe_len;
    }
    
out:

    grub_check_free(lookup);
    grub_check_free(decompress_data);
    check_free(file, grub_file_close);

    return ret;
}

grub_err_t ventoy_cmd_windows_wimboot_data(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int rc = 0;
    int wim64 = 0;
    int datalen = 0;
    int dataflag = 0;
    grub_uint32_t exe_len = 0;
    grub_uint32_t jump_align = 0;
    const char *addr = NULL;
    ventoy_chain_head *chain = NULL;
    grub_uint8_t *param = NULL;
    grub_uint8_t *exe_data = NULL;
    ventoy_windows_data *rtdata = NULL;
    char exename[128] = {0};
    wim_tail wim_data;

    (void)ctxt;
    (void)argc;

    addr = grub_env_get("vtoy_chain_mem_addr");
    if (!addr)
    {
        debug("Failed to find vtoy_chain_mem_addr\n");
        return 1;
    }

    chain = (ventoy_chain_head *)(void *)grub_strtoul(addr, NULL, 16);

    if (grub_memcmp(&g_ventoy_guid, &chain->os_param.guid, 16) != 0)
    {
        debug("os_param.guid not match\n");
        return 1;
    }

    datalen = ventoy_get_windows_rtdata_len(chain->os_param.vtoy_img_path, &dataflag);

    rc = ventoy_extract_init_exe(args[0], &exe_data, &exe_len, exename);
    if (rc)
    {
        return 1;
    }
    wim64 = ventoy_is_pe64(exe_data);

    grub_memset(&wim_data, 0, sizeof(wim_data));
    ventoy_cat_exe_file_data(&wim_data, exe_len, exe_data, datalen);
    grub_check_free(exe_data);

    jump_align = ventoy_align(wim_data.jump_exe_len, 16);
    param = wim_data.jump_bin_data;

    grub_memcpy(param + jump_align, &chain->os_param, sizeof(ventoy_os_param));

    rtdata = (ventoy_windows_data *)(param + jump_align + sizeof(ventoy_os_param));
    ventoy_fill_windows_rtdata(rtdata, chain->os_param.vtoy_img_path, dataflag);

    ventoy_memfile_env_set("vtoy_wimboot_mem", param, (ulonglong)(wim_data.bin_align_len));

    grub_env_set(args[1], exename);
    grub_env_set(args[2], wim64 ? "64" : "32");

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

