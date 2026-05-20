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
int ventoy_dynamic_tree_menu(img_iterator_node *node)
{
    int offset = 1;
    img_info *img = NULL;
    const char *dir_class = NULL;
    const char *dir_alias = NULL;
    img_iterator_node *child = NULL;
    const menu_tip *tip = NULL;

    if (node->isocnt == 0 || node->done == 1)
    {
        return 0;
    }

    if (node->parent && node->parent->dirlen < node->dirlen)
    {
        offset = node->parent->dirlen;
    }

    if (node == &g_img_iterator_head)
    {
        if (g_default_menu_mode == 0)
        {
            if (g_tree_view_menu_style == 0)
            {
                vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos,
                              "menuentry \"%-10s [%s]\" --class=\"vtoyret\" VTOY_RET {\n  "
                              "  echo 'return ...' \n"
                              "}\n", "<--", ventoy_get_vmenu_title("VTLANG_RET_TO_LISTVIEW"));
            }
            else
            {
                vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos,
                              "menuentry \"[%s]\" --class=\"vtoyret\" VTOY_RET {\n  "
                              "  echo 'return ...' \n"
                              "}\n", ventoy_get_vmenu_title("VTLANG_RET_TO_LISTVIEW"));
            }
        }

        g_tree_script_pre = g_tree_script_pos;
    }
    else
    {
        node->dir[node->dirlen - 1] = 0;
        dir_class = ventoy_plugin_get_menu_class(vtoy_class_directory, node->dir, node->dir);
        if (!dir_class)
        {
            dir_class = "vtoydir";
        }

        tip = ventoy_plugin_get_menu_tip(vtoy_tip_directory, node->dir);

        dir_alias = ventoy_plugin_get_menu_alias(vtoy_alias_directory, node->dir);
        if (dir_alias)
        {
            if (g_tree_view_menu_style == 0)
            {
                vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos,
                              "submenu \"%-10s %s\" --class=\"%s\" --id=\"DIR_%s\" _VTIP_%p {\n",
                              "DIR", dir_alias, dir_class, node->dir + offset, tip);
            }
            else
            {
                vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos,
                              "submenu \"%s\" --class=\"%s\" --id=\"DIR_%s\" _VTIP_%p {\n",
                              dir_alias, dir_class, node->dir + offset, tip);
            }
        }
        else
        {
            dir_alias = node->dir + offset;

            if (g_tree_view_menu_style == 0)
            {
                vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos,
                              "submenu \"%-10s [%s]\" --class=\"%s\" --id=\"DIR_%s\" _VTIP_%p {\n",
                              "DIR", dir_alias, dir_class, node->dir + offset, tip);
            }
            else
            {
                vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos,
                              "submenu \"[%s]\" --class=\"%s\" --id=\"DIR_%s\" _VTIP_%p {\n",
                              dir_alias, dir_class, node->dir + offset, tip);
            }
        }

        if (g_tree_view_menu_style == 0)
        {
            vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos,
                          "menuentry \"%-10s [%s/..]\" --class=\"vtoyret\" VTOY_RET {\n  "
                          "  echo 'return ...' \n"
                          "}\n", "<--", node->dir);
        }
        else
        {
            vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos,
                          "menuentry \"[%s/..]\" --class=\"vtoyret\" VTOY_RET {\n  "
                          "  echo 'return ...' \n"
                          "}\n", node->dir);
        }
    }

    while ((child = ventoy_get_min_child(node)) != NULL)
    {
        ventoy_dynamic_tree_menu(child);
    }

    while ((img = ventoy_get_min_iso(node)) != NULL)
    {
        if (g_tree_view_menu_style == 0)
        {
            vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos,
                          "menuentry \"%-10s %s%s\" --class=\"%s\" --id=\"VID_%p\" {\n"
                          "  %s_%s \n"
                          "}\n",
                          grub_get_human_size(img->size, GRUB_HUMAN_SIZE_SHORT),
                          img->unsupport ? "[***********] " : "",
                          img->alias ? img->alias : img->name, img->class, img,
                          img->menu_prefix,
                          img->unsupport ? "unsupport_menuentry" : "common_menuentry");
        }
        else
        {
            vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos,
                          "menuentry \"%s%s\" --class=\"%s\" --id=\"VID_%p\" {\n"
                          "  %s_%s \n"
                          "}\n",
                          img->unsupport ? "[***********] " : "",
                          img->alias ? img->alias : img->name, img->class, img,
                          img->menu_prefix,
                          img->unsupport ? "unsupport_menuentry" : "common_menuentry");
        }
    }

    if (node != &g_img_iterator_head)
    {
        vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos, "}\n");
    }

    node->done = 1;
    return 0;
}

int ventoy_set_default_menu(void)
{
    int img_len = 0;
    char *pos = NULL;
    char *end = NULL;
    char *def = NULL;
    const char *strdata = NULL;
    img_info *cur = NULL;
    img_info *default_node = NULL;
    const char *default_image = NULL;

    default_image = ventoy_get_env("VTOY_DEFAULT_IMAGE");
    if (default_image && default_image[0] == '/')
    {
        img_len = grub_strlen(default_image);

        for (cur = g_ventoy_img_list; cur; cur = cur->next)
        {
            if (img_len == cur->pathlen && grub_strcmp(default_image, cur->path) == 0)
            {
                default_node = cur;
                break;
            }
        }

        if (!default_node)
        {
            return 1;
        }

        if (0 == g_default_menu_mode)
        {
            vtoy_ssprintf(g_list_script_buf, g_list_script_pos, "set default='VID_%p'\n", default_node);
        }
        else
        {
            def = grub_strdup(default_image);
            if (!def)
            {
                return 1;
            }

            vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos, "set default=%c", '\'');

            strdata = ventoy_get_env("VTOY_DEFAULT_SEARCH_ROOT");
            if (strdata && strdata[0] == '/')
            {
                pos = def + grub_strlen(strdata);
                if (*pos == '/')
                {
                    pos++;
                }
            }
            else
            {
                pos = def + 1;
            }

            while ((end = grub_strchr(pos, '/')) != NULL)
            {
                *end = 0;
                vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos, "DIR_%s>", pos);
                pos = end + 1;
            }

            vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos, "VID_%p'\n", default_node);
            grub_free(def);
        }
    }

    return 0;
}

