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


static initrd_info * ventoy_find_initrd_by_name(initrd_info *list, const char *name)
{
    initrd_info *node = list;

    while (node)
    {
        if (grub_strcmp(node->name, name) == 0)
        {
            return node;
        }
        node = node->next;
    }

    return NULL;
}


static void ventoy_parse_directory(char *path, char *dir, int buflen)
{   
    int end;
    char *pos;

    pos = grub_strstr(path, ")");
    if (!pos)
    {
        pos = path;
    }

    end = grub_snprintf(dir, buflen, "%s", pos + 1);
    while (end > 0)
    {
        if (dir[end] == '/')
        {
            dir[end + 1] = 0;
            break;
        }
        end--;
    }
}

static grub_err_t ventoy_isolinux_initrd_collect(grub_file_t file, const char *prefix)
{
    int i = 0;
    int offset;
    int prefixlen = 0;
    char *buf = NULL;
    char *pos = NULL;
    char *start = NULL;
    char *nextline = NULL;
    initrd_info *img = NULL;

    prefixlen = grub_strlen(prefix);

    buf = grub_zalloc(file->size + 2);
    if (!buf)
    {
        return 0;
    }

    grub_file_read(file, buf, file->size);

    for (start = buf; start; start = nextline)
    {
        nextline = ventoy_get_line(start);

        VTOY_SKIP_SPACE(start);

        offset = 7; // strlen("initrd=") or "INITRD " or "initrd "
        pos = grub_strstr(start, "initrd=");
        if (pos == NULL)
        {
            pos = start;

            if (grub_strncmp(start, "INITRD", 6) != 0 && grub_strncmp(start, "initrd", 6) != 0)
            {
                if (grub_strstr(start, "xen") &&
                    ((pos = grub_strstr(start, "--- /install.img")) != NULL ||
                     (pos = grub_strstr(start, "--- initrd.img")) != NULL
                    ))
                {
                    offset = 4; // "--- "
                }
                else
                {
                    continue;
                }
            }
        }

        pos += offset; 

        while (1)
        {
            i = 0;
            img = grub_zalloc(sizeof(initrd_info));
            if (!img)
            {
                break;
            }

            if (*pos != '/')
            {
                grub_strcpy(img->name, prefix);
                i = prefixlen;
            }

            while (i < 255 && (0 == ventoy_is_word_end(*pos)))
            {
                img->name[i++] = *pos++;
            }

            if (ventoy_find_initrd_by_name(g_initrd_img_list, img->name))
            {
                grub_free(img);
            }
            else
            {
                if (g_initrd_img_list)
                {
                    img->prev = g_initrd_img_tail;
                    g_initrd_img_tail->next = img;
                }
                else
                {
                    g_initrd_img_list = img;
                }

                g_initrd_img_tail = img;
                g_initrd_img_count++;
            }

            if (*pos == ',')
            {
                pos++;
            }
            else
            {
                break;
            }
        }
    }

    grub_free(buf);
    return GRUB_ERR_NONE;
}

static int ventoy_isolinux_initrd_hook(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    grub_file_t file = NULL;
    ventoy_initrd_ctx *ctx = (ventoy_initrd_ctx *)data;

    (void)info;

    if (NULL == grub_strstr(filename, ".cfg") && NULL == grub_strstr(filename, ".CFG"))
    {
        return 0;
    }

    debug("init hook dir <%s%s>\n", ctx->path_prefix, filename);

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s", ctx->path_prefix, filename);
    if (!file)
    {
        return 0;
    }

    ventoy_isolinux_initrd_collect(file, ctx->dir_prefix);
    grub_file_close(file);

    return 0;
}

grub_err_t ventoy_cmd_isolinux_initrd_collect(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_fs_t fs;
    grub_device_t dev = NULL;
    char *device_name = NULL;
    ventoy_initrd_ctx ctx;
    char directory[256];
    
    (void)ctxt;
    (void)argc;

    device_name = grub_file_get_device_name(args[0]);
    if (!device_name)
    {
        goto end;
    }

    dev = grub_device_open(device_name);
    if (!dev)
    {
        goto end;        
    }

    fs = grub_fs_probe(dev);
    if (!fs)
    {
        goto end;
    }

    debug("isolinux initrd collect %s\n", args[0]);

    ventoy_parse_directory(args[0], directory, sizeof(directory) - 1);
    ctx.path_prefix = args[0];
    ctx.dir_prefix = (argc > 1) ? args[1] : directory;

    debug("path_prefix=<%s> dir_prefix=<%s>\n", ctx.path_prefix, ctx.dir_prefix);

    fs->fs_dir(dev, directory, ventoy_isolinux_initrd_hook, &ctx);

end:
    check_free(device_name, grub_free);
    check_free(dev, grub_device_close);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static int ventoy_linux_initrd_collect_hook(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    int len;
    initrd_info *img = NULL;
    
    (void)data;

    if (0 == info->dir)
    {
        if (grub_strncmp(filename, "initrd", 6) == 0)
        {
            len = (int)grub_strlen(filename);
            if (grub_strcmp(filename + len - 4, ".img") == 0)
            {
                img = grub_zalloc(sizeof(initrd_info));
                if (img)
                {
                    grub_snprintf(img->name, sizeof(img->name), "/boot/%s", filename);

                    if (ventoy_find_initrd_by_name(g_initrd_img_list, img->name))
                    {
                        grub_free(img);
                    }
                    else
                    {                        
                        if (g_initrd_img_list)
                        {
                            img->prev = g_initrd_img_tail;
                            g_initrd_img_tail->next = img;
                        }
                        else
                        {
                            g_initrd_img_list = img;
                        }

                        g_initrd_img_tail = img;
                        g_initrd_img_count++;
                    }
                }
            }
        }
    }

    return 0;
}

static int ventoy_linux_collect_boot_initrds(void)
{
    grub_fs_t fs;
    grub_device_t dev = NULL;

    dev = grub_device_open("loop");
    if (!dev)
    {
        debug("failed to open device loop\n");
        goto end;        
    }

    fs = grub_fs_probe(dev);
    if (!fs)
    {
        debug("failed to probe fs %d\n", grub_errno);
        goto end;
    }

    fs->fs_dir(dev, "/boot", ventoy_linux_initrd_collect_hook, NULL);

end:
    return 0;    
}

static grub_err_t ventoy_grub_cfg_initrd_collect(const char *fileName)
{
    int i = 0;
    int len = 0;
    int dollar = 0;
    int quotation = 0;
    int initrd_dollar = 0;
    grub_file_t file = NULL;
    char *buf = NULL;
    char *start = NULL;
    char *nextline = NULL;
    initrd_info *img = NULL;

    debug("grub initrd collect %s\n", fileName);

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", fileName);
    if (!file)
    {
        return 0;
    }
    
    buf = grub_zalloc(file->size + 2);
    if (!buf)
    {
        grub_file_close(file);
        return 0;
    }

    grub_file_read(file, buf, file->size);

    for (start = buf; start; start = nextline)
    {
        nextline = ventoy_get_line(start);

        VTOY_SKIP_SPACE(start);

        if (grub_strncmp(start, "initrd", 6) != 0)
        {
            continue;
        }

        start += 6;
        while (*start && (!ventoy_isspace(*start)))
        {
            start++;
        }

        VTOY_SKIP_SPACE(start);

        if (*start == '"')
        {
            quotation = 1;
            start++;
        }

        while (*start)
        {
            img = grub_zalloc(sizeof(initrd_info));
            if (!img)
            {
                break;
            }

            dollar = 0;
            for (i = 0; i < 255 && (0 == ventoy_is_word_end(*start)); i++)
            {
                img->name[i] = *start++;
                if (img->name[i] == '$')
                {
                    dollar = 1;
                }
            }

            if (quotation)
            {
                len = (int)grub_strlen(img->name);
                if (len > 2 && img->name[len - 1] == '"')
                {
                    img->name[len - 1] = 0;
                }
                debug("Remove quotation <%s>\n", img->name);
            }

            /* special process for /boot/initrd$XXX.img */
            if (dollar == 1)
            {
                if (grub_strncmp(img->name, "/boot/initrd$", 13) == 0)
                {
                    len = (int)grub_strlen(img->name);
                    if (grub_strcmp(img->name + len - 4, ".img") == 0)
                    {
                        initrd_dollar++;                        
                    }                    
                }
            }

            if (dollar == 1 || ventoy_find_initrd_by_name(g_initrd_img_list, img->name))
            {
                grub_free(img);
            }
            else
            {
                if (g_initrd_img_list)
                {
                    img->prev = g_initrd_img_tail;
                    g_initrd_img_tail->next = img;
                }
                else
                {
                    g_initrd_img_list = img;
                }

                g_initrd_img_tail = img;
                g_initrd_img_count++;
            }

            if (*start == ' ' || *start == '\t')
            {
                VTOY_SKIP_SPACE(start);
            }
            else
            {
                break;
            }
        }
    }

    grub_free(buf);
    grub_file_close(file);

    if (initrd_dollar > 0 && grub_strncmp(fileName, "(loop)/", 7) == 0)
    {
        debug("collect initrd variable %d\n", initrd_dollar);
        ventoy_linux_collect_boot_initrds();
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static int ventoy_grub_initrd_hook(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    char filePath[256];
    ventoy_initrd_ctx *ctx = (ventoy_initrd_ctx *)data;

    (void)info;

    debug("ventoy_grub_initrd_hook %s\n", filename);

    if (NULL == grub_strstr(filename, ".cfg") && 
        NULL == grub_strstr(filename, ".CFG") && 
        NULL == grub_strstr(filename, ".conf"))
    {
        return 0;
    }

    debug("init hook dir <%s%s>\n", ctx->path_prefix, filename);

    grub_snprintf(filePath, sizeof(filePath) - 1, "%s%s", ctx->dir_prefix, filename);
    ventoy_grub_cfg_initrd_collect(filePath);

    return 0;
}

grub_err_t ventoy_cmd_grub_initrd_collect(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_fs_t fs;
    grub_device_t dev = NULL;
    char *device_name = NULL;
    ventoy_initrd_ctx ctx;
    
    (void)ctxt;
    (void)argc;

    if (argc != 2)
    {
        return 0;
    }

    debug("grub initrd collect %s %s\n", args[0], args[1]);

    if (grub_strcmp(args[0], "file") == 0)
    {
        return ventoy_grub_cfg_initrd_collect(args[1]);
    }

    device_name = grub_file_get_device_name(args[1]);
    if (!device_name)
    {
        debug("failed to get device name %s\n", args[1]);
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

    ctx.dir_prefix = args[1];
    ctx.path_prefix = grub_strstr(args[1], device_name);
    if (ctx.path_prefix)
    {
        ctx.path_prefix += grub_strlen(device_name) + 1;
    }
    else
    {
        ctx.path_prefix = args[1];
    }

    debug("ctx.path_prefix:<%s>\n", ctx.path_prefix);

    fs->fs_dir(dev, ctx.path_prefix, ventoy_grub_initrd_hook, &ctx);

end:
    check_free(device_name, grub_free);
    check_free(dev, grub_device_close);


    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_specify_initrd_file(grub_extcmd_context_t ctxt, int argc, char **args)
{
    initrd_info *img = NULL;

    (void)ctxt;
    (void)argc;

    debug("ventoy_cmd_specify_initrd_file %s\n", args[0]);

    img = grub_zalloc(sizeof(initrd_info));
    if (!img)
    {
        return 1;
    }

    grub_strncpy(img->name, args[0], sizeof(img->name));
    if (ventoy_find_initrd_by_name(g_initrd_img_list, img->name))
    {
        debug("%s is already exist\n", args[0]);
        grub_free(img);
    }
    else
    {
        if (g_initrd_img_list)
        {
            img->prev = g_initrd_img_tail;
            g_initrd_img_tail->next = img;
        }
        else
        {
            g_initrd_img_list = img;
        }

        g_initrd_img_tail = img;
        g_initrd_img_count++;
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}
