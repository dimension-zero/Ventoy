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

grub_err_t ventoy_cmd_debug(grub_extcmd_context_t ctxt, int argc, char **args)
{
    if (argc != 1)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s {on|off}", cmd_raw_name);
    }

    if (0 == grub_strcmp(args[0], "on"))
    {
        g_ventoy_debug = 1;
        grub_env_set("vtdebug_flag", "debug");
    }
    else
    {
        g_ventoy_debug = 0;
        grub_env_set("vtdebug_flag", "");
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_break(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;

    if (argc < 1 || (args[0][0] != '0' && args[0][0] != '1'))
    {
        grub_printf("Usage: %s {level} [debug]\r\n", cmd_raw_name);
        grub_printf(" level:\r\n");
        grub_printf("    01/11: busybox / (+cat log)\r\n");
        grub_printf("    02/12: initrd / (+cat log)\r\n");
        grub_printf("    03/13: hook / (+cat log)\r\n");
        grub_printf("\r\n");
        grub_printf(" debug:\r\n");
        grub_printf("    0: debug is off\r\n");
        grub_printf("    1: debug is on\r\n");
        grub_printf("\r\n");
        VENTOY_CMD_RETURN(GRUB_ERR_NONE);
    }

    g_ventoy_break_level = (grub_uint8_t)grub_strtoul(args[0], NULL, 16);

    if (argc > 1 && grub_strtoul(args[1], NULL, 10) > 0)
    {
        g_ventoy_debug_level = 1;
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_strstr(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;

    if (argc != 2)
    {
        return 1;
    }

    return (grub_strstr(args[0], args[1])) ? 0 : 1;
}

grub_err_t ventoy_cmd_strbegin(grub_extcmd_context_t ctxt, int argc, char **args)
{
    char *c0, *c1;

    (void)ctxt;

    if (argc != 2)
    {
        return 1;
    }

    c0 = args[0];
    c1 = args[1];

    while (*c0 && *c1)
    {
        if (*c0 != *c1)
        {
            return 1;
        }
        c0++;
        c1++;
    }

    if (*c1)
    {
        return 1;
    }

    return 0;
}

grub_err_t ventoy_cmd_strcasebegin(grub_extcmd_context_t ctxt, int argc, char **args)
{
    char *c0, *c1;

    (void)ctxt;

    if (argc != 2)
    {
        return 1;
    }

    c0 = args[0];
    c1 = args[1];

    while (*c0 && *c1)
    {
        if ((*c0 != *c1) && (*c0 != grub_toupper(*c1)))
        {
            return 1;
        }
        c0++;
        c1++;
    }

    if (*c1)
    {
        return 1;
    }

    return 0;
}

grub_err_t ventoy_cmd_incr(grub_extcmd_context_t ctxt, int argc, char **args)
{
    long value_long = 0;
    char buf[32];

    if ((argc != 2) || (!ventoy_is_decimal(args[1])))
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s {Variable} {Int}", cmd_raw_name);
    }

    if (GRUB_ERR_NONE != ventoy_check_decimal_var(args[0], &value_long))
    {
        return grub_errno;
    }

    value_long += grub_strtol(args[1], NULL, 10);

    grub_snprintf(buf, sizeof(buf), "%ld", value_long);
    grub_env_set(args[0], buf);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_mod(grub_extcmd_context_t ctxt, int argc, char **args)
{
    ulonglong value1 = 0;
    ulonglong value2 = 0;
    char buf[32];

    if (argc != 3)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s {Int} {Int} {Variable}", cmd_raw_name);
    }

    value1 = grub_strtoull(args[0], NULL, 10);
    value2 = grub_strtoull(args[1], NULL, 10);

    grub_snprintf(buf, sizeof(buf), "%llu", (value1 & (value2 - 1)));
    grub_env_set(args[2], buf);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_file_size(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int rc = 1;
    char buf[32];
    grub_file_t file;

    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc != 2)
    {
        return rc;
    }

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (file == NULL)
    {
        debug("failed to open file <%s> for udf check\n", args[0]);
        return 1;
    }

    grub_snprintf(buf, sizeof(buf), "%llu", (unsigned long long)file->size);

    grub_env_set(args[1], buf);

    grub_file_close(file);
    rc = 0;

    return rc;
}

grub_err_t ventoy_cmd_load_wimboot(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_file_t file;

    (void)ctxt;
    (void)argc;
    (void)args;

    g_wimboot_enable = 0;
    g_wimiso_size = 0;
    grub_check_free(g_wimiso_path);
    grub_check_free(g_wimiso_chunk_list.chunk);

    file = grub_file_open(args[0], VENTOY_FILE_TYPE);
    if (!file)
    {
        return 0;
    }

    grub_memset(&g_wimiso_chunk_list, 0, sizeof(g_wimiso_chunk_list));
    g_wimiso_chunk_list.chunk = grub_malloc(sizeof(ventoy_img_chunk) * DEFAULT_CHUNK_NUM);
    if (NULL == g_wimiso_chunk_list.chunk)
    {
        return grub_error(GRUB_ERR_OUT_OF_MEMORY, "Can't allocate image chunk memoty\n");
    }

    g_wimiso_chunk_list.max_chunk = DEFAULT_CHUNK_NUM;
    g_wimiso_chunk_list.cur_chunk = 0;

    ventoy_get_block_list(file, &g_wimiso_chunk_list, file->device->disk->partition->start);

    g_wimboot_enable = 1;
    g_wimiso_path = grub_strdup(args[0]);
    g_wimiso_size = (grub_uint32_t)(file->size);
    grub_file_close(file);

    return 0;
}

grub_err_t ventoy_cmd_concat_efi_iso(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int len = 0;
    int totlen = 0;
    int offset = 0;
    grub_file_t file;
    char *buf = NULL;
    char *data = NULL;
    ventoy_iso9660_override *dirent;

    (void)ctxt;

    if (argc != 2)
    {
        return 1;
    }

    totlen = sizeof(ventoy_chain_head);

    if (ventoy_load_efiboot_template(&buf, &len, &offset))
    {
        debug("failed to load efiboot template %d\n", len);
        return 1;
    }

    totlen += len;

    debug("efiboot template len:%d offset:%d\n", len, offset);

    file = ventoy_grub_file_open(GRUB_FILE_TYPE_LINUX_INITRD, "%s", args[0]);
    if (file == NULL)
    {
        debug("failed to open file <%s>\n", args[0]);
        return 1;
    }

    if (grub_strncmp(args[0], g_iso_path, grub_strlen(g_iso_path)))
    {
        file->vlnk = 1;
    }

    totlen += ventoy_align_2k(file->size);

    dirent = (ventoy_iso9660_override *)(buf + offset);
    dirent->first_sector = len / 2048;
    dirent->first_sector_be = grub_swap_bytes32(dirent->first_sector);
    dirent->size = (grub_uint32_t)file->size;
    dirent->size_be = grub_swap_bytes32(dirent->size);

    debug("rawiso len:%d efilen:%d total:%d\n", len, (int)file->size, totlen);

#ifdef GRUB_MACHINE_EFI
    data = (char *)grub_efi_allocate_iso_buf(totlen);
#else
    data = (char *)grub_malloc(totlen);
#endif

    ventoy_fill_os_param(file, (ventoy_os_param *)data);

    grub_memcpy(data + sizeof(ventoy_chain_head), buf, len);
    grub_check_free(buf);

    grub_file_read(file, data + sizeof(ventoy_chain_head) + len, file->size);
    grub_file_close(file);

    ventoy_memfile_env_set(args[1], data, (ulonglong)totlen);

    return 0;
}

grub_err_t ventoy_cmd_set_wim_prompt(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    g_vtoy_load_prompt = 0;
    grub_memset(g_vtoy_prompt_msg, 0, sizeof(g_vtoy_prompt_msg));

    if (argc == 2 && args[0][0] == '1')
    {
        g_vtoy_load_prompt = 1;
        grub_snprintf(g_vtoy_prompt_msg, sizeof(g_vtoy_prompt_msg), "%s", args[1]);
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

int ventoy_need_prompt_load_file(void)
{
    return g_vtoy_load_prompt;
}

grub_ssize_t ventoy_load_file_with_prompt(grub_file_t file, void *buf, grub_ssize_t size)
{
    grub_uint64_t ro = 0;
    grub_uint64_t div = 0;
    grub_ssize_t left = size;
    char *cur = (char *)buf;

    grub_printf("\r%s   1%%    ", g_vtoy_prompt_msg);
    grub_refresh();

    while (left >= VTOY_SIZE_2MB)
    {
        grub_file_read(file, cur, VTOY_SIZE_2MB);
        cur += VTOY_SIZE_2MB;
        left -= VTOY_SIZE_2MB;

        div = grub_divmod64((grub_uint64_t)((size - left) * 100), (grub_uint64_t)size, &ro);
        if (div < 1)
        {
            div = 1;
        }
        grub_printf("\r%s   %d%%    ", g_vtoy_prompt_msg, (int)div);
        grub_refresh();
    }

    if (left > 0)
    {
        grub_file_read(file, cur, left);
    }

    grub_printf("\r%s   100%%     \n", g_vtoy_prompt_msg);
    grub_refresh();

    return size;
}

grub_err_t ventoy_cmd_load_file_to_mem(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int rc = 1;
    char *buf = NULL;
    grub_file_t file;
    enum grub_file_type type;

    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc != 3)
    {
        return rc;
    }

    if (grub_strcmp(args[0], "nodecompress") == 0)
    {
        type = VENTOY_FILE_TYPE;
    }
    else
    {
        type = GRUB_FILE_TYPE_LINUX_INITRD;
    }

    file = ventoy_grub_file_open(type, "%s", args[1]);
    if (file == NULL)
    {
        debug("failed to open file <%s>\n", args[1]);
        return 1;
    }

#ifdef GRUB_MACHINE_EFI
    buf = (char *)grub_efi_allocate_chain_buf(file->size);
#else
    buf = (char *)grub_malloc(file->size);
#endif

    if (!buf)
    {
        grub_file_close(file);
        return 1;
    }

    if (g_vtoy_load_prompt)
    {
         ventoy_load_file_with_prompt(file, buf, file->size);
    }
    else
    {
        grub_file_read(file, buf, file->size);
    }

    ventoy_memfile_env_set(args[2], buf, (ulonglong)(file->size));

    grub_file_close(file);
    rc = 0;

    return rc;
}

grub_err_t ventoy_cmd_load_img_memdisk(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int rc = 1;
    int headlen;
    char *buf = NULL;
    grub_file_t file;

    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc != 2)
    {
        return rc;
    }

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (file == NULL)
    {
        debug("failed to open file <%s> for udf check\n", args[0]);
        return 1;
    }

    headlen = sizeof(ventoy_chain_head);

#ifdef GRUB_MACHINE_EFI
    buf = (char *)grub_efi_allocate_iso_buf(headlen + file->size);
#else
    buf = (char *)grub_malloc(headlen + file->size);
#endif

    ventoy_fill_os_param(file, (ventoy_os_param *)buf);

    grub_file_read(file, buf + headlen, file->size);

    ventoy_memfile_env_set(args[1], buf, (ulonglong)(file->size));

    grub_file_close(file);
    rc = 0;

    return rc;
}

grub_err_t ventoy_cmd_iso9660_is_joliet(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    if (grub_iso9660_is_joliet())
    {
        debug("This time has joliet process\n");
        return 0;
    }
    else
    {
        return 1;
    }
}

grub_err_t ventoy_cmd_iso9660_nojoliet(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;

    if (argc != 1)
    {
        return 1;
    }

    if (args[0][0] == '1')
    {
        grub_iso9660_set_nojoliet(1);
    }
    else
    {
        grub_iso9660_set_nojoliet(0);
    }

    return 0;
}

grub_err_t ventoy_cmd_is_udf(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    int rc = 1;
    grub_file_t file;
    grub_uint8_t buf[32];

    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc != 1)
    {
        return rc;
    }

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (file == NULL)
    {
        debug("failed to open file <%s> for udf check\n", args[0]);
        return 1;
    }

    for (i = 16; i < 32; i++)
    {
        grub_file_seek(file, i * 2048);
        grub_file_read(file, buf, sizeof(buf));
        if (buf[0] == 255)
        {
            break;
        }
    }

    i++;
    grub_file_seek(file, i * 2048);
    grub_file_read(file, buf, sizeof(buf));

    if (grub_memcmp(buf + 1, "BEA01", 5) == 0)
    {
        i++;
        grub_file_seek(file, i * 2048);
        grub_file_read(file, buf, sizeof(buf));

        if (grub_memcmp(buf + 1, "NSR02", 5) == 0 ||
            grub_memcmp(buf + 1, "NSR03", 5) == 0)
        {
            rc = 0;
        }
    }

    grub_file_close(file);

    debug("ISO UDF: %s\n", rc ? "NO" : "YES");

    return rc;
}

grub_err_t ventoy_cmd_cmp(grub_extcmd_context_t ctxt, int argc, char **args)
{
    long value_long1 = 0;
    long value_long2 = 0;

    if ((argc != 3) || (!ventoy_is_decimal(args[0])) || (!ventoy_is_decimal(args[2])))
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s {Int1} { eq|ne|gt|lt|ge|le } {Int2}", cmd_raw_name);
    }

    value_long1 = grub_strtol(args[0], NULL, 10);
    value_long2 = grub_strtol(args[2], NULL, 10);

    if (0 == grub_strcmp(args[1], "eq"))
    {
        grub_errno = (value_long1 == value_long2) ? GRUB_ERR_NONE : GRUB_ERR_TEST_FAILURE;
    }
    else if (0 == grub_strcmp(args[1], "ne"))
    {
        grub_errno = (value_long1 != value_long2) ? GRUB_ERR_NONE : GRUB_ERR_TEST_FAILURE;
    }
    else if (0 == grub_strcmp(args[1], "gt"))
    {
        grub_errno = (value_long1 > value_long2) ? GRUB_ERR_NONE : GRUB_ERR_TEST_FAILURE;
    }
    else if (0 == grub_strcmp(args[1], "lt"))
    {
        grub_errno = (value_long1 < value_long2) ? GRUB_ERR_NONE : GRUB_ERR_TEST_FAILURE;
    }
    else if (0 == grub_strcmp(args[1], "ge"))
    {
        grub_errno = (value_long1 >= value_long2) ? GRUB_ERR_NONE : GRUB_ERR_TEST_FAILURE;
    }
