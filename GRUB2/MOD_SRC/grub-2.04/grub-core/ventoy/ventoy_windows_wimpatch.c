/******************************************************************************
 * ventoy_windows.c 
 *
 * Copyright (c) 2020, longpanda <admin@ventoy.net>
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
#include <grub/time.h>
#include <grub/crypto.h>
#include <grub/ventoy.h>
#include "ventoy_def.h"
#include "ventoy_windows_priv.h"
wim_patch *ventoy_find_wim_patch(const char *path)
{
    int len = (int)grub_strlen(path);
    wim_patch *node = g_wim_patch_head;

    while (node)
    {
        if (len == node->pathlen && 0 == grub_strcmp(path, node->path))
        {
            return node;
        }
        node = node->next;
    }

    return NULL;
}

int ventoy_collect_wim_patch(const char *bcdfile)
{
    int i, j, k;
    int rc = 1;
    grub_uint64_t magic;
    grub_file_t file = NULL;
    char *buf = NULL;
    wim_patch *node = NULL;
    char c;
    grub_uint8_t byte;
    char valid;
    char path[256];

    g_ventoy_case_insensitive = 1;
    file = grub_file_open(bcdfile, VENTOY_FILE_TYPE);
    g_ventoy_case_insensitive = 0;
    if (!file)
    {
        debug("Failed to open file %s\n", bcdfile);
        grub_errno = 0;
        goto end;
    }

    buf = grub_malloc(file->size + 8);
    if (!buf)
    {
        goto end;
    }

    grub_file_read(file, buf, file->size);

    for (i = 0; i < (int)file->size - 8; i++)
    {        
        if (buf[i + 8] != 0)
        {
            continue;
        }
        
        magic = *(grub_uint64_t *)(buf + i);
        
        /* .wim .WIM .Wim */
        if ((magic == 0x006D00690077002EULL) ||
            (magic == 0x004D00490057002EULL) ||
            (magic == 0x006D00690057002EULL))
        {
            for (j = i; j > 0; j-= 2)
            {
                if (*(grub_uint16_t *)(buf + j) == 0)
                {
                    break;
                }
            }

            if (j > 0)
            {
                byte = (grub_uint8_t)(*(grub_uint16_t *)(buf + j + 2));
                if (byte != '/' && byte != '\\')
                {
                    continue;
                }
                
                valid = 1;
                for (k = 0, j += 2; k < (int)sizeof(path) - 1 && j < i + 8; j += 2)
                {
                    byte = (grub_uint8_t)(*(grub_uint16_t *)(buf + j));
                    c = (char)byte;
                    if (byte > '~' || byte < ' ') /* not printable */
                    {
                        valid = 0;
                        break;
                    }
                    else if (c == '\\')
                    {
                        c = '/';
                    }
                    
                    path[k++] = c;
                }
                path[k++] = 0;

                debug("@@@@ Find wim flag:<%s>\n", path);

                if (0 == valid)
                {
                    debug("Invalid wim file %d\n", k);
                }
                else if (NULL == ventoy_find_wim_patch(path))
                {
                    node = grub_zalloc(sizeof(wim_patch));
                    if (node)
                    {
                        node->pathlen = grub_snprintf(node->path, sizeof(node->path), "%s", path);

                        debug("add patch <%s>\n", path);
                        
                        if (g_wim_patch_head)
                        {
                            node->next = g_wim_patch_head;
                        }
                        g_wim_patch_head = node;

                        g_wim_total_patch_count++;
                    }
                }
                else
                {
                    debug("wim <%s> already exist\n", path);
                }
            }
        }
    }

end:
    check_free(file, grub_file_close);
    grub_check_free(buf);
    return rc;
}

grub_err_t ventoy_cmd_wim_patch_count(grub_extcmd_context_t ctxt, int argc, char **args)
{
    char buf[32];
    
    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc == 1)
    {
        grub_snprintf(buf, sizeof(buf), "%d", g_wim_total_patch_count);
        ventoy_set_env(args[0], buf);
    }
    
    return 0;
}

grub_err_t ventoy_cmd_collect_wim_patch(grub_extcmd_context_t ctxt, int argc, char **args)
{
    wim_patch *node = NULL;
    
    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc != 2)
    {
        return 1;
    }

    debug("ventoy_cmd_collect_wim_patch %s %s\n", args[0], args[1]);

    if (grub_strcmp(args[0], "bcd") == 0)
    {
        ventoy_collect_wim_patch(args[1]);
        return 0;
    }

    if (NULL == ventoy_find_wim_patch(args[1]))
    {
        node = grub_zalloc(sizeof(wim_patch));
        if (node)
        {
            node->pathlen = grub_snprintf(node->path, sizeof(node->path), "%s", args[1]);

            debug("add patch <%s>\n", args[1]);
            
            if (g_wim_patch_head)
            {
                node->next = g_wim_patch_head;
            }
            g_wim_patch_head = node;

            g_wim_total_patch_count++;
        }
    }

    return 0;
}


grub_err_t ventoy_cmd_dump_wim_patch(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i = 0;
    wim_patch *node = NULL;

    (void)ctxt;
    (void)argc;
    (void)args;

    for (node = g_wim_patch_head; node; node = node->next)
    {
        grub_printf("%d %s [%s]\n", i++, node->path, node->valid ? "SUCCESS" : "FAIL");
    }

    return 0;
}


