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

int ventoy_env_init(void)
{
    int i;
    char buf[64];

    grub_env_set("vtdebug_flag", "");

    grub_register_vtoy_menu_lang_hook(ventoy_menu_lang_read_hook);
    ventoy_ctrl_var_init();
    ventoy_global_var_init();

    g_part_list_buf = grub_malloc(VTOY_PART_BUF_LEN);
    g_tree_script_buf = grub_malloc(VTOY_MAX_SCRIPT_BUF);
    g_list_script_buf = grub_malloc(VTOY_MAX_SCRIPT_BUF);
    for (i = 0; i < VTOY_MAX_CONF_REPLACE; i++)
    {
        g_conf_replace_new_buf[i] = grub_malloc(vtoy_max_replace_file_size);
    }

    ventoy_filt_register(0, ventoy_wrapper_open);

    g_grub_param = (ventoy_grub_param *)grub_zalloc(sizeof(ventoy_grub_param));
    if (g_grub_param)
    {
        g_grub_param->grub_env_get = grub_env_get;
        g_grub_param->grub_env_set = (grub_env_set_pf)grub_env_set;
        g_grub_param->grub_env_printf = (grub_env_printf_pf)grub_printf;
        grub_snprintf(buf, sizeof(buf), "%p", g_grub_param);
        grub_env_set("env_param", buf);
        grub_env_set("ventoy_env_param", buf);

        grub_env_export("env_param");
        grub_env_export("ventoy_env_param");
    }

    grub_env_export("vtoy_winpeshl_ini_addr");
    grub_env_export("vtoy_winpeshl_ini_size");

    grub_snprintf(buf, sizeof(buf), "0x%lx", (ulong)ventoy_chain_file_size);
    grub_env_set("vtoy_chain_file_size", buf);
    grub_env_export("vtoy_chain_file_size");

    grub_snprintf(buf, sizeof(buf), "0x%lx", (ulong)ventoy_chain_file_read);
    grub_env_set("vtoy_chain_file_read", buf);
    grub_env_export("vtoy_chain_file_read");

    grub_snprintf(buf, sizeof(buf), "0x%lx", (ulong)ventoy_get_vmenu_title);
    grub_env_set("VTOY_VMENU_FUNC_ADDR", buf);
    grub_env_export("VTOY_VMENU_FUNC_ADDR");

    grub_snprintf(buf, sizeof(buf), "%s-%s", GRUB_TARGET_CPU, GRUB_PLATFORM);
    grub_env_set("grub_cpu_platform", buf);
    grub_env_export("grub_cpu_platform");

    return 0;
}



cmd_para ventoy_cmds[] =
{
    { "vt_browser_disk",  ventoy_cmd_browser_disk,  0, NULL, "",   "",    NULL },
    { "vt_browser_dir",  ventoy_cmd_browser_dir,  0, NULL, "",   "",    NULL },
    { "vt_incr",  ventoy_cmd_incr,  0, NULL, "{Var} {INT}",   "Increase integer variable",    NULL },
    { "vt_mod",  ventoy_cmd_mod,  0, NULL, "{Int} {Int} {Var}",   "mod integer variable",    NULL },
    { "vt_strstr",  ventoy_cmd_strstr,  0, NULL, "",   "",    NULL },
    { "vt_str_begin",  ventoy_cmd_strbegin,  0, NULL, "",   "",    NULL },
    { "vt_str_casebegin",  ventoy_cmd_strcasebegin,  0, NULL, "",   "",    NULL },
    { "vt_debug", ventoy_cmd_debug, 0, NULL, "{on|off}",   "turn debug on/off",    NULL },
    { "vtdebug", ventoy_cmd_debug, 0, NULL, "{on|off}",   "turn debug on/off",    NULL },
    { "vtbreak", ventoy_cmd_break, 0, NULL, "{level}",   "set debug break",    NULL },
    { "vt_cmp",   ventoy_cmd_cmp, 0, NULL, "{Int1} { eq|ne|gt|lt|ge|le } {Int2}", "Comare two integers", NULL },
    { "vt_device", ventoy_cmd_device, 0, NULL, "path var", "", NULL },
    { "vt_check_compatible",   ventoy_cmd_check_compatible, 0, NULL, "", "", NULL },
    { "vt_list_img", ventoy_cmd_list_img, 0, NULL, "{device} {cntvar}", "find all iso file in device", NULL },
    { "vt_clear_img", ventoy_cmd_clear_img, 0, NULL, "", "clear image list", NULL },
    { "vt_img_name", ventoy_cmd_img_name, 0, NULL, "{imageID} {var}", "get image name", NULL },
    { "vt_chosen_img_path", ventoy_cmd_chosen_img_path, 0, NULL, "{var}", "get chosen img path", NULL },
    { "vt_ext_select_img_path", ventoy_cmd_ext_select_img_path, 0, NULL, "{var}", "select chosen img path", NULL },
    { "vt_img_sector", ventoy_cmd_img_sector, 0, NULL, "{imageName}", "", NULL },
    { "vt_dump_img_sector", ventoy_cmd_dump_img_sector, 0, NULL, "", "", NULL },
    { "vt_load_wimboot", ventoy_cmd_load_wimboot, 0, NULL, "", "", NULL },
    { "vt_load_vhdboot", ventoy_cmd_load_vhdboot, 0, NULL, "", "", NULL },
    { "vt_patch_vhdboot", ventoy_cmd_patch_vhdboot, 0, NULL, "", "", NULL },
    { "vt_raw_chain_data", ventoy_cmd_raw_chain_data, 0, NULL, "", "", NULL },
    { "vt_get_vtoy_type", ventoy_cmd_get_vtoy_type, 0, NULL, "", "", NULL },
    { "vt_check_custom_boot", ventoy_cmd_check_custom_boot, 0, NULL, "", "", NULL },
    { "vt_dump_custom_boot", ventoy_cmd_dump_custom_boot, 0, NULL, "", "", NULL },

    { "vt_skip_svd", ventoy_cmd_skip_svd, 0, NULL, "", "", NULL },
    { "vt_cpio_busybox64", ventoy_cmd_cpio_busybox_64, 0, NULL, "", "", NULL },
    { "vt_load_cpio", ventoy_cmd_load_cpio, 0, NULL, "", "", NULL },
    { "vt_trailer_cpio", ventoy_cmd_trailer_cpio, 0, NULL, "", "", NULL },
    { "vt_push_last_entry", ventoy_cmd_push_last_entry, 0, NULL, "", "", NULL },
    { "vt_pop_last_entry", ventoy_cmd_pop_last_entry, 0, NULL, "", "", NULL },
    { "vt_get_lib_module_ver", ventoy_cmd_lib_module_ver, 0, NULL, "", "", NULL },

    { "vt_load_part_table", ventoy_cmd_load_part_table, 0, NULL, "", "", NULL },
    { "vt_check_part_exist", ventoy_cmd_part_exist, 0, NULL, "", "", NULL },
    { "vt_get_fs_label", ventoy_cmd_get_fs_label, 0, NULL, "", "", NULL },
    { "vt_fs_enum_1st_file", ventoy_cmd_fs_enum_1st_file, 0, NULL, "", "", NULL },
    { "vt_fs_enum_1st_dir", ventoy_cmd_fs_enum_1st_dir, 0, NULL, "", "", NULL },
    { "vt_file_basename", ventoy_cmd_basename, 0, NULL, "", "", NULL },
    { "vt_file_basefile", ventoy_cmd_basefile, 0, NULL, "", "", NULL },
    { "vt_enum_video_mode", ventoy_cmd_enum_video_mode, 0, NULL, "", "", NULL },
    { "vt_get_video_mode", ventoy_cmd_get_video_mode, 0, NULL, "", "", NULL },
    { "vt_update_cur_video_mode", vt_cmd_update_cur_video_mode, 0, NULL, "", "", NULL },


    { "vt_find_first_bootable_hd", ventoy_cmd_find_bootable_hdd, 0, NULL, "", "", NULL },
    { "vt_dump_menu", ventoy_cmd_dump_menu, 0, NULL, "", "", NULL },
    { "vt_dynamic_menu", ventoy_cmd_dynamic_menu, 0, NULL, "", "", NULL },
    { "vt_check_mode", ventoy_cmd_check_mode, 0, NULL, "", "", NULL },
    { "vt_dump_img_list", ventoy_cmd_dump_img_list, 0, NULL, "", "", NULL },
    { "vt_dump_injection", ventoy_cmd_dump_injection, 0, NULL, "", "", NULL },
    { "vt_dump_auto_install", ventoy_cmd_dump_auto_install, 0, NULL, "", "", NULL },
    { "vt_dump_persistence", ventoy_cmd_dump_persistence, 0, NULL, "", "", NULL },
    { "vt_select_auto_install", ventoy_cmd_sel_auto_install, 0, NULL, "", "", NULL },
    { "vt_select_persistence", ventoy_cmd_sel_persistence, 0, NULL, "", "", NULL },
    { "vt_select_conf_replace", ventoy_select_conf_replace, 0, NULL, "", "", NULL },

    { "vt_iso9660_nojoliet", ventoy_cmd_iso9660_nojoliet, 0, NULL, "", "", NULL },
    { "vt_iso9660_isjoliet", ventoy_cmd_iso9660_is_joliet, 0, NULL, "", "", NULL },
    { "vt_is_udf", ventoy_cmd_is_udf, 0, NULL, "", "", NULL },
    { "vt_file_size", ventoy_cmd_file_size, 0, NULL, "", "", NULL },
    { "vt_load_file_to_mem", ventoy_cmd_load_file_to_mem, 0, NULL, "", "", NULL },
    { "vt_load_img_memdisk", ventoy_cmd_load_img_memdisk, 0, NULL, "", "", NULL },
    { "vt_concat_efi_iso", ventoy_cmd_concat_efi_iso, 0, NULL, "", "", NULL },

    { "vt_linux_parse_initrd_isolinux", ventoy_cmd_isolinux_initrd_collect, 0, NULL, "{cfgfile}", "", NULL },
    { "vt_linux_parse_initrd_grub", ventoy_cmd_grub_initrd_collect, 0, NULL, "{cfgfile}", "", NULL },
    { "vt_linux_specify_initrd_file", ventoy_cmd_specify_initrd_file, 0, NULL, "", "", NULL },
    { "vt_linux_clear_initrd", ventoy_cmd_clear_initrd_list, 0, NULL, "", "", NULL },
    { "vt_linux_dump_initrd", ventoy_cmd_dump_initrd_list, 0, NULL, "", "", NULL },
    { "vt_linux_initrd_count", ventoy_cmd_initrd_count, 0, NULL, "", "", NULL },
    { "vt_linux_valid_initrd_count", ventoy_cmd_valid_initrd_count, 0, NULL, "", "", NULL },
    { "vt_linux_locate_initrd", ventoy_cmd_linux_locate_initrd, 0, NULL, "", "", NULL },
    { "vt_linux_chain_data", ventoy_cmd_linux_chain_data, 0, NULL, "", "", NULL },
    { "vt_linux_get_main_initrd_index", ventoy_cmd_linux_get_main_initrd_index, 0, NULL, "", "", NULL },

    { "vt_windows_reset",      ventoy_cmd_wimdows_reset, 0, NULL, "", "", NULL },
    { "vt_windows_chain_data", ventoy_cmd_windows_chain_data, 0, NULL, "", "", NULL },
    { "vt_windows_wimboot_data", ventoy_cmd_windows_wimboot_data, 0, NULL, "", "", NULL },
    { "vt_windows_collect_wim_patch", ventoy_cmd_collect_wim_patch, 0, NULL, "", "", NULL },
    { "vt_windows_locate_wim_patch", ventoy_cmd_locate_wim_patch, 0, NULL, "", "", NULL },
    { "vt_windows_count_wim_patch", ventoy_cmd_wim_patch_count, 0, NULL, "", "", NULL },
    { "vt_dump_wim_patch", ventoy_cmd_dump_wim_patch, 0, NULL, "", "", NULL },
    { "vt_wim_check_bootable", ventoy_cmd_wim_check_bootable, 0, NULL, "", "", NULL },
    { "vt_wim_chain_data", ventoy_cmd_wim_chain_data, 0, NULL, "", "", NULL },

    { "vt_add_replace_file", ventoy_cmd_add_replace_file, 0, NULL, "", "", NULL },
    { "vt_get_replace_file_cnt", ventoy_cmd_get_replace_file_cnt, 0, NULL, "", "", NULL },
    { "vt_test_block_list", ventoy_cmd_test_block_list, 0, NULL, "", "", NULL },
    { "vt_file_exist_nocase", ventoy_cmd_file_exist_nocase, 0, NULL, "", "", NULL },


    { "vt_load_plugin", ventoy_cmd_load_plugin, 0, NULL, "", "", NULL },
    { "vt_check_plugin_json", ventoy_cmd_plugin_check_json, 0, NULL, "", "", NULL },
    { "vt_check_password", ventoy_cmd_check_password, 0, NULL, "", "", NULL },

    { "vt_1st_line", ventoy_cmd_read_1st_line, 0, NULL, "", "", NULL },
    { "vt_file_strstr", ventoy_cmd_file_strstr, 0, NULL, "", "", NULL },
    { "vt_img_part_info", ventoy_cmd_img_part_info, 0, NULL, "", "", NULL },


    { "vt_parse_iso_volume", ventoy_cmd_parse_volume, 0, NULL, "", "", NULL },
    { "vt_parse_iso_create_date", ventoy_cmd_parse_create_date, 0, NULL, "", "", NULL },
    { "vt_parse_freenas_ver", ventoy_cmd_parse_freenas_ver, 0, NULL, "", "", NULL },
    { "vt_unix_parse_freebsd_ver", ventoy_cmd_unix_freebsd_ver, 0, NULL, "", "", NULL },
    { "vt_unix_parse_freebsd_ver_elf", ventoy_cmd_unix_freebsd_ver_elf, 0, NULL, "", "", NULL },
    { "vt_unix_reset", ventoy_cmd_unix_reset, 0, NULL, "", "", NULL },
    { "vt_unix_check_vlnk", ventoy_cmd_unix_check_vlnk, 0, NULL, "", "", NULL },
    { "vt_unix_replace_conf", ventoy_cmd_unix_replace_conf, 0, NULL, "", "", NULL },
    { "vt_unix_replace_grub_conf", ventoy_cmd_unix_replace_grub_conf, 0, NULL, "", "", NULL },
    { "vt_unix_replace_ko", ventoy_cmd_unix_replace_ko, 0, NULL, "", "", NULL },
    { "vt_unix_ko_fillmap", ventoy_cmd_unix_ko_fillmap, 0, NULL, "", "", NULL },
    { "vt_unix_fill_image_desc", ventoy_cmd_unix_fill_image_desc, 0, NULL, "", "", NULL },
    { "vt_unix_gzip_new_ko", ventoy_cmd_unix_gzip_newko, 0, NULL, "", "", NULL },
    { "vt_unix_chain_data", ventoy_cmd_unix_chain_data, 0, NULL, "", "", NULL },

    { "vt_img_hook_root", ventoy_cmd_img_hook_root, 0, NULL, "", "", NULL },
    { "vt_img_unhook_root", ventoy_cmd_img_unhook_root, 0, NULL, "", "", NULL },
    { "vt_acpi_param", ventoy_cmd_acpi_param, 0, NULL, "", "", NULL },
    { "vt_check_secureboot_var", ventoy_cmd_check_secureboot_var, 0, NULL, "", "", NULL },
    { "vt_clear_key", ventoy_cmd_clear_key, 0, NULL, "", "", NULL },
    { "vt_img_check_range", ventoy_cmd_img_check_range, 0, NULL, "", "", NULL },
    { "vt_is_pe64", ventoy_cmd_is_pe64, 0, NULL, "", "", NULL },
    { "vt_sel_wimboot", ventoy_cmd_sel_wimboot, 0, NULL, "", "", NULL },
    { "vt_set_wim_load_prompt", ventoy_cmd_set_wim_prompt, 0, NULL, "", "", NULL },
    { "vt_set_theme", ventoy_cmd_set_theme, 0, NULL, "", "", NULL },
    { "vt_set_theme_path", ventoy_cmd_set_theme_path, 0, NULL, "", "", NULL },
    { "vt_select_theme_cfg", ventoy_cmd_select_theme_cfg, 0, NULL, "", "", NULL },

    { "vt_get_efi_vdisk_offset", ventoy_cmd_get_efivdisk_offset, 0, NULL, "", "", NULL },
    { "vt_search_replace_initrd", ventoy_cmd_search_replace_initrd, 0, NULL, "", "", NULL },
    { "vt_push_pager", ventoy_cmd_push_pager, 0, NULL, "", "", NULL },
    { "vt_pop_pager", ventoy_cmd_pop_pager, 0, NULL, "", "", NULL },
    { "vt_check_json_path_case", ventoy_cmd_chk_json_pathcase, 0, NULL, "", "", NULL },
    { "vt_append_extra_sector", ventoy_cmd_append_ext_sector, 0, NULL, "", "", NULL },
    { "gptpriority", grub_cmd_gptpriority, 0, NULL, "", "", NULL },
    { "vt_syslinux_need_nojoliet", grub_cmd_syslinux_nojoliet, 0, NULL, "", "", NULL },
    { "vt_vlnk_check", grub_cmd_check_vlnk, 0, NULL, "", "", NULL },
    { "vt_vlnk_dump_part", grub_cmd_vlnk_dump_part, 0, NULL, "", "", NULL },
    { "vt_is_vlnk_name", grub_cmd_is_vlnk_name, 0, NULL, "", "", NULL },
    { "vt_get_vlnk_dst", grub_cmd_get_vlnk_dst, 0, NULL, "", "", NULL },
    { "vt_set_fake_vlnk", ventoy_cmd_set_fake_vlnk, 0, NULL, "", "", NULL },
    { "vt_reset_fake_vlnk", ventoy_cmd_reset_fake_vlnk, 0, NULL, "", "", NULL },
    { "vt_iso_vd_id_parse", ventoy_cmd_iso_vd_id_parse, 0, NULL, "", "", NULL },
    { "vt_iso_vd_id_clear", ventoy_iso_vd_id_clear, 0, NULL, "", "", NULL },
    { "vt_iso_vd_id_begin", ventoy_cmd_iso_vd_id_begin, 0, NULL, "", "", NULL },
    { "vt_fn_mutex_lock", ventoy_cmd_fn_mutex_lock, 0, NULL, "", "", NULL },
    { "vt_efi_dump_rsv_page", ventoy_cmd_dump_rsv_page, 0, NULL, "", "", NULL },
    { "vt_is_standard_winiso", ventoy_cmd_is_standard_winiso, 0, NULL, "", "", NULL },
    { "vt_sel_winpe_wim", ventoy_cmd_sel_winpe_wim, 0, NULL, "", "", NULL },
    { "vt_need_secondary_menu", ventoy_cmd_need_secondary_menu, 0, NULL, "", "", NULL },
    { "vt_show_secondary_menu", ventoy_cmd_show_secondary_menu, 0, NULL, "", "", NULL },
    { "vt_fs_ignore_case", ventoy_cmd_fs_ignore_case, 0, NULL, "", "", NULL },
    { "vt_systemd_menu", ventoy_cmd_linux_systemd_menu, 0, NULL, "", "", NULL },
    { "vt_limine_menu", ventoy_cmd_linux_limine_menu, 0, NULL, "", "", NULL },
    { "vt_secondary_recover_mode", ventoy_cmd_secondary_recover_mode, 0, NULL, "", "", NULL },
    { "vt_load_menu_lang", ventoy_cmd_load_menu_lang, 0, NULL, "", "", NULL },
    { "vt_init_menu_lang", ventoy_cmd_init_menu_lang, 0, NULL, "", "", NULL },
    { "vt_cur_menu_lang", ventoy_cmd_cur_menu_lang, 0, NULL, "", "", NULL },
    { "vt_vtoychksum_exist", ventoy_cmd_vtoychksum_exist, 0, NULL, "", "", NULL },
    { "vt_cmp_checksum", ventoy_cmd_cmp_checksum, 0, NULL, "", "", NULL },
    { "vt_push_menu_lang", ventoy_cmd_push_menulang, 0, NULL, "", "", NULL },
    { "vt_pop_menu_lang", ventoy_cmd_pop_menulang, 0, NULL, "", "", NULL },
    { "vt_linux_initrd", ventoy_cmd_linux_initrd, 0, NULL, "", "", NULL },

};

int ventoy_register_all_cmd(void)
{
    grub_uint32_t i;
    cmd_para *cur = NULL;

    for (i = 0; i < ARRAY_SIZE(ventoy_cmds); i++)
    {
        cur = ventoy_cmds + i;
        cur->cmd = grub_register_extcmd(cur->name, cur->func, cur->flags,
                                        cur->summary, cur->description, cur->parser);
    }

    return 0;
}

int ventoy_unregister_all_cmd(void)
{
    grub_uint32_t i;

    for (i = 0; i < ARRAY_SIZE(ventoy_cmds); i++)
    {
        grub_unregister_extcmd(ventoy_cmds[i].cmd);
    }

    return 0;
}


