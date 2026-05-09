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


#define VTOY_APPEND_EXT_SIZE 4096

static char *ventoy_systemd_conf_tag(char *buf, const char *tag, int optional)
{
    int taglen = 0;
    char *start = NULL;
    char *nextline = NULL;

    taglen = grub_strlen(tag);
    for (start = buf; start; start = nextline)
    {
        nextline = ventoy_get_line(start);
        VTOY_SKIP_SPACE(start);

        if (grub_strncmp(start, tag, taglen) == 0 && (start[taglen] == ' ' || start[taglen] == '\t'))
        {
            start += taglen;
            VTOY_SKIP_SPACE(start); 
            return start;
        }
    }

    if (optional == 0)
    {
        debug("tag<%s> NOT found\n", tag);        
    }
    return NULL;
}

static int ventoy_systemd_conf_hook(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    int oldpos = 0;
    char *tag = NULL;
    char *bkbuf = NULL;
    char *filebuf = NULL;
    grub_file_t file = NULL;
    systemd_menu_ctx *ctx = (systemd_menu_ctx *)data;

    debug("ventoy_systemd_conf_hook %s\n", filename);

    if (info->dir || NULL == grub_strstr(filename, ".conf"))
    {
        return 0;
    }


    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s/loader/entries/%s", ctx->dev, filename);
    if (!file)
    {
        return 0;
    }

    filebuf = grub_zalloc(2 * file->size + 8);
    if (!filebuf)
    {
        goto out;
    }

    bkbuf = filebuf + file->size + 4;
    grub_file_read(file, bkbuf, file->size);

    oldpos = ctx->pos;

    /* title --> menuentry */
    grub_memcpy(filebuf, bkbuf, file->size);
    tag = ventoy_systemd_conf_tag(filebuf, "title", 0);
    vtoy_check_goto_out(tag);
    vtoy_len_ssprintf(ctx->buf, ctx->pos, ctx->len, "menuentry \"%s\" {\n", tag);

    /* linux xxx */
    grub_memcpy(filebuf, bkbuf, file->size);
    tag = ventoy_systemd_conf_tag(filebuf, "linux", 0);
    if (!tag)
    {
        ctx->pos = oldpos;
        goto out;
    }
    vtoy_len_ssprintf(ctx->buf, ctx->pos, ctx->len, "  echo \"Loading kernel ...\"\n  linux %s ", tag);

    /* kernel options */
    grub_memcpy(filebuf, bkbuf, file->size);
    tag = ventoy_systemd_conf_tag(filebuf, "options", 0);
    vtoy_len_ssprintf(ctx->buf, ctx->pos, ctx->len, "%s \n", tag ? tag : "");

    
    /* initrd xxx xxx xxx */
    vtoy_len_ssprintf(ctx->buf, ctx->pos, ctx->len, "  echo \"Loading initrd ...\"\n  %s ", ctx->initrd_cmd);
    grub_memcpy(filebuf, bkbuf, file->size);
    tag = ventoy_systemd_conf_tag(filebuf, "initrd", 1);
    while (tag)
    {
        vtoy_len_ssprintf(ctx->buf, ctx->pos, ctx->len, "%s ", tag);
        tag = ventoy_systemd_conf_tag(tag + grub_strlen(tag) + 1, "initrd", 1);
    }

    vtoy_len_ssprintf(ctx->buf, ctx->pos, ctx->len, "\n  boot\n}\n");

out:
    grub_check_free(filebuf);
    grub_file_close(file);
    return 0;
}

grub_err_t ventoy_cmd_linux_initrd(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;    
    int pos = 0;
    char *buf = NULL;

    (void)ctxt;

    buf = (char *)grub_malloc(VTOY_SIZE_4KB);
    if (!buf)
    {
        return 1;
    }

    pos += grub_snprintf(buf + pos, VTOY_SIZE_4KB - pos, "initrd mem:%s:size:%s",
        grub_env_get("ventoy_cpio_addr"), grub_env_get("ventoy_cpio_size"));
    
    for (i = 0; i < argc; i++)
    {
        pos += grub_snprintf(buf + pos, VTOY_SIZE_4KB - pos, " newc:initrd%03d:%s", i + 1, args[i]);
    }

    grub_script_execute_sourcecode(buf);
    grub_free(buf);

    return 0;
}

grub_err_t ventoy_cmd_linux_systemd_menu(grub_extcmd_context_t ctxt, int argc, char **args)
{
    static char *buf = NULL;
    grub_fs_t fs;
    char *device_name = NULL;
    grub_device_t dev = NULL;
    systemd_menu_ctx ctx;
    
    (void)ctxt;
    (void)argc;

    if (!buf)
    {
        buf = grub_malloc(VTOY_LINUX_SYSTEMD_MENU_MAX_BUF);
        if (!buf)
        {
            goto end;
        }        
    }

    device_name = grub_file_get_device_name(args[0]);
    if (!device_name)
    {
        debug("failed to get device name %s\n", args[0]);
        goto end;
    }

    dev = grub_device_open(device_name);
    if (!dev)
    {
        debug("failed to open device %s\n", device_name);
        goto end;        
    }

    fs = grub_fs_probe(dev);
    if (!fs)
    {
        debug("failed to probe fs %d\n", grub_errno);
        goto end;
    }

    ctx.dev = args[0];
    ctx.buf = buf;
    ctx.initrd_cmd = args[2] ? args[2] : "initrd";
    ctx.pos = 0;
    ctx.len = VTOY_LINUX_SYSTEMD_MENU_MAX_BUF;
    fs->fs_dir(dev, "/loader/entries", ventoy_systemd_conf_hook, &ctx);

    ventoy_memfile_env_set(args[1], buf, (ulonglong)(ctx.pos));

end:
    grub_check_free(device_name);
    check_free(dev, grub_device_close);
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static int ventoy_limine_path_convert(char *path)
{
    char newpath[256] = {0};

    if (grub_strncmp(path, "boot://2/", 9) == 0)
    {
        grub_snprintf(newpath, sizeof(newpath), "(vtimghd,2)/%s", path + 9);
    }
    else if (grub_strncmp(path, "boot://1/", 9) == 0)
    {
        grub_snprintf(newpath, sizeof(newpath), "(vtimghd,1)/%s", path + 9);
    }

    if (newpath[0])
    {
        grub_snprintf(path, 1024, "%s", newpath);
    }

    return 0;
}

grub_err_t ventoy_cmd_linux_limine_menu(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int pos = 0;
    int sub = 0;
    int len = VTOY_LINUX_SYSTEMD_MENU_MAX_BUF;
    char *filebuf = NULL;
    char *start = NULL;
    char *nextline = NULL;
    grub_file_t file = NULL;
    char *title = NULL;
    char *kernel = NULL;
    char *initrd = NULL;
    char *param = NULL;
    static char *buf = NULL;
    
    (void)ctxt;
    (void)argc;

    if (!buf)
    {
        buf = grub_malloc(len + 4 * 1024);
        if (!buf)
        {
            goto end;
        }        
    }

    title = buf + len;
    kernel = title + 1024;
    initrd = kernel + 1024;
    param = initrd + 1024;
    
    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, args[0]);
    if (!file)
    {
        return 0;
    }

    filebuf = grub_zalloc(file->size + 8);
    if (!filebuf)
    {
        goto end;
    }

    grub_file_read(file, filebuf, file->size);
    grub_file_close(file);

    
    title[0] = kernel[0] = initrd[0] = param[0] = 0;
    for (start = filebuf; start; start = nextline)
    {
        nextline = ventoy_get_line(start);
        VTOY_SKIP_SPACE(start);

        if (start[0] == ':')
        {
            if (start[1] == ':')
            {
                grub_snprintf(title, 1024, "%s", start + 2);
            }
            else
            {
                if (sub)
                {
                    vtoy_len_ssprintf(buf, pos, len, "}\n");
                    sub = 0;
                }

                if (nextline && nextline[0] == ':' && nextline[1] == ':')
                {
                    vtoy_len_ssprintf(buf, pos, len, "submenu \"[+] %s\" {\n", start + 2);
                    sub = 1;
                    title[0] = 0;
                }
                else
                {
                    grub_snprintf(title, 1024, "%s", start + 1);                    
                }
            }
        }
        else if (grub_strncmp(start, "KERNEL_PATH=", 12) == 0)
        {
            grub_snprintf(kernel, 1024, "%s", start + 12);
        }
        else if (grub_strncmp(start, "MODULE_PATH=", 12) == 0)
        {
            grub_snprintf(initrd, 1024, "%s", start + 12);
        }
        else if (grub_strncmp(start, "KERNEL_CMDLINE=", 15) == 0)
        {
            grub_snprintf(param, 1024, "%s", start + 15);
        }

        if (title[0] && kernel[0] && initrd[0] && param[0])
        {
            ventoy_limine_path_convert(kernel);
            ventoy_limine_path_convert(initrd);
        
            vtoy_len_ssprintf(buf, pos, len, "menuentry \"%s\" {\n", title);
            vtoy_len_ssprintf(buf, pos, len, "  echo \"Downloading kernel ...\"\n  linux %s %s\n", kernel, param);
            vtoy_len_ssprintf(buf, pos, len, "  echo \"Downloading initrd ...\"\n  initrd %s\n", initrd);
            vtoy_len_ssprintf(buf, pos, len, "}\n");
        
            title[0] = kernel[0] = initrd[0] = param[0] = 0;
        }
    }

    if (sub)
    {
        vtoy_len_ssprintf(buf, pos, len, "}\n");
        sub = 0;
    }

    ventoy_memfile_env_set(args[1], buf, (ulonglong)pos);

end:
    grub_check_free(filebuf);
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

