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
grub_err_t ventoy_select_conf_replace(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    int n;
    grub_uint64_t offset = 0;
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

