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
int ventoy_collect_replace_initrd(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    int curpos;
    int printlen;
    grub_size_t len;
    replace_fs_dir *pfsdir = (replace_fs_dir *)data;

    if (pfsdir->initrd[0])
    {
        return 1;
    }

    curpos = pfsdir->curpos;
    len = grub_strlen(filename);

    if (info->dir)
    {
        if ((len == 1 && filename[0] == '.') ||
            (len == 2 && filename[0] == '.' && filename[1] == '.'))
        {
            return 0;
        }

        //debug("#### [DIR] <%s> <%s>\n", pfsdir->fullpath, filename);
        pfsdir->dircnt++;

        printlen = grub_snprintf(pfsdir->fullpath + curpos, 512 - curpos, "%s/", filename);
        pfsdir->curpos = curpos + printlen;
        pfsdir->fs->fs_dir(pfsdir->dev, pfsdir->fullpath, ventoy_collect_replace_initrd, pfsdir);
        pfsdir->curpos = curpos;
        pfsdir->fullpath[curpos] = 0;
    }
    else
    {
        //debug("#### [FILE] <%s> <%s>\n", pfsdir->fullpath, filename);
        pfsdir->filecnt++;

        /* We consider the xxx.img file bigger than 32MB is the initramfs file */
        if (len > 4 && grub_strncmp(filename + len - 4, ".img", 4) == 0)
        {
            if (info->size > 32 * VTOY_SIZE_1MB)
            {
                grub_snprintf(pfsdir->initrd, sizeof(pfsdir->initrd), "%s%s", pfsdir->fullpath, filename);
                return 1;
            }
        }
    }

    return 0;
}

grub_err_t ventoy_cmd_search_replace_initrd(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    char *pos = NULL;
    char *device_name = NULL;
    grub_device_t dev = NULL;
    grub_fs_t fs = NULL;
    replace_fs_dir *pfsdir = NULL;

    (void)ctxt;

    if (argc != 2)
    {
        debug("ventoy_cmd_search_replace_initrd, invalid param num %d\n", argc);
        return 1;
    }

    pfsdir = grub_zalloc(sizeof(replace_fs_dir));
    if (!pfsdir)
    {
        return 1;
    }

    device_name = grub_file_get_device_name(args[0]);
    if (!device_name)
    {
        goto fail;
    }

    dev = grub_device_open(device_name);
    if (!dev)
    {
        goto fail;
    }

    fs = grub_fs_probe(dev);
    if (!fs)
    {
        goto fail;
    }

    pfsdir->dev = dev;
    pfsdir->fs = fs;
    pfsdir->curpos = 1;
    pfsdir->fullpath[0] = '/';
    fs->fs_dir(dev, "/", ventoy_collect_replace_initrd, pfsdir);

    if (pfsdir->initrd[0])
    {
        debug("Replace initrd <%s> <%d %d>\n", pfsdir->initrd, pfsdir->dircnt, pfsdir->filecnt);

        for (i = 0; i < (int)sizeof(pfsdir->initrd) && pfsdir->initrd[i]; i++)
        {
            if (pfsdir->initrd[i] == '/')
            {
                pfsdir->initrd[i] = '\\';
            }
        }

        pos = (pfsdir->initrd[0] == '\\') ? pfsdir->initrd + 1 : pfsdir->initrd;
        grub_env_set(args[1], pos);
    }
    else
    {
        debug("Replace initrd NOT found <%s> <%d %d>\n", args[0], pfsdir->dircnt, pfsdir->filecnt);
    }

fail:

    grub_check_free(pfsdir);
    grub_check_free(device_name);
    check_free(dev, grub_device_close);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_push_pager(grub_extcmd_context_t ctxt, int argc, char **args)
{
    const char *pager = NULL;

    (void)ctxt;
    (void)argc;
    (void)args;

    pager = grub_env_get("pager");
    if (NULL == pager)
    {
        g_pager_flag = 1;
        grub_env_set("pager", "1");
    }
    else if (pager[0] == '1')
    {
        g_pager_flag = 0;
    }
    else
    {
        grub_snprintf(g_old_pager, sizeof(g_old_pager), "%s", pager);
        g_pager_flag = 2;
        grub_env_set("pager", "1");
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_pop_pager(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    if (g_pager_flag == 1)
    {
        grub_env_unset("pager");
    }
    else if (g_pager_flag == 2)
    {
        grub_env_set("pager", g_old_pager);
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

int ventoy_chk_case_file(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    if (g_json_case_mis_path[0])
    {
        return 1;
    }

    if (0 == info->dir && grub_strcasecmp(filename, "ventoy.json") == 0)
    {
        grub_snprintf(g_json_case_mis_path, 32, "%s/%s", (char *)data, filename);
        return 1;
    }
    return 0;
}

int ventoy_chk_case_dir(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    char path[16];
    chk_case_fs_dir *fs_dir = (chk_case_fs_dir *)data;

    if (g_json_case_mis_path[0])
    {
        return 1;
    }

    if (info->dir && (filename[0] == 'v' || filename[0] == 'V'))
    {
        if (grub_strcasecmp(filename, "ventoy") == 0)
        {
            grub_snprintf(path, sizeof(path), "/%s", filename);
            fs_dir->fs->fs_dir(fs_dir->dev, path, ventoy_chk_case_file, path);
            if (g_json_case_mis_path[0])
            {
                return 1;
            }
        }
    }

    return 0;
}

grub_err_t ventoy_cmd_chk_json_pathcase(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int fstype = 0;
    char *device_name = NULL;
    grub_device_t dev = NULL;
    grub_fs_t fs = NULL;
    chk_case_fs_dir fs_dir;

    (void)ctxt;
    (void)argc;
    (void)args;

    device_name = grub_file_get_device_name(args[0]);
    if (!device_name)
    {
        goto out;
    }

    dev = grub_device_open(device_name);
    if (!dev)
    {
        goto out;
    }

    fs = grub_fs_probe(dev);
    if (!fs)
    {
        goto out;
    }

    fstype = ventoy_get_fs_type(fs->name);
    if (fstype == ventoy_fs_fat || fstype == ventoy_fs_exfat || fstype >= ventoy_fs_max)
    {
        goto out;
    }

    g_json_case_mis_path[0] = 0;
    fs_dir.dev = dev;
    fs_dir.fs = fs;
    fs->fs_dir(dev, "/", ventoy_chk_case_dir, &fs_dir);

    if (g_json_case_mis_path[0])
    {
        grub_env_set("VTOY_PLUGIN_PATH_CASE_MISMATCH", g_json_case_mis_path);
    }

out:

    grub_check_free(device_name);
    check_free(dev, grub_device_close);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

