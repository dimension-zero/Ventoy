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

    /* part 2: chain head */
    disk = wimfile->device->disk;
    chain->disk_drive = disk->id;
    chain->disk_sector_size = (1 << disk->log_sector_size);
    chain->real_img_size_in_bytes = ventoy_align_2k(file->size) + ventoy_align_2k(wimsize);
    chain->virt_img_size_in_bytes = chain->real_img_size_in_bytes;
    chain->boot_catalog = boot_catlog;

    if (!ventoy_is_efi_os())
    {
        grub_file_seek(file, boot_catlog * 2048);
        grub_file_read(file, chain->boot_catalog_sector, sizeof(chain->boot_catalog_sector));
    }

    /* part 3: image chunk */
    chain->img_chunk_offset = sizeof(ventoy_chain_head);
    chain->img_chunk_num = g_wimiso_chunk_list.cur_chunk + wimchunk.cur_chunk;
    grub_memcpy((char *)chain + chain->img_chunk_offset, g_wimiso_chunk_list.chunk, img_chunk1_size);

    chunknode = (ventoy_img_chunk *)((char *)chain + chain->img_chunk_offset);
    for (i = 0; i < g_wimiso_chunk_list.cur_chunk; i++)
    {
        chunknode->disk_end_sector = chunknode->disk_end_sector - chunknode->disk_start_sector;
        chunknode->disk_start_sector = 0;
        chunknode++;
    }

    /* fs cluster size >= 2048, so don't need to proc align */

    /* align by 2048 */
    chunknode = wimchunk.chunk + wimchunk.cur_chunk - 1;
    i = (chunknode->disk_end_sector + 1 - chunknode->disk_start_sector) % 4;
    if (i)
    {
        chunknode->disk_end_sector += 4 - i;
    }

    isosector = (grub_uint32_t)((file->size + 2047) / 2048);
    for (i = 0; i < wimchunk.cur_chunk; i++)
    {
        chunknode = wimchunk.chunk + i;
        chunknode->img_start_sector = isosector;
        chunknode->img_end_sector = chunknode->img_start_sector + 
            ((chunknode->disk_end_sector + 1 - chunknode->disk_start_sector) / 4) - 1;
        isosector = chunknode->img_end_sector + 1;
    }
    
    grub_memcpy((char *)chain + chain->img_chunk_offset + img_chunk1_size, wimchunk.chunk, img_chunk2_size);

    /* part 4: override chunk */
    chain->override_chunk_offset = chain->img_chunk_offset + img_chunk1_size + img_chunk2_size;
    chain->override_chunk_num = 1;

    override = (ventoy_override_chunk *)((char *)chain + chain->override_chunk_offset);
    override->img_offset = 0;
    override->override_size = g_wimiso_size;

    grub_file_seek(file, 0);
    grub_file_read(file, override->override_data, file->size);
    
    dirent = (ventoy_iso9660_override *)(override->override_data + imgoffset);
    dirent->first_sector    = (grub_uint32_t)((file->size + 2047) / 2048);
    dirent->size            = (grub_uint32_t)(wimsize);
    dirent->first_sector_be = grub_swap_bytes32(dirent->first_sector);
    dirent->size_be         = grub_swap_bytes32(dirent->size);

    debug("imgoffset=%u first_sector=0x%x size=0x%x\n", imgoffset, dirent->first_sector, dirent->size);

    if (ventoy_is_efi_os() == 0)
    {
        ventoy_windows_drive_map(chain, 0);        
    }

    grub_file_close(file);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_normal_wim_chain_data(grub_file_t wimfile)
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
    
    debug("normal wim chain data begin <%s> ...\n", wimfile->name);

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
    override_size = sizeof(ventoy_override_chunk);
    
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
        grub_printf("Failed to alloc chain win3 memory size %u\n", size);
        grub_file_close(file);
        return 1;
    }

    ventoy_memfile_env_set("vtoy_chain_mem", chain, (ulonglong)size);

    grub_memset(chain, 0, sizeof(ventoy_chain_head));

    /* part 1: os parameter */
    g_ventoy_chain_type = ventoy_chain_wim;
    ventoy_fill_os_param(file, &(chain->os_param));

    /* part 2: chain head */
    disk = file->device->disk;
    chain->disk_drive = disk->id;
    chain->disk_sector_size = (1 << disk->log_sector_size);
    chain->real_img_size_in_bytes = ventoy_align_2k(file->size) + ventoy_align_2k(wimsize);
    chain->virt_img_size_in_bytes = chain->real_img_size_in_bytes;
    chain->boot_catalog = boot_catlog;

    if (!ventoy_is_efi_os())
    {
        grub_file_seek(file, boot_catlog * 2048);
        grub_file_read(file, chain->boot_catalog_sector, sizeof(chain->boot_catalog_sector));
    }

    /* part 3: image chunk */
    chain->img_chunk_offset = sizeof(ventoy_chain_head);
    chain->img_chunk_num = g_wimiso_chunk_list.cur_chunk + wimchunk.cur_chunk;
    grub_memcpy((char *)chain + chain->img_chunk_offset, g_wimiso_chunk_list.chunk, img_chunk1_size);

    /* fs cluster size >= 2048, so don't need to proc align */

    /* align by 2048 */
    chunknode = wimchunk.chunk + wimchunk.cur_chunk - 1;
    i = (chunknode->disk_end_sector + 1 - chunknode->disk_start_sector) % 4;
    if (i)
    {
        chunknode->disk_end_sector += 4 - i;
    }

    isosector = (grub_uint32_t)((file->size + 2047) / 2048);
    for (i = 0; i < wimchunk.cur_chunk; i++)
    {
        chunknode = wimchunk.chunk + i;
        chunknode->img_start_sector = isosector;
        chunknode->img_end_sector = chunknode->img_start_sector + 
            ((chunknode->disk_end_sector + 1 - chunknode->disk_start_sector) / 4) - 1;
        isosector = chunknode->img_end_sector + 1;
    }
    
    grub_memcpy((char *)chain + chain->img_chunk_offset + img_chunk1_size, wimchunk.chunk, img_chunk2_size);

    /* part 4: override chunk */
    chain->override_chunk_offset = chain->img_chunk_offset + img_chunk1_size + img_chunk2_size;
    chain->override_chunk_num = 1;

    override = (ventoy_override_chunk *)((char *)chain + chain->override_chunk_offset);
    override->img_offset = imgoffset;
    override->override_size = sizeof(ventoy_iso9660_override);
    
    dirent = (ventoy_iso9660_override *)(override->override_data);
    dirent->first_sector    = (grub_uint32_t)((file->size + 2047) / 2048);
    dirent->size            = (grub_uint32_t)(wimsize);
    dirent->first_sector_be = grub_swap_bytes32(dirent->first_sector);
    dirent->size_be         = grub_swap_bytes32(dirent->size);

    debug("imgoffset=%u first_sector=0x%x size=0x%x\n", imgoffset, dirent->first_sector, dirent->size);

    if (ventoy_is_efi_os() == 0)
    {
        ventoy_windows_drive_map(chain, 0);        
    }

    grub_file_close(file);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_wim_chain_data(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_err_t ret;
    grub_file_t wimfile;

    (void)ctxt;
    (void)argc;

    wimfile = grub_file_open(args[0], VENTOY_FILE_TYPE);
    if (!wimfile)
    {
        return 1;
    }

    if (wimfile->vlnk)
    {
        ret = ventoy_vlnk_wim_chain_data(wimfile);
    }
    else
    {
        ret = ventoy_normal_wim_chain_data(wimfile);
    }

    grub_file_close(wimfile);
    return ret;
}

int ventoy_chain_file_size(const char *path)
{
    int size;
    grub_file_t file;

    file = grub_file_open(path, VENTOY_FILE_TYPE);
    size = (int)(file->size);

    grub_file_close(file);
    
    return size;
}

int ventoy_chain_file_read(const char *path, int offset, int len, void *buf)
{
    int size;
    grub_file_t file;

    file = grub_file_open(path, VENTOY_FILE_TYPE);
    grub_file_seek(file, offset);
    size = grub_file_read(file, buf, len);
    grub_file_close(file);
    
    return size;
}

