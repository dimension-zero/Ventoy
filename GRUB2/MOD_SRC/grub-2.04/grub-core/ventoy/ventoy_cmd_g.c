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


        replace->new_file_virtual_id = (grub_uint32_t)grub_strtoul(args[0], NULL, 10);
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_get_replace_file_cnt(grub_extcmd_context_t ctxt, int argc, char **args)
{
    char buf[32];
    ventoy_grub_param_file_replace *replace = &(g_grub_param->file_replace);

    (void)ctxt;

    if (argc >= 1)
    {
        grub_snprintf(buf, sizeof(buf), "%u", replace->old_name_cnt);
        grub_env_set(args[0], buf);
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_dump_menu(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc == 0)
    {
        grub_printf("List Mode: CurLen:%d  MaxLen:%u\n", g_list_script_pos, VTOY_MAX_SCRIPT_BUF);
        grub_printf("%s", g_list_script_buf);
    }
    else
    {
        grub_printf("Tree Mode: CurLen:%d  MaxLen:%u\n", g_tree_script_pos, VTOY_MAX_SCRIPT_BUF);
        grub_printf("%s", g_tree_script_buf);
    }

    return 0;
}

grub_err_t ventoy_cmd_dump_img_list(grub_extcmd_context_t ctxt, int argc, char **args)
{
    img_info *cur = g_ventoy_img_list;

    (void)ctxt;
    (void)argc;
    (void)args;

    while (cur)
    {
        grub_printf("path:<%s> id=%d list_index=%d\n", cur->path, cur->id, cur->plugin_list_index);
        grub_printf("name:<%s>\n\n", cur->name);
        cur = cur->next;
    }

    return 0;
}

grub_err_t ventoy_cmd_dump_injection(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    ventoy_plugin_dump_injection();

    return 0;
}

grub_err_t ventoy_cmd_dump_auto_install(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    ventoy_plugin_dump_auto_install();

    return 0;
}

grub_err_t ventoy_cmd_dump_persistence(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    ventoy_plugin_dump_persistence();

    return 0;
}

int ventoy_check_mode_by_name(char *filename, const char *suffix)
{
    int i;
    int len1;
    int len2;

    len1 = (int)grub_strlen(filename);
    len2 = (int)grub_strlen(suffix);

    if (len1 <= len2)
    {
        return 0;
    }

    for (i = len1 - 1; i >= 0; i--)
    {
        if (filename[i] == '.')
        {
            break;
        }
    }

    if (i < len2 + 1)
    {
        return 0;
    }

    if (filename[i - len2 - 1] != '_')
    {
        return 0;
    }

    if (grub_strncasecmp(filename + (i - len2), suffix, len2) == 0)
    {
        return 1;
    }

    return 0;
}

grub_err_t ventoy_cmd_check_mode(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc != 1 && argc != 2)
    {
        return 1;
    }

    if (args[0][0] == '0')
    {
        if (g_ventoy_memdisk_mode)
        {
            return 0;
        }

        if (argc == 2 && ventoy_check_mode_by_name(args[1], "vtmemdisk"))
        {
            return 0;
        }

        return 1;
    }
    else if (args[0][0] == '1')
    {
        return g_ventoy_iso_raw ? 0 : 1;
    }
    else if (args[0][0] == '2')
    {
        return g_ventoy_iso_uefi_drv ? 0 : 1;
    }
    else if (args[0][0] == '3')
    {
        if (g_ventoy_grub2_mode)
        {
            return 0;
        }

        if (argc == 2 && ventoy_check_mode_by_name(args[1], "vtgrub2"))
        {
            return 0;
        }

        return 1;
    }
    else if (args[0][0] == '4')
    {
        if (g_ventoy_wimboot_mode)
        {
            return 0;
        }

        if (argc == 2 && ventoy_check_mode_by_name(args[1], "vtwimboot"))
        {
            return 0;
        }

        return 1;
    }

    return 1;
}

grub_err_t ventoy_cmd_dynamic_menu(grub_extcmd_context_t ctxt, int argc, char **args)
{
    static int configfile_mode = 0;
    char memfile[128] = {0};

    (void)ctxt;
    (void)argc;
    (void)args;

    /*
     * args[0]:  0:normal     1:configfile
     * args[1]:  0:list_buf   1:tree_buf
     */

    if (argc != 2)
    {
        debug("Invalid argc %d\n", argc);
        return 0;
    }

    VTOY_CMD_CHECK(1);

    if (args[0][0] == '0')
    {
        if (args[1][0] == '0')
        {
            grub_script_execute_sourcecode(g_list_script_buf);
        }
        else
        {
            grub_script_execute_sourcecode(g_tree_script_buf);
        }
    }
    else
    {
        if (configfile_mode)
        {
            debug("Now already in F3 mode %d\n", configfile_mode);
            return 0;
        }

        if (args[1][0] == '0')
        {
            grub_snprintf(memfile, sizeof(memfile), "configfile mem:0x%llx:size:%d",
                (ulonglong)(ulong)g_list_script_buf, g_list_script_pos);
        }
        else
        {
             g_ventoy_last_entry = -1;
            grub_snprintf(memfile, sizeof(memfile), "configfile mem:0x%llx:size:%d",
                (ulonglong)(ulong)g_tree_script_buf, g_tree_script_pos);
        }

        configfile_mode = 1;
        grub_script_execute_sourcecode(memfile);
        configfile_mode = 0;
    }

    return 0;
}

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

