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
    else if (0 == grub_strcmp(args[1], "le"))
    {
        grub_errno = (value_long1 <= value_long2) ? GRUB_ERR_NONE : GRUB_ERR_TEST_FAILURE;
    }
    else
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s {Int1} { eq ne gt lt ge le } {Int2}", cmd_raw_name);
    }

    return grub_errno;
}

grub_err_t ventoy_cmd_device(grub_extcmd_context_t ctxt, int argc, char **args)
{
    char *pos = NULL;
    char buf[128] = {0};

    if (argc != 2)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s path var", cmd_raw_name);
    }

    grub_strncpy(buf, (args[0][0] == '(') ? args[0] + 1 : args[0], sizeof(buf) - 1);
    pos = grub_strstr(buf, ",");
    if (pos)
    {
        *pos = 0;
    }

    grub_env_set(args[1], buf);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_check_compatible(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    char buf[256];
    grub_disk_t disk;
    char *pos = NULL;
    const char *files[] = { "ventoy.dat", "VENTOY.DAT" };

    (void)ctxt;

    if (argc != 1)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s  (loop)", cmd_raw_name);
    }

    for (i = 0; i < (int)ARRAY_SIZE(files); i++)
    {
        grub_snprintf(buf, sizeof(buf) - 1, "[ -e \"%s/%s\" ]", args[0], files[i]);
        if (0 == grub_script_execute_sourcecode(buf))
        {
            debug("file %s exist, ventoy_compatible YES\n", buf);
            grub_env_set("ventoy_compatible", "YES");
            VENTOY_CMD_RETURN(GRUB_ERR_NONE);
        }
        else
        {
            debug("file %s NOT exist\n", buf);
        }
    }

    grub_snprintf(buf, sizeof(buf) - 1, "%s", args[0][0] == '(' ? (args[0] + 1) : args[0]);
    pos = grub_strstr(buf, ")");
    if (pos)
    {
        *pos = 0;
    }

    disk = grub_disk_open(buf);
    if (disk)
    {
        grub_disk_read(disk, 16 << 2, 0, 1024, g_img_swap_tmp_buf);
        grub_disk_close(disk);

        g_img_swap_tmp_buf[703] = 0;
        for (i = 318; i < 703; i++)
        {
            if (g_img_swap_tmp_buf[i] == 'V' &&
                0 == grub_strncmp(g_img_swap_tmp_buf + i, VENTOY_COMPATIBLE_STR, VENTOY_COMPATIBLE_STR_LEN))
            {
                debug("Ventoy compatible string exist at  %d, ventoy_compatible YES\n", i);
                grub_env_set("ventoy_compatible", "YES");
                VENTOY_CMD_RETURN(GRUB_ERR_NONE);
            }
        }
    }
    else
    {
        debug("failed to open disk <%s>\n", buf);
    }

    grub_env_set("ventoy_compatible", "NO");
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

int ventoy_cmp_img(img_info *img1, img_info *img2)
{
    char *s1, *s2;
    int c1 = 0;
    int c2 = 0;

    if (g_plugin_image_list == VENTOY_IMG_WHITE_LIST)
    {
        return (img1->plugin_list_index - img2->plugin_list_index);
    }

    for (s1 = img1->name, s2 = img2->name; *s1 && *s2; s1++, s2++)
    {
        c1 = *s1;
        c2 = *s2;

        if (0 == g_sort_case_sensitive)
        {
            if (grub_islower(c1))
            {
                c1 = c1 - 'a' + 'A';
            }

            if (grub_islower(c2))
            {
                c2 = c2 - 'a' + 'A';
            }
        }

        if (c1 != c2)
        {
            break;
        }
    }

    return (c1 - c2);
}

int ventoy_cmp_subdir(img_iterator_node *node1, img_iterator_node *node2)
{
    int i = 0;
    int c1 = 0;
    int c2 = 0;
    int len = 0;
    char *s1, *s2;

    if (g_plugin_image_list == VENTOY_IMG_WHITE_LIST)
    {
        return (node1->plugin_list_index - node2->plugin_list_index);
    }

    s1 = node1->dir;
    s2 = node2->dir;
    len = grub_min(node1->dirlen, node2->dirlen);

    for (i = 0; i < len - 1; i++)
    {
        c1 = *s1++;
        c2 = *s2++;

        if (0 == g_sort_case_sensitive)
        {
            if (grub_islower(c1))
            {
                c1 = c1 - 'a' + 'A';
            }

            if (grub_islower(c2))
            {
                c2 = c2 - 'a' + 'A';
            }
        }

        if (c1 != c2)
        {
            return (c1 - c2);
        }
    }

    if (len == node1->dirlen)
    {
        c1 = 0;
    }

    if (len == node2->dirlen)
    {
        c2 = 0;
    }

    return (c1 - c2);
}

void ventoy_swap_img(img_info *img1, img_info *img2)
{
    grub_memcpy(&g_img_swap_tmp, img1, sizeof(img_info));

    grub_memcpy(img1, img2, sizeof(img_info));
    img1->next = g_img_swap_tmp.next;
    img1->prev = g_img_swap_tmp.prev;

    g_img_swap_tmp.next = img2->next;
    g_img_swap_tmp.prev = img2->prev;
    grub_memcpy(img2, &g_img_swap_tmp, sizeof(img_info));
}

int ventoy_img_name_valid(const char *filename, grub_size_t namelen)
{
    (void)namelen;

    if (g_filt_dot_underscore_file && filename[0] == '.' && filename[1] == '_')
    {
        return 0;
    }

    return 1;
}

