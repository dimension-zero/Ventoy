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
grub_err_t ventoy_cmd_set_wim_prompt(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    g_vtoy_load_prompt = 0;
    grub_memset(g_vtoy_prompt_msg, 0, sizeof(g_vtoy_prompt_msg));

    if (argc == 2 && args[0][0] == '1')
    {
        g_vtoy_load_prompt = 1;
        grub_snprintf(g_vtoy_prompt_msg, sizeof(g_vtoy_prompt_msg), "%s", args[1]);
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

int ventoy_need_prompt_load_file(void)
{
    return g_vtoy_load_prompt;
}

grub_ssize_t ventoy_load_file_with_prompt(grub_file_t file, void *buf, grub_ssize_t size)
{
    grub_uint64_t ro = 0;
    grub_uint64_t div = 0;
    grub_ssize_t left = size;
    char *cur = (char *)buf;

    grub_printf("\r%s   1%%    ", g_vtoy_prompt_msg);
    grub_refresh();

    while (left >= VTOY_SIZE_2MB)
    {
        grub_file_read(file, cur, VTOY_SIZE_2MB);
        cur += VTOY_SIZE_2MB;
        left -= VTOY_SIZE_2MB;

        div = grub_divmod64((grub_uint64_t)((size - left) * 100), (grub_uint64_t)size, &ro);
        if (div < 1)
        {
            div = 1;
        }
        grub_printf("\r%s   %d%%    ", g_vtoy_prompt_msg, (int)div);
        grub_refresh();
    }

    if (left > 0)
    {
        grub_file_read(file, cur, left);
    }

    grub_printf("\r%s   100%%     \n", g_vtoy_prompt_msg);
    grub_refresh();

    return size;
}

grub_err_t ventoy_cmd_load_file_to_mem(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int rc = 1;
    char *buf = NULL;
    grub_file_t file;
    enum grub_file_type type;

    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc != 3)
    {
        return rc;
    }

    if (grub_strcmp(args[0], "nodecompress") == 0)
    {
        type = VENTOY_FILE_TYPE;
    }
    else
    {
        type = GRUB_FILE_TYPE_LINUX_INITRD;
    }

    file = ventoy_grub_file_open(type, "%s", args[1]);
    if (file == NULL)
    {
        debug("failed to open file <%s>\n", args[1]);
        return 1;
    }

#ifdef GRUB_MACHINE_EFI
    buf = (char *)grub_efi_allocate_chain_buf(file->size);
#else
    buf = (char *)grub_malloc(file->size);
#endif

    if (!buf)
    {
        grub_file_close(file);
        return 1;
    }

    if (g_vtoy_load_prompt)
    {
         ventoy_load_file_with_prompt(file, buf, file->size);
    }
    else
    {
        grub_file_read(file, buf, file->size);
    }

    ventoy_memfile_env_set(args[2], buf, (ulonglong)(file->size));

    grub_file_close(file);
    rc = 0;

    return rc;
}

grub_err_t ventoy_cmd_load_img_memdisk(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int rc = 1;
    int headlen;
    char *buf = NULL;
    grub_file_t file;

    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc != 2)
    {
        return rc;
    }

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (file == NULL)
    {
        debug("failed to open file <%s> for udf check\n", args[0]);
        return 1;
    }

    headlen = sizeof(ventoy_chain_head);

#ifdef GRUB_MACHINE_EFI
    buf = (char *)grub_efi_allocate_iso_buf(headlen + file->size);
#else
    buf = (char *)grub_malloc(headlen + file->size);
#endif

    ventoy_fill_os_param(file, (ventoy_os_param *)buf);

    grub_file_read(file, buf + headlen, file->size);

    ventoy_memfile_env_set(args[1], buf, (ulonglong)(file->size));

    grub_file_close(file);
    rc = 0;

    return rc;
}

grub_err_t ventoy_cmd_iso9660_is_joliet(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    if (grub_iso9660_is_joliet())
    {
        debug("This time has joliet process\n");
        return 0;
    }
    else
    {
        return 1;
    }
}

grub_err_t ventoy_cmd_iso9660_nojoliet(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;

    if (argc != 1)
    {
        return 1;
    }

    if (args[0][0] == '1')
    {
        grub_iso9660_set_nojoliet(1);
    }
    else
    {
        grub_iso9660_set_nojoliet(0);
    }

    return 0;
}

grub_err_t ventoy_cmd_is_udf(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    int rc = 1;
    grub_file_t file;
    grub_uint8_t buf[32];

    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc != 1)
    {
        return rc;
    }

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (file == NULL)
    {
        debug("failed to open file <%s> for udf check\n", args[0]);
        return 1;
    }

    for (i = 16; i < 32; i++)
    {
        grub_file_seek(file, i * 2048);
        grub_file_read(file, buf, sizeof(buf));
        if (buf[0] == 255)
        {
            break;
        }
    }

    i++;
    grub_file_seek(file, i * 2048);
    grub_file_read(file, buf, sizeof(buf));

    if (grub_memcmp(buf + 1, "BEA01", 5) == 0)
    {
        i++;
        grub_file_seek(file, i * 2048);
        grub_file_read(file, buf, sizeof(buf));

        if (grub_memcmp(buf + 1, "NSR02", 5) == 0 ||
            grub_memcmp(buf + 1, "NSR03", 5) == 0)
        {
            rc = 0;
        }
    }

    grub_file_close(file);

    debug("ISO UDF: %s\n", rc ? "NO" : "YES");

    return rc;
}

