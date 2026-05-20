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
int ventoy_get_disk_guid(const char *filename, grub_uint8_t *guid, grub_uint8_t *signature)
{
    grub_disk_t disk;
    char *device_name;
    char *pos;
    char *pos2;

    device_name = grub_file_get_device_name(filename);
    if (!device_name)
    {
        return 1;
    }

    pos = device_name;
    if (pos[0] == '(')
    {
        pos++;
    }

    pos2 = grub_strstr(pos, ",");
    if (!pos2)
    {
        pos2 = grub_strstr(pos, ")");
    }

    if (pos2)
    {
        *pos2 = 0;
    }

    disk = grub_disk_open(pos);
    if (disk)
    {
        grub_disk_read(disk, 0, 0x180, 16, guid);
        grub_disk_read(disk, 0, 0x1b8, 4, signature);
        grub_disk_close(disk);
    }
    else
    {
        return 1;
    }

    grub_free(device_name);
    return 0;
}

grub_uint32_t ventoy_get_iso_boot_catlog(grub_file_t file)
{
    eltorito_descriptor desc;

    grub_memset(&desc, 0, sizeof(desc));
    grub_file_seek(file, 17 * 2048);
    grub_file_read(file, &desc, sizeof(desc));

    if (desc.type != 0 || desc.version != 1)
    {
        return 0;
    }

    if (grub_strncmp((char *)desc.id, "CD001", 5) != 0 ||
        grub_strncmp((char *)desc.system_id, "EL TORITO SPECIFICATION", 23) != 0)
    {
        return 0;
    }

    return desc.sector;
}

grub_uint32_t ventoy_get_bios_eltorito_rba(grub_file_t file, grub_uint32_t sector)
{
    grub_uint8_t buf[512];

    grub_file_seek(file, sector * 2048);
    grub_file_read(file, buf, sizeof(buf));

    if (buf[0] == 0x01 && buf[1] == 0x00 &&
        buf[30] == 0x55 && buf[31] == 0xaa && buf[32] == 0x88)
    {
        return *((grub_uint32_t *)(buf + 40));
    }

    return 0;
}

int ventoy_has_efi_eltorito(grub_file_t file, grub_uint32_t sector)
{
    int i;
    int x86count = 0;
    grub_uint8_t buf[512];
    grub_uint8_t parttype[] = { 0x04, 0x06, 0x0B, 0x0C };

    grub_file_seek(file, sector * 2048);
    grub_file_read(file, buf, sizeof(buf));

    if (buf[0] == 0x01 && buf[1] == 0xEF)
    {
        debug("%s efi eltorito in Validation Entry\n", file->name);
        return 1;
    }

    if (buf[0] == 0x01 && buf[1] == 0x00)
    {
        x86count++;
    }

    for (i = 64; i < (int)sizeof(buf); i += 32)
    {
        if ((buf[i] == 0x90 || buf[i] == 0x91) && buf[i + 1] == 0xEF)
        {
            debug("%s efi eltorito offset %d 0x%02x\n", file->name, i, buf[i]);
            return 1;
        }

        if ((buf[i] == 0x90 || buf[i] == 0x91) && buf[i + 1] == 0x00 && x86count == 1)
        {
            debug("0x9100 assume %s efi eltorito offset %d 0x%02x\n", file->name, i, buf[i]);
            return 1;
        }
    }

    if (x86count && buf[32] == 0x88 && buf[33] == 0x04)
    {
        for (i = 0; i < (int)(ARRAY_SIZE(parttype)); i++)
        {
            if (buf[36] == parttype[i])
            {
                debug("hard disk image assume %s efi eltorito, part type 0x%x\n", file->name, buf[36]);
                return 1;
            }
        }
    }

    debug("%s does not contain efi eltorito\n", file->name);
    return 0;
}

void ventoy_fill_os_param(grub_file_t file, ventoy_os_param *param)
{
    char *pos;
    const char *fs = NULL;
    const char *val = NULL;
    const char *cdprompt = NULL;
    grub_uint32_t i;
    grub_uint8_t  chksum = 0;
    grub_disk_t   disk;

    disk = file->device->disk;
    grub_memcpy(&param->guid, &g_ventoy_guid, sizeof(ventoy_guid));

    param->vtoy_disk_size = disk->total_sectors * (1 << disk->log_sector_size);
    param->vtoy_disk_part_id = disk->partition->number + 1;
    param->vtoy_disk_part_type = ventoy_get_fs_type(file->fs->name);

    pos = grub_strstr(file->name, "/");
    if (!pos)
    {
        pos = file->name;
    }

    grub_snprintf(param->vtoy_img_path, sizeof(param->vtoy_img_path), "%s", pos);

    ventoy_get_disk_guid(file->name, param->vtoy_disk_guid, param->vtoy_disk_signature);

    param->vtoy_img_size = file->size;

    param->vtoy_reserved[0] = g_ventoy_break_level;
    param->vtoy_reserved[1] = g_ventoy_debug_level;

    param->vtoy_reserved[2] = g_ventoy_chain_type;

    /* Windows CD/DVD prompt   0:suppress  1:reserved */
    param->vtoy_reserved[4] = 0;
    if (g_ventoy_chain_type == 1) /* Windows */
    {
        cdprompt = ventoy_get_env("VTOY_WINDOWS_CD_PROMPT");
        if (cdprompt && cdprompt[0] == '1' && cdprompt[1] == 0)
        {
            param->vtoy_reserved[4] = 1;
        }
    }

    fs = ventoy_get_env("ventoy_fs_probe");
    if (fs && grub_strcmp(fs, "udf") == 0)
    {
        param->vtoy_reserved[3] = 1;
    }

    param->vtoy_reserved[5] = 0;
    val = ventoy_get_env("VTOY_LINUX_REMOUNT");
    if (val && val[0] == '1' && val[1] == 0)
    {
        param->vtoy_reserved[5] = 1;
    }

    /* ventoy_disk_signature used for vlnk */
    param->vtoy_reserved[6] = file->vlnk;
    grub_memcpy(param->vtoy_reserved + 7, g_ventoy_part_info->MBR.BootCode + 0x1b8, 4);


    /* Windows UEFI force resolution lock */
    if (g_ventoy_chain_type == 1) /* Windows */
    {
        val = ventoy_get_env("VTOY_WIN_UEFI_RES_LOCK");
        if (val && val[1] == 0)
        {
            if (val[0] == '1')
            {
                param->vtoy_reserved[11] = 1;
            }
            else if (val[0] == '2')
            {
                param->vtoy_reserved[11] = 2;
            }
        }
    }


    /* calculate checksum */
    for (i = 0; i < sizeof(ventoy_os_param); i++)
    {
        chksum += *((grub_uint8_t *)param + i);
    }
    param->chksum = (grub_uint8_t)(0x100 - chksum);

    return;
}

const char* g_chunk_err_msg[VTOY_CHUNK_ERR_MAX] =
{
    "success",
    "File system use more than 1 disks! (maybe RAID)",
    "File system enable RAID feature, this is NOT supported!",
    "File is compressed in disk, this is not supported!",
    "File not flat in disk! (maybe compressed)",
    "Read buffer overflow!",
};

const char * ventoy_get_chunk_err_msg(grub_uint32_t err)
{
    if (err < VTOY_CHUNK_ERR_MAX)
    {
        return g_chunk_err_msg[err];
    }

    return "XXXX";
}

int ventoy_check_block_list(grub_file_t file, ventoy_img_chunk_list *chunklist,
grub_disk_addr_t start, char *err, grub_uint32_t len)
{
    grub_uint32_t i = 0;
    grub_uint64_t total = 0;
    grub_uint64_t fileblk = 0;
    ventoy_img_chunk *chunk = NULL;

    if (chunklist->err_code)
    {
        if (err)
        {
            grub_snprintf(err, len, "%s", ventoy_get_chunk_err_msg(chunklist->err_code));
        }

        return 1;
    }

    if (err)
    {
        grub_snprintf(err, len, "Unsupported chunk list.");
    }

    for (i = 0; i < chunklist->cur_chunk; i++)
    {
        chunk = chunklist->chunk + i;

        if (chunk->disk_start_sector <= start)
        {
            debug("%u disk start invalid %lu\n", i, (ulong)start);
            return 1;
        }

        total += chunk->disk_end_sector + 1 - chunk->disk_start_sector;
    }

    fileblk = (file->size + 511) / 512;

    if (total != fileblk)
    {
        debug("Invalid total: %llu %llu\n", (ulonglong)total, (ulonglong)fileblk);
        if ((file->size % 512) && (total + 1 == fileblk))
        {
            debug("maybe img file to be processed.\n");
            return 0;
        }

        return 1;
    }

    return 0;
}

int ventoy_get_block_list(grub_file_t file, ventoy_img_chunk_list *chunklist, grub_disk_addr_t start)
{
    int fs_type;
    int len;
    grub_uint32_t i = 0;
    grub_uint32_t sector = 0;
    grub_uint32_t count = 0;
    grub_off_t size = 0;
    grub_off_t read = 0;

    fs_type = ventoy_get_fs_type(file->fs->name);
    if (fs_type == ventoy_fs_exfat)
    {
        grub_fat_get_file_chunk(start, file, chunklist);
    }
    else if (fs_type == ventoy_fs_ext)
    {
        grub_ext_get_file_chunk(start, file, chunklist);
    }
    else if (fs_type == ventoy_fs_btrfs)
    {
        grub_btrfs_get_file_chunk(start, file, chunklist);
    }
    else
    {
        file->read_hook = (grub_disk_read_hook_t)(void *)grub_disk_blocklist_read;
        file->read_hook_data = chunklist;

        for (size = file->size; size > 0; size -= read)
        {
            read = (size > VTOY_SIZE_1GB) ? VTOY_SIZE_1GB : size;
            grub_file_read(file, NULL, read);
        }

        for (i = 0; start > 0 && i < chunklist->cur_chunk; i++)
        {
            chunklist->chunk[i].disk_start_sector += start;
            chunklist->chunk[i].disk_end_sector += start;
        }

        if (ventoy_fs_udf == fs_type)
        {
            for (i = 0; i < chunklist->cur_chunk; i++)
            {
                count = (chunklist->chunk[i].disk_end_sector + 1 - chunklist->chunk[i].disk_start_sector) >> 2;
                chunklist->chunk[i].img_start_sector = sector;
                chunklist->chunk[i].img_end_sector = sector + count - 1;
                sector += count;
            }
        }
    }

    len = (int)grub_strlen(file->name);
    if ((len > 4 && grub_strncasecmp(file->name + len - 4, ".img", 4) == 0) ||
        (len > 4 && grub_strncasecmp(file->name + len - 4, ".vhd", 4) == 0) ||
        (len > 5 && grub_strncasecmp(file->name + len - 5, ".vhdx", 5) == 0) ||
        (len > 5 && grub_strncasecmp(file->name + len - 5, ".vtoy", 5) == 0))
    {
        for (i = 0; i < chunklist->cur_chunk; i++)
        {
            count = chunklist->chunk[i].disk_end_sector + 1 - chunklist->chunk[i].disk_start_sector;
            if (count < 4)
            {
                count = 1;
            }
            else
            {
                count >>= 2;
            }

            chunklist->chunk[i].img_start_sector = sector;
            chunklist->chunk[i].img_end_sector = sector + count - 1;
            sector += count;
        }
    }

    return 0;
}

grub_err_t ventoy_cmd_img_sector(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int rc;
    int fs_type;
    grub_file_t file;
    grub_disk_addr_t start;
    char errmsg[128];

    (void)ctxt;
    (void)argc;

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (!file)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Can't open file %s\n", args[0]);
    }

    g_conf_replace_count = 0;
    grub_memset(g_conf_replace_node, 0, sizeof(g_conf_replace_node ));
    grub_memset(g_conf_replace_offset, 0, sizeof(g_conf_replace_offset ));

    if (g_img_chunk_list.chunk)
    {
        grub_free(g_img_chunk_list.chunk);
    }

    fs_type = ventoy_get_fs_type(file->fs->name);
    if (fs_type >= ventoy_fs_max)
    {
        grub_file_close(file);
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Unsupported filesystem %s\n", file->fs->name);
    }

    /* get image chunk data */
    grub_memset(&g_img_chunk_list, 0, sizeof(g_img_chunk_list));
    g_img_chunk_list.chunk = grub_malloc(sizeof(ventoy_img_chunk) * DEFAULT_CHUNK_NUM);
    if (NULL == g_img_chunk_list.chunk)
    {
        return grub_error(GRUB_ERR_OUT_OF_MEMORY, "Can't allocate image chunk memoty\n");
    }

    g_img_chunk_list.max_chunk = DEFAULT_CHUNK_NUM;
    g_img_chunk_list.cur_chunk = 0;

    start = file->device->disk->partition->start;

    ventoy_get_block_list(file, &g_img_chunk_list, start);

    rc = ventoy_check_block_list(file, &g_img_chunk_list, start, errmsg, sizeof(errmsg));
    grub_file_close(file);

    if (rc)
    {
        if (fs_type == ventoy_fs_btrfs)
        {
            vtoy_tip(10, "%s\n\nWill exit in 10 seconds...\n", errmsg);
            grub_exit();
        }
        return grub_error(GRUB_ERR_NOT_IMPLEMENTED_YET, "%s\n", errmsg);
    }

    grub_memset(&g_grub_param->file_replace, 0, sizeof(g_grub_param->file_replace));
    grub_memset(&g_grub_param->img_replace, 0, sizeof(g_grub_param->img_replace));
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

