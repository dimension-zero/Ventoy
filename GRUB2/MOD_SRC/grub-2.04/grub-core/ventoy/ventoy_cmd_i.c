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

    }

    rc = 0;

end:

    check_free(device_name, grub_free);
    check_free(dev, grub_device_close);

    return rc;
}

int ventoy_fs_enum_1st_file(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    if (!info->dir)
    {
        grub_snprintf((char *)data, 256, "%s", filename);
        return 1;
    }

    return 0;
}

int ventoy_fs_enum_1st_dir(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    if (info->dir && filename && filename[0] != '.')
    {
        grub_snprintf((char *)data, 256, "%s", filename);
        return 1;
    }

    return 0;
}

grub_err_t ventoy_fs_enum_1st_child(int argc, char **args, grub_fs_dir_hook_t hook)
{
    int rc = 1;
    char *device_name = NULL;
    grub_device_t dev = NULL;
    grub_fs_t fs = NULL;
    char name[256] ={0};

    if (argc != 3)
    {
        debug("ventoy_fs_enum_1st_child, invalid param num %d\n", argc);
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
    if (!fs)
    {
        debug("grub_fs_probe failed, %s\n", device_name);
        goto end;
    }

    fs->fs_dir(dev, args[1], hook, name);
    if (name[0])
    {
        ventoy_set_env(args[2], name);
    }

    rc = 0;

end:

    check_free(device_name, grub_free);
    check_free(dev, grub_device_close);

    return rc;
}

grub_err_t ventoy_cmd_fs_enum_1st_file(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    return ventoy_fs_enum_1st_child(argc, args, ventoy_fs_enum_1st_file);
}

grub_err_t ventoy_cmd_fs_enum_1st_dir(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    return ventoy_fs_enum_1st_child(argc, args, ventoy_fs_enum_1st_dir);
}

grub_err_t ventoy_cmd_basename(grub_extcmd_context_t ctxt, int argc, char **args)
{
    char c;
    char *pos = NULL;
    char *end = NULL;

    (void)ctxt;

    if (argc != 2)
    {
        debug("ventoy_cmd_basename, invalid param num %d\n", argc);
        return 1;
    }

    for (pos = args[0]; *pos; pos++)
    {
        if (*pos == '.')
        {
            end = pos;
        }
    }

    if (end)
    {
        c = *end;
        *end = 0;
    }

    grub_env_set(args[1], args[0]);

    if (end)
    {
        *end = c;
    }

    return 0;
}

grub_err_t ventoy_cmd_basefile(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    int len;
    const char *buf;

    (void)ctxt;

    if (argc != 2)
    {
        debug("ventoy_cmd_basefile, invalid param num %d\n", argc);
        return 1;
    }

    buf = args[0];
    len = (int)grub_strlen(buf);
    for (i = len; i > 0; i--)
    {
        if (buf[i - 1] == '/')
        {
            grub_env_set(args[1], buf + i);
            return 0;
        }
    }

    grub_env_set(args[1], buf);

    return 0;
}

grub_err_t ventoy_cmd_enum_video_mode(grub_extcmd_context_t ctxt, int argc, char **args)
{
    struct grub_video_mode_info info;
    char buf[32];

    (void)ctxt;
    (void)argc;
    (void)args;

    if (!g_video_mode_list)
    {
        ventoy_enum_video_mode();
    }

    if (grub_video_get_info(&info) == GRUB_ERR_NONE)
    {
        grub_snprintf(buf, sizeof(buf), "Resolution (%ux%u)", info.width, info.height);
    }
    else
    {
        grub_snprintf(buf, sizeof(buf), "Resolution (0x0)");
    }

    grub_env_set("VTOY_CUR_VIDEO_MODE", buf);

    grub_snprintf(buf, sizeof(buf), "%d", g_video_mode_num);
    grub_env_set("VTOY_VIDEO_MODE_NUM", buf);

    VENTOY_CMD_RETURN(0);
}

grub_err_t vt_cmd_update_cur_video_mode(grub_extcmd_context_t ctxt, int argc, char **args)
{
    struct grub_video_mode_info info;
    char buf[32];

    (void)ctxt;
    (void)argc;
    (void)args;

    if (grub_video_get_info(&info) == GRUB_ERR_NONE)
    {
        grub_snprintf(buf, sizeof(buf), "%ux%ux%u", info.width, info.height, info.bpp);
    }
    else
    {
        grub_snprintf(buf, sizeof(buf), "0x0x0");
    }

    grub_env_set(args[0], buf);

    VENTOY_CMD_RETURN(0);
}

grub_err_t ventoy_cmd_get_video_mode(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int id;
    char buf[32];

    (void)ctxt;
    (void)argc;

    if (!g_video_mode_list)
    {
        return 0;
    }

    id = (int)grub_strtoul(args[0], NULL, 10);
    if (id < g_video_mode_num)
    {
        grub_snprintf(buf, sizeof(buf), "%ux%ux%u",
            g_video_mode_list[id].width, g_video_mode_list[id].height, g_video_mode_list[id].bpp);
    }

    grub_env_set(args[1], buf);

    VENTOY_CMD_RETURN(0);
}

grub_err_t ventoy_cmd_get_efivdisk_offset(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_uint32_t i;
    grub_uint32_t loadsector = 0;
    grub_file_t file;
    char value[32];
    grub_uint32_t boot_catlog = 0;
    grub_uint8_t buf[512];

    (void)ctxt;

    if (argc != 2)
    {
        debug("ventoy_cmd_get_efivdisk_offset, invalid param num %d\n", argc);
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
        debug("No bootcatlog found\n");
        grub_file_close(file);
        return 1;
    }

    grub_memset(buf, 0, sizeof(buf));
    grub_file_seek(file, boot_catlog * 2048);
    grub_file_read(file, buf, sizeof(buf));
    grub_file_close(file);

    for (i = 0; i < sizeof(buf); i += 32)
    {
        if ((buf[i] == 0 || buf[i] == 0x90 || buf[i] == 0x91) && buf[i + 1] == 0xEF)
        {
            if (buf[i + 32] == 0x88)
            {
                loadsector = *(grub_uint32_t *)(buf + i + 32 + 8);
                grub_snprintf(value, sizeof(value), "%u", loadsector * 4); //change to sector size 512
                break;
            }
        }
    }

    if (loadsector == 0)
    {
        debug("No EFI eltorito info found\n");
        return 1;
    }

    debug("ventoy_cmd_get_efivdisk_offset <%s>\n", value);
    grub_env_set(args[1], value);
    VENTOY_CMD_RETURN(0);
}

int ventoy_collect_replace_initrd(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    int curpos;
    int printlen;
    grub_size_t len;
    replace_fs_dir *pfsdir = (replace_fs_dir *)data;

    if (pfsdir->initrd[0])
    {
        return 1;
    }

    curpos = pfsdir->curpos;
    len = grub_strlen(filename);

    if (info->dir)
    {
        if ((len == 1 && filename[0] == '.') ||
            (len == 2 && filename[0] == '.' && filename[1] == '.'))
        {
            return 0;
        }

        //debug("#### [DIR] <%s> <%s>\n", pfsdir->fullpath, filename);
        pfsdir->dircnt++;

        printlen = grub_snprintf(pfsdir->fullpath + curpos, 512 - curpos, "%s/", filename);
        pfsdir->curpos = curpos + printlen;
        pfsdir->fs->fs_dir(pfsdir->dev, pfsdir->fullpath, ventoy_collect_replace_initrd, pfsdir);
        pfsdir->curpos = curpos;
        pfsdir->fullpath[curpos] = 0;
    }
    else
    {
        //debug("#### [FILE] <%s> <%s>\n", pfsdir->fullpath, filename);
        pfsdir->filecnt++;

        /* We consider the xxx.img file bigger than 32MB is the initramfs file */
        if (len > 4 && grub_strncmp(filename + len - 4, ".img", 4) == 0)
        {
            if (info->size > 32 * VTOY_SIZE_1MB)
            {
                grub_snprintf(pfsdir->initrd, sizeof(pfsdir->initrd), "%s%s", pfsdir->fullpath, filename);
                return 1;
            }
        }
    }

    return 0;
}

grub_err_t ventoy_cmd_search_replace_initrd(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    char *pos = NULL;
    char *device_name = NULL;
    grub_device_t dev = NULL;
    grub_fs_t fs = NULL;
    replace_fs_dir *pfsdir = NULL;

    (void)ctxt;

    if (argc != 2)
    {
        debug("ventoy_cmd_search_replace_initrd, invalid param num %d\n", argc);
        return 1;
    }

    pfsdir = grub_zalloc(sizeof(replace_fs_dir));
    if (!pfsdir)
    {
        return 1;
    }

    device_name = grub_file_get_device_name(args[0]);
    if (!device_name)
    {
        goto fail;
    }

    dev = grub_device_open(device_name);
    if (!dev)
    {
        goto fail;
    }

    fs = grub_fs_probe(dev);
    if (!fs)
    {
        goto fail;
    }

    pfsdir->dev = dev;
    pfsdir->fs = fs;
    pfsdir->curpos = 1;
    pfsdir->fullpath[0] = '/';
    fs->fs_dir(dev, "/", ventoy_collect_replace_initrd, pfsdir);

    if (pfsdir->initrd[0])
    {
        debug("Replace initrd <%s> <%d %d>\n", pfsdir->initrd, pfsdir->dircnt, pfsdir->filecnt);

        for (i = 0; i < (int)sizeof(pfsdir->initrd) && pfsdir->initrd[i]; i++)
        {
            if (pfsdir->initrd[i] == '/')
            {
                pfsdir->initrd[i] = '\\';
            }
        }

        pos = (pfsdir->initrd[0] == '\\') ? pfsdir->initrd + 1 : pfsdir->initrd;
        grub_env_set(args[1], pos);
    }
    else
    {
        debug("Replace initrd NOT found <%s> <%d %d>\n", args[0], pfsdir->dircnt, pfsdir->filecnt);
    }

fail:

    grub_check_free(pfsdir);
    grub_check_free(device_name);
    check_free(dev, grub_device_close);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_push_pager(grub_extcmd_context_t ctxt, int argc, char **args)
{
    const char *pager = NULL;

    (void)ctxt;
    (void)argc;
    (void)args;

    pager = grub_env_get("pager");
    if (NULL == pager)
    {
        g_pager_flag = 1;
        grub_env_set("pager", "1");
    }
    else if (pager[0] == '1')
    {
        g_pager_flag = 0;
    }
    else
    {
        grub_snprintf(g_old_pager, sizeof(g_old_pager), "%s", pager);
        g_pager_flag = 2;
        grub_env_set("pager", "1");
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_pop_pager(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    if (g_pager_flag == 1)
    {
        grub_env_unset("pager");
    }
    else if (g_pager_flag == 2)
    {
        grub_env_set("pager", g_old_pager);
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

int ventoy_chk_case_file(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    if (g_json_case_mis_path[0])
    {
        return 1;
    }

    if (0 == info->dir && grub_strcasecmp(filename, "ventoy.json") == 0)
    {
        grub_snprintf(g_json_case_mis_path, 32, "%s/%s", (char *)data, filename);
        return 1;
    }
    return 0;
}

int ventoy_chk_case_dir(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    char path[16];
    chk_case_fs_dir *fs_dir = (chk_case_fs_dir *)data;

    if (g_json_case_mis_path[0])
    {
