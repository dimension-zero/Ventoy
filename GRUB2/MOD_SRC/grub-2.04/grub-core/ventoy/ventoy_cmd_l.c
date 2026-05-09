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

        else
        {
            grub_memcpy(readchk, filebuf, g_chksum_retlen[index]);
        }
    }
    else if (chktype == 3 || chktype == 4)
    {
        grub_snprintf(fchksum, sizeof(fchksum), "global VENTOY_CHECKSUM");
        ventoy_find_checksum(file, uchkname, g_chksum_retlen[index], args[1], chktype, readchk);
        if (readchk[0] == 0)
        {
            grub_printf("\n\n%s value not found in %s.\n", uchkname, fchksum);
            goto end;
        }
    }
    else
    {
        grub_snprintf(fchksum, sizeof(fchksum), "local VENTOY_CHECKSUM");
        ventoy_find_checksum(file, uchkname, g_chksum_retlen[index], args[1], chktype, readchk);
        if (readchk[0] == 0)
        {
            grub_file_close(file);
            file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s/VENTOY_CHECKSUM", isopart);
            if (file)
            {
                grub_snprintf(fchksum, sizeof(fchksum), "global VENTOY_CHECKSUM");
                ventoy_find_checksum(file, uchkname, g_chksum_retlen[index], args[1], 3, readchk);
                if (readchk[0] == 0)
                {
                    grub_printf("\n\n%s value not found in both local and global VENTOY_CHECKSUM.\n", uchkname);
                    goto end;
                }
            }
        }
    }

    if (grub_strcasecmp(calc_value, readchk) == 0)
    {
        grub_printf("\n\nCheck %s value with %s file.  [ SUCCESS ]\n", uchkname, fchksum);
    }
    else
    {
        grub_printf("\n\nCheck %s value with %s file.  [ ERROR ]\n", uchkname, fchksum);
        grub_printf("The %s value in %s file is:\n%s\n", uchkname, fchksum, readchk);
    }

end:
    grub_refresh();
    check_free(file, grub_file_close);
    VENTOY_CMD_RETURN(0);
}

int ventoy_find_all_checksum
(
    grub_file_t file,
    char *path,
    int chktype,
    int exists[VTOY_CHKSUM_NUM],
    int *ptotexist
)
{
    int i;
    int ulen;
    int tot = 0;
    char c = 0;
    char *pos = NULL;
    char *pos1 = NULL;
    char *pos2 = NULL;
    char *buf = NULL;
    char *currline = NULL;
    char *nextline = NULL;
    const char *uname = NULL;

    tot = *ptotexist;

    /* read file to buffer */
    buf = grub_malloc(file->size + 4);
    if (!buf)
    {
        return 1;
    }
    grub_file_read(file, buf, file->size);
    buf[file->size] = 0;

    /* parse each line */
    for (currline = buf; currline; currline = nextline)
    {
        nextline = ventoy_get_line(currline);
        VTOY_SKIP_SPACE(currline);

        for (i = 0; i < VTOY_CHKSUM_NUM; i++)
        {
            if (exists[i])
            {
                continue;
            }

            uname = g_lower_chksum_name[i];
            ulen = g_lower_chksum_namelen[i];

            if (grub_strncasecmp(currline, uname, ulen) == 0)
            {
                pos = grub_strchr(currline, '=');
                pos1 = grub_strchr(currline, '(');
                pos2 = grub_strchr(currline, ')');

                if (pos && pos1 && pos2)
                {
                    c = *pos2;
                    *pos2 = 0;
                    if (ventoy_chksum_pathcmp(chktype, path, pos1 + 1) == 0)
                    {
                        exists[i] = 1;
                        tot++;
                    }
                    *pos2 = c;
                }
            }
            else if (ventoy_str_len_alnum(currline, g_chksum_retlen[i]))
            {
                VTOY_SKIP_SPACE_NEXT_EX(pos, currline, g_chksum_retlen[i]);
                if (ventoy_chksum_pathcmp(chktype, path, pos) == 0)
                {
                    exists[i] = 1;
                    tot++;
                }
            }

            if (tot >= VTOY_CHKSUM_NUM)
            {
                goto end;
            }
        }
    }

end:

    *ptotexist = tot;
    grub_free(buf);
    return 0;
}

grub_err_t ventoy_cmd_vtoychksum_exist(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i = 0;
    int cnt = 0;
    char c = 0;
    int tip = 0;
    char *pos = NULL;
    grub_file_t file = NULL;
    const char *isopart = NULL;
    int exists[VTOY_CHKSUM_NUM] = { 0, 0, 0, 0 };
    int totexist = 0;

    (void)argc;
    (void)ctxt;

    isopart = grub_env_get("vtoy_iso_part");

    for (i = 0; i < VTOY_CHKSUM_NUM; i++)
    {
        if (ventoy_check_file_exist("%s%s.%s", isopart, args[0], g_lower_chksum_name[i]))
        {
            exists[i] = 1;
            totexist++;
        }
    }

    if (totexist == VTOY_CHKSUM_NUM)
    {
        goto end;
    }

    cnt = ventoy_str_chrcnt(args[0], '/');
    if (cnt > 1)
    {
        pos = grub_strrchr(args[0], '/');
        c = *pos;
        *pos = 0;
        file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s/VENTOY_CHECKSUM", isopart, args[0]);
        *pos = c;

        if (file)
        {
            if (tip == 0 && file->size > (32 * VTOY_SIZE_1KB))
            {
                tip = 1;
                grub_printf("Reading checksum file...\n");
                grub_refresh();
            }

            debug("parse local VENTOY_CHECKSUM\n");
            ventoy_find_all_checksum(file, args[0], 2, exists, &totexist);
            grub_file_close(file);
        }
    }

    if (totexist == VTOY_CHKSUM_NUM)
    {
        goto end;
    }

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s/VENTOY_CHECKSUM", isopart);
    if (file)
    {
        if (tip == 0 && file->size > (32 * VTOY_SIZE_1KB))
        {
            tip = 1;
            grub_printf("Reading checksum file...\n");
            grub_refresh();
        }

        debug("parse global VENTOY_CHECKSUM\n");
        ventoy_find_all_checksum(file, args[0], (cnt > 1) ? 3 : 4, exists, &totexist);
        grub_file_close(file);
    }

end:

    ventoy_env_int_set("VT_EXIST_MD5", exists[0]);
    ventoy_env_int_set("VT_EXIST_SHA1", exists[1]);
    ventoy_env_int_set("VT_EXIST_SHA256", exists[2]);
    ventoy_env_int_set("VT_EXIST_SHA512", exists[3]);

    VENTOY_CMD_RETURN(0);
}


const char * ventoy_menu_lang_read_hook(struct grub_env_var *var, const char *val)
{
    (void)var;
    return ventoy_get_vmenu_title(val);
}

