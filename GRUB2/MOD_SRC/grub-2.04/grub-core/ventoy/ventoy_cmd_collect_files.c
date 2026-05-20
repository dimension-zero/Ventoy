/******************************************************************************
 * ventoy_cmd.c
 *
 * Copyright (c) 2021, longpanda <admin@ventoy.net>
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
#include <grub/misc.h>
#include <grub/kernel.h>
#ifdef GRUB_MACHINE_EFI
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#endif
#include <grub/time.h>
#include <grub/video.h>
#include <grub/acpi.h>
#include <grub/charset.h>
#include <grub/crypto.h>
#include <grub/lib/crc.h>
#include <grub/random.h>
#include <grub/ventoy.h>
#include "ventoy_def.h"
#include "miniz.h"
#include "ventoy_cmd_priv.h"
int ventoy_collect_img_files(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    //int i = 0;
    int type = 0;
    int ignore = 0;
    int index = 0;
    int vlnk = 0;
    grub_size_t len;
    img_info *img;
    img_info *tail;
    const menu_tip *tip;
    img_iterator_node *tmp;
    img_iterator_node *new_node;
    img_iterator_node *node = (img_iterator_node *)data;

    if (g_enumerate_time_checked == 0)
    {
        g_enumerate_finish_time_ms = grub_get_time_ms();
        if ((g_enumerate_finish_time_ms - g_enumerate_start_time_ms) >= 3000)
        {
            grub_cls();
            grub_printf("\n\n Ventoy scanning files, please wait...\n");
            grub_refresh();
            g_enumerate_time_checked = 1;
        }
    }

    len = grub_strlen(filename);

    if (info->dir)
    {
        if (node->level + 1 > g_img_max_search_level)
        {
            return 0;
        }

        if ((len == 1 && filename[0] == '.') ||
            (len == 2 && filename[0] == '.' && filename[1] == '.'))
        {
            return 0;
        }

        if (!ventoy_img_name_valid(filename, len))
        {
            return 0;
        }

        if (g_filt_trash_dir)
        {
            if (0 == grub_strncmp(filename, ".trash-", 7) ||
                0 == grub_strcmp(filename, ".Trashes") ||
                0 == grub_strncmp(filename, "$RECYCLE.BIN", 12))
            {
                return 0;
            }
        }

        if (g_plugin_image_list == VENTOY_IMG_WHITE_LIST)
        {
            grub_snprintf(g_img_swap_tmp_buf, sizeof(g_img_swap_tmp_buf), "%s%s/", node->dir, filename);
            index = ventoy_plugin_get_image_list_index(vtoy_class_directory, g_img_swap_tmp_buf);
            if (index == 0)
            {
                debug("Directory %s not found in image_list plugin config...\n", g_img_swap_tmp_buf);
                return 0;
            }
        }

        new_node = grub_zalloc(sizeof(img_iterator_node));
        if (new_node)
        {
            new_node->level = node->level + 1;
            new_node->plugin_list_index = index;
            new_node->dirlen = grub_snprintf(new_node->dir, sizeof(new_node->dir), "%s%s/", node->dir, filename);

            g_enum_fs->fs_dir(g_enum_dev, new_node->dir, ventoy_check_ignore_flag, &ignore);
            if (ignore)
            {
                debug("Directory %s ignored...\n", new_node->dir);
                grub_free(new_node);
                return 0;
            }

            new_node->tail = node->tail;

            new_node->parent = node;
            if (!node->firstchild)
            {
                node->firstchild = new_node;
            }

            if (g_img_iterator_tail)
            {
                g_img_iterator_tail->next = new_node;
                g_img_iterator_tail = new_node;
            }
            else
            {
                g_img_iterator_head.next = new_node;
                g_img_iterator_tail = new_node;
            }
        }
    }
    else
    {
        debug("Find a file %s\n", filename);
        if (len < 4)
        {
            return 0;
        }

        if (FILE_FLT(ISO) && 0 == grub_strcasecmp(filename + len - 4, ".iso"))
        {
            type = img_type_iso;
        }
        else if (FILE_FLT(WIM) && g_wimboot_enable && (0 == grub_strcasecmp(filename + len - 4, ".wim")))
        {
            type = img_type_wim;
        }
        else if (FILE_FLT(VHD) && g_vhdboot_enable && (0 == grub_strcasecmp(filename + len - 4, ".vhd") ||
                (len >= 5 && 0 == grub_strcasecmp(filename + len - 5, ".vhdx"))))
        {
            type = img_type_vhd;
        }
        #ifdef GRUB_MACHINE_EFI
        else if (FILE_FLT(EFI) && 0 == grub_strcasecmp(filename + len - 4, ".efi"))
        {
            type = img_type_efi;
        }
        #endif
        else if (FILE_FLT(IMG) && 0 == grub_strcasecmp(filename + len - 4, ".img"))
        {
            if (len == 18 && grub_strncmp(filename, "ventoy_", 7) == 0)
            {
                if (grub_strncmp(filename + 7, "wimboot", 7) == 0 ||
                    grub_strncmp(filename + 7, "vhdboot", 7) == 0)
                {
                    return 0;
                }
            }
            type = img_type_img;
        }
        else if (FILE_FLT(VTOY) && len >= 5 && 0 == grub_strcasecmp(filename + len - 5, ".vtoy"))
        {
            type = img_type_vtoy;
        }
        else if (len >= 9 && 0 == grub_strcasecmp(filename + len - 5, ".vcfg"))
        {
            if (filename[len - 9] == '.' || (len >= 10 && filename[len - 10] == '.'))
            {
                grub_snprintf(g_img_swap_tmp_buf, sizeof(g_img_swap_tmp_buf), "%s%s", node->dir, filename);
                ventoy_plugin_add_custom_boot(g_img_swap_tmp_buf);
            }
            return 0;
        }
        else
        {
            return 0;
        }

        if (g_filt_dot_underscore_file && filename[0] == '.' && filename[1] == '_')
        {
            return 0;
        }

        if (g_plugin_image_list)
        {
            grub_snprintf(g_img_swap_tmp_buf, sizeof(g_img_swap_tmp_buf), "%s%s", node->dir, filename);
            index = ventoy_plugin_get_image_list_index(vtoy_class_image_file, g_img_swap_tmp_buf);
            if (VENTOY_IMG_WHITE_LIST == g_plugin_image_list && index == 0)
            {
                debug("File %s not found in image_list plugin config...\n", g_img_swap_tmp_buf);
                return 0;
            }
            else if (VENTOY_IMG_BLACK_LIST == g_plugin_image_list && index > 0)
            {
                debug("File %s found in image_blacklist plugin config %d ...\n", g_img_swap_tmp_buf, index);
                return 0;
            }
        }

        if (info->size == VTOY_FILT_MIN_FILE_SIZE || info->size == 0)
        {
            if (grub_file_is_vlnk_suffix(filename, len))
            {
                vlnk = 1;
                if (ventoy_add_vlnk_file(node->dir, filename) != 0)
                {
                    return 0;
                }
            }
        }

        img = grub_zalloc(sizeof(img_info));
        if (img)
        {
            img->type = type;
            img->plugin_list_index = index;
            grub_snprintf(img->name, sizeof(img->name), "%s", filename);

            img->pathlen = grub_snprintf(img->path, sizeof(img->path), "%s%s", node->dir, img->name);

            img->size = info->size;
            if (vlnk || 0 == img->size)
            {
                if (node->dir[0] == '/')
                {
                    img->size = ventoy_grub_get_file_size("%s%s%s", g_iso_path, node->dir, filename);
                }
                else
                {
                    img->size = ventoy_grub_get_file_size("%s/%s%s", g_iso_path, node->dir, filename);
                }
            }

            if (img->size < VTOY_FILT_MIN_FILE_SIZE)
            {
                debug("img <%s> size too small %llu\n", img->name, (ulonglong)img->size);
                grub_free(img);
                return 0;
            }

            if (g_ventoy_img_list)
            {
                tail = *(node->tail);
                img->prev = tail;
                tail->next = img;
            }
            else
            {
                g_ventoy_img_list = img;
            }

            img->id = g_ventoy_img_count;
            img->parent = node;
            if (node && NULL == node->firstiso)
            {
                node->firstiso = img;
            }

            node->isocnt++;
            tmp = node->parent;
            while (tmp)
            {
                tmp->isocnt++;
                tmp = tmp->parent;
            }

            *((img_info **)(node->tail)) = img;
            g_ventoy_img_count++;

            img->alias = ventoy_plugin_get_menu_alias(vtoy_alias_image_file, img->path);

            tip = ventoy_plugin_get_menu_tip(vtoy_tip_image_file, img->path);
            if (tip)
            {
                img->tip1 = tip->tip1;
                img->tip2 = tip->tip2;
            }

            img->class = ventoy_plugin_get_menu_class(vtoy_class_image_file, img->name, img->path);
            if (!img->class)
            {
                img->class = g_menu_class[type];
            }
            img->menu_prefix = g_menu_prefix[type];

            if (img_type_iso == type)
            {
                if (ventoy_plugin_check_memdisk(img->path))
                {
                    img->menu_prefix = "miso";
                }
            }
            else if (img_type_img == type)
            {
                if (ventoy_plugin_check_memdisk(img->path))
                {
                    img->menu_prefix = "mimg";
                }
            }

            debug("Add %s%s to list %d\n", node->dir, filename, g_ventoy_img_count);
        }
    }

    return 0;
}

