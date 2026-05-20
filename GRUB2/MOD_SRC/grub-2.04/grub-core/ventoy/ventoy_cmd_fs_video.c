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
int ventoy_get_fs_type(const char *fs)
{
    if (NULL == fs)
    {
        return ventoy_fs_max;
    }
    else if (grub_strncmp(fs, "exfat", 5) == 0)
    {
        return ventoy_fs_exfat;
    }
    else if (grub_strncmp(fs, "ntfs", 4) == 0)
    {
        return ventoy_fs_ntfs;
    }
    else if (grub_strncmp(fs, "ext", 3) == 0)
    {
        return ventoy_fs_ext;
    }
    else if (grub_strncmp(fs, "xfs", 3) == 0)
    {
        return ventoy_fs_xfs;
    }
    else if (grub_strncmp(fs, "udf", 3) == 0)
    {
        return ventoy_fs_udf;
    }
    else if (grub_strncmp(fs, "fat", 3) == 0)
    {
        return ventoy_fs_fat;
    }
    else if (grub_strncmp(fs, "btrfs", 5) == 0)
    {
        return ventoy_fs_btrfs;
    }

    return ventoy_fs_max;
}

int ventoy_string_check(const char *str, grub_char_check_func check)
{
    if (!str)
    {
        return 0;
    }

    for ( ; *str; str++)
    {
        if (!check(*str))
        {
            return 0;
        }
    }

    return 1;
}


grub_ssize_t ventoy_fs_read(grub_file_t file, char *buf, grub_size_t len)
{
    grub_memcpy(buf, (char *)file->data + file->offset, len);
    return len;
}

int ventoy_control_get_flag(const char *key)
{
    const char *val = ventoy_get_env(key);

    if (val && val[0] == '1' && val[1] == 0)
    {
        return 1;
    }
    return 0;
}

grub_err_t ventoy_fs_close(grub_file_t file)
{
    grub_file_close(g_old_file);
    grub_free(file->data);

    file->device = 0;
    file->name = 0;

    return 0;
}

int ventoy_video_hook(const struct grub_video_mode_info *info, void *hook_arg)
{
    int i;

    (void)hook_arg;

    if (info->mode_type & GRUB_VIDEO_MODE_TYPE_PURE_TEXT)
    {
        return 0;
    }

    for (i = 0; i < g_video_mode_num; i++)
    {
        if (g_video_mode_list[i].width == info->width &&
            g_video_mode_list[i].height == info->height &&
            g_video_mode_list[i].bpp == info->bpp)
        {
            return 0;
        }
    }

    g_video_mode_list[g_video_mode_num].width = info->width;
    g_video_mode_list[g_video_mode_num].height = info->height;
    g_video_mode_list[g_video_mode_num].bpp = info->bpp;
    g_video_mode_num++;

    if (g_video_mode_num == g_video_mode_max)
    {
        g_video_mode_max *= 2;
        g_video_mode_list = grub_realloc(g_video_mode_list, g_video_mode_max * sizeof(ventoy_video_mode));
    }

    return 0;
}

int ventoy_video_mode_cmp(ventoy_video_mode *v1, ventoy_video_mode *v2)
{
    if (v1->bpp == v2->bpp)
    {
        if (v1->width == v2->width)
        {
            if (v1->height == v2->height)
            {
                return 0;
            }
            else
            {
                return (v1->height < v2->height) ? -1 : 1;
            }
        }
        else
        {
            return (v1->width < v2->width) ? -1 : 1;
        }
    }
    else
    {
        return (v1->bpp < v2->bpp) ? -1 : 1;
    }
}

int ventoy_enum_video_mode(void)
{
    int i, j;
    grub_video_adapter_t adapter;
    grub_video_driver_id_t id;
    ventoy_video_mode mode;

    g_video_mode_num = 0;
    g_video_mode_max = 1024;
    g_video_mode_list = grub_malloc(sizeof(ventoy_video_mode) * g_video_mode_max);
    if (!g_video_mode_list)
    {
        return 0;
    }

    #ifdef GRUB_MACHINE_PCBIOS
    grub_dl_load ("vbe");
    #endif

    id = grub_video_get_driver_id ();

    FOR_VIDEO_ADAPTERS (adapter)
    {
        if (!adapter->iterate ||
            (adapter->id != id && (id != GRUB_VIDEO_DRIVER_NONE ||
             adapter->init() != GRUB_ERR_NONE)))
        {
            continue;
        }

        adapter->iterate(ventoy_video_hook, NULL);

        if (adapter->id != id)
        {
            adapter->fini();
        }
    }

    /* sort video mode */
    for (i = 0; i < g_video_mode_num; i++)
    for (j = i + 1; j < g_video_mode_num; j++)
    {
        if (ventoy_video_mode_cmp(g_video_mode_list + i, g_video_mode_list + j) < 0)
        {
            grub_memcpy(&mode, g_video_mode_list + i, sizeof(ventoy_video_mode));
            grub_memcpy(g_video_mode_list + i, g_video_mode_list + j, sizeof(ventoy_video_mode));
            grub_memcpy(g_video_mode_list + j, &mode, sizeof(ventoy_video_mode));
        }
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

int ventoy_pre_parse_data(char *src, int size)
{
    char c;
    char *pos = NULL;
    char buf[256];

    if (size < 20 || grub_strncmp(src, "ventoy_left_top_color", 21))
    {
        return 0;
    }

    pos = src + 21;
    while (*pos && *pos != '\r' && *pos != '\n')
    {
        pos++;
    }

    c = *pos;
    *pos = 0;

    if (grub_strlen(src) > 200)
    {
        goto end;
    }

    grub_snprintf(buf, sizeof(buf),
        "regexp -s 1:%s -s 2:%s -s 3:%s \"@([^@]*)@([^@]*)@([^@]*)@\" \"%s\"",
        ventoy_left_key, ventoy_top_key, ventoy_color_key, src);

    grub_script_execute_sourcecode(buf);

end:
    *pos = c;
    return 0;
}

grub_file_t ventoy_wrapper_open(grub_file_t rawFile, enum grub_file_type type)
{
    int len;
    grub_file_t file;
    static struct grub_fs vtoy_fs =
    {
        .name = "vtoy",
        .fs_dir = 0,
        .fs_open = 0,
        .fs_read = ventoy_fs_read,
        .fs_close = ventoy_fs_close,
        .fs_label = 0,
        .next = 0
    };

    if (type != 52)
    {
        return rawFile;
    }

    file = (grub_file_t)grub_zalloc(sizeof (*file));
    if (!file)
    {
        return 0;
    }

    file->data = grub_malloc(rawFile->size + 4096);
    if (!file->data)
    {
        return 0;
    }

    grub_file_read(rawFile, file->data, rawFile->size);
    ventoy_pre_parse_data((char *)file->data, (int)rawFile->size);
    len = ventoy_fill_data(4096, (char *)file->data + rawFile->size);

    g_old_file = rawFile;

    file->size = rawFile->size + len;
    file->device = rawFile->device;
    file->fs = &vtoy_fs;
    file->not_easily_seekable = 1;

    return file;
}

