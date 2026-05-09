/******************************************************************************
 * ventoy_linux.c 
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
#include <grub/ventoy.h>
#include "ventoy_def.h"
#include "ventoy_linux_priv.h"


#define VTOY_APPEND_EXT_SIZE 4096


static grub_uint32_t ventoy_linux_get_virt_chunk_count(void)
{
    int i;
    grub_uint32_t count = g_valid_initrd_count;
    
    if (g_conf_replace_count > 0)
    {
        for (i = 0; i < g_conf_replace_count; i++)
        {
            if (g_conf_replace_offset[i] > 0)
            {
                count++;
            }
        }
    }
    
    if (g_append_ext_sector > 0)
    {
        count++;
    }
    
    return count;
}

static grub_uint32_t ventoy_linux_get_virt_chunk_size(void)
{
    int i;
    grub_uint32_t size;
    
    size = (sizeof(ventoy_virt_chunk) + g_ventoy_cpio_size) * g_valid_initrd_count;

    if (g_conf_replace_count > 0)
    {
        for (i = 0; i < g_conf_replace_count; i++)
        {
            if (g_conf_replace_offset[i] > 0)
            {
                size += sizeof(ventoy_virt_chunk) + g_conf_replace_new_len_align[i];
            }
        }
    }
    
    if (g_append_ext_sector > 0)
    {
        size += sizeof(ventoy_virt_chunk) + VTOY_APPEND_EXT_SIZE;
    }

    return size;
}

static void ventoy_linux_fill_virt_data(    grub_uint64_t isosize, ventoy_chain_head *chain)
{
    int i = 0;
    int id = 0;
    int virtid = 0;
    initrd_info *node;
    grub_uint64_t sector;
    grub_uint32_t offset;
    grub_uint32_t cpio_secs;
    grub_uint32_t initrd_secs;
    char *override;
    ventoy_virt_chunk *cur;
    ventoy_grub_param_file_replace *replace = NULL;
    char name[32];

    override = (char *)chain + chain->virt_chunk_offset;
    sector = (isosize + 2047) / 2048;
    cpio_secs = g_ventoy_cpio_size / 2048;

    offset = ventoy_linux_get_virt_chunk_count() * sizeof(ventoy_virt_chunk);
    cur = (ventoy_virt_chunk *)override;

    for (node = g_initrd_img_list; node; node = node->next)
    {
        if (node->size == 0)
        {
            continue;
        }

        initrd_secs = (grub_uint32_t)((node->size + 2047) / 2048);

        cur->mem_sector_start   = sector;
        cur->mem_sector_end     = cur->mem_sector_start + cpio_secs;
        cur->mem_sector_offset  = offset;
        cur->remap_sector_start = cur->mem_sector_end;
        cur->remap_sector_end   = cur->remap_sector_start + initrd_secs;
        cur->org_sector_start   = (grub_uint32_t)(node->offset / 2048);

        grub_memcpy(g_ventoy_runtime_buf, &chain->os_param, sizeof(ventoy_os_param));

        grub_memset(name, 0, 16);
        grub_snprintf(name, sizeof(name), "initrd%03d", ++id);

        grub_memcpy(g_ventoy_initrd_head + 1, name, 16);
        ventoy_cpio_newc_fill_int((grub_uint32_t)node->size, g_ventoy_initrd_head->c_filesize, 8);

        grub_memcpy(override + offset, g_ventoy_cpio_buf, g_ventoy_cpio_size);

        chain->virt_img_size_in_bytes += g_ventoy_cpio_size + initrd_secs * 2048;

        offset += g_ventoy_cpio_size;
        sector += cpio_secs + initrd_secs;
        cur++;
        virtid++;
    }

    /* Lenovo EasyStartup need an addional sector for boundary check */
    if (g_append_ext_sector > 0)
    {
        cpio_secs = VTOY_APPEND_EXT_SIZE / 2048;
    
        cur->mem_sector_start   = sector;
        cur->mem_sector_end     = cur->mem_sector_start + cpio_secs;
        cur->mem_sector_offset  = offset;
        cur->remap_sector_start = 0;
        cur->remap_sector_end   = 0;
        cur->org_sector_start   = 0;

        grub_memset(override + offset, 0, VTOY_APPEND_EXT_SIZE);

        chain->virt_img_size_in_bytes += VTOY_APPEND_EXT_SIZE;

        offset += VTOY_APPEND_EXT_SIZE;
        sector += cpio_secs;
        cur++;
        virtid++;
    }

    if (g_conf_replace_count > 0)
    {
        for (i = 0; i < g_conf_replace_count; i++)
        {
            if (g_conf_replace_offset[i] > 0)
            {
                cpio_secs = g_conf_replace_new_len_align[i] / 2048;
            
                cur->mem_sector_start   = sector;
                cur->mem_sector_end     = cur->mem_sector_start + cpio_secs;
                cur->mem_sector_offset  = offset;
                cur->remap_sector_start = 0;
                cur->remap_sector_end   = 0;
                cur->org_sector_start   = 0;

                grub_memcpy(override + offset, g_conf_replace_new_buf[i], g_conf_replace_new_len[i]);

                chain->virt_img_size_in_bytes += g_conf_replace_new_len_align[i];

                replace = g_grub_param->img_replace + i;
                if (replace->magic == GRUB_IMG_REPLACE_MAGIC)
                {
                    replace->new_file_virtual_id = virtid;
                }

                offset += g_conf_replace_new_len_align[i];
                sector += cpio_secs;
                cur++;
                virtid++;
            }
        }
    }

    return;
}

static grub_uint32_t ventoy_linux_get_override_chunk_count(void)
{
    int i;
    grub_uint32_t count = g_valid_initrd_count;

    if (g_conf_replace_count > 0)
    {
        for (i = 0; i < g_conf_replace_count; i++)
        {
            if (g_conf_replace_offset[i] > 0)
            {
                count++;
            }
        }
    }

    if (g_svd_replace_offset > 0)
    {
        count++;
    }
    
    return count;
}

static grub_uint32_t ventoy_linux_get_override_chunk_size(void)
{
    int i;
    int count = g_valid_initrd_count;

    if (g_conf_replace_count > 0)
    {
        for (i = 0; i < g_conf_replace_count; i++)
        {
            if (g_conf_replace_offset[i] > 0)
            {
                count++;
            }
        }
    }

    if (g_svd_replace_offset > 0)
    {
        count++;
    }

    return sizeof(ventoy_override_chunk) * count;
}

static void ventoy_linux_fill_override_data(    grub_uint64_t isosize, void *override)
{
    int i;
    initrd_info *node;
    grub_uint32_t mod;
    grub_uint32_t newlen;
    grub_uint64_t sector;
    ventoy_override_chunk *cur;
    ventoy_iso9660_override *dirent;
    ventoy_udf_override *udf;

    sector = (isosize + 2047) / 2048;

    cur = (ventoy_override_chunk *)override;
    for (node = g_initrd_img_list; node; node = node->next)
    {
        if (node->size == 0)
        {
            continue;
        }

        newlen = (grub_uint32_t)(node->size + g_ventoy_cpio_size);
        mod = newlen % 4; 
        if (mod > 0)
        {
            newlen += 4 - mod; /* cpio must align with 4 */
        }

        if (node->iso_type == 0)
        {
            dirent = (ventoy_iso9660_override *)node->override_data;

            node->override_length   = sizeof(ventoy_iso9660_override);
            dirent->first_sector    = (grub_uint32_t)sector;
            dirent->size            = newlen;
            dirent->first_sector_be = grub_swap_bytes32(dirent->first_sector);
            dirent->size_be         = grub_swap_bytes32(dirent->size);

            sector += (dirent->size + 2047) / 2048;
        }
        else
        {
            udf = (ventoy_udf_override *)node->override_data;
            
            node->override_length = sizeof(ventoy_udf_override);
            udf->length   = newlen;
            udf->position = (grub_uint32_t)sector - node->udf_start_block;

            sector += (udf->length + 2047) / 2048;
        }

        cur->img_offset = node->override_offset;
        cur->override_size = node->override_length;
        grub_memcpy(cur->override_data, node->override_data, cur->override_size);
        cur++;
    }

    if (g_conf_replace_count > 0)
    {
        for (i = 0; i < g_conf_replace_count; i++)
        {
            if (g_conf_replace_offset[i] > 0)
            {        
                cur->img_offset = g_conf_replace_offset[i];
                cur->override_size = sizeof(ventoy_iso9660_override);

                newlen = (grub_uint32_t)(g_conf_replace_new_len[i]);

                dirent = (ventoy_iso9660_override *)cur->override_data;
                dirent->first_sector    = (grub_uint32_t)sector;
                dirent->size            = newlen;
                dirent->first_sector_be = grub_swap_bytes32(dirent->first_sector);
                dirent->size_be         = grub_swap_bytes32(dirent->size);

                sector += (dirent->size + 2047) / 2048;
                cur++;
            }
        }
    }

    if (g_svd_replace_offset > 0)
    {        
        cur->img_offset = g_svd_replace_offset;
        cur->override_size = 1;
        cur->override_data[0] = 0xFF;
        cur++;
    }

    return;
}
grub_err_t ventoy_cmd_linux_chain_data(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int len = 0;
    int ventoy_compatible = 0;
    grub_uint32_t size = 0;
    grub_uint64_t isosize = 0;
    grub_uint32_t boot_catlog = 0;
    grub_uint32_t img_chunk_size = 0;
    grub_uint32_t override_count = 0;
    grub_uint32_t override_size = 0;
    grub_uint32_t virt_chunk_count = 0;
    grub_uint32_t virt_chunk_size = 0;
    grub_file_t file;
    grub_disk_t disk;
    const char *pLastChain = NULL;
    const char *compatible;
    ventoy_chain_head *chain;
    
    (void)ctxt;
    (void)argc;

    compatible = grub_env_get("ventoy_compatible");
    if (compatible && compatible[0] == 'Y')
    {
        ventoy_compatible = 1;
    }

    if ((NULL == g_img_chunk_list.chunk) || (0 == ventoy_compatible && g_ventoy_cpio_buf == NULL))
    {
        grub_printf("ventoy not ready\n");
        return 1;
    }

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (!file)
    {
        return 1;
    }

    isosize = file->size;

    len = (int)grub_strlen(args[0]);
    if (len >= 4 && 0 == grub_strcasecmp(args[0] + len - 4, ".img"))
    {
        debug("boot catlog %u for img file\n", boot_catlog);
    }
    else
    {
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
    }
    
    img_chunk_size = g_img_chunk_list.cur_chunk * sizeof(ventoy_img_chunk);

    override_count = ventoy_linux_get_override_chunk_count();
    virt_chunk_count = ventoy_linux_get_virt_chunk_count();
    
    if (ventoy_compatible)
    {
        size = sizeof(ventoy_chain_head) + img_chunk_size;
    }
    else
    {
        override_size = ventoy_linux_get_override_chunk_size();
        virt_chunk_size = ventoy_linux_get_virt_chunk_size();
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
        grub_printf("Failed to alloc chain linux memory size %u\n", size);
        grub_file_close(file);
        return 1;
    }

    ventoy_memfile_env_set("vtoy_chain_mem", chain, (ulonglong)size);

    grub_memset(chain, 0, sizeof(ventoy_chain_head));

    /* part 1: os parameter */
    g_ventoy_chain_type = ventoy_chain_linux;
    ventoy_fill_os_param(file, &(chain->os_param));

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

    if (ventoy_compatible)
    {
        return 0;
    }

    /* part 4: override chunk */
    if (override_count > 0)
    {
        chain->override_chunk_offset = chain->img_chunk_offset + img_chunk_size;
        chain->override_chunk_num = override_count;
        ventoy_linux_fill_override_data(isosize, (char *)chain + chain->override_chunk_offset);        
    }

    /* part 5: virt chunk */
    if (virt_chunk_count > 0)
    {        
        chain->virt_chunk_offset = chain->override_chunk_offset + override_size;
        chain->virt_chunk_num = virt_chunk_count;
        ventoy_linux_fill_virt_data(isosize, chain);
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}
