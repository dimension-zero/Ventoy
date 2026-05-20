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
int ventoy_gzip_compress(void *mem_in, int mem_in_len, void *mem_out, int mem_out_len)
{
	mz_stream s;
    grub_uint8_t *outbuf;
    grub_uint8_t gzHdr[10] =
    {
		0x1F, 0x8B,	/* magic */
		8,		    /* z method */
		0,		    /* flags */
		0,0,0,0,	/* mtime */
		4,		    /* xfl */
		3,		    /* OS */
	};

	grub_memset(&s, 0, sizeof(mz_stream));

    mz_deflateInit2(&s, 1, MZ_DEFLATED, -MZ_DEFAULT_WINDOW_BITS, 6, MZ_DEFAULT_STRATEGY);

    outbuf = (grub_uint8_t *)mem_out;

    mem_out_len -= sizeof(gzHdr) + 8;
    grub_memcpy(outbuf, gzHdr, sizeof(gzHdr));
    outbuf += sizeof(gzHdr);

    s.avail_in = mem_in_len;
    s.next_in = mem_in;

    s.avail_out = mem_out_len;
    s.next_out = outbuf;

    mz_deflate(&s, MZ_FINISH);

    mz_deflateEnd(&s);

    outbuf += s.total_out;
    *(grub_uint32_t *)outbuf = grub_getcrc32c(0, outbuf, s.total_out);
    *(grub_uint32_t *)(outbuf + 4) = (grub_uint32_t)(s.total_out);

    return s.total_out + sizeof(gzHdr) + 8;
}


#if 0
ventoy grub cmds
#endif

grub_err_t ventoy_cmd_debug(grub_extcmd_context_t ctxt, int argc, char **args)
{
    if (argc != 1)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s {on|off}", cmd_raw_name);
    }

    if (0 == grub_strcmp(args[0], "on"))
    {
        g_ventoy_debug = 1;
        grub_env_set("vtdebug_flag", "debug");
    }
    else
    {
        g_ventoy_debug = 0;
        grub_env_set("vtdebug_flag", "");
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_break(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;

    if (argc < 1 || (args[0][0] != '0' && args[0][0] != '1'))
    {
        grub_printf("Usage: %s {level} [debug]\r\n", cmd_raw_name);
        grub_printf(" level:\r\n");
        grub_printf("    01/11: busybox / (+cat log)\r\n");
        grub_printf("    02/12: initrd / (+cat log)\r\n");
        grub_printf("    03/13: hook / (+cat log)\r\n");
        grub_printf("\r\n");
        grub_printf(" debug:\r\n");
        grub_printf("    0: debug is off\r\n");
        grub_printf("    1: debug is on\r\n");
        grub_printf("\r\n");
        VENTOY_CMD_RETURN(GRUB_ERR_NONE);
    }

    g_ventoy_break_level = (grub_uint8_t)grub_strtoul(args[0], NULL, 16);

    if (argc > 1 && grub_strtoul(args[1], NULL, 10) > 0)
    {
        g_ventoy_debug_level = 1;
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_strstr(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;

    if (argc != 2)
    {
        return 1;
    }

    return (grub_strstr(args[0], args[1])) ? 0 : 1;
}

grub_err_t ventoy_cmd_strbegin(grub_extcmd_context_t ctxt, int argc, char **args)
{
    char *c0, *c1;

    (void)ctxt;

    if (argc != 2)
    {
        return 1;
    }

    c0 = args[0];
    c1 = args[1];

    while (*c0 && *c1)
    {
        if (*c0 != *c1)
        {
            return 1;
        }
        c0++;
        c1++;
    }

    if (*c1)
    {
        return 1;
    }

    return 0;
}

grub_err_t ventoy_cmd_strcasebegin(grub_extcmd_context_t ctxt, int argc, char **args)
{
    char *c0, *c1;

    (void)ctxt;

    if (argc != 2)
    {
        return 1;
    }

    c0 = args[0];
    c1 = args[1];

    while (*c0 && *c1)
    {
        if ((*c0 != *c1) && (*c0 != grub_toupper(*c1)))
        {
            return 1;
        }
        c0++;
        c1++;
    }

    if (*c1)
    {
        return 1;
    }

    return 0;
}

grub_err_t ventoy_cmd_incr(grub_extcmd_context_t ctxt, int argc, char **args)
{
    long value_long = 0;
    char buf[32];

    if ((argc != 2) || (!ventoy_is_decimal(args[1])))
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s {Variable} {Int}", cmd_raw_name);
    }

    if (GRUB_ERR_NONE != ventoy_check_decimal_var(args[0], &value_long))
    {
        return grub_errno;
    }

    value_long += grub_strtol(args[1], NULL, 10);

    grub_snprintf(buf, sizeof(buf), "%ld", value_long);
    grub_env_set(args[0], buf);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_mod(grub_extcmd_context_t ctxt, int argc, char **args)
{
    ulonglong value1 = 0;
    ulonglong value2 = 0;
    char buf[32];

    if (argc != 3)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s {Int} {Int} {Variable}", cmd_raw_name);
    }

    value1 = grub_strtoull(args[0], NULL, 10);
    value2 = grub_strtoull(args[1], NULL, 10);

    grub_snprintf(buf, sizeof(buf), "%llu", (value1 & (value2 - 1)));
    grub_env_set(args[2], buf);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_file_size(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int rc = 1;
    char buf[32];
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

    grub_snprintf(buf, sizeof(buf), "%llu", (unsigned long long)file->size);

    grub_env_set(args[1], buf);

    grub_file_close(file);
    rc = 0;

    return rc;
}

grub_err_t ventoy_cmd_load_wimboot(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_file_t file;

    (void)ctxt;
    (void)argc;
    (void)args;

    g_wimboot_enable = 0;
    g_wimiso_size = 0;
    grub_check_free(g_wimiso_path);
    grub_check_free(g_wimiso_chunk_list.chunk);

    file = grub_file_open(args[0], VENTOY_FILE_TYPE);
    if (!file)
    {
        return 0;
    }

    grub_memset(&g_wimiso_chunk_list, 0, sizeof(g_wimiso_chunk_list));
    g_wimiso_chunk_list.chunk = grub_malloc(sizeof(ventoy_img_chunk) * DEFAULT_CHUNK_NUM);
    if (NULL == g_wimiso_chunk_list.chunk)
    {
        return grub_error(GRUB_ERR_OUT_OF_MEMORY, "Can't allocate image chunk memoty\n");
    }

    g_wimiso_chunk_list.max_chunk = DEFAULT_CHUNK_NUM;
    g_wimiso_chunk_list.cur_chunk = 0;

    ventoy_get_block_list(file, &g_wimiso_chunk_list, file->device->disk->partition->start);

    g_wimboot_enable = 1;
    g_wimiso_path = grub_strdup(args[0]);
    g_wimiso_size = (grub_uint32_t)(file->size);
    grub_file_close(file);

    return 0;
}

grub_err_t ventoy_cmd_concat_efi_iso(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int len = 0;
    int totlen = 0;
    int offset = 0;
    grub_file_t file;
    char *buf = NULL;
    char *data = NULL;
    ventoy_iso9660_override *dirent;

    (void)ctxt;

    if (argc != 2)
    {
        return 1;
    }

    totlen = sizeof(ventoy_chain_head);

    if (ventoy_load_efiboot_template(&buf, &len, &offset))
    {
        debug("failed to load efiboot template %d\n", len);
        return 1;
    }

    totlen += len;

    debug("efiboot template len:%d offset:%d\n", len, offset);

    file = ventoy_grub_file_open(GRUB_FILE_TYPE_LINUX_INITRD, "%s", args[0]);
    if (file == NULL)
    {
        debug("failed to open file <%s>\n", args[0]);
        return 1;
    }

    if (grub_strncmp(args[0], g_iso_path, grub_strlen(g_iso_path)))
    {
        file->vlnk = 1;
    }

    totlen += ventoy_align_2k(file->size);

    dirent = (ventoy_iso9660_override *)(buf + offset);
    dirent->first_sector = len / 2048;
    dirent->first_sector_be = grub_swap_bytes32(dirent->first_sector);
    dirent->size = (grub_uint32_t)file->size;
    dirent->size_be = grub_swap_bytes32(dirent->size);

    debug("rawiso len:%d efilen:%d total:%d\n", len, (int)file->size, totlen);

#ifdef GRUB_MACHINE_EFI
    data = (char *)grub_efi_allocate_iso_buf(totlen);
#else
    data = (char *)grub_malloc(totlen);
#endif

    ventoy_fill_os_param(file, (ventoy_os_param *)data);

    grub_memcpy(data + sizeof(ventoy_chain_head), buf, len);
    grub_check_free(buf);

    grub_file_read(file, data + sizeof(ventoy_chain_head) + len, file->size);
    grub_file_close(file);

    ventoy_memfile_env_set(args[1], data, (ulonglong)totlen);

    return 0;
}

