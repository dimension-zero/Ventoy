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
int ventoy_lib_module_callback(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    const char *pos = filename + 1;

    if (info->dir)
    {
        while (*pos)
        {
            if (*pos == '.')
            {
                if ((*(pos - 1) >= '0' && *(pos - 1) <= '9') && (*(pos + 1) >= '0' && *(pos + 1) <= '9'))
                {
                    grub_strncpy((char *)data, filename, 128);
                    return 1;
                }
            }
            pos++;
        }
    }

    return 0;
}

grub_err_t ventoy_cmd_lib_module_ver(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int rc = 1;
    char *device_name = NULL;
    grub_device_t dev = NULL;
    grub_fs_t fs = NULL;
    char buf[128] = {0};

    (void)ctxt;

    if (argc != 3)
    {
        debug("ventoy_cmd_lib_module_ver, invalid param num %d\n", argc);
        return 1;
    }

    debug("ventoy_cmd_lib_module_ver %s %s %s\n", args[0], args[1], args[2]);

    device_name = grub_file_get_device_name(args[0]);
    if (!device_name)
    {
        debug("grub_file_get_device_name failed, %s\n", args[0]);
        goto end;
    }

    dev = grub_device_open(device_name);
    if (!dev)
    {
        debug("grub_device_open failed, %s\n", device_name);
        goto end;
    }

    fs = grub_fs_probe(dev);
    if (!fs)
    {
        debug("grub_fs_probe failed, %s\n", device_name);
        goto end;
    }

    fs->fs_dir(dev, args[1], ventoy_lib_module_callback, buf);

    if (buf[0])
    {
        ventoy_set_env(args[2], buf);
    }

    rc = 0;

end:

    check_free(device_name, grub_free);
    check_free(dev, grub_device_close);

    return rc;
}

int ventoy_load_part_table(const char *diskname)
{
    char name[64];
    int ret;
    grub_disk_t disk;
    grub_device_t dev;

    g_ventoy_part_info = grub_zalloc(sizeof(ventoy_gpt_info));
    if (!g_ventoy_part_info)
    {
        return 1;
    }

    disk = grub_disk_open(diskname);
    if (!disk)
    {
        debug("Failed to open disk %s\n", diskname);
        return 1;
    }

    g_ventoy_disk_size = disk->total_sectors * (1U << disk->log_sector_size);

    g_ventoy_disk_bios_id = disk->id;

    grub_disk_read(disk, 0, 0, sizeof(ventoy_gpt_info), g_ventoy_part_info);
    grub_disk_close(disk);

    grub_snprintf(name, sizeof(name), "%s,1", diskname);
    dev = grub_device_open(name);
    if (dev)
    {
        /* Check for official Ventoy device */
        ret = ventoy_check_official_device(dev);
        grub_device_close(dev);

        if (ret)
        {
            return 1;
        }
    }

    g_ventoy_disk_part_size[0] = ventoy_get_vtoy_partsize(0);
    g_ventoy_disk_part_size[1] = ventoy_get_vtoy_partsize(1);

    return 0;
}

void ventoy_prompt_end(void)
{
    int op = 0;
    char c;

    grub_printf("\n\n\n");
    grub_printf(" 1 --- Exit grub\n");
    grub_printf(" 2 --- Reboot\n");
    grub_printf(" 3 --- Shut down\n");
    grub_printf("Please enter your choice: ");
    grub_refresh();

    while (1)
    {
        c = grub_getkey();
        if (c >= '1' && c <= '3')
        {
            if (op == 0)
            {
                op = c - '0';
                grub_printf("%c", c);
                grub_refresh();
            }
        }
        else if (c == '\r' || c == '\n')
        {
            if (op)
            {
                if (op == 1)
                {
                    grub_exit();
                }
                else if (op == 2)
                {
                    grub_reboot();
                }
                else if (op == 3)
                {
                    grub_script_execute_sourcecode("halt");
                }
            }
        }
        else if (c == '\b')
        {
            if (op)
            {
                op = 0;
                grub_printf("\rPlease enter your choice:   ");
                grub_printf("\rPlease enter your choice: ");
                grub_refresh();
            }
        }
    }
}

grub_err_t ventoy_cmd_load_part_table(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int ret;

    (void)argc;
    (void)ctxt;

    ret = ventoy_load_part_table(args[0]);
    if (ret)
    {
        ventoy_prompt_end();
    }

    g_ventoy_disk_part_size[0] = ventoy_get_vtoy_partsize(0);
    g_ventoy_disk_part_size[1] = ventoy_get_vtoy_partsize(1);

    return 0;
}

grub_err_t ventoy_cmd_check_custom_boot(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int ret = 1;
    const char *vcfg = NULL;

    (void)argc;
    (void)ctxt;

    vcfg = ventoy_plugin_get_custom_boot(args[0]);
    if (vcfg)
    {
        debug("custom boot <%s>:<%s>\n", args[0], vcfg);
        grub_env_set(args[1], vcfg);
        ret = 0;
    }
    else
    {
        debug("custom boot <%s>:<NOT FOUND>\n", args[0]);
    }

    grub_errno = 0;
    return ret;
}


grub_err_t ventoy_cmd_part_exist(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int id;
    grub_uint8_t zeroguid[16] = {0};

    (void)argc;
    (void)ctxt;

    id = (int)grub_strtoul(args[0], NULL, 10);
    grub_errno = 0;

    if (grub_memcmp(g_ventoy_part_info->Head.Signature, "EFI PART", 8) == 0)
    {
        if (id >= 1 && id <= 128)
        {
            if (grub_memcmp(g_ventoy_part_info->PartTbl[id - 1].PartGuid, zeroguid, 16))
            {
                return 0;
            }
        }
    }
    else
    {
        if (id >= 1 && id <= 4)
        {
            if (g_ventoy_part_info->MBR.PartTbl[id - 1].FsFlag)
            {
                return 0;
            }
        }
    }

    return 1;
}

grub_err_t ventoy_cmd_get_fs_label(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int rc = 1;
    char *device_name = NULL;
    grub_device_t dev = NULL;
    grub_fs_t fs = NULL;
    char *label = NULL;

    (void)ctxt;

    debug("get fs label for %s\n", args[0]);

    if (argc != 2)
    {
        debug("ventoy_cmd_get_fs_label, invalid param num %d\n", argc);
        return 1;
    }

    device_name = grub_file_get_device_name(args[0]);
    if (!device_name)
    {
        debug("grub_file_get_device_name failed, %s\n", args[0]);
        goto end;
    }

    dev = grub_device_open(device_name);
    if (!dev)
    {
        debug("grub_device_open failed, %s\n", device_name);
        goto end;
    }

    fs = grub_fs_probe(dev);
    if (NULL == fs || NULL == fs->fs_label)
    {
        debug("grub_fs_probe failed, %s %p %p\n", device_name, fs, fs->fs_label);
        goto end;
    }

    fs->fs_label(dev, &label);
    if (label)
    {
        debug("label=<%s>\n", label);
        ventoy_set_env(args[1], label);
        grub_free(label);
    }

    rc = 0;

end:

    check_free(device_name, grub_free);
    check_free(dev, grub_device_close);

    return rc;
}

