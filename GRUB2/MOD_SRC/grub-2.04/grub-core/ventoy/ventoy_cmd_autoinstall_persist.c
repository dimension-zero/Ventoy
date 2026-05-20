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
grub_err_t ventoy_cmd_sel_auto_install(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i = 0;
    int pos = 0;
    int defidx = 1;
    char *buf = NULL;
    grub_file_t file = NULL;
    char configfile[128];
    install_template *node = NULL;

    (void)ctxt;
    (void)argc;
    (void)args;

    debug("select auto installation argc:%d\n", argc);

    if (argc < 1)
    {
        return 0;
    }

    node = ventoy_plugin_find_install_template(args[0]);
    if (!node)
    {
        debug("Auto install template not found for %s\n", args[0]);
        return 0;
    }

    if (node->autosel >= 0 && node->autosel <= node->templatenum)
    {
        defidx = node->autosel;
        if (node->timeout < 0)
        {
            node->cursel = node->autosel - 1;
            debug("Auto install template auto select %d\n", node->autosel);
            goto load;
        }
    }

    buf = (char *)grub_malloc(VTOY_MAX_SCRIPT_BUF);
    if (!buf)
    {
        return 0;
    }

    if (node->timeout > 0)
    {
        vtoy_ssprintf(buf, pos, "set timeout=%d\n", node->timeout);
    }

    vtoy_ssprintf(buf, pos, "menuentry \"$VTLANG_NO_AUTOINS_SCRIPT\" --class=\"sel_auto_install\" {\n"
                  "  echo %s\n}\n", "");

    for (i = 0; i < node->templatenum; i++)
    {
        vtoy_ssprintf(buf, pos, "menuentry \"%s %s\" --class=\"sel_auto_install\" {\n"
                  "  echo \"\"\n}\n",
                  ventoy_get_vmenu_title("VTLANG_AUTOINS_USE"),
                  node->templatepath[i].path);
    }

    g_ventoy_menu_esc = 1;
    g_ventoy_suppress_esc = 1;
    g_ventoy_suppress_esc_default = defidx;
    g_ventoy_secondary_menu_on = 1;

    grub_snprintf(configfile, sizeof(configfile), "configfile mem:0x%llx:size:%d", (ulonglong)(ulong)buf, pos);
    grub_script_execute_sourcecode(configfile);

    g_ventoy_menu_esc = 0;
    g_ventoy_suppress_esc = 0;
    g_ventoy_suppress_esc_default = 1;
    g_ventoy_secondary_menu_on = 0;

    grub_free(buf);

    node->cursel = g_ventoy_last_entry - 1;

load:
    grub_check_free(node->filebuf);
    node->filelen = 0;

    if (node->cursel >= 0 && node->cursel < node->templatenum)
    {
        file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s", ventoy_get_env("vtoy_iso_part"),
            node->templatepath[node->cursel].path);
        if (file)
        {
            node->filebuf = grub_malloc(file->size + 8);
            if (node->filebuf)
            {
                grub_file_read(file, node->filebuf, file->size);
                grub_file_close(file);

                grub_memset(node->filebuf + file->size, 0, 8);
                node->filelen = (int)file->size;

                ventoy_auto_install_var_expand(node);
            }
        }
        else
        {
            debug("Failed to open auto install script <%s%s>\n",
                ventoy_get_env("vtoy_iso_part"), node->templatepath[node->cursel].path);
        }
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_sel_persistence(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i = 0;
    int pos = 0;
    int defidx = 1;
    char *buf = NULL;
    char configfile[128];
    persistence_config *node;

    (void)ctxt;
    (void)argc;
    (void)args;

    debug("select persistence argc:%d\n", argc);

    if (argc < 1)
    {
        return 0;
    }

    node = ventoy_plugin_find_persistent(args[0]);
    if (!node)
    {
        debug("Persistence image not found for %s\n", args[0]);
        return 0;
    }

    if (node->autosel >= 0 && node->autosel <= node->backendnum)
    {
        defidx = node->autosel;
        if (node->timeout < 0)
        {
            node->cursel = node->autosel - 1;
            debug("Persistence image auto select %d\n", node->autosel);
            return 0;
        }
    }

    buf = (char *)grub_malloc(VTOY_MAX_SCRIPT_BUF);
    if (!buf)
    {
        return 0;
    }

    if (node->timeout > 0)
    {
        vtoy_ssprintf(buf, pos, "set timeout=%d\n", node->timeout);
    }

    vtoy_ssprintf(buf, pos, "menuentry \"$VTLANG_NO_PERSIST\" --class=\"sel_persistence\" {\n"
                  "  echo %s\n}\n", "");

    for (i = 0; i < node->backendnum; i++)
    {
        vtoy_ssprintf(buf, pos, "menuentry \"%s %s\" --class=\"sel_persistence\" {\n"
                      "  echo \"\"\n}\n",
                      ventoy_get_vmenu_title("VTLANG_PERSIST_USE"),
                      node->backendpath[i].path);

    }

    g_ventoy_menu_esc = 1;
    g_ventoy_suppress_esc = 1;
    g_ventoy_suppress_esc_default = defidx;
    g_ventoy_secondary_menu_on = 1;

    grub_snprintf(configfile, sizeof(configfile), "configfile mem:0x%llx:size:%d", (ulonglong)(ulong)buf, pos);
    grub_script_execute_sourcecode(configfile);

    g_ventoy_menu_esc = 0;
    g_ventoy_suppress_esc = 0;
    g_ventoy_suppress_esc_default = 1;
    g_ventoy_secondary_menu_on = 0;

    grub_free(buf);

    node->cursel = g_ventoy_last_entry - 1;

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_dump_img_sector(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_uint32_t i;
    ventoy_img_chunk *cur;

    (void)ctxt;
    (void)argc;
    (void)args;

    for (i = 0; i < g_img_chunk_list.cur_chunk; i++)
    {
        cur = g_img_chunk_list.chunk + i;
        grub_printf("image:[%u - %u]   <==>  disk:[%llu - %llu]\n",
            cur->img_start_sector, cur->img_end_sector,
            (unsigned long long)cur->disk_start_sector, (unsigned long long)cur->disk_end_sector
            );
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_test_block_list(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_uint32_t i;
    grub_file_t file;
    ventoy_img_chunk_list chunklist;
    char errmsg[128];

    (void)ctxt;
    (void)argc;

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (!file)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Can't open file %s\n", args[0]);
    }

    /* get image chunk data */
    grub_memset(&chunklist, 0, sizeof(chunklist));
    chunklist.chunk = grub_malloc(sizeof(ventoy_img_chunk) * DEFAULT_CHUNK_NUM);
    if (NULL == chunklist.chunk)
    {
        return grub_error(GRUB_ERR_OUT_OF_MEMORY, "Can't allocate image chunk memoty\n");
    }

    chunklist.max_chunk = DEFAULT_CHUNK_NUM;
    chunklist.cur_chunk = 0;

    ventoy_get_block_list(file, &chunklist, 0);

    if (0 != ventoy_check_block_list(file, &chunklist, 0, errmsg, sizeof(errmsg)))
    {
        grub_printf("%s\n", errmsg);
        grub_printf("########## UNSUPPORTED ###############\n");
    }

    grub_printf("filesystem: <%s> entry number:<%u>\n", file->fs->name, chunklist.cur_chunk);

    for (i = 0; i < chunklist.cur_chunk; i++)
    {
        grub_printf("%llu+%llu,", (ulonglong)chunklist.chunk[i].disk_start_sector,
            (ulonglong)(chunklist.chunk[i].disk_end_sector + 1 - chunklist.chunk[i].disk_start_sector));
    }

    grub_printf("\n==================================\n");

    for (i = 0; i < chunklist.cur_chunk; i++)
    {
        grub_printf("%2u: [%llu %llu] - [%llu %llu]\n", i,
            (ulonglong)chunklist.chunk[i].img_start_sector,
            (ulonglong)chunklist.chunk[i].img_end_sector,
            (ulonglong)chunklist.chunk[i].disk_start_sector,
            (ulonglong)chunklist.chunk[i].disk_end_sector
            );
    }

    grub_free(chunklist.chunk);
    grub_file_close(file);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_add_replace_file(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    ventoy_grub_param_file_replace *replace = NULL;

    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc >= 2)
    {
        replace = &(g_grub_param->file_replace);
        replace->magic = GRUB_FILE_REPLACE_MAGIC;

        replace->old_name_cnt = 0;
        for (i = 0; i < 4 && i + 1 < argc; i++)
        {
            replace->old_name_cnt++;
            grub_snprintf(replace->old_file_name[i], sizeof(replace->old_file_name[i]), "%s", args[i + 1]);
        }

        replace->new_file_virtual_id = (grub_uint32_t)grub_strtoul(args[0], NULL, 10);
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

