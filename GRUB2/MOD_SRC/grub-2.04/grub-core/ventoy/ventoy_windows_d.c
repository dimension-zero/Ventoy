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

grub_err_t ventoy_cmd_windows_chain_data(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int unknown_image = 0;
    int ventoy_compatible = 0;
    grub_uint32_t size = 0;
    grub_uint64_t isosize = 0;
    grub_uint32_t boot_catlog = 0;
    grub_uint32_t img_chunk_size = 0;
    grub_uint32_t override_size = 0;
    grub_uint32_t virt_chunk_size = 0;
    grub_file_t file;
    grub_disk_t disk;
    const char *pLastChain = NULL;
    const char *compatible;
    ventoy_chain_head *chain;
    
    (void)ctxt;
    (void)argc;

    debug("chain data begin <%s> ...\n", args[0]);

    compatible = grub_env_get("ventoy_compatible");
    if (compatible && compatible[0] == 'Y')
    {
        ventoy_compatible = 1;
    }

    if (NULL == g_img_chunk_list.chunk)
    {
        grub_printf("ventoy not ready\n");
        return 1;
    }

    if (0 == ventoy_compatible && g_wim_valid_patch_count == 0)
    {
        unknown_image = 1;
        if (!g_ventoy_wimboot_mode)
        {
            debug("Warning: %s was not recognized by Ventoy\n", args[0]);            
        }
    }

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (!file)
    {
        return 1;
    }

    isosize = file->size;

    boot_catlog = ventoy_get_iso_boot_catlog(file);
    if (boot_catlog)
    {
        if (ventoy_is_efi_os() && (!ventoy_has_efi_eltorito(file, boot_catlog)))
        {
            grub_env_set("LoadIsoEfiDriver", "on");
        }
    }
    else
    {
        if (ventoy_is_efi_os())
        {
            grub_env_set("LoadIsoEfiDriver", "on");
        }
        else
        {
            return grub_error(GRUB_ERR_BAD_ARGUMENT, "File %s is not bootable", args[0]);
        }
    }

    g_suppress_wincd_override_offset = 0;
    if (!ventoy_is_efi_os()) /* legacy mode */
    {
        ventoy_suppress_windows_cd_prompt();
    }

    img_chunk_size = g_img_chunk_list.cur_chunk * sizeof(ventoy_img_chunk);
    
    if (ventoy_compatible || unknown_image)
    {
        override_size = g_suppress_wincd_override_offset > 0 ? sizeof(ventoy_override_chunk) : 0;
        size = sizeof(ventoy_chain_head) + img_chunk_size + override_size;
    }
    else
    {
        override_size = ventoy_get_override_chunk_num() * sizeof(ventoy_override_chunk);
        virt_chunk_size = ventoy_windows_get_virt_data_size();
        size = sizeof(ventoy_chain_head) + img_chunk_size + override_size + virt_chunk_size;
    }

    pLastChain = grub_env_get("vtoy_chain_mem_addr");
    if (pLastChain)
    {
        chain = (ventoy_chain_head *)grub_strtoul(pLastChain, NULL, 16);
        if (chain)
        {
            debug("free last chain memory %p\n", chain);
            grub_free(chain);
        }
    }

    chain = ventoy_alloc_chain(size);
    if (!chain)
    {
        grub_printf("Failed to alloc chain win1 memory size %u\n", size);
        grub_file_close(file);
        return 1;
    }

    ventoy_memfile_env_set("vtoy_chain_mem", chain, (ulonglong)size);

    grub_memset(chain, 0, sizeof(ventoy_chain_head));

    /* part 1: os parameter */
    g_ventoy_chain_type = ventoy_chain_windows;
    ventoy_fill_os_param(file, &(chain->os_param));

    if (0 == unknown_image)
    {
        ventoy_update_before_chain(&(chain->os_param), args[0]);
    }

    /* part 2: chain head */
    disk = file->device->disk;
    chain->disk_drive = disk->id;
    chain->disk_sector_size = (1 << disk->log_sector_size);
    chain->real_img_size_in_bytes = file->size;
    chain->virt_img_size_in_bytes = (file->size + 2047) / 2048 * 2048;
    chain->boot_catalog = boot_catlog;

    if (!ventoy_is_efi_os())
    {
        grub_file_seek(file, boot_catlog * 2048);
        grub_file_read(file, chain->boot_catalog_sector, sizeof(chain->boot_catalog_sector));
    }

    /* part 3: image chunk */
    chain->img_chunk_offset = sizeof(ventoy_chain_head);
    chain->img_chunk_num = g_img_chunk_list.cur_chunk;
    grub_memcpy((char *)chain + chain->img_chunk_offset, g_img_chunk_list.chunk, img_chunk_size);

    if (ventoy_compatible || unknown_image)
    {
        if (g_suppress_wincd_override_offset > 0)
        {
            chain->override_chunk_offset = chain->img_chunk_offset + img_chunk_size;
            chain->override_chunk_num = 1;
            ventoy_fill_suppress_wincd_override_data((char *)chain + chain->override_chunk_offset);
        }

        return 0;
    }

    if (0 == g_wim_valid_patch_count)
    {
        return 0;
    }

    /* part 4: override chunk */
    chain->override_chunk_offset = chain->img_chunk_offset + img_chunk_size;
    chain->override_chunk_num = ventoy_get_override_chunk_num();

    if (g_iso_fs_type == 0)
    {
        ventoy_windows_fill_override_data_iso9660(isosize, (char *)chain + chain->override_chunk_offset);
    }
    else
    {
        ventoy_windows_fill_override_data_udf(file, (char *)chain + chain->override_chunk_offset);        
    }

    /* part 5: virt chunk */
    chain->virt_chunk_offset = chain->override_chunk_offset + override_size;
    chain->virt_chunk_num = g_wim_valid_patch_count;
    ventoy_windows_fill_virt_data(isosize, chain);

    if (ventoy_is_efi_os() == 0)
    {
        ventoy_windows_drive_map(chain, file->vlnk);        
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_uint32_t ventoy_get_wim_iso_offset(const char *filepath)
{
    grub_uint32_t imgoffset;
    grub_file_t file;
    char cmdbuf[128];
    
    grub_snprintf(cmdbuf, sizeof(cmdbuf), "loopback wimiso \"%s\"", filepath);
    grub_script_execute_sourcecode(cmdbuf);

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", "(wimiso)/boot/boot.wim");
    if (!file)
    {
        grub_printf("Failed to open boot.wim file in the image file\n");
        return 0;
    }

    imgoffset = (grub_uint32_t)grub_iso9660_get_last_file_dirent_pos(file) + 2;

    debug("wimiso wim direct offset: %u\n", imgoffset);
    
    grub_file_close(file);
    
    grub_script_execute_sourcecode("loopback -d wimiso");

    return imgoffset;
}

int ventoy_get_wim_chunklist(grub_file_t wimfile, ventoy_img_chunk_list *wimchunk)
{
    grub_memset(wimchunk, 0, sizeof(ventoy_img_chunk_list));
    wimchunk->chunk = grub_malloc(sizeof(ventoy_img_chunk) * DEFAULT_CHUNK_NUM);
    if (NULL == wimchunk->chunk)
    {
        return grub_error(GRUB_ERR_OUT_OF_MEMORY, "Can't allocate image chunk memoty\n");
    }
    
    wimchunk->max_chunk = DEFAULT_CHUNK_NUM;
    wimchunk->cur_chunk = 0;
    
    ventoy_get_block_list(wimfile, wimchunk, wimfile->device->disk->partition->start);

    return 0;
}

grub_err_t ventoy_cmd_is_standard_winiso(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    int ret = 1;
    char prefix[32] = {0};
    const char *chkfile[] = 
    {
        "boot/bcd", "boot/boot.sdi", NULL
    };

    (void)ctxt;
    (void)argc;

    if (ventoy_check_file_exist("%s/sources/boot.wim", args[0]))
    {
        prefix[0] = 0;
    }
    else if (ventoy_check_file_exist("%s/x86/sources/boot.wim", args[0]))
    {
        grub_snprintf(prefix, sizeof(prefix), "/x86");
    }
    else if (ventoy_check_file_exist("%s/x64/sources/boot.wim", args[0]))
    {
        grub_snprintf(prefix, sizeof(prefix), "/x64");
    }
    else
    {
        debug("No boot.wim found.\n");
        goto out;
    }

    for (i = 0; chkfile[i]; i++)
    {
        if (!ventoy_check_file_exist("%s%s/%s", args[0], prefix, chkfile[i]))
        {
            debug("%s not found.\n", chkfile[i]);
            goto out;
        }
    }

    if ((!ventoy_check_file_exist("%s%s/sources/install.wim", args[0], prefix)) && 
        (!ventoy_check_file_exist("%s%s/sources/install.esd", args[0], prefix)))
    {
        debug("No install.wim(esd) found.\n");
        goto out;
    }

    if (!ventoy_check_file_exist("%s/setup.exe", args[0]))
    {
        debug("No setup.exe found.\n");
        goto out;
    }

    ret = 0;
    debug("This is standard Windows ISO.\n");

out:

    return ret;
}

grub_err_t ventoy_cmd_wim_check_bootable(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_uint32_t boot_index;
    grub_file_t file = NULL;
    wim_header *wimhdr = NULL;
    
    (void)ctxt;
    (void)argc;

    wimhdr = grub_zalloc(sizeof(wim_header));
    if (!wimhdr)
    {
        return 1;
    }

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (!file)
    {
        grub_free(wimhdr);
        return 1;
    }

    grub_file_read(file, wimhdr, sizeof(wim_header));
    grub_file_close(file);
    boot_index = wimhdr->boot_index;
    grub_free(wimhdr);

    if (boot_index == 0)
    {
        return 1;
    }
    
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_vlnk_wim_chain_data(grub_file_t wimfile)
{
    grub_uint32_t i = 0;
    grub_uint32_t imgoffset = 0;
    grub_uint32_t size = 0;
    grub_uint32_t isosector = 0;
    grub_uint64_t wimsize = 0;
    grub_uint32_t boot_catlog = 0;
    grub_uint32_t img_chunk1_size = 0;
    grub_uint32_t img_chunk2_size = 0;
    grub_uint32_t override_size = 0;
    grub_file_t file;
    grub_disk_t disk;
    const char *pLastChain = NULL;
    ventoy_chain_head *chain;
    ventoy_iso9660_override *dirent;
    ventoy_img_chunk *chunknode;
    ventoy_override_chunk *override;
    ventoy_img_chunk_list wimchunk;
    
    debug("vlnk wim chain data begin <%s> ...\n", wimfile->name);

    if (NULL == g_wimiso_chunk_list.chunk || NULL == g_wimiso_path)
    {
        grub_printf("ventoy not ready\n");
        return 1;
    }

    imgoffset = ventoy_get_wim_iso_offset(g_wimiso_path);
    if (imgoffset == 0)
    {
        grub_printf("image offset not found\n");
        return 1;
    }

    if (0 != ventoy_get_wim_chunklist(wimfile, &wimchunk))
    {
        grub_printf("Failed to get wim chunklist\n");
        return 1;
    }
    wimsize = wimfile->size;

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", g_wimiso_path);
    if (!file)
    {
        return 1;
    }

    boot_catlog = ventoy_get_iso_boot_catlog(file);

    img_chunk1_size = g_wimiso_chunk_list.cur_chunk * sizeof(ventoy_img_chunk);
    img_chunk2_size = wimchunk.cur_chunk * sizeof(ventoy_img_chunk);
    override_size = sizeof(ventoy_override_chunk) + g_wimiso_size;

    size = sizeof(ventoy_chain_head) + img_chunk1_size + img_chunk2_size + override_size;

    pLastChain = grub_env_get("vtoy_chain_mem_addr");
    if (pLastChain)
    {
        chain = (ventoy_chain_head *)grub_strtoul(pLastChain, NULL, 16);
        if (chain)
        {
            debug("free last chain memory %p\n", chain);
            grub_free(chain);
        }
    }

    chain = ventoy_alloc_chain(size);
    if (!chain)
    {
        grub_printf("Failed to alloc chain win2 memory size %u\n", size);
        grub_file_close(file);
        return 1;
    }

    ventoy_memfile_env_set("vtoy_chain_mem", chain, (ulonglong)size);

    grub_memset(chain, 0, sizeof(ventoy_chain_head));

    /* part 1: os parameter */
    g_ventoy_chain_type = ventoy_chain_wim;
    ventoy_fill_os_param(wimfile, &(chain->os_param));
