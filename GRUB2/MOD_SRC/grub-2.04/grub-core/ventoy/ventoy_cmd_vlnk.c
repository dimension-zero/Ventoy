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
int ventoy_vlnk_iterate_partition(struct grub_disk *disk, const grub_partition_t partition, void *data)
{
    ventoy_vlnk_part *node = NULL;
    grub_uint32_t SelfSig;
    grub_uint32_t *pSig = (grub_uint32_t *)data;

    /* skip Ventoy partition 1/2 */
    grub_memcpy(&SelfSig, g_ventoy_part_info->MBR.BootCode + 0x1b8, 4);
    if (partition->number < 2 && SelfSig == *pSig)
    {
        return 0;
    }

    node = grub_zalloc(sizeof(ventoy_vlnk_part));
    if (node)
    {
        node->disksig = *pSig;
        node->partoffset = (partition->start << GRUB_DISK_SECTOR_BITS);
        grub_snprintf(node->disk, sizeof(node->disk) - 1, "%s", disk->name);
        grub_snprintf(node->device, sizeof(node->device) - 1, "%s,%d", disk->name, partition->number + 1);

        node->next = g_vlnk_part_list;
        g_vlnk_part_list = node;
    }

    return 0;
}

int ventoy_vlnk_iterate_disk(const char *name, void *data)
{
    grub_disk_t disk;
    grub_uint32_t sig;

    (void)data;

    disk = grub_disk_open(name);
    if (disk)
    {
        grub_disk_read(disk, 0, 0x1b8, 4, &sig);
        grub_partition_iterate(disk, ventoy_vlnk_iterate_partition, &sig);
        grub_disk_close(disk);
    }

    return 0;
}

int ventoy_vlnk_probe_fs(ventoy_vlnk_part *cur)
{
    const char *fs[ventoy_fs_max + 1] =
    {
        "exfat", "ntfs", "ext2", "xfs", "udf", "fat", "btrfs", NULL
    };

    if (!cur->dev)
    {
        cur->dev = grub_device_open(cur->device);
    }

    if (cur->dev)
    {
        cur->fs = grub_fs_list_probe(cur->dev, fs);
    }

    return 0;
}

int ventoy_check_vlnk_data(ventoy_vlnk *vlnk, int print, char *dst, int size)
{
    int diskfind = 0;
    int partfind = 0;
    int filefind = 0;
    char *disk, *device;
    grub_uint32_t readcrc, calccrc;
    ventoy_vlnk_part *cur;
    grub_fs_t fs = NULL;

    if (grub_memcmp(&(vlnk->guid), &g_ventoy_guid, sizeof(ventoy_guid)))
    {
        if (print)
        {
            grub_printf("VLNK invalid guid\n");
            grub_refresh();
        }
        return 1;
    }

    readcrc = vlnk->crc32;
    vlnk->crc32 = 0;
    calccrc = grub_getcrc32c(0, vlnk, sizeof(ventoy_vlnk));
    if (readcrc != calccrc)
    {
        if (print)
        {
            grub_printf("VLNK invalid crc 0x%08x 0x%08x\n", calccrc, readcrc);
            grub_refresh();
        }
        return 1;
    }

    if (!g_vlnk_part_list)
    {
        grub_disk_dev_iterate(ventoy_vlnk_iterate_disk, NULL);
    }

    for (cur = g_vlnk_part_list; cur && filefind == 0; cur = cur->next)
    {
        if (cur->disksig == vlnk->disk_signature)
        {
            diskfind = 1;
            disk = cur->disk;
            if (cur->partoffset == vlnk->part_offset)
            {
                partfind = 1;
                device = cur->device;

                if (cur->probe == 0)
                {
                    cur->probe = 1;
                    ventoy_vlnk_probe_fs(cur);
                }

                if (!fs)
                {
                    fs = cur->fs;
                }

                if (cur->fs)
                {
                    struct grub_file file;

                    grub_memset(&file, 0, sizeof(file));
                    file.device = cur->dev;
                    if (cur->fs->fs_open(&file, vlnk->filepath) == GRUB_ERR_NONE)
                    {
                        filefind = 1;
                        cur->fs->fs_close(&file);
                        grub_snprintf(dst, size - 1, "(%s)%s", cur->device, vlnk->filepath);
                    }
                    else
                    {
                        grub_errno = 0;
                    }
                }
            }
        }
    }

    if (print)
    {
        grub_printf("\n==== VLNK Information ====\n"
                    "Disk Signature: %08x\n"
                    "Partition Offset: %llu\n"
                    "File Path: <%s>\n\n",
                    vlnk->disk_signature, (ulonglong)vlnk->part_offset, vlnk->filepath);

        if (diskfind)
        {
            grub_printf("Disk Find: [ YES ] [ %s ]\n", disk);
        }
        else
        {
            grub_printf("Disk Find: [ NO ]\n");
        }

        if (partfind)
        {
            grub_printf("Part Find: [ YES ] [ %s ] [ %s ]\n", device, fs ? fs->name : "N/A");
        }
        else
        {
            grub_printf("Part Find: [ NO ]\n");
        }
        grub_printf("File Find: [ %s ]\n", filefind ? "YES" : "NO");
        if (filefind)
        {
            grub_printf("VLNK File: <%s>\n", dst);
        }

        grub_printf("\n");
        grub_refresh();
    }

    return (1 - filefind);
}

int ventoy_add_vlnk_file(char *dir, const char *name)
{
    int rc = 1;
    char src[512];
    char dst[512];
    grub_file_t file = NULL;
    ventoy_vlnk vlnk;

    if (!dir)
    {
        grub_snprintf(src, sizeof(src), "%s%s", g_iso_path, name);
    }
    else if (dir[0] == '/')
    {
        grub_snprintf(src, sizeof(src), "%s%s%s", g_iso_path, dir, name);
    }
    else
    {
        grub_snprintf(src, sizeof(src), "%s/%s%s", g_iso_path, dir, name);
    }

    file = grub_file_open(src, VENTOY_FILE_TYPE);
    if (!file)
    {
        return 1;
    }

    grub_memset(&vlnk, 0, sizeof(vlnk));
    grub_file_read(file, &vlnk, sizeof(vlnk));
    grub_file_close(file);

    if (ventoy_check_vlnk_data(&vlnk, 0, dst, sizeof(dst)) == 0)
    {
        rc = grub_file_add_vlnk(src, dst);
    }

    return rc;
}

