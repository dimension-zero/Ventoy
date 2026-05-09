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

GRUB_MOD_LICENSE ("GPLv3+");

grub_uint8_t g_check_mbr_data[] = {
    0xEB, 0x63, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x56, 0x54, 0x00, 0x47, 0x65, 0x00, 0x48, 0x44, 0x00, 0x52, 0x64, 0x00, 0x20, 0x45, 0x72, 0x0D,
};

initrd_info *g_initrd_img_list = NULL;
initrd_info *g_initrd_img_tail = NULL;
int g_initrd_img_count = 0;
int g_valid_initrd_count = 0;
int g_default_menu_mode = 0;
int g_filt_dot_underscore_file = 0;
int g_filt_trash_dir = 1;
int g_sort_case_sensitive = 0;
int g_tree_view_menu_style = 0;
grub_file_t g_old_file;
int g_ventoy_last_entry_back;

char g_iso_path[256];
char g_img_swap_tmp_buf[1024];
img_info g_img_swap_tmp;
img_info *g_ventoy_img_list = NULL;

int g_ventoy_img_count = 0;

grub_device_t g_enum_dev = NULL;
grub_fs_t g_enum_fs = NULL;
int g_img_max_search_level = -1;
img_iterator_node g_img_iterator_head;
img_iterator_node *g_img_iterator_tail = NULL;

grub_uint8_t g_ventoy_break_level = 0;
grub_uint8_t g_ventoy_debug_level = 0;
grub_uint8_t g_ventoy_chain_type = 0;

grub_uint8_t *g_ventoy_cpio_buf = NULL;
grub_uint32_t g_ventoy_cpio_size = 0;
cpio_newc_header *g_ventoy_initrd_head = NULL;
grub_uint8_t *g_ventoy_runtime_buf = NULL;

int g_plugin_image_list = 0;

ventoy_grub_param *g_grub_param = NULL;

ventoy_guid  g_ventoy_guid = VENTOY_GUID;

ventoy_img_chunk_list g_img_chunk_list;

int g_wimboot_enable = 0;
ventoy_img_chunk_list g_wimiso_chunk_list;
char *g_wimiso_path = NULL;
grub_uint32_t g_wimiso_size = 0;

int g_vhdboot_enable = 0;

grub_uint64_t g_svd_replace_offset = 0;

int g_conf_replace_count = 0;
grub_uint64_t g_conf_replace_offset[VTOY_MAX_CONF_REPLACE] = { 0 };
conf_replace *g_conf_replace_node[VTOY_MAX_CONF_REPLACE] = { NULL };
grub_uint8_t *g_conf_replace_new_buf[VTOY_MAX_CONF_REPLACE] = { NULL };
int g_conf_replace_new_len[VTOY_MAX_CONF_REPLACE] = { 0 };
int g_conf_replace_new_len_align[VTOY_MAX_CONF_REPLACE] = { 0 };

int g_ventoy_disk_bios_id = 0;
ventoy_gpt_info *g_ventoy_part_info = NULL;
grub_uint64_t g_ventoy_disk_size = 0;
grub_uint64_t g_ventoy_disk_part_size[2];

char *g_tree_script_buf = NULL;
int g_tree_script_pos = 0;
int g_tree_script_pre = 0;

char *g_list_script_buf = NULL;
int g_list_script_pos = 0;

char *g_part_list_buf = NULL;
int g_part_list_pos = 0;
grub_uint64_t g_part_end_max = 0;

int g_video_mode_max = 0;
int g_video_mode_num = 0;
ventoy_video_mode *g_video_mode_list = NULL;

int g_enumerate_time_checked = 0;
grub_uint64_t g_enumerate_start_time_ms;
grub_uint64_t g_enumerate_finish_time_ms;
int g_vtoy_file_flt[VTOY_FILE_FLT_BUTT] = {0};

char g_iso_vd_id_publisher[130];
char g_iso_vd_id_prepare[130];
char g_iso_vd_id_application[130];

int g_pager_flag = 0;
char g_old_pager[32];

const char *g_menu_class[img_type_max] =
{
    "vtoyiso", "vtoywim", "vtoyefi", "vtoyimg", "vtoyvhd", "vtoyvtoy"
};

const char *g_menu_prefix[img_type_max] =
{
    "iso", "wim", "efi", "img", "vhd", "vtoy"
};

const char *g_lower_chksum_name[VTOY_CHKSUM_NUM] = { "md5", "sha1", "sha256", "sha512" };
int g_lower_chksum_namelen[VTOY_CHKSUM_NUM] = { 3, 4, 6, 6 };
int g_chksum_retlen[VTOY_CHKSUM_NUM] = { 32, 40, 64, 128 };

int g_vtoy_secondary_need_recover = 0;

int g_vtoy_load_prompt = 0;
char g_vtoy_prompt_msg[64];

char g_json_case_mis_path[32];

ventoy_vlnk_part *g_vlnk_part_list = NULL;

        return 1;
    }

    if (info->dir && (filename[0] == 'v' || filename[0] == 'V'))
    {
        if (grub_strcasecmp(filename, "ventoy") == 0)
        {
            grub_snprintf(path, sizeof(path), "/%s", filename);
            fs_dir->fs->fs_dir(fs_dir->dev, path, ventoy_chk_case_file, path);
            if (g_json_case_mis_path[0])
            {
                return 1;
            }
        }
    }

    return 0;
}

grub_err_t ventoy_cmd_chk_json_pathcase(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int fstype = 0;
    char *device_name = NULL;
    grub_device_t dev = NULL;
    grub_fs_t fs = NULL;
    chk_case_fs_dir fs_dir;

    (void)ctxt;
    (void)argc;
    (void)args;

    device_name = grub_file_get_device_name(args[0]);
    if (!device_name)
    {
        goto out;
    }

    dev = grub_device_open(device_name);
    if (!dev)
    {
        goto out;
    }

    fs = grub_fs_probe(dev);
    if (!fs)
    {
        goto out;
    }

    fstype = ventoy_get_fs_type(fs->name);
    if (fstype == ventoy_fs_fat || fstype == ventoy_fs_exfat || fstype >= ventoy_fs_max)
    {
        goto out;
    }

    g_json_case_mis_path[0] = 0;
    fs_dir.dev = dev;
    fs_dir.fs = fs;
    fs->fs_dir(dev, "/", ventoy_chk_case_dir, &fs_dir);

    if (g_json_case_mis_path[0])
    {
        grub_env_set("VTOY_PLUGIN_PATH_CASE_MISMATCH", g_json_case_mis_path);
    }

out:

    grub_check_free(device_name);
    check_free(dev, grub_device_close);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

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
