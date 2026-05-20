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
grub_err_t ventoy_cmd_clear_img(grub_extcmd_context_t ctxt, int argc, char **args)
{
    img_info *next = NULL;
    img_info *cur = g_ventoy_img_list;

    (void)ctxt;
    (void)argc;
    (void)args;

    while (cur)
    {
        next = cur->next;
        grub_free(cur);
        cur = next;
    }

    g_ventoy_img_list = NULL;
    g_ventoy_img_count = 0;

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_img_name(grub_extcmd_context_t ctxt, int argc, char **args)
{
    long img_id = 0;
    img_info *cur = g_ventoy_img_list;

    (void)ctxt;

    if (argc != 2 || (!ventoy_is_decimal(args[0])))
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s {imageID} {var}", cmd_raw_name);
    }

    img_id = grub_strtol(args[0], NULL, 10);
    if (img_id >= g_ventoy_img_count)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "No such many images %ld %ld", img_id, g_ventoy_img_count);
    }

    debug("Find image %ld name \n", img_id);

    while (cur && img_id > 0)
    {
        img_id--;
        cur = cur->next;
    }

    if (!cur)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "No such many images");
    }

    debug("image name is %s\n", cur->name);

    grub_env_set(args[1], cur->name);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_ext_select_img_path(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int len = 0;
    char id[32] = {0};
    img_info *cur = g_ventoy_img_list;

    (void)ctxt;

    if (argc != 1)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s {var}", cmd_raw_name);
    }

    len = (int)grub_strlen(args[0]);

    while (cur)
    {
        if (len == cur->pathlen && 0 == grub_strcmp(args[0], cur->path))
        {
            break;
        }
        cur = cur->next;
    }

    if (!cur)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "No such image");
    }

    grub_snprintf(id, sizeof(id), "VID_%p", cur);
    grub_env_set("chosen", id);
    grub_env_export("chosen");

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

char g_fake_vlnk_src[512];
char g_fake_vlnk_dst[512];
grub_uint64_t g_fake_vlnk_size;
grub_err_t ventoy_cmd_set_fake_vlnk(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    g_fake_vlnk_size = (grub_uint64_t)grub_strtoull(args[2], NULL, 10);

    grub_strncpy(g_fake_vlnk_dst, args[0], sizeof(g_fake_vlnk_dst));
    grub_snprintf(g_fake_vlnk_src, sizeof(g_fake_vlnk_src), "%s/________VENTOYVLNK.vlnk.%s", g_iso_path, args[1]);

    grub_file_vtoy_vlnk(g_fake_vlnk_src, g_fake_vlnk_dst);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_reset_fake_vlnk(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    g_fake_vlnk_src[0] = 0;
    g_fake_vlnk_dst[0] = 0;
    g_fake_vlnk_size = 0;
    grub_file_vtoy_vlnk(NULL, NULL);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}


grub_err_t ventoy_cmd_chosen_img_path(grub_extcmd_context_t ctxt, int argc, char **args)
{
    char value[32];
    char *pos = NULL;
    char *last = NULL;
    const char *id = NULL;
    img_info *cur = NULL;

    (void)ctxt;

    if (argc < 1 || argc > 3)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s {var}", cmd_raw_name);
    }

    if (g_fake_vlnk_src[0] && g_fake_vlnk_dst[0])
    {
        pos = grub_strchr(g_fake_vlnk_src, '/');
        grub_env_set(args[0], pos);
        if (argc > 1)
        {
            grub_snprintf(value, sizeof(value), "%llu", (ulonglong)(g_fake_vlnk_size));
            grub_env_set(args[1], value);
        }

        if (argc > 2)
        {
            for (last = pos; *pos; pos++)
            {
                if (*pos == '/')
                {
                    last = pos;
                }
            }
            grub_env_set(args[2], last + 1);
        }

        goto end;
    }

    id = grub_env_get("chosen");

    pos = grub_strstr(id, "VID_");
    if (pos)
    {
        cur = (img_info *)(void *)grub_strtoul(pos + 4, NULL, 16);
    }
    else
    {
        cur = g_ventoy_img_list;
    }

    if (!cur)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "No such image");
    }

    grub_env_set(args[0], cur->path);

    if (argc > 1)
    {
        grub_snprintf(value, sizeof(value), "%llu", (ulonglong)(cur->size));
        grub_env_set(args[1], value);
    }

    if (argc > 2)
    {
        grub_snprintf(value, sizeof(value), "%llu", (ulonglong)(cur->size));
        grub_env_set(args[2], cur->name);
    }

end:
    g_svd_replace_offset = 0;

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}


grub_err_t ventoy_cmd_list_img(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int len;
    grub_fs_t fs;
    grub_device_t dev = NULL;
    img_info *cur = NULL;
    img_info *tail = NULL;
    img_info *min = NULL;
    img_info *head = NULL;
    const char *strdata = NULL;
    char *device_name = NULL;
    char buf[32];
    img_iterator_node *node = NULL;
    img_iterator_node *tmp = NULL;

    (void)ctxt;

    if (argc != 2)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s {device} {cntvar}", cmd_raw_name);
    }

    if (g_ventoy_img_list || g_ventoy_img_count)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Must clear image before list");
    }

    VTOY_CMD_CHECK(1);

    g_enumerate_time_checked  = 0;
    g_enumerate_start_time_ms = grub_get_time_ms();

    strdata = ventoy_get_env("VTOY_FILT_DOT_UNDERSCORE_FILE");
    if (strdata && strdata[0] == '1' && strdata[1] == 0)
    {
        g_filt_dot_underscore_file = 1;
    }

    strdata = ventoy_get_env("VTOY_FILT_TRASH_DIR");
    if (strdata && strdata[0] == '0' && strdata[1] == 0)
    {
        g_filt_trash_dir = 0;
    }

    strdata = ventoy_get_env("VTOY_SORT_CASE_SENSITIVE");
    if (strdata && strdata[0] == '1' && strdata[1] == 0)
    {
        g_sort_case_sensitive = 1;
    }

    device_name = grub_file_get_device_name(args[0]);
    if (!device_name)
    {
        goto fail;
    }

    g_enum_dev = dev = grub_device_open(device_name);
    if (!dev)
    {
        goto fail;
    }

    g_enum_fs = fs = grub_fs_probe(dev);
    if (!fs)
    {
        goto fail;
    }

    if (ventoy_get_fs_type(fs->name) >= ventoy_fs_max)
    {
        debug("unsupported fs:<%s>\n", fs->name);
        ventoy_set_env("VTOY_NO_ISO_TIP", "unsupported file system");
        goto fail;
    }

    ventoy_set_env("vtoy_iso_fs", fs->name);

    strdata = ventoy_get_env("VTOY_DEFAULT_MENU_MODE");
    if (strdata && strdata[0] == '1')
    {
        g_default_menu_mode = 1;
    }

    grub_memset(&g_img_iterator_head, 0, sizeof(g_img_iterator_head));

    grub_snprintf(g_iso_path, sizeof(g_iso_path), "%s", args[0]);

    strdata = ventoy_get_env("VTOY_DEFAULT_SEARCH_ROOT");
    if (strdata && strdata[0] == '/')
    {
        len = grub_snprintf(g_img_iterator_head.dir, sizeof(g_img_iterator_head.dir) - 1, "%s", strdata);
        if (g_img_iterator_head.dir[len - 1] != '/')
        {
            g_img_iterator_head.dir[len++] = '/';
        }
        g_img_iterator_head.dirlen = len;
    }
    else
    {
        g_img_iterator_head.dirlen = 1;
        grub_strcpy(g_img_iterator_head.dir, "/");
    }

    g_img_iterator_head.tail = &tail;

    if (g_img_max_search_level < 0)
    {
        g_img_max_search_level = GRUB_INT_MAX;
        strdata = ventoy_get_env("VTOY_MAX_SEARCH_LEVEL");
        if (strdata && ventoy_is_decimal(strdata))
        {
            g_img_max_search_level = (int)grub_strtoul(strdata, NULL, 10);
        }
    }

    g_vtoy_file_flt[VTOY_FILE_FLT_ISO]  = ventoy_control_get_flag("VTOY_FILE_FLT_ISO");
    g_vtoy_file_flt[VTOY_FILE_FLT_WIM]  = ventoy_control_get_flag("VTOY_FILE_FLT_WIM");
    g_vtoy_file_flt[VTOY_FILE_FLT_EFI]  = ventoy_control_get_flag("VTOY_FILE_FLT_EFI");
    g_vtoy_file_flt[VTOY_FILE_FLT_IMG]  = ventoy_control_get_flag("VTOY_FILE_FLT_IMG");
    g_vtoy_file_flt[VTOY_FILE_FLT_VHD]  = ventoy_control_get_flag("VTOY_FILE_FLT_VHD");
    g_vtoy_file_flt[VTOY_FILE_FLT_VTOY] = ventoy_control_get_flag("VTOY_FILE_FLT_VTOY");

    for (node = &g_img_iterator_head; node; node = node->next)
    {
        fs->fs_dir(dev, node->dir, ventoy_collect_img_files, node);
    }

    strdata = ventoy_get_env("VTOY_TREE_VIEW_MENU_STYLE");
    if (strdata && strdata[0] == '1' && strdata[1] == 0)
    {
        g_tree_view_menu_style = 1;
    }

    ventoy_set_default_menu();

    for (node = &g_img_iterator_head; node; node = node->next)
    {
        ventoy_dynamic_tree_menu(node);
    }

    /* free node */
    node = g_img_iterator_head.next;
    while (node)
    {
        tmp = node->next;
        grub_free(node);
        node = tmp;
    }

    /* sort image list by image name */
    while (g_ventoy_img_list)
    {
        min = g_ventoy_img_list;
        for (cur = g_ventoy_img_list->next; cur; cur = cur->next)
        {
            if (ventoy_cmp_img(min, cur) > 0)
            {
                min = cur;
            }
        }

        if (min->prev)
        {
            min->prev->next = min->next;
        }

        if (min->next)
        {
            min->next->prev = min->prev;
        }

        if (min == g_ventoy_img_list)
        {
            g_ventoy_img_list = min->next;
        }

        if (head == NULL)
        {
            head = tail = min;
            min->prev = NULL;
            min->next = NULL;
        }
        else
        {
            tail->next = min;
            min->prev = tail;
            min->next = NULL;
            tail = min;
        }
    }

    g_ventoy_img_list = head;

    if (g_default_menu_mode == 1)
    {
        vtoy_ssprintf(g_list_script_buf, g_list_script_pos,
                      "menuentry \"%s [%s]\" --class=\"vtoyret\" VTOY_RET {\n  "
                      "  echo 'return ...' \n"
                      "}\n", "<--", ventoy_get_vmenu_title("VTLANG_RET_TO_TREEVIEW"));
    }

    for (cur = g_ventoy_img_list; cur; cur = cur->next)
    {
        vtoy_ssprintf(g_list_script_buf, g_list_script_pos,
                  "menuentry \"%s%s\" --class=\"%s\" --id=\"VID_%p\" {\n"
                  "  %s_%s \n"
                  "}\n",
                  cur->unsupport ? "[***********] " : "",
                  cur->alias ? cur->alias : cur->name, cur->class, cur,
                  cur->menu_prefix,
                  cur->unsupport ? "unsupport_menuentry" : "common_menuentry");
    }

    g_tree_script_buf[g_tree_script_pos] = 0;
    g_list_script_buf[g_list_script_pos] = 0;

    grub_snprintf(buf, sizeof(buf), "%d", g_ventoy_img_count);
    grub_env_set(args[1], buf);

fail:

    check_free(device_name, grub_free);
    check_free(dev, grub_device_close);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

