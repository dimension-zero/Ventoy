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


int
ventoy_password_get (char buf[], unsigned buf_size)
{
  unsigned i, cur_len = 0;
  int key;
  struct grub_term_coordinate *pos = grub_term_save_pos ();

    while (1)
    {
        key = grub_getkey ();
        if (key == '\n' || key == '\r')
            break;

        if (key == GRUB_TERM_ESC)
        {
            cur_len = 0;
            break;
        }

        if (key == '\b')
        {
            if (cur_len)
            {
                grub_term_restore_pos (pos);
                for (i = 0; i < cur_len; i++)
                    grub_xputs (" ");
                grub_term_restore_pos (pos);
                cur_len--;
                for (i = 0; i < cur_len; i++)
                    grub_xputs ("*");
                grub_refresh ();
            }
            continue;
        }

        if (!grub_isprint (key))
            continue;

        if (cur_len + 2 < buf_size)
            buf[cur_len++] = key;
        grub_xputs ("*");
        grub_refresh ();
    }

    grub_memset (buf + cur_len, 0, buf_size - cur_len);

    grub_xputs ("\n");
    grub_refresh ();
    grub_free (pos);

    return (key != GRUB_TERM_ESC);
}

int ventoy_get_password(char buf[], unsigned buf_size)
{
    const char *env = NULL;

    env = grub_env_get("VTOY_SHOW_PASSWORD_ASTERISK");
    if (env && env[0] == '0' && env[1] == 0)
    {
        return grub_password_get(buf, buf_size);
    }
    else
    {
        return ventoy_password_get(buf, buf_size);
    }
}

int ventoy_check_password(const vtoy_password *pwd, int retry)
{
    int offset;
    char input[256];
    grub_uint8_t md5[16];

    while (retry--)
    {
        grub_memset(input, 0, sizeof(input));

        grub_printf("Enter password: ");
        grub_refresh();

        if (pwd->type == VTOY_PASSWORD_TXT)
        {
            ventoy_get_password(input, 128);
            if (grub_strcmp(pwd->text, input) == 0)
            {
                return 0;
            }
        }
        else if (pwd->type == VTOY_PASSWORD_MD5)
        {
            ventoy_get_password(input, 128);
            grub_crypto_hash(GRUB_MD_MD5, md5, input, grub_strlen(input));
            if (grub_memcmp(pwd->md5, md5, 16) == 0)
            {
                return 0;
            }
        }
        else if (pwd->type == VTOY_PASSWORD_SALT_MD5)
        {
            offset = (int)grub_snprintf(input, 128, "%s", pwd->salt);
            ventoy_get_password(input + offset, 128);

            grub_crypto_hash(GRUB_MD_MD5, md5, input, grub_strlen(input));
            if (grub_memcmp(pwd->md5, md5, 16) == 0)
            {
                return 0;
            }
        }

        grub_printf("Invalid password!\n\n");
        grub_refresh();
    }

    return 1;
}

img_info * ventoy_get_min_iso(img_iterator_node *node)
{
    img_info *minimg = NULL;
    img_info *img = (img_info *)(node->firstiso);

    while (img && (img_iterator_node *)(img->parent) == node)
    {
        if (img->select == 0 && (NULL == minimg || ventoy_cmp_img(img, minimg) < 0))
        {
            minimg = img;
        }
        img = img->next;
    }

    if (minimg)
    {
        minimg->select = 1;
    }

    return minimg;
}

img_iterator_node * ventoy_get_min_child(img_iterator_node *node)
{
    img_iterator_node *Minchild = NULL;
    img_iterator_node *child = node->firstchild;

    while (child && child->parent == node)
    {
        if (child->select == 0 && (NULL == Minchild || ventoy_cmp_subdir(child, Minchild) < 0))
        {
            Minchild = child;
        }
        child = child->next;
    }

    if (Minchild)
    {
        Minchild->select = 1;
    }

    return Minchild;
}

int ventoy_dynamic_tree_menu(img_iterator_node *node)
{
    int offset = 1;
    img_info *img = NULL;
    const char *dir_class = NULL;
    const char *dir_alias = NULL;
    img_iterator_node *child = NULL;
    const menu_tip *tip = NULL;

    if (node->isocnt == 0 || node->done == 1)
    {
        return 0;
    }

    if (node->parent && node->parent->dirlen < node->dirlen)
    {
        offset = node->parent->dirlen;
    }

    if (node == &g_img_iterator_head)
    {
        if (g_default_menu_mode == 0)
        {
            if (g_tree_view_menu_style == 0)
            {
                vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos,
                              "menuentry \"%-10s [%s]\" --class=\"vtoyret\" VTOY_RET {\n  "
                              "  echo 'return ...' \n"
                              "}\n", "<--", ventoy_get_vmenu_title("VTLANG_RET_TO_LISTVIEW"));
            }
            else
            {
                vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos,
                              "menuentry \"[%s]\" --class=\"vtoyret\" VTOY_RET {\n  "
                              "  echo 'return ...' \n"
                              "}\n", ventoy_get_vmenu_title("VTLANG_RET_TO_LISTVIEW"));
            }
        }

        g_tree_script_pre = g_tree_script_pos;
    }
    else
    {
        node->dir[node->dirlen - 1] = 0;
        dir_class = ventoy_plugin_get_menu_class(vtoy_class_directory, node->dir, node->dir);
        if (!dir_class)
        {
            dir_class = "vtoydir";
        }

        tip = ventoy_plugin_get_menu_tip(vtoy_tip_directory, node->dir);

        dir_alias = ventoy_plugin_get_menu_alias(vtoy_alias_directory, node->dir);
        if (dir_alias)
        {
            if (g_tree_view_menu_style == 0)
            {
                vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos,
                              "submenu \"%-10s %s\" --class=\"%s\" --id=\"DIR_%s\" _VTIP_%p {\n",
                              "DIR", dir_alias, dir_class, node->dir + offset, tip);
            }
            else
            {
                vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos,
                              "submenu \"%s\" --class=\"%s\" --id=\"DIR_%s\" _VTIP_%p {\n",
                              dir_alias, dir_class, node->dir + offset, tip);
            }
        }
        else
        {
            dir_alias = node->dir + offset;

            if (g_tree_view_menu_style == 0)
            {
                vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos,
                              "submenu \"%-10s [%s]\" --class=\"%s\" --id=\"DIR_%s\" _VTIP_%p {\n",
                              "DIR", dir_alias, dir_class, node->dir + offset, tip);
            }
            else
            {
                vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos,
                              "submenu \"[%s]\" --class=\"%s\" --id=\"DIR_%s\" _VTIP_%p {\n",
                              dir_alias, dir_class, node->dir + offset, tip);
            }
        }

        if (g_tree_view_menu_style == 0)
        {
            vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos,
                          "menuentry \"%-10s [%s/..]\" --class=\"vtoyret\" VTOY_RET {\n  "
                          "  echo 'return ...' \n"
                          "}\n", "<--", node->dir);
        }
        else
        {
            vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos,
                          "menuentry \"[%s/..]\" --class=\"vtoyret\" VTOY_RET {\n  "
                          "  echo 'return ...' \n"
                          "}\n", node->dir);
        }
    }

    while ((child = ventoy_get_min_child(node)) != NULL)
    {
        ventoy_dynamic_tree_menu(child);
    }

    while ((img = ventoy_get_min_iso(node)) != NULL)
    {
        if (g_tree_view_menu_style == 0)
        {
            vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos,
                          "menuentry \"%-10s %s%s\" --class=\"%s\" --id=\"VID_%p\" {\n"
                          "  %s_%s \n"
                          "}\n",
                          grub_get_human_size(img->size, GRUB_HUMAN_SIZE_SHORT),
                          img->unsupport ? "[***********] " : "",
                          img->alias ? img->alias : img->name, img->class, img,
                          img->menu_prefix,
                          img->unsupport ? "unsupport_menuentry" : "common_menuentry");
        }
        else
        {
            vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos,
                          "menuentry \"%s%s\" --class=\"%s\" --id=\"VID_%p\" {\n"
                          "  %s_%s \n"
                          "}\n",
                          img->unsupport ? "[***********] " : "",
                          img->alias ? img->alias : img->name, img->class, img,
                          img->menu_prefix,
                          img->unsupport ? "unsupport_menuentry" : "common_menuentry");
        }
    }

    if (node != &g_img_iterator_head)
    {
        vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos, "}\n");
    }

    node->done = 1;
    return 0;
}

int ventoy_set_default_menu(void)
{
    int img_len = 0;
    char *pos = NULL;
    char *end = NULL;
    char *def = NULL;
    const char *strdata = NULL;
    img_info *cur = NULL;
    img_info *default_node = NULL;
    const char *default_image = NULL;

    default_image = ventoy_get_env("VTOY_DEFAULT_IMAGE");
    if (default_image && default_image[0] == '/')
    {
        img_len = grub_strlen(default_image);

        for (cur = g_ventoy_img_list; cur; cur = cur->next)
        {
            if (img_len == cur->pathlen && grub_strcmp(default_image, cur->path) == 0)
            {
                default_node = cur;
                break;
            }
        }

        if (!default_node)
        {
            return 1;
        }

        if (0 == g_default_menu_mode)
        {
            vtoy_ssprintf(g_list_script_buf, g_list_script_pos, "set default='VID_%p'\n", default_node);
        }
        else
        {
            def = grub_strdup(default_image);
            if (!def)
            {
                return 1;
            }

            vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos, "set default=%c", '\'');

            strdata = ventoy_get_env("VTOY_DEFAULT_SEARCH_ROOT");
            if (strdata && strdata[0] == '/')
            {
                pos = def + grub_strlen(strdata);
                if (*pos == '/')
                {
                    pos++;
                }
            }
            else
            {
                pos = def + 1;
            }

            while ((end = grub_strchr(pos, '/')) != NULL)
            {
                *end = 0;
                vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos, "DIR_%s>", pos);
                pos = end + 1;
            }

            vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos, "VID_%p'\n", default_node);
            grub_free(def);
        }
    }

    return 0;
}

grub_err_t ventoy_cmd_clear_img(grub_extcmd_context_t ctxt, int argc, char **args)
{
    img_info *next = NULL;
    img_info *cur = g_ventoy_img_list;

    (void)ctxt;
    (void)argc;
    (void)args;

    while (cur)
    {
        next = cur->next;
        grub_free(cur);
        cur = next;
    }

    g_ventoy_img_list = NULL;
    g_ventoy_img_count = 0;

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_img_name(grub_extcmd_context_t ctxt, int argc, char **args)
{
    long img_id = 0;
    img_info *cur = g_ventoy_img_list;

    (void)ctxt;

    if (argc != 2 || (!ventoy_is_decimal(args[0])))
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s {imageID} {var}", cmd_raw_name);
    }

    img_id = grub_strtol(args[0], NULL, 10);
    if (img_id >= g_ventoy_img_count)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "No such many images %ld %ld", img_id, g_ventoy_img_count);
    }

    debug("Find image %ld name \n", img_id);

    while (cur && img_id > 0)
    {
        img_id--;
        cur = cur->next;
    }

    if (!cur)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "No such many images");
    }

    debug("image name is %s\n", cur->name);

    grub_env_set(args[1], cur->name);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_ext_select_img_path(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int len = 0;
    char id[32] = {0};
    img_info *cur = g_ventoy_img_list;

    (void)ctxt;

    if (argc != 1)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s {var}", cmd_raw_name);
    }

    len = (int)grub_strlen(args[0]);

    while (cur)
    {
        if (len == cur->pathlen && 0 == grub_strcmp(args[0], cur->path))
        {
            break;
        }
        cur = cur->next;
    }

    if (!cur)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "No such image");
    }

    grub_snprintf(id, sizeof(id), "VID_%p", cur);
    grub_env_set("chosen", id);
    grub_env_export("chosen");

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

char g_fake_vlnk_src[512];
char g_fake_vlnk_dst[512];
grub_uint64_t g_fake_vlnk_size;
grub_err_t ventoy_cmd_set_fake_vlnk(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    g_fake_vlnk_size = (grub_uint64_t)grub_strtoull(args[2], NULL, 10);

    grub_strncpy(g_fake_vlnk_dst, args[0], sizeof(g_fake_vlnk_dst));
    grub_snprintf(g_fake_vlnk_src, sizeof(g_fake_vlnk_src), "%s/________VENTOYVLNK.vlnk.%s", g_iso_path, args[1]);

    grub_file_vtoy_vlnk(g_fake_vlnk_src, g_fake_vlnk_dst);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_reset_fake_vlnk(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    g_fake_vlnk_src[0] = 0;
    g_fake_vlnk_dst[0] = 0;
    g_fake_vlnk_size = 0;
    grub_file_vtoy_vlnk(NULL, NULL);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}


grub_err_t ventoy_cmd_chosen_img_path(grub_extcmd_context_t ctxt, int argc, char **args)
{
    char value[32];
    char *pos = NULL;
    char *last = NULL;
    const char *id = NULL;
    img_info *cur = NULL;

    (void)ctxt;

    if (argc < 1 || argc > 3)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s {var}", cmd_raw_name);
    }

    if (g_fake_vlnk_src[0] && g_fake_vlnk_dst[0])
    {
        pos = grub_strchr(g_fake_vlnk_src, '/');
        grub_env_set(args[0], pos);
        if (argc > 1)
        {
            grub_snprintf(value, sizeof(value), "%llu", (ulonglong)(g_fake_vlnk_size));
            grub_env_set(args[1], value);
        }

        if (argc > 2)
        {
            for (last = pos; *pos; pos++)
            {
                if (*pos == '/')
                {
                    last = pos;
                }
            }
            grub_env_set(args[2], last + 1);
        }

        goto end;
    }

    id = grub_env_get("chosen");

    pos = grub_strstr(id, "VID_");
    if (pos)
    {
        cur = (img_info *)(void *)grub_strtoul(pos + 4, NULL, 16);
    }
    else
    {
        cur = g_ventoy_img_list;
    }

    if (!cur)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "No such image");
    }

    grub_env_set(args[0], cur->path);

    if (argc > 1)
    {
        grub_snprintf(value, sizeof(value), "%llu", (ulonglong)(cur->size));
        grub_env_set(args[1], value);
    }

    if (argc > 2)
    {
        grub_snprintf(value, sizeof(value), "%llu", (ulonglong)(cur->size));
        grub_env_set(args[2], cur->name);
    }

end:
    g_svd_replace_offset = 0;

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}


grub_err_t ventoy_cmd_list_img(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int len;
    grub_fs_t fs;
    grub_device_t dev = NULL;
    img_info *cur = NULL;
    img_info *tail = NULL;
    img_info *min = NULL;
    img_info *head = NULL;
    const char *strdata = NULL;
    char *device_name = NULL;
    char buf[32];
    img_iterator_node *node = NULL;
    img_iterator_node *tmp = NULL;

    (void)ctxt;

    if (argc != 2)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s {device} {cntvar}", cmd_raw_name);
    }

    if (g_ventoy_img_list || g_ventoy_img_count)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Must clear image before list");
    }

    VTOY_CMD_CHECK(1);

    g_enumerate_time_checked  = 0;
    g_enumerate_start_time_ms = grub_get_time_ms();

    strdata = ventoy_get_env("VTOY_FILT_DOT_UNDERSCORE_FILE");
    if (strdata && strdata[0] == '1' && strdata[1] == 0)
    {
        g_filt_dot_underscore_file = 1;
    }

    strdata = ventoy_get_env("VTOY_FILT_TRASH_DIR");
    if (strdata && strdata[0] == '0' && strdata[1] == 0)
    {
        g_filt_trash_dir = 0;
    }

    strdata = ventoy_get_env("VTOY_SORT_CASE_SENSITIVE");
    if (strdata && strdata[0] == '1' && strdata[1] == 0)
    {
        g_sort_case_sensitive = 1;
    }

    device_name = grub_file_get_device_name(args[0]);
    if (!device_name)
    {
        goto fail;
    }

    g_enum_dev = dev = grub_device_open(device_name);
    if (!dev)
    {
        goto fail;
    }

    g_enum_fs = fs = grub_fs_probe(dev);
    if (!fs)
    {
        goto fail;
    }

    if (ventoy_get_fs_type(fs->name) >= ventoy_fs_max)
    {
        debug("unsupported fs:<%s>\n", fs->name);
        ventoy_set_env("VTOY_NO_ISO_TIP", "unsupported file system");
        goto fail;
    }

    ventoy_set_env("vtoy_iso_fs", fs->name);

