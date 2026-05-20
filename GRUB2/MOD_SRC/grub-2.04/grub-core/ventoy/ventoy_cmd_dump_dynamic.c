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
grub_err_t ventoy_cmd_get_replace_file_cnt(grub_extcmd_context_t ctxt, int argc, char **args)
{
    char buf[32];
    ventoy_grub_param_file_replace *replace = &(g_grub_param->file_replace);

    (void)ctxt;

    if (argc >= 1)
    {
        grub_snprintf(buf, sizeof(buf), "%u", replace->old_name_cnt);
        grub_env_set(args[0], buf);
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_dump_menu(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc == 0)
    {
        grub_printf("List Mode: CurLen:%d  MaxLen:%u\n", g_list_script_pos, VTOY_MAX_SCRIPT_BUF);
        grub_printf("%s", g_list_script_buf);
    }
    else
    {
        grub_printf("Tree Mode: CurLen:%d  MaxLen:%u\n", g_tree_script_pos, VTOY_MAX_SCRIPT_BUF);
        grub_printf("%s", g_tree_script_buf);
    }

    return 0;
}

grub_err_t ventoy_cmd_dump_img_list(grub_extcmd_context_t ctxt, int argc, char **args)
{
    img_info *cur = g_ventoy_img_list;

    (void)ctxt;
    (void)argc;
    (void)args;

    while (cur)
    {
        grub_printf("path:<%s> id=%d list_index=%d\n", cur->path, cur->id, cur->plugin_list_index);
        grub_printf("name:<%s>\n\n", cur->name);
        cur = cur->next;
    }

    return 0;
}

grub_err_t ventoy_cmd_dump_injection(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    ventoy_plugin_dump_injection();

    return 0;
}

grub_err_t ventoy_cmd_dump_auto_install(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    ventoy_plugin_dump_auto_install();

    return 0;
}

grub_err_t ventoy_cmd_dump_persistence(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    ventoy_plugin_dump_persistence();

    return 0;
}

int ventoy_check_mode_by_name(char *filename, const char *suffix)
{
    int i;
    int len1;
    int len2;

    len1 = (int)grub_strlen(filename);
    len2 = (int)grub_strlen(suffix);

    if (len1 <= len2)
    {
        return 0;
    }

    for (i = len1 - 1; i >= 0; i--)
    {
        if (filename[i] == '.')
        {
            break;
        }
    }

    if (i < len2 + 1)
    {
        return 0;
    }

    if (filename[i - len2 - 1] != '_')
    {
        return 0;
    }

    if (grub_strncasecmp(filename + (i - len2), suffix, len2) == 0)
    {
        return 1;
    }

    return 0;
}

grub_err_t ventoy_cmd_check_mode(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc != 1 && argc != 2)
    {
        return 1;
    }

    if (args[0][0] == '0')
    {
        if (g_ventoy_memdisk_mode)
        {
            return 0;
        }

        if (argc == 2 && ventoy_check_mode_by_name(args[1], "vtmemdisk"))
        {
            return 0;
        }

        return 1;
    }
    else if (args[0][0] == '1')
    {
        return g_ventoy_iso_raw ? 0 : 1;
    }
    else if (args[0][0] == '2')
    {
        return g_ventoy_iso_uefi_drv ? 0 : 1;
    }
    else if (args[0][0] == '3')
    {
        if (g_ventoy_grub2_mode)
        {
            return 0;
        }

        if (argc == 2 && ventoy_check_mode_by_name(args[1], "vtgrub2"))
        {
            return 0;
        }

        return 1;
    }
    else if (args[0][0] == '4')
    {
        if (g_ventoy_wimboot_mode)
        {
            return 0;
        }

        if (argc == 2 && ventoy_check_mode_by_name(args[1], "vtwimboot"))
        {
            return 0;
        }

        return 1;
    }

    return 1;
}

grub_err_t ventoy_cmd_dynamic_menu(grub_extcmd_context_t ctxt, int argc, char **args)
{
    static int configfile_mode = 0;
    char memfile[128] = {0};

    (void)ctxt;
    (void)argc;
    (void)args;

    /*
     * args[0]:  0:normal     1:configfile
     * args[1]:  0:list_buf   1:tree_buf
     */

    if (argc != 2)
    {
        debug("Invalid argc %d\n", argc);
        return 0;
    }

    VTOY_CMD_CHECK(1);

    if (args[0][0] == '0')
    {
        if (args[1][0] == '0')
        {
            grub_script_execute_sourcecode(g_list_script_buf);
        }
        else
        {
            grub_script_execute_sourcecode(g_tree_script_buf);
        }
    }
    else
    {
        if (configfile_mode)
        {
            debug("Now already in F3 mode %d\n", configfile_mode);
            return 0;
        }

        if (args[1][0] == '0')
        {
            grub_snprintf(memfile, sizeof(memfile), "configfile mem:0x%llx:size:%d",
                (ulonglong)(ulong)g_list_script_buf, g_list_script_pos);
        }
        else
        {
             g_ventoy_last_entry = -1;
            grub_snprintf(memfile, sizeof(memfile), "configfile mem:0x%llx:size:%d",
                (ulonglong)(ulong)g_tree_script_buf, g_tree_script_pos);
        }

        configfile_mode = 1;
        grub_script_execute_sourcecode(memfile);
        configfile_mode = 0;
    }

    return 0;
}
