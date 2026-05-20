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

