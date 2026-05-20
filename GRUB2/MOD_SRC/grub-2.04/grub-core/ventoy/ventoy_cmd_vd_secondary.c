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
grub_err_t ventoy_iso_vd_id_clear(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    g_iso_vd_id_publisher[0] = 0;
    g_iso_vd_id_prepare[0] = 0;
    g_iso_vd_id_application[0] = 0;

    return 0;
}

grub_err_t ventoy_cmd_iso_vd_id_parse(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int ret = 1;
    int offset = 318;
    grub_file_t file = NULL;

    (void)ctxt;
    (void)argc;

    file = grub_file_open(args[0], VENTOY_FILE_TYPE);
    if (!file)
    {
        grub_printf("Failed to open %s\n", args[0]);
        goto out;
    }

    grub_file_seek(file, 16 * 2048 + offset);
    grub_file_read(file, g_iso_vd_id_publisher, 128);

    offset += 128;
    grub_file_seek(file, 16 * 2048 + offset);
    grub_file_read(file, g_iso_vd_id_prepare, 128);

    offset += 128;
    grub_file_seek(file, 16 * 2048 + offset);
    grub_file_read(file, g_iso_vd_id_application, 128);

out:

    check_free(file, grub_file_close);
    grub_errno = GRUB_ERR_NONE;
    return ret;
}

grub_err_t ventoy_cmd_iso_vd_id_begin(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int ret = 1;
    char *id = g_iso_vd_id_publisher;

    (void)ctxt;
    (void)argc;

    if (args[0][0] == '1')
    {
        id = g_iso_vd_id_prepare;
    }
    else if (args[0][0] == '2')
    {
        id = g_iso_vd_id_application;
    }

    if (args[1][0] == '0' && grub_strncasecmp(id, args[2], grub_strlen(args[2])) == 0)
    {
        ret = 0;
    }

    if (args[1][0] == '1' && grub_strncmp(id, args[2], grub_strlen(args[2])) == 0)
    {
        ret = 0;
    }

    grub_errno = GRUB_ERR_NONE;
    return ret;
}

grub_err_t ventoy_cmd_fn_mutex_lock(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;

    g_ventoy_fn_mutex = 0;
    if (argc == 1 && args[0][0] == '1' && args[0][1] == 0)
    {
        g_ventoy_fn_mutex = 1;
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_dump_rsv_page(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_uint64_t total;
    grub_uint64_t org_required;
    grub_uint64_t new_required;

    (void)ctxt;
    (void)argc;
    (void)args;

#ifdef GRUB_MACHINE_EFI
    grub_efi_get_reserved_page_num(&total, &org_required, &new_required);
    grub_printf("Total pages: %llu\n", (unsigned long long)total);
    grub_printf("OrgReq pages: %llu\n", (unsigned long long)org_required);
    grub_printf("NewReq pages: %llu\n", (unsigned long long)new_required);
#else
    (void)total;
    (void)org_required;
    (void)new_required;
    grub_printf("Non EFI mode!\n");
#endif

    grub_refresh();

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_need_secondary_menu(grub_extcmd_context_t ctxt, int argc, char **args)
{
    const char *env = NULL;

    (void)ctxt;
    (void)argc;

    if (g_ventoy_memdisk_mode || g_ventoy_grub2_mode || g_ventoy_wimboot_mode || g_ventoy_iso_raw)
    {
        return 1;
    }

    if (ventoy_check_mode_by_name(args[0], "vtgrub2") ||
        ventoy_check_mode_by_name(args[0], "vtwimboot") ||
        ventoy_check_mode_by_name(args[0], "vtmemdisk") ||
        ventoy_check_mode_by_name(args[0], "vtnormal")
        )
    {
        return 1;
    }

    env = grub_env_get("VTOY_SECONDARY_BOOT_MENU");
    if (env && env[0] == '0' && env[1] == 0)
    {
        return 1;
    }

    return 0;
}

grub_err_t ventoy_cmd_show_secondary_menu(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int n = 0;
    int pos = 0;
    int len = 0;
    int select = 0;
    int timeout = 0;
    char *cmd = NULL;
    const char *env = NULL;
    ulonglong fsize = 0;
    char cfgfile[128];
    int seldata[16] = {0};

    (void)ctxt;
    (void)argc;

    len = 8 * VTOY_SIZE_1KB;
    cmd = (char *)grub_malloc(len);
    if (!cmd)
    {
        return 1;
    }

    g_vtoy_secondary_need_recover = 0;
    grub_env_unset("VTOY_SECOND_EXIT");
    grub_env_unset("VTOY_CHKSUM_FILE_PATH");

    env = grub_env_get("VTOY_SECONDARY_TIMEOUT");
    if (env)
    {
        timeout = (int)grub_strtol(env, NULL, 10);
    }

    if (timeout > 0)
    {
        vtoy_len_ssprintf(cmd, pos, len, "set timeout=%d\n", timeout);
    }

    fsize = grub_strtoull(args[2], NULL, 10);

    vtoy_dummy_menuentry(cmd, pos, len, "$VTLANG_NORMAL_MODE", "second_normal"); seldata[n++] = 1;

    if (grub_strcmp(args[1], "Unix") != 0)
    {
        if (grub_strcmp(args[1], "Windows") == 0)
        {
            vtoy_dummy_menuentry(cmd, pos, len, "$VTLANG_WIMBOOT_MODE", "second_wimboot"); seldata[n++] = 2;
        }
        else
        {
            vtoy_dummy_menuentry(cmd, pos, len, "$VTLANG_GRUB2_MODE", "second_grub2"); seldata[n++] = 3;
        }

        if (fsize <= VTOY_SIZE_1GB)
        {
            vtoy_dummy_menuentry(cmd, pos, len, "$VTLANG_MEMDISK_MODE", "second_memdisk"); seldata[n++] = 4;
        }
    }

    vtoy_dummy_menuentry(cmd, pos, len, "$VTLANG_FILE_CHKSUM", "second_checksum"); seldata[n++] = 5;
    vtoy_dummy_menuentry(cmd, pos, len, "$VTLANG_RETURN_PRV_NOESC", "second_return"); seldata[n++] = 6;

    do {
        grub_errno = GRUB_ERR_NONE;
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

        select = seldata[g_ventoy_last_entry];

        if (select == 2)
        {
            g_ventoy_wimboot_mode = 1;
            g_vtoy_secondary_need_recover = 1;
        }
        else if (select == 3)
        {
            g_ventoy_grub2_mode = 1;
            g_vtoy_secondary_need_recover = 2;
        }
        else if (select == 4)
        {
            g_ventoy_memdisk_mode = 1;
            g_vtoy_secondary_need_recover = 3;
        }
        else if (select == 5)
        {
            grub_env_set("VTOY_CHKSUM_FILE_PATH", args[0]);
            grub_script_execute_sourcecode("configfile $vtoy_efi_part/grub/checksum.cfg");
        }
        else if (select == 6)
        {
            grub_env_set("VTOY_SECOND_EXIT", "1");
        }
    }while (select == 5);

    grub_free(cmd);
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_secondary_recover_mode(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    if (g_vtoy_secondary_need_recover == 1)
    {
        g_ventoy_wimboot_mode = 0;
    }
    else if (g_vtoy_secondary_need_recover == 2)
    {
        g_ventoy_grub2_mode = 0;
    }
    else if (g_vtoy_secondary_need_recover == 3)
    {
        g_ventoy_memdisk_mode = 0;
    }

    g_vtoy_secondary_need_recover = 0;

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_fs_ignore_case(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;

    if (args[0][0] == '0')
    {
        g_ventoy_case_insensitive = 0;
    }
    else
    {
        g_ventoy_case_insensitive = 1;
    }

    return 0;
}

grub_err_t ventoy_cmd_init_menu_lang(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;

    ventoy_plugin_load_menu_lang(1, args[0]);
    VENTOY_CMD_RETURN(0);
}

grub_err_t ventoy_cmd_load_menu_lang(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;

    ventoy_plugin_load_menu_lang(0, args[0]);
    VENTOY_CMD_RETURN(0);
}

