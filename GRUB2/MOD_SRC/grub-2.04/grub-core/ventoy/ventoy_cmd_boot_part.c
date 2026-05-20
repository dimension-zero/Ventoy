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

grub_err_t ventoy_cmd_file_exist_nocase(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_file_t file;

    (void)ctxt;

    if (argc != 1)
    {
        return 1;
    }

    g_ventoy_case_insensitive = 1;
    file = grub_file_open(args[0], VENTOY_FILE_TYPE);
    g_ventoy_case_insensitive = 0;

    grub_errno = 0;

    if (file)
    {
        grub_file_close(file);
        return 0;
    }
    return 1;
}

grub_err_t ventoy_cmd_find_bootable_hdd(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int id = 0;
    int find = 0;
    grub_disk_t disk;
    const char *isopath = NULL;
    char hdname[32];
    ventoy_mbr_head mbr;

    (void)ctxt;
    (void)argc;

    if (argc != 1)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s variable\n", cmd_raw_name);
    }

    isopath = grub_env_get("vtoy_iso_part");
    if (!isopath)
    {
        debug("isopath is null %p\n", isopath);
        return 0;
    }

    debug("isopath is %s\n", isopath);

    for (id = 0; id < 30 && (find == 0); id++)
    {
        grub_snprintf(hdname, sizeof(hdname), "hd%d,", id);
        if (grub_strstr(isopath, hdname))
        {
            debug("skip %s ...\n", hdname);
            continue;
        }

        grub_snprintf(hdname, sizeof(hdname), "hd%d", id);

        disk = grub_disk_open(hdname);
        if (!disk)
        {
            debug("%s not exist\n", hdname);
            break;
        }

        grub_memset(&mbr, 0, sizeof(mbr));
        if (0 == grub_disk_read(disk, 0, 0, 512, &mbr))
        {
            if (mbr.Byte55 == 0x55 && mbr.ByteAA == 0xAA)
            {
                if (mbr.PartTbl[0].Active == 0x80 || mbr.PartTbl[1].Active == 0x80 ||
                    mbr.PartTbl[2].Active == 0x80 || mbr.PartTbl[3].Active == 0x80)
                {

                    grub_env_set(args[0], hdname);
                    find = 1;
                }
            }
            debug("%s is %s\n", hdname, find ? "bootable" : "NOT bootable");
        }
        else
        {
            debug("read %s failed\n", hdname);
        }

        grub_disk_close(disk);
    }

    return 0;
}

grub_err_t ventoy_cmd_read_1st_line(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int len = 1024;
    grub_file_t file;
    char *buf = NULL;

    (void)ctxt;
    (void)argc;

    if (argc != 2)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s file var \n", cmd_raw_name);
    }

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (!file)
    {
        debug("failed to open file %s\n", args[0]);
        return 0;
    }

    buf = grub_malloc(len);
    if (!buf)
    {
        goto end;
    }

    buf[len - 1] = 0;
    grub_file_read(file, buf, len - 1);

    ventoy_get_line(buf);
    ventoy_set_env(args[1], buf);

end:

    grub_check_free(buf);
    grub_file_close(file);

    return 0;
}

int ventoy_img_partition_callback (struct grub_disk *disk, const grub_partition_t partition, void *data)
{
    grub_uint64_t end_max = 0;
    int *pCnt = (int *)data;

    (void)disk;

    (*pCnt)++;
    g_part_list_pos += grub_snprintf(g_part_list_buf + g_part_list_pos, VTOY_MAX_SCRIPT_BUF - g_part_list_pos,
        "0 %llu linear /dev/ventoy %llu\n",
        (ulonglong)partition->len, (ulonglong)partition->start);

    end_max = (partition->len + partition->start) * 512;
    if (end_max > g_part_end_max)
    {
        g_part_end_max = end_max;
    }

    return 0;
}

grub_err_t ventoy_cmd_img_part_info(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int cnt = 0;
    char *device_name = NULL;
    grub_device_t dev = NULL;
    char buf[64];

    (void)ctxt;

    g_part_list_pos = 0;
    g_part_end_max = 0;
    grub_env_unset("vtoy_img_part_file");

    if (argc != 1)
    {
        return 1;
    }

    device_name = grub_file_get_device_name(args[0]);
    if (!device_name)
    {
        debug("ventoy_cmd_img_part_info failed, %s\n", args[0]);
        goto end;
    }

    dev = grub_device_open(device_name);
    if (!dev)
    {
        debug("grub_device_open failed, %s\n", device_name);
        goto end;
    }

    grub_partition_iterate(dev->disk, ventoy_img_partition_callback, &cnt);

    grub_snprintf(buf, sizeof(buf), "newc:vtoy_dm_table:mem:0x%llx:size:%d", (ulonglong)(ulong)g_part_list_buf, g_part_list_pos);
    grub_env_set("vtoy_img_part_file", buf);

    grub_snprintf(buf, sizeof(buf), "%d", cnt);
    grub_env_set("vtoy_img_part_cnt", buf);

    grub_snprintf(buf, sizeof(buf), "%llu", (ulonglong)g_part_end_max);
    grub_env_set("vtoy_img_max_part_end", buf);

end:

    check_free(device_name, grub_free);
    check_free(dev, grub_device_close);

    return 0;
}


grub_err_t ventoy_cmd_file_strstr(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int rc = 1;
    grub_file_t file;
    char *buf = NULL;

    (void)ctxt;
    (void)argc;

    if (argc != 2)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s file str \n", cmd_raw_name);
    }

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (!file)
    {
        debug("failed to open file %s\n", args[0]);
        return 1;
    }

    buf = grub_malloc(file->size + 1);
    if (!buf)
    {
        goto end;
    }

    buf[file->size] = 0;
    grub_file_read(file, buf, file->size);

    if (grub_strstr(buf, args[1]))
    {
        rc = 0;
    }

end:

    grub_check_free(buf);
    grub_file_close(file);

    return rc;
}

grub_err_t ventoy_cmd_parse_volume(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int len;
    grub_file_t file;
    char buf[64];
    grub_uint64_t size;
    ventoy_iso9660_vd pvd;

    (void)ctxt;
    (void)argc;

    if (argc != 4)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s sysid volid space \n", cmd_raw_name);
    }

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (!file)
    {
        debug("failed to open file %s\n", args[0]);
        return 0;
    }

    grub_file_seek(file, 16 * 2048);
    len = (int)grub_file_read(file, &pvd, sizeof(pvd));
    if (len != sizeof(pvd))
    {
        debug("failed to read pvd %d\n", len);
        goto end;
    }

    grub_memset(buf, 0, sizeof(buf));
    grub_memcpy(buf, pvd.sys, sizeof(pvd.sys));
    ventoy_set_env(args[1], buf);

    grub_memset(buf, 0, sizeof(buf));
    grub_memcpy(buf, pvd.vol, sizeof(pvd.vol));
    ventoy_set_env(args[2], buf);

    size = pvd.space;
    size *= 2048;
    grub_snprintf(buf, sizeof(buf), "%llu", (ulonglong)size);
    ventoy_set_env(args[3], buf);

end:
    grub_file_close(file);

    return 0;
}

grub_err_t ventoy_cmd_parse_create_date(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int len;
    grub_file_t file;
    char buf[64];

    (void)ctxt;
    (void)argc;

    if (argc != 2)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s var \n", cmd_raw_name);
    }

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (!file)
    {
        debug("failed to open file %s\n", args[0]);
        return 0;
    }

    grub_memset(buf, 0, sizeof(buf));
    grub_file_seek(file, 16 * 2048 + 813);
    len = (int)grub_file_read(file, buf, 17);
    if (len != 17)
    {
        debug("failed to read create date %d\n", len);
        goto end;
    }

    ventoy_set_env(args[1], buf);

end:
    grub_file_close(file);

    return 0;
}

grub_err_t ventoy_cmd_img_hook_root(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    ventoy_env_hook_root(1);

    return 0;
}

