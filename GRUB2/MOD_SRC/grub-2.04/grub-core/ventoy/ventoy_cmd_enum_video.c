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
int ventoy_fs_enum_1st_file(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    if (!info->dir)
    {
        grub_snprintf((char *)data, 256, "%s", filename);
        return 1;
    }

    return 0;
}

int ventoy_fs_enum_1st_dir(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    if (info->dir && filename && filename[0] != '.')
    {
        grub_snprintf((char *)data, 256, "%s", filename);
        return 1;
    }

    return 0;
}

grub_err_t ventoy_fs_enum_1st_child(int argc, char **args, grub_fs_dir_hook_t hook)
{
    int rc = 1;
    char *device_name = NULL;
    grub_device_t dev = NULL;
    grub_fs_t fs = NULL;
    char name[256] ={0};

    if (argc != 3)
    {
        debug("ventoy_fs_enum_1st_child, invalid param num %d\n", argc);
        return 1;
    }

    device_name = grub_file_get_device_name(args[0]);
    if (!device_name)
    {
        debug("grub_file_get_device_name failed, %s\n", args[0]);
        goto end;
    }

    dev = grub_device_open(device_name);
    if (!dev)
    {
        debug("grub_device_open failed, %s\n", device_name);
        goto end;
    }

    fs = grub_fs_probe(dev);
    if (!fs)
    {
        debug("grub_fs_probe failed, %s\n", device_name);
        goto end;
    }

    fs->fs_dir(dev, args[1], hook, name);
    if (name[0])
    {
        ventoy_set_env(args[2], name);
    }

    rc = 0;

end:

    check_free(device_name, grub_free);
    check_free(dev, grub_device_close);

    return rc;
}

grub_err_t ventoy_cmd_fs_enum_1st_file(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    return ventoy_fs_enum_1st_child(argc, args, ventoy_fs_enum_1st_file);
}

grub_err_t ventoy_cmd_fs_enum_1st_dir(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    return ventoy_fs_enum_1st_child(argc, args, ventoy_fs_enum_1st_dir);
}

grub_err_t ventoy_cmd_basename(grub_extcmd_context_t ctxt, int argc, char **args)
{
    char c;
    char *pos = NULL;
    char *end = NULL;

    (void)ctxt;

    if (argc != 2)
    {
        debug("ventoy_cmd_basename, invalid param num %d\n", argc);
        return 1;
    }

    for (pos = args[0]; *pos; pos++)
    {
        if (*pos == '.')
        {
            end = pos;
        }
    }

    if (end)
    {
        c = *end;
        *end = 0;
    }

    grub_env_set(args[1], args[0]);

    if (end)
    {
        *end = c;
    }

    return 0;
}

grub_err_t ventoy_cmd_basefile(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    int len;
    const char *buf;

    (void)ctxt;

    if (argc != 2)
    {
        debug("ventoy_cmd_basefile, invalid param num %d\n", argc);
        return 1;
    }

    buf = args[0];
    len = (int)grub_strlen(buf);
    for (i = len; i > 0; i--)
    {
        if (buf[i - 1] == '/')
        {
            grub_env_set(args[1], buf + i);
            return 0;
        }
    }

    grub_env_set(args[1], buf);

    return 0;
}

grub_err_t ventoy_cmd_enum_video_mode(grub_extcmd_context_t ctxt, int argc, char **args)
{
    struct grub_video_mode_info info;
    char buf[32];

    (void)ctxt;
    (void)argc;
    (void)args;

    if (!g_video_mode_list)
    {
        ventoy_enum_video_mode();
    }

    if (grub_video_get_info(&info) == GRUB_ERR_NONE)
    {
        grub_snprintf(buf, sizeof(buf), "Resolution (%ux%u)", info.width, info.height);
    }
    else
    {
        grub_snprintf(buf, sizeof(buf), "Resolution (0x0)");
    }

    grub_env_set("VTOY_CUR_VIDEO_MODE", buf);

    grub_snprintf(buf, sizeof(buf), "%d", g_video_mode_num);
    grub_env_set("VTOY_VIDEO_MODE_NUM", buf);

    VENTOY_CMD_RETURN(0);
}

grub_err_t vt_cmd_update_cur_video_mode(grub_extcmd_context_t ctxt, int argc, char **args)
{
    struct grub_video_mode_info info;
    char buf[32];

    (void)ctxt;
    (void)argc;
    (void)args;

    if (grub_video_get_info(&info) == GRUB_ERR_NONE)
    {
        grub_snprintf(buf, sizeof(buf), "%ux%ux%u", info.width, info.height, info.bpp);
    }
    else
    {
        grub_snprintf(buf, sizeof(buf), "0x0x0");
    }

    grub_env_set(args[0], buf);

    VENTOY_CMD_RETURN(0);
}

grub_err_t ventoy_cmd_get_video_mode(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int id;
    char buf[32];

    (void)ctxt;
    (void)argc;

    if (!g_video_mode_list)
    {
        return 0;
    }

    id = (int)grub_strtoul(args[0], NULL, 10);
    if (id < g_video_mode_num)
    {
        grub_snprintf(buf, sizeof(buf), "%ux%ux%u",
            g_video_mode_list[id].width, g_video_mode_list[id].height, g_video_mode_list[id].bpp);
    }

    grub_env_set(args[1], buf);

    VENTOY_CMD_RETURN(0);
}

grub_err_t ventoy_cmd_get_efivdisk_offset(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_uint32_t i;
    grub_uint32_t loadsector = 0;
    grub_file_t file;
    char value[32];
    grub_uint32_t boot_catlog = 0;
    grub_uint8_t buf[512];

    (void)ctxt;

    if (argc != 2)
    {
        debug("ventoy_cmd_get_efivdisk_offset, invalid param num %d\n", argc);
        return 1;
    }

    file = grub_file_open(args[0], VENTOY_FILE_TYPE);
    if (!file)
    {
        debug("failed to open %s\n", args[0]);
        return 1;
    }

    boot_catlog = ventoy_get_iso_boot_catlog(file);
    if (boot_catlog == 0)
    {
        debug("No bootcatlog found\n");
        grub_file_close(file);
        return 1;
    }

    grub_memset(buf, 0, sizeof(buf));
    grub_file_seek(file, boot_catlog * 2048);
    grub_file_read(file, buf, sizeof(buf));
    grub_file_close(file);

    for (i = 0; i < sizeof(buf); i += 32)
    {
        if ((buf[i] == 0 || buf[i] == 0x90 || buf[i] == 0x91) && buf[i + 1] == 0xEF)
        {
            if (buf[i + 32] == 0x88)
            {
                loadsector = *(grub_uint32_t *)(buf + i + 32 + 8);
                grub_snprintf(value, sizeof(value), "%u", loadsector * 4); //change to sector size 512
                break;
            }
        }
    }

    if (loadsector == 0)
    {
        debug("No EFI eltorito info found\n");
        return 1;
    }

    debug("ventoy_cmd_get_efivdisk_offset <%s>\n", value);
    grub_env_set(args[1], value);
    VENTOY_CMD_RETURN(0);
}

