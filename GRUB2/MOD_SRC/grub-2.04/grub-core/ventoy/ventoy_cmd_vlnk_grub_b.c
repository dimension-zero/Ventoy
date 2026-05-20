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
grub_err_t grub_cmd_gptpriority(grub_extcmd_context_t ctxt, int argc, char **args)
{
  grub_disk_t disk;
  grub_partition_t part;
  char priority_str[3]; /* Maximum value 15 */

  (void)ctxt;

  if (argc < 2 || argc > 3)
    return grub_error (GRUB_ERR_BAD_ARGUMENT,
                       "gptpriority DISKNAME PARTITIONNUM [VARNAME]");

  /* Open the disk if it exists */
  disk = grub_disk_open (args[0]);
  if (!disk)
    {
      return grub_error (GRUB_ERR_BAD_ARGUMENT,
                         "Not a disk");
    }

  part = grub_partition_probe (disk, args[1]);
  if (!part)
    {
      grub_disk_close (disk);
      return grub_error (GRUB_ERR_BAD_ARGUMENT,
                         "No such partition");
    }

  if (grub_strcmp (part->partmap->name, "gpt"))
    {
      grub_disk_close (disk);
      return grub_error (GRUB_ERR_BAD_PART_TABLE,
                         "Not a GPT partition");
    }

  grub_snprintf (priority_str, sizeof(priority_str), "%u",
                 (grub_uint32_t)((part->gpt_attrib >> 48) & 0xfULL));

  if (argc == 3)
    {
      grub_env_set (args[2], priority_str);
      grub_env_export (args[2]);
    }
  else
    {
      grub_printf ("Priority is %s\n", priority_str);
    }

  grub_disk_close (disk);
  return GRUB_ERR_NONE;
}


grub_err_t grub_cmd_syslinux_nojoliet(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int ret = 1;
    int joliet = 0;
    grub_file_t file = NULL;
    grub_uint32_t loadrba = 0;
    grub_uint32_t boot_catlog = 0;
    grub_uint8_t sector[512];
    boot_info_table *info = NULL;

    (void)ctxt;
    (void)argc;

    /* This also trigger a iso9660 fs parse */
    if (ventoy_check_file_exist("(loop)/isolinux/isolinux.cfg"))
    {
        return 0;
    }

    joliet = grub_iso9660_is_joliet();
    if (joliet == 0)
    {
        return 1;
    }

    file = grub_file_open(args[0], VENTOY_FILE_TYPE);
    if (!file)
    {
        debug("failed to open %s\n", args[0]);
        return 1;
    }

    boot_catlog = ventoy_get_iso_boot_catlog(file);
    if (boot_catlog == 0)
    {
        debug("no bootcatlog found %u\n", boot_catlog);
        goto out;
    }

    loadrba = ventoy_get_bios_eltorito_rba(file, boot_catlog);
    if (loadrba == 0)
    {
        debug("no bios eltorito rba found %u\n", loadrba);
        goto out;
    }

    grub_file_seek(file, loadrba * 2048);
    grub_file_read(file, sector, 512);

    info = (boot_info_table *)sector;
    if (info->bi_data0 == 0x7c6ceafa &&
        info->bi_data1 == 0x90900000 &&
        info->bi_PrimaryVolumeDescriptor == 16 &&
        info->bi_BootFileLocation == loadrba)
    {
        debug("bootloader is syslinux, %u.\n", loadrba);
        ret = 0;
    }

out:

    grub_file_close(file);
    grub_errno = GRUB_ERR_NONE;
    return ret;
}

grub_err_t grub_cmd_vlnk_dump_part(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int n = 0;
    ventoy_vlnk_part *node;

    (void)ctxt;
    (void)argc;
    (void)args;

    for (node = g_vlnk_part_list; node; node = node->next)
    {
        grub_printf("[%d] %s  disksig:%08x  offset:%llu  fs:%s\n",
                    ++n, node->device, node->disksig,
                    (ulonglong)node->partoffset, (node->fs ? node->fs->name : "N/A"));
    }

    return 0;
}

grub_err_t grub_cmd_is_vlnk_name(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int len = 0;

    (void)ctxt;

    if (argc == 1)
    {
        len = (int)grub_strlen(args[0]);
        if (grub_file_is_vlnk_suffix(args[0], len))
        {
            return 0;
        }
    }

    return 1;
}

grub_err_t grub_cmd_get_vlnk_dst(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int vlnk = 0;
    const char *name = NULL;

    (void)ctxt;

    if (argc == 2)
    {
        grub_env_unset(args[1]);
        name = grub_file_get_vlnk(args[0], &vlnk);
        if (vlnk)
        {
            debug("VLNK SRC: <%s>\n", args[0]);
            debug("VLNK DST: <%s>\n", name);
            grub_env_set(args[1], name);
            return 0;
        }
    }

    return 1;
}

grub_err_t grub_cmd_check_vlnk(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int ret = 1;
    int len = 0;
    grub_file_t file = NULL;
    ventoy_vlnk vlnk;
    char dst[512];

    (void)ctxt;

    if (argc != 1)
    {
        goto out;
    }

    len = (int)grub_strlen(args[0]);
    if (!grub_file_is_vlnk_suffix(args[0], len))
    {
        grub_printf("Invalid vlnk suffix\n");
        goto out;
    }

    file = grub_file_open(args[0], VENTOY_FILE_TYPE | GRUB_FILE_TYPE_NO_VLNK);
    if (!file)
    {
        grub_printf("Failed to open %s\n", args[0]);
        goto out;
    }

    if (file->size != 32768)
    {
        grub_printf("Invalid vlnk file (size=%llu).\n", (ulonglong)file->size);
        goto out;
    }

    grub_memset(&vlnk, 0, sizeof(vlnk));
    grub_file_read(file, &vlnk, sizeof(vlnk));

    ret = ventoy_check_vlnk_data(&vlnk, 1, dst, sizeof(dst));

out:

    grub_refresh();
    check_free(file, grub_file_close);
    grub_errno = GRUB_ERR_NONE;
    return ret;
}

