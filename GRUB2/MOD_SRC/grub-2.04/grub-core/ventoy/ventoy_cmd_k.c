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

    (void)args;

#ifdef GRUB_MACHINE_EFI
    grub_efi_get_reserved_page_num(&total, &org_required, &new_required);
    grub_printf("Total pages: %llu\n", (unsigned long long)total);
    grub_printf("OrgReq pages: %llu\n", (unsigned long long)org_required);
    grub_printf("NewReq pages: %llu\n", (unsigned long long)new_required);
#else
    (void)total;
    (void)org_required;
    (void)new_required;
    grub_printf("Non EFI mode!\n");
#endif

    grub_refresh();

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_need_secondary_menu(grub_extcmd_context_t ctxt, int argc, char **args)
{
    const char *env = NULL;

    (void)ctxt;
    (void)argc;

    if (g_ventoy_memdisk_mode || g_ventoy_grub2_mode || g_ventoy_wimboot_mode || g_ventoy_iso_raw)
    {
        return 1;
    }

    if (ventoy_check_mode_by_name(args[0], "vtgrub2") ||
        ventoy_check_mode_by_name(args[0], "vtwimboot") ||
        ventoy_check_mode_by_name(args[0], "vtmemdisk") ||
        ventoy_check_mode_by_name(args[0], "vtnormal")
        )
    {
        return 1;
    }

    env = grub_env_get("VTOY_SECONDARY_BOOT_MENU");
    if (env && env[0] == '0' && env[1] == 0)
    {
        return 1;
    }

    return 0;
}

grub_err_t ventoy_cmd_show_secondary_menu(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int n = 0;
    int pos = 0;
    int len = 0;
    int select = 0;
    int timeout = 0;
    char *cmd = NULL;
    const char *env = NULL;
    ulonglong fsize = 0;
    char cfgfile[128];
    int seldata[16] = {0};

    (void)ctxt;
    (void)argc;

    len = 8 * VTOY_SIZE_1KB;
    cmd = (char *)grub_malloc(len);
    if (!cmd)
    {
        return 1;
    }

    g_vtoy_secondary_need_recover = 0;
    grub_env_unset("VTOY_SECOND_EXIT");
    grub_env_unset("VTOY_CHKSUM_FILE_PATH");

    env = grub_env_get("VTOY_SECONDARY_TIMEOUT");
    if (env)
    {
        timeout = (int)grub_strtol(env, NULL, 10);
    }

    if (timeout > 0)
    {
        vtoy_len_ssprintf(cmd, pos, len, "set timeout=%d\n", timeout);
    }

    fsize = grub_strtoull(args[2], NULL, 10);

    vtoy_dummy_menuentry(cmd, pos, len, "$VTLANG_NORMAL_MODE", "second_normal"); seldata[n++] = 1;

    if (grub_strcmp(args[1], "Unix") != 0)
    {
        if (grub_strcmp(args[1], "Windows") == 0)
        {
            vtoy_dummy_menuentry(cmd, pos, len, "$VTLANG_WIMBOOT_MODE", "second_wimboot"); seldata[n++] = 2;
        }
        else
        {
            vtoy_dummy_menuentry(cmd, pos, len, "$VTLANG_GRUB2_MODE", "second_grub2"); seldata[n++] = 3;
        }

        if (fsize <= VTOY_SIZE_1GB)
        {
            vtoy_dummy_menuentry(cmd, pos, len, "$VTLANG_MEMDISK_MODE", "second_memdisk"); seldata[n++] = 4;
        }
    }

    vtoy_dummy_menuentry(cmd, pos, len, "$VTLANG_FILE_CHKSUM", "second_checksum"); seldata[n++] = 5;
    vtoy_dummy_menuentry(cmd, pos, len, "$VTLANG_RETURN_PRV_NOESC", "second_return"); seldata[n++] = 6;

    do {
        grub_errno = GRUB_ERR_NONE;
        g_ventoy_menu_esc = 1;
        g_ventoy_suppress_esc = 1;
        g_ventoy_suppress_esc_default = 0;
        g_ventoy_secondary_menu_on = 1;
        grub_snprintf(cfgfile, sizeof(cfgfile), "configfile mem:0x%llx:size:%d", (ulonglong)(ulong)cmd, pos);
        grub_script_execute_sourcecode(cfgfile);
        g_ventoy_menu_esc = 0;
        g_ventoy_suppress_esc = 0;
        g_ventoy_suppress_esc_default = 1;
        g_ventoy_secondary_menu_on = 0;

        select = seldata[g_ventoy_last_entry];

        if (select == 2)
        {
            g_ventoy_wimboot_mode = 1;
            g_vtoy_secondary_need_recover = 1;
        }
        else if (select == 3)
        {
            g_ventoy_grub2_mode = 1;
            g_vtoy_secondary_need_recover = 2;
        }
        else if (select == 4)
        {
            g_ventoy_memdisk_mode = 1;
            g_vtoy_secondary_need_recover = 3;
        }
        else if (select == 5)
        {
            grub_env_set("VTOY_CHKSUM_FILE_PATH", args[0]);
            grub_script_execute_sourcecode("configfile $vtoy_efi_part/grub/checksum.cfg");
        }
        else if (select == 6)
        {
            grub_env_set("VTOY_SECOND_EXIT", "1");
        }
    }while (select == 5);

    grub_free(cmd);
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_secondary_recover_mode(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    if (g_vtoy_secondary_need_recover == 1)
    {
        g_ventoy_wimboot_mode = 0;
    }
    else if (g_vtoy_secondary_need_recover == 2)
    {
        g_ventoy_grub2_mode = 0;
    }
    else if (g_vtoy_secondary_need_recover == 3)
    {
        g_ventoy_memdisk_mode = 0;
    }

    g_vtoy_secondary_need_recover = 0;

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_fs_ignore_case(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;

    if (args[0][0] == '0')
    {
        g_ventoy_case_insensitive = 0;
    }
    else
    {
        g_ventoy_case_insensitive = 1;
    }

    return 0;
}

grub_err_t ventoy_cmd_init_menu_lang(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;

    ventoy_plugin_load_menu_lang(1, args[0]);
    VENTOY_CMD_RETURN(0);
}

grub_err_t ventoy_cmd_load_menu_lang(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;

    ventoy_plugin_load_menu_lang(0, args[0]);
    VENTOY_CMD_RETURN(0);
}

int ventoy_chksum_pathcmp(int chktype, char *rlpath, char *rdpath)
{
    char *pos1 = NULL;
    char *pos2 = NULL;

    if (chktype == 2)
    {
        pos1 = ventoy_str_basename(rlpath);
        pos2 = ventoy_str_basename(rdpath);
        return grub_strcmp(pos1, pos2);
    }
    else if (chktype == 3 || chktype == 4)
    {
        if (grub_strcmp(rlpath, rdpath) == 0 || grub_strcmp(rlpath + 1, rdpath) == 0)
        {
            return 0;
        }
    }

    return 1;
}

int ventoy_find_checksum
(
    grub_file_t file,
    const char *uname,
    int retlen,
    char *path,
    int chktype,
    char *chksum
)
{
    int ulen;
    char *pos = NULL;
    char *pos1 = NULL;
    char *pos2 = NULL;
    char *buf = NULL;
    char *currline = NULL;
    char *nextline = NULL;

    ulen = (int)grub_strlen(uname);

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

        if (grub_strncasecmp(currline, uname, ulen) == 0)
        {
            pos = grub_strchr(currline, '=');
            pos1 = grub_strchr(currline, '(');
            pos2 = grub_strchr(currline, ')');

            if (pos && pos1 && pos2)
            {
                *pos2 = 0;
                if (ventoy_chksum_pathcmp(chktype, path, pos1 + 1) == 0)
                {
                    VTOY_SKIP_SPACE_NEXT(pos, 1);
                    grub_memcpy(chksum, pos, retlen);
                    goto end;
                }
            }
        }
        else if (ventoy_str_len_alnum(currline, retlen))
        {
            VTOY_SKIP_SPACE_NEXT_EX(pos, currline, retlen);
            if (ventoy_chksum_pathcmp(chktype, path, pos) == 0)
            {
                grub_memcpy(chksum, currline, retlen);
                goto end;
            }
        }
    }

end:
    grub_free(buf);
    return 0;
}

int ventoy_check_chkfile(const char *isopart, char *path, const char *lchkname, grub_file_t *pfile)
{
    int ret = 0;
    int cnt = 0;
    char c = 0;
    char *pos = NULL;
    grub_file_t file = NULL;

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s.%s", isopart, path, lchkname);
    if (file)
    {
        VTOY_GOTO_END(1);
    }

    cnt = ventoy_str_chrcnt(path, '/');
    if (cnt > 1)
    {
        pos = grub_strrchr(path, '/');
        c = *pos;
        *pos = 0;

        file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s/VENTOY_CHECKSUM", isopart, path);
        if (file)
        {
            *pos = c;
            VTOY_GOTO_END(2);
        }
        *pos = c;
    }

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s/VENTOY_CHECKSUM", isopart);
    if (file)
    {
        ret = (cnt > 1) ? 3 : 4;
    }

end:

    if (pfile)
    {
        *pfile = file;
    }
    else
    {
        check_free(file, grub_file_close);
    }
    return ret;
}

grub_err_t ventoy_cmd_cmp_checksum(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int index = 0;
    int chktype = 0;
    char *pos = NULL;
    grub_file_t file = NULL;
    const char *calc_value = NULL;
    const char *isopart = NULL;
    char fchksum[64];
    char readchk[256] = {0};
    char filebuf[512] = {0};
    char uchkname[16];

    (void)ctxt;

    index = (int)grub_strtol(args[0], NULL, 10);
    if (argc != 2 || index < 0 || index >= VTOY_CHKSUM_NUM)
    {
        return 1;
    }

    grub_strncpy(uchkname, g_lower_chksum_name[index], sizeof(uchkname));
    ventoy_str_toupper(uchkname);

    isopart = grub_env_get("vtoy_iso_part");
    calc_value = grub_env_get("VT_LAST_CHECK_SUM");

    chktype = ventoy_check_chkfile(isopart, args[1], g_lower_chksum_name[index], &file);
    if (chktype <= 0)
    {
        grub_printf("\n\nNo checksum file found.\n");
        goto end;
    }

    if (chktype == 1)
    {
        grub_snprintf(fchksum, sizeof(fchksum), ".%s", g_lower_chksum_name[index]);
        grub_memset(filebuf, 0, sizeof(filebuf));
        grub_file_read(file, filebuf, 511);

        pos = grub_strchr(filebuf, '=');
        if (pos)
        {
            VTOY_SKIP_SPACE_NEXT(pos, 1);
            grub_memcpy(readchk, pos, g_chksum_retlen[index]);
        }
