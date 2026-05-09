/******************************************************************************
 * ventoy_linux.c 
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
#include <grub/ventoy.h>
#include "ventoy_def.h"
#include "ventoy_linux_priv.h"

GRUB_MOD_LICENSE ("GPLv3+");

#define VTOY_APPEND_EXT_SIZE 4096
int g_append_ext_sector = 0;

char * ventoy_get_line(char *start)
{
    if (start == NULL)
    {
        return NULL;
    }

    while (*start && *start != '\n')
    {
        start++;
    }

    if (*start == 0)
    {
        return NULL;
    }
    else
    {
        *start = 0;
        return start + 1;
    }
}

grub_err_t ventoy_cmd_clear_initrd_list(grub_extcmd_context_t ctxt, int argc, char **args)
{
    initrd_info *node = g_initrd_img_list;
    initrd_info *next;
    
    (void)ctxt;
    (void)argc;
    (void)args;

    while (node)
    {
        next = node->next;
        grub_free(node);
        node = next;
    }

    g_initrd_img_list = NULL;
    g_initrd_img_tail = NULL;
    g_initrd_img_count = 0;
    g_valid_initrd_count = 0;

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_dump_initrd_list(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i = 0;
    initrd_info *node = g_initrd_img_list;
    
    (void)ctxt;
    (void)argc;
    (void)args;

    grub_printf("###################\n");
    grub_printf("initrd info list: valid count:%d\n", g_valid_initrd_count);

    while (node)
    {
        grub_printf("%s ", node->size > 0 ? "*" : " ");
        grub_printf("%02u %s  offset:%llu  size:%llu \n", i++, node->name, (unsigned long long)node->offset, 
                    (unsigned long long)node->size);
        node = node->next;
    }

    grub_printf("###################\n");

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_initrd_count(grub_extcmd_context_t ctxt, int argc, char **args)
{
    char buf[32] = {0};
    
    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc == 1)
    {
        grub_snprintf(buf, sizeof(buf), "%d", g_initrd_img_count);
        grub_env_set(args[0], buf);
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_valid_initrd_count(grub_extcmd_context_t ctxt, int argc, char **args)
{
    char buf[32] = {0};
    
    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc == 1)
    {
        grub_snprintf(buf, sizeof(buf), "%d", g_valid_initrd_count);
        grub_env_set(args[0], buf);
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static grub_err_t ventoy_linux_locate_initrd(int filt, int *filtcnt)
{
    int data;
    int filtbysize = 1;
    int sizefilt = 0;
    grub_file_t file;
    initrd_info *node;

    debug("ventoy_linux_locate_initrd %d\n", filt);

    g_valid_initrd_count = 0;

    if (grub_env_get("INITRD_NO_SIZE_FILT"))
    {
        filtbysize = 0;
    }
    
    for (node = g_initrd_img_list; node; node = node->next)
    {
        file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "(loop)%s", node->name);
        if (!file)
        {
            continue;
        }

        debug("file <%s> size:%d\n", node->name, (int)file->size);

        /* initrd file too small */
        if (filtbysize 
            && (NULL == grub_strstr(node->name, "minirt.gz"))
            && (NULL == grub_strstr(node->name, "initrd.xz"))
            && (NULL == grub_strstr(node->name, "initrd.gz"))
            )
        {
            if (filt > 0 && file->size <= g_ventoy_cpio_size + 2048)
            {
                debug("file size too small %d\n", (int)g_ventoy_cpio_size);
                grub_file_close(file);
                sizefilt++;
                continue;
            }
        }

        /* skip hdt.img */
        if (file->size <= VTOY_SIZE_1MB && grub_strcmp(node->name, "/boot/hdt.img") == 0)
        {
            continue;
        }

        if (grub_strcmp(file->fs->name, "iso9660") == 0)
        {
            node->iso_type = 0;
            node->override_offset = grub_iso9660_get_last_file_dirent_pos(file) + 2;
            
            grub_file_read(file, &data, 1); // just read for hook trigger
            node->offset = grub_iso9660_get_last_read_pos(file);
        }
        else
        {
            /* TBD */
        }

        node->size = file->size;
        g_valid_initrd_count++;

        grub_file_close(file);
    }

    *filtcnt = sizefilt;

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}


grub_err_t ventoy_cmd_linux_get_main_initrd_index(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int index = 0;
    char buf[32];
    initrd_info *node = NULL;
    
    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc != 1)
    {
        return 1;
    }

    if (g_initrd_img_count == 1)
    {
        ventoy_set_env(args[0], "0");
        VENTOY_CMD_RETURN(GRUB_ERR_NONE);
    }

    for (node = g_initrd_img_list; node; node = node->next)
    {
        if (node->size <= 0)
        {
            continue;
        }
    
        if (grub_strstr(node->name, "ucode") || grub_strstr(node->name, "-firmware"))
        {
            index++;
            continue;
        }

        grub_snprintf(buf, sizeof(buf), "%d", index);
        ventoy_set_env(args[0], buf);
        break;
    }

    debug("main initrd index:%d\n", index);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_linux_locate_initrd(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int sizefilt = 0;

    (void)ctxt;
    (void)argc;
    (void)args;

    ventoy_linux_locate_initrd(1, &sizefilt);

    if (g_valid_initrd_count == 0 && sizefilt > 0)
    {
        ventoy_linux_locate_initrd(0, &sizefilt);
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}
