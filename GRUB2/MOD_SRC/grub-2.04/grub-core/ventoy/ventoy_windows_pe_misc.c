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
int wim_name_cmp(const char *search, grub_uint16_t *name, grub_uint16_t namelen)
{
    char c1 = vtoy_to_upper(*search);
    char c2 = vtoy_to_upper(*name);

    while (namelen > 0 && (c1 == c2))
    {
        search++;
        name++;
        namelen--;
        
        c1 = vtoy_to_upper(*search);
        c2 = vtoy_to_upper(*name);
    }

    if (namelen == 0 && *search == 0)
    {
        return 0;
    }

    return 1;
}

int ventoy_is_pe64(grub_uint8_t *buffer)
{
    grub_uint32_t pe_off;

    if (buffer[0] != 'M' || buffer[1] != 'Z')
    {
        return 0;
    }
    
    pe_off = *(grub_uint32_t *)(buffer + 60);

    if (buffer[pe_off] != 'P' || buffer[pe_off + 1] != 'E')
    {
        return 0;
    }

    if (*(grub_uint16_t *)(buffer + pe_off + 24) == 0x020b)
    {
        return 1;
    }

    return 0;
}

grub_err_t ventoy_cmd_is_pe64(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int ret = 1;
    grub_file_t file;
    grub_uint8_t buf[512];
    
    (void)ctxt;
    (void)argc;

    file = grub_file_open(args[0], VENTOY_FILE_TYPE);
    if (!file)
    {
        return 1;
    }

    grub_memset(buf, 0, 512);
    grub_file_read(file, buf, 512);
    if (ventoy_is_pe64(buf))
    {
        debug("%s is PE64\n", args[0]);
        ret = 0;
    }
    else
    {
        debug("%s is PE32\n", args[0]);
    }
    grub_file_close(file);

    return ret;
}

grub_err_t ventoy_cmd_sel_wimboot(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int size;
    char *buf = NULL;
    char configfile[128];
    
    (void)ctxt;
    (void)argc;
    (void)args;

    debug("select wimboot argc:%d\n", argc);

    buf = (char *)grub_malloc(8192);
    if (!buf)
    {
        return 0;
    }

    size = (int)grub_snprintf(buf, 8192, 
        "menuentry \"Windows Setup (32-bit)\" {\n"
        "    set vtoy_wimboot_sel=32\n"
        "}\n"
        "menuentry \"Windows Setup (64-bit)\" {\n"
        "    set vtoy_wimboot_sel=64\n"
        "}\n"
        );
    buf[size] = 0;

    g_ventoy_menu_esc = 1;
    g_ventoy_suppress_esc = 1;
    g_ventoy_suppress_esc_default = 1;

    grub_snprintf(configfile, sizeof(configfile), "configfile mem:0x%llx:size:%d", (ulonglong)(ulong)buf, size);
    grub_script_execute_sourcecode(configfile);
    
    g_ventoy_menu_esc = 0;
    g_ventoy_suppress_esc = 0;

    grub_free(buf);

    if (g_ventoy_last_entry == 0)
    {
        debug("last entry=%d %s=32\n", g_ventoy_last_entry, args[0]);
        grub_env_set(args[0], "32");
    }
    else
    {
        debug("last entry=%d %s=64\n", g_ventoy_last_entry, args[0]);
        grub_env_set(args[0], "64");
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

