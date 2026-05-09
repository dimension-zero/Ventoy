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

    grub_uint32_t align = 0;
    grub_file_t file = NULL;
    conf_replace *node = NULL;
    conf_replace *nodes[VTOY_MAX_CONF_REPLACE] = { NULL };
    ventoy_grub_param_file_replace *replace = NULL;

    (void)ctxt;
    (void)argc;
    (void)args;

    debug("select conf replace argc:%d\n", argc);

    if (argc < 2)
    {
        return 0;
    }

    n = ventoy_plugin_find_conf_replace(args[1], nodes);
    if (!n)
    {
        debug("Conf replace not found for %s\n", args[1]);
        goto end;
    }

    debug("Find %d conf replace for %s\n", n, args[1]);

    g_conf_replace_count = n;
    for (i = 0; i < n; i++)
    {
        node = nodes[i];

        file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "(loop)%s", node->orgconf);
        if (file)
        {
            offset = grub_iso9660_get_last_file_dirent_pos(file);
            grub_file_close(file);
        }
        else if (node->img > 0)
        {
            offset = 0;
        }
        else
        {
            debug("<(loop)%s> NOT exist\n", node->orgconf);
            continue;
        }

        file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s", args[0], node->newconf);
        if (!file)
        {
            debug("New config file <%s%s> NOT exist\n", args[0], node->newconf);
            continue;
        }

        align = ((int)file->size + 2047) / 2048 * 2048;

        if (align > vtoy_max_replace_file_size)
        {
            debug("New config file <%s%s> too big\n", args[0], node->newconf);
            grub_file_close(file);
            continue;
        }

        grub_file_read(file, g_conf_replace_new_buf[i], file->size);
        grub_file_close(file);
        g_conf_replace_new_len[i] = (int)file->size;
        g_conf_replace_new_len_align[i] = align;

        g_conf_replace_node[i] = node;
        g_conf_replace_offset[i] = offset + 2;

        if (node->img > 0)
        {
            replace = &(g_grub_param->img_replace[i]);
            replace->magic = GRUB_IMG_REPLACE_MAGIC;
            grub_snprintf(replace->old_file_name[replace->old_name_cnt], 256, "%s", node->orgconf);
            replace->old_name_cnt++;
        }

        debug("conf_replace OK: newlen[%d]: %d img:%d\n", i, g_conf_replace_new_len[i], node->img);
    }

end:
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

int ventoy_var_expand(int *record, int *flag, const char *var, char *expand, int len)
{
    int i = 0;
    int n = 0;
    char c;
    const char *ch = var;

    *record = 0;
    expand[0] = 0;

    while (*ch)
    {
        if (*ch == '_' || (*ch >= '0' && *ch <= '9') || (*ch >= 'A' && *ch <= 'Z') || (*ch >= 'a' && *ch <= 'z'))
        {
            ch++;
            n++;
        }
        else
        {
            debug("Invalid variable letter <%c>\n", *ch);
            goto end;
        }
    }

    if (n > 32)
    {
        debug("Invalid variable length:%d <%s>\n", n, var);
        goto end;
    }

    if (grub_strncmp(var, "VT_", 3) == 0) /* built-in variables */
    {

    }
    else
    {
        if (*flag == 0)
        {
            *flag = 1;
            grub_printf("\n===================  Variables Expansion  ===================\n\n");
        }

        grub_printf("<%s>: ", var);
        grub_refresh();

        while (i < (len - 1))
        {
            c = grub_getkey();
            if ((c == '\n') || (c == '\r'))
            {
                if (i > 0)
                {
                    grub_printf("\n");
                    grub_refresh();
                    *record = 1;
                    break;
                }
            }
            else if (grub_isprint(c))
            {
                if (i + 1 < (len - 1))
                {
                    grub_printf("%c", c);
                    grub_refresh();
                    expand[i++] = c;
                    expand[i] = 0;
                }
            }
            else if (c == '\b')
            {
                if (i > 0)
                {
                    expand[i - 1] = ' ';
                    grub_printf("\r<%s>: %s", var, expand);

                    expand[i - 1] = 0;
                    grub_printf("\r<%s>: %s", var, expand);

                    grub_refresh();
                    i--;
                }
            }
        }
    }

end:
    if (expand[0] == 0)
    {
        grub_snprintf(expand, len, "$$%s$$", var);
    }

    return 0;
}

int ventoy_auto_install_var_expand(install_template *node)
{
    int pos = 0;
    int flag = 0;
    int record = 0;
    int newlen = 0;
    char *start = NULL;
    char *end = NULL;
    char *newbuf = NULL;
    char *curline = NULL;
    char *nextline = NULL;
    grub_uint8_t *code = NULL;
    char value[512];
    var_node *CurNode = NULL;
    var_node *pVarList = NULL;

    code = (grub_uint8_t *)node->filebuf;

    if (node->filelen >= VTOY_SIZE_1MB)
    {
        debug("auto install script too long %d\n", node->filelen);
        return 0;
    }

    if ((code[0] == 0xff && code[1] == 0xfe) || (code[0] == 0xfe && code[1] == 0xff))
    {
        debug("UCS-2 encoding NOT supported\n");
        return 0;
    }

    start = grub_strstr(node->filebuf, "$$");
    if (!start)
    {
        debug("no need to expand variable, no start.\n");
        return 0;
    }

    end = grub_strstr(start + 2, "$$");
    if (!end)
    {
        debug("no need to expand variable, no end.\n");
        return 0;
    }

    newlen = grub_max(node->filelen * 10, VTOY_SIZE_128KB);
    newbuf = grub_malloc(newlen);
    if (!newbuf)
    {
        debug("Failed to alloc newbuf %d\n", newlen);
        return 0;
    }

    for (curline = node->filebuf; curline; curline = nextline)
    {
        nextline = ventoy_get_line(curline);

        start = grub_strstr(curline, "$$");
        if (start)
        {
            end = grub_strstr(start + 2, "$$");
        }

        if (start && end)
        {
            *start = *end = 0;
            VTOY_APPEND_NEWBUF(curline);

            for (CurNode = pVarList; CurNode; CurNode = CurNode->next)
            {
                if (grub_strcmp(start + 2, CurNode->var) == 0)
                {
                    grub_snprintf(value, sizeof(value) - 1, "%s", CurNode->val);
                    break;
                }
            }

            if (!CurNode)
            {
                value[sizeof(value) - 1] = 0;
                ventoy_var_expand(&record, &flag, start + 2, value, sizeof(value) - 1);

                if (record)
                {
                    CurNode = grub_zalloc(sizeof(var_node));
                    if (CurNode)
                    {
                        grub_snprintf(CurNode->var, sizeof(CurNode->var), "%s", start + 2);
                        grub_snprintf(CurNode->val, sizeof(CurNode->val), "%s", value);
                        CurNode->next = pVarList;
                        pVarList = CurNode;
                    }
                }
            }

            VTOY_APPEND_NEWBUF(value);

            VTOY_APPEND_NEWBUF(end + 2);
        }
        else
        {
            VTOY_APPEND_NEWBUF(curline);
        }

        if (pos > 0 && newbuf[pos - 1] == '\r')
        {
            newbuf[pos - 1] = '\n';
        }
        else
        {
            newbuf[pos++] = '\n';
        }
    }

    grub_free(node->filebuf);
    node->filebuf = newbuf;
    node->filelen = pos;

    while (pVarList)
    {
        CurNode = pVarList->next;
        grub_free(pVarList);
        pVarList = CurNode;
    }

    return 0;
}

grub_err_t ventoy_cmd_sel_auto_install(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i = 0;
    int pos = 0;
    int defidx = 1;
    char *buf = NULL;
    grub_file_t file = NULL;
    char configfile[128];
    install_template *node = NULL;

    (void)ctxt;
    (void)argc;
    (void)args;

    debug("select auto installation argc:%d\n", argc);

    if (argc < 1)
    {
        return 0;
    }

    node = ventoy_plugin_find_install_template(args[0]);
    if (!node)
    {
        debug("Auto install template not found for %s\n", args[0]);
        return 0;
    }

    if (node->autosel >= 0 && node->autosel <= node->templatenum)
    {
        defidx = node->autosel;
        if (node->timeout < 0)
        {
            node->cursel = node->autosel - 1;
            debug("Auto install template auto select %d\n", node->autosel);
            goto load;
        }
    }

    buf = (char *)grub_malloc(VTOY_MAX_SCRIPT_BUF);
    if (!buf)
    {
        return 0;
    }

    if (node->timeout > 0)
    {
        vtoy_ssprintf(buf, pos, "set timeout=%d\n", node->timeout);
    }

    vtoy_ssprintf(buf, pos, "menuentry \"$VTLANG_NO_AUTOINS_SCRIPT\" --class=\"sel_auto_install\" {\n"
                  "  echo %s\n}\n", "");

    for (i = 0; i < node->templatenum; i++)
    {
        vtoy_ssprintf(buf, pos, "menuentry \"%s %s\" --class=\"sel_auto_install\" {\n"
                  "  echo \"\"\n}\n",
                  ventoy_get_vmenu_title("VTLANG_AUTOINS_USE"),
                  node->templatepath[i].path);
    }

    g_ventoy_menu_esc = 1;
    g_ventoy_suppress_esc = 1;
    g_ventoy_suppress_esc_default = defidx;
    g_ventoy_secondary_menu_on = 1;

    grub_snprintf(configfile, sizeof(configfile), "configfile mem:0x%llx:size:%d", (ulonglong)(ulong)buf, pos);
    grub_script_execute_sourcecode(configfile);

    g_ventoy_menu_esc = 0;
    g_ventoy_suppress_esc = 0;
    g_ventoy_suppress_esc_default = 1;
    g_ventoy_secondary_menu_on = 0;

    grub_free(buf);

    node->cursel = g_ventoy_last_entry - 1;

load:
    grub_check_free(node->filebuf);
    node->filelen = 0;

    if (node->cursel >= 0 && node->cursel < node->templatenum)
    {
        file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s", ventoy_get_env("vtoy_iso_part"),
            node->templatepath[node->cursel].path);
        if (file)
        {
            node->filebuf = grub_malloc(file->size + 8);
            if (node->filebuf)
            {
                grub_file_read(file, node->filebuf, file->size);
                grub_file_close(file);

                grub_memset(node->filebuf + file->size, 0, 8);
                node->filelen = (int)file->size;

                ventoy_auto_install_var_expand(node);
            }
        }
        else
        {
            debug("Failed to open auto install script <%s%s>\n",
                ventoy_get_env("vtoy_iso_part"), node->templatepath[node->cursel].path);
        }
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_sel_persistence(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i = 0;
    int pos = 0;
    int defidx = 1;
    char *buf = NULL;
    char configfile[128];
    persistence_config *node;

    (void)ctxt;
    (void)argc;
    (void)args;

    debug("select persistence argc:%d\n", argc);

    if (argc < 1)
    {
        return 0;
    }

    node = ventoy_plugin_find_persistent(args[0]);
    if (!node)
    {
        debug("Persistence image not found for %s\n", args[0]);
        return 0;
    }

    if (node->autosel >= 0 && node->autosel <= node->backendnum)
    {
        defidx = node->autosel;
        if (node->timeout < 0)
        {
            node->cursel = node->autosel - 1;
            debug("Persistence image auto select %d\n", node->autosel);
            return 0;
        }
    }

    buf = (char *)grub_malloc(VTOY_MAX_SCRIPT_BUF);
    if (!buf)
    {
        return 0;
    }

    if (node->timeout > 0)
    {
        vtoy_ssprintf(buf, pos, "set timeout=%d\n", node->timeout);
    }

    vtoy_ssprintf(buf, pos, "menuentry \"$VTLANG_NO_PERSIST\" --class=\"sel_persistence\" {\n"
                  "  echo %s\n}\n", "");

    for (i = 0; i < node->backendnum; i++)
    {
        vtoy_ssprintf(buf, pos, "menuentry \"%s %s\" --class=\"sel_persistence\" {\n"
                      "  echo \"\"\n}\n",
                      ventoy_get_vmenu_title("VTLANG_PERSIST_USE"),
                      node->backendpath[i].path);

    }

    g_ventoy_menu_esc = 1;
    g_ventoy_suppress_esc = 1;
    g_ventoy_suppress_esc_default = defidx;
    g_ventoy_secondary_menu_on = 1;

    grub_snprintf(configfile, sizeof(configfile), "configfile mem:0x%llx:size:%d", (ulonglong)(ulong)buf, pos);
    grub_script_execute_sourcecode(configfile);

    g_ventoy_menu_esc = 0;
    g_ventoy_suppress_esc = 0;
    g_ventoy_suppress_esc_default = 1;
    g_ventoy_secondary_menu_on = 0;

    grub_free(buf);

    node->cursel = g_ventoy_last_entry - 1;

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_dump_img_sector(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_uint32_t i;
    ventoy_img_chunk *cur;

    (void)ctxt;
    (void)argc;
    (void)args;

    for (i = 0; i < g_img_chunk_list.cur_chunk; i++)
    {
        cur = g_img_chunk_list.chunk + i;
        grub_printf("image:[%u - %u]   <==>  disk:[%llu - %llu]\n",
            cur->img_start_sector, cur->img_end_sector,
            (unsigned long long)cur->disk_start_sector, (unsigned long long)cur->disk_end_sector
            );
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_test_block_list(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_uint32_t i;
    grub_file_t file;
    ventoy_img_chunk_list chunklist;
    char errmsg[128];

    (void)ctxt;
    (void)argc;

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (!file)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Can't open file %s\n", args[0]);
    }

    /* get image chunk data */
    grub_memset(&chunklist, 0, sizeof(chunklist));
    chunklist.chunk = grub_malloc(sizeof(ventoy_img_chunk) * DEFAULT_CHUNK_NUM);
    if (NULL == chunklist.chunk)
    {
        return grub_error(GRUB_ERR_OUT_OF_MEMORY, "Can't allocate image chunk memoty\n");
    }

    chunklist.max_chunk = DEFAULT_CHUNK_NUM;
    chunklist.cur_chunk = 0;

    ventoy_get_block_list(file, &chunklist, 0);

    if (0 != ventoy_check_block_list(file, &chunklist, 0, errmsg, sizeof(errmsg)))
    {
        grub_printf("%s\n", errmsg);
        grub_printf("########## UNSUPPORTED ###############\n");
    }

    grub_printf("filesystem: <%s> entry number:<%u>\n", file->fs->name, chunklist.cur_chunk);

    for (i = 0; i < chunklist.cur_chunk; i++)
    {
        grub_printf("%llu+%llu,", (ulonglong)chunklist.chunk[i].disk_start_sector,
            (ulonglong)(chunklist.chunk[i].disk_end_sector + 1 - chunklist.chunk[i].disk_start_sector));
    }

    grub_printf("\n==================================\n");

    for (i = 0; i < chunklist.cur_chunk; i++)
    {
        grub_printf("%2u: [%llu %llu] - [%llu %llu]\n", i,
            (ulonglong)chunklist.chunk[i].img_start_sector,
            (ulonglong)chunklist.chunk[i].img_end_sector,
            (ulonglong)chunklist.chunk[i].disk_start_sector,
            (ulonglong)chunklist.chunk[i].disk_end_sector
            );
    }

    grub_free(chunklist.chunk);
    grub_file_close(file);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_add_replace_file(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    ventoy_grub_param_file_replace *replace = NULL;

    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc >= 2)
    {
        replace = &(g_grub_param->file_replace);
        replace->magic = GRUB_FILE_REPLACE_MAGIC;

        replace->old_name_cnt = 0;
        for (i = 0; i < 4 && i + 1 < argc; i++)
        {
            replace->old_name_cnt++;
            grub_snprintf(replace->old_file_name[i], sizeof(replace->old_file_name[i]), "%s", args[i + 1]);
        }
