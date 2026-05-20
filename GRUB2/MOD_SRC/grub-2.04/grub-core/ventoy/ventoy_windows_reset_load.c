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
grub_err_t ventoy_cmd_wimdows_reset(grub_extcmd_context_t ctxt, int argc, char **args)
{
    wim_patch *next = NULL;
    wim_patch *node = g_wim_patch_head;

    (void)ctxt;
    (void)argc;
    (void)args;

    while (node)
    {
        next = node->next;
        grub_free(node);
        node = next;
    }

    g_wim_patch_head = NULL;
    g_wim_total_patch_count = 0;
    g_wim_valid_patch_count = 0;

    return 0;
}

int ventoy_load_jump_exe(const char *path, grub_uint8_t **data, grub_uint32_t *size, wim_hash *hash)
{
    grub_uint32_t i;
    grub_uint32_t align;
    grub_file_t file;

    debug("windows load jump %s\n", path);
    
    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", path);
    if (!file)
    {
        debug("Can't open file %s\n", path); 
        return 1;
    }

    align = ventoy_align((int)file->size, 2048);

    debug("file %s size:%d align:%u\n", path, (int)file->size, align);

    *size = (grub_uint32_t)file->size;
    *data = (grub_uint8_t *)grub_malloc(align);
    if ((*data) == NULL)
    {
        debug("Failed to alloc memory size %u\n", align);
        goto end;
    }

    grub_file_read(file, (*data), file->size);

    if (hash)
    {
        grub_crypto_hash(GRUB_MD_SHA1, hash->sha1, (*data), file->size);

        if (g_ventoy_debug)
        {
            debug("%s", "jump bin 64 hash: ");
            for (i = 0; i < sizeof(hash->sha1); i++)
            {
                ventoy_debug("%02x ", hash->sha1[i]);
            }
            ventoy_debug("\n");
        }
    }

end:

    grub_file_close(file);
    return 0;
}

int ventoy_get_override_info(grub_file_t file, wim_tail *wim_data)
{
    grub_uint32_t start_block;
    grub_uint64_t file_offset;
    grub_uint64_t override_offset;
    grub_uint32_t override_len;
    grub_uint64_t fe_entry_size_offset;
    
    if (grub_strcmp(file->fs->name, "iso9660") == 0)
    {
        g_iso_fs_type = wim_data->iso_type = 0;
        override_len = sizeof(ventoy_iso9660_override);
        override_offset = grub_iso9660_get_last_file_dirent_pos(file) + 2;

        grub_file_read(file, &start_block, 1); // just read for hook trigger
        file_offset = grub_iso9660_get_last_read_pos(file);

        debug("iso9660 wim size:%llu override_offset:%llu file_offset:%llu\n", 
            (ulonglong)file->size, (ulonglong)override_offset, (ulonglong)file_offset);
    }
    else
    {
        g_iso_fs_type = wim_data->iso_type = 1;    
        override_len = sizeof(ventoy_udf_override);
        override_offset = grub_udf_get_last_file_attr_offset(file, &start_block, &fe_entry_size_offset);
        
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

