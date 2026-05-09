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

static int ventoy_cpio_newc_get_int(char *value)
{
    char buf[16] = {0};

    grub_memcpy(buf, value, 8);
    return (int)grub_strtoul(buf, NULL, 16);
}

static void ventoy_cpio_newc_fill_int(grub_uint32_t value, char *buf, int buflen)
{
    int i;
    int len;
    char intbuf[32];

    len = grub_snprintf(intbuf, sizeof(intbuf), "%x", value);

    for (i = 0; i < buflen; i++)
    {
        buf[i] = '0';
    }

    if (len > buflen)
    {
        grub_printf("int buf len overflow %d %d\n", len, buflen);
    }
    else
    {
        grub_memcpy(buf + buflen - len, intbuf, len);    
    }
}

int ventoy_cpio_newc_fill_head(void *buf, int filesize, const void *filedata, const char *name)
{
    int namelen = 0;
    int headlen = 0;
    static grub_uint32_t cpio_ino = 0xFFFFFFF0;
    cpio_newc_header *cpio = (cpio_newc_header *)buf;
    
    namelen = grub_strlen(name) + 1;
    headlen = sizeof(cpio_newc_header) + namelen;
    headlen = ventoy_align(headlen, 4);

    grub_memset(cpio, '0', sizeof(cpio_newc_header));
    grub_memset(cpio + 1, 0, headlen - sizeof(cpio_newc_header));

    grub_memcpy(cpio->c_magic, "070701", 6);
    ventoy_cpio_newc_fill_int(cpio_ino--, cpio->c_ino, 8);
    ventoy_cpio_newc_fill_int(0100777, cpio->c_mode, 8);
    ventoy_cpio_newc_fill_int(1, cpio->c_nlink, 8);
    ventoy_cpio_newc_fill_int(filesize, cpio->c_filesize, 8);
    ventoy_cpio_newc_fill_int(namelen, cpio->c_namesize, 8);
    grub_memcpy(cpio + 1, name, namelen);

    if (filedata)
    {
        grub_memcpy((char *)cpio + headlen, filedata, filesize);
    }

    return headlen;
}
static int ventoy_cpio_busybox64(cpio_newc_header *head, const char *file)
{
    char *name;
    int namelen;
    int offset;
    int count = 0;
    char filepath[128];

    grub_snprintf(filepath, sizeof(filepath), "ventoy/busybox/%s", file);
    
    name = (char *)(head + 1);
    while (name[0] && count < 2)
    {
        if (grub_strcmp(name, "ventoy/busybox/ash") == 0)
        {
            grub_memcpy(name, "ventoy/busybox/32h", 18);
            count++;
        }
        else if (grub_strcmp(name, filepath) == 0)
        {
            grub_memcpy(name, "ventoy/busybox/ash", 18);
            count++;
        }

        namelen = ventoy_cpio_newc_get_int(head->c_namesize);
        offset = sizeof(cpio_newc_header) + namelen;
        offset = ventoy_align(offset, 4);
        offset += ventoy_cpio_newc_get_int(head->c_filesize);
        offset = ventoy_align(offset, 4);
        
        head = (cpio_newc_header *)((char *)head + offset);
        name = (char *)(head + 1);
    }

    return 0;
}


grub_err_t ventoy_cmd_cpio_busybox_64(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    debug("ventoy_cmd_busybox_64 %d\n", argc);
    ventoy_cpio_busybox64((cpio_newc_header *)g_ventoy_cpio_buf, args[0]);
    return 0;
}

grub_err_t ventoy_cmd_skip_svd(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    grub_file_t file;
    char buf[16];
    
    (void)ctxt;
    (void)argc;

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (!file)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Can't open file %s\n", args[0]); 
    }

    for (i = 0; i < 10; i++)
    {
        buf[0] = 0;
        grub_file_seek(file, (17 + i) * 2048);
        grub_file_read(file, buf, 16);

        if (buf[0] == 2 && grub_strncmp(buf + 1, "CD001", 5) == 0)
        {
            debug("Find SVD at VD %d\n", i);
            g_svd_replace_offset = (17 + i) * 2048;
            break;
        }
    }

    if (i >= 10)
    {
        debug("SVD not found %d\n", (int)g_svd_replace_offset);
    }

    grub_file_close(file);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_append_ext_sector(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    if (args[0][0] == '1')
    {
        g_append_ext_sector = 1;        
    }
    else
    {
        g_append_ext_sector = 0;
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_load_cpio(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    int rc;
    char *pos = NULL;
    char *template_file = NULL;
    char *template_buf = NULL;
    char *persistent_buf = NULL;
    char *injection_buf = NULL;
    dud *dudnode = NULL;
    char tmpname[128];
    const char *injection_file = NULL;
    grub_uint8_t *buf = NULL;
    grub_uint32_t mod;
    grub_uint32_t headlen;
    grub_uint32_t initrd_head_len;
    grub_uint32_t padlen;
    grub_uint32_t img_chunk_size;
    grub_uint32_t template_size = 0;
    grub_uint32_t persistent_size = 0;
    grub_uint32_t injection_size = 0;
    grub_uint32_t dud_size = 0;
    grub_file_t file;
    grub_file_t archfile;
    grub_file_t tmpfile;
    install_template *template_node = NULL;
    ventoy_img_chunk_list chunk_list;

    (void)ctxt;
    (void)argc;

    if (argc != 4)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s cpiofile\n", cmd_raw_name); 
    }

    if (g_img_chunk_list.chunk == NULL || g_img_chunk_list.cur_chunk == 0)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "image chunk is null\n");
    }

    img_chunk_size = g_img_chunk_list.cur_chunk * sizeof(ventoy_img_chunk);

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s/%s", args[0], VTOY_COMM_CPIO);
    if (!file)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Can't open file %s/%s\n", args[0], VTOY_COMM_CPIO); 
    }

    archfile = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s/%s", args[0], VTOY_ARCH_CPIO);
    if (!archfile)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Can't open file %s/%s\n", args[0], VTOY_ARCH_CPIO);
        grub_file_close(file);
    }

    debug("load %s %s success\n", VTOY_COMM_CPIO, VTOY_ARCH_CPIO);

    if (g_ventoy_cpio_buf)
    {
        grub_free(g_ventoy_cpio_buf);
        g_ventoy_cpio_buf = NULL;
        g_ventoy_cpio_size = 0;
    }

    rc = ventoy_plugin_get_persistent_chunklist(args[1], -1, &chunk_list);
    if (rc == 0 && chunk_list.cur_chunk > 0 && chunk_list.chunk)
    {
        persistent_size = chunk_list.cur_chunk * sizeof(ventoy_img_chunk);
        persistent_buf = (char *)(chunk_list.chunk);
    }

    template_file = ventoy_plugin_get_cur_install_template(args[1], &template_node);
    if (template_file)
    {
        debug("auto install template: <%s> <addr:%p> <len:%d>\n", 
            template_file, template_node->filebuf, template_node->filelen);
        
        template_size = template_node->filelen;
        template_buf = grub_malloc(template_size);
        if (template_buf)
        {
            grub_memcpy(template_buf, template_node->filebuf, template_size);
        }
    }
    else
    {
        debug("auto install script skipped or not configed %s\n", args[1]);
    }

    injection_file = ventoy_plugin_get_injection(args[1]);
    if (injection_file)
    {
        debug("injection archive: <%s>\n", injection_file);
        tmpfile = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s", args[2], injection_file);
        if (tmpfile)
        {
            debug("injection archive size:%d\n", (int)tmpfile->size);
            injection_size = tmpfile->size;
            injection_buf = grub_malloc(injection_size);
            if (injection_buf)
            {
                grub_file_read(tmpfile, injection_buf, injection_size);
            }

            grub_file_close(tmpfile);
        }
        else
        {
            debug("Failed to open injection archive %s%s\n", args[2], injection_file);
        }
    }
    else
    {
        debug("injection not configed %s\n", args[1]);
    }

    dudnode = ventoy_plugin_find_dud(args[1]);
    if (dudnode)
    {
        debug("dud file: <%d>\n", dudnode->dudnum);
        ventoy_plugin_load_dud(dudnode, args[2]);
        for (i = 0; i < dudnode->dudnum; i++)
        {
            if (dudnode->files[i].size > 0)
            {
                dud_size += dudnode->files[i].size + sizeof(cpio_newc_header);                
            }
        }
    }
    else
    {
        debug("dud not configed %s\n", args[1]);
    }

    g_ventoy_cpio_buf = grub_malloc(file->size + archfile->size + 40960 + template_size + 
        persistent_size + injection_size + dud_size + img_chunk_size);
    if (NULL == g_ventoy_cpio_buf)
    {
        grub_file_close(file);
        grub_file_close(archfile);
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Can't alloc memory %llu\n", file->size);
    }

    grub_file_read(file, g_ventoy_cpio_buf, file->size);
    buf = (grub_uint8_t *)(g_ventoy_cpio_buf + file->size - 4);
    while (*((grub_uint32_t *)buf) != 0x37303730)
    {
        buf -= 4;
    }

    grub_file_read(archfile, buf, archfile->size);
    buf += (archfile->size - 4);
    while (*((grub_uint32_t *)buf) != 0x37303730)
    {
        buf -= 4;
    }

    /* get initrd head len */
    initrd_head_len = ventoy_cpio_newc_fill_head(buf, 0, NULL, "initrd000.xx");

    /* step1: insert image chunk data to cpio */
    headlen = ventoy_cpio_newc_fill_head(buf, img_chunk_size, g_img_chunk_list.chunk, "ventoy/ventoy_image_map");
    buf += headlen + ventoy_align(img_chunk_size, 4);

    if (template_buf)
    {
        headlen = ventoy_cpio_newc_fill_head(buf, template_size, template_buf, "ventoy/autoinstall");
        buf += headlen + ventoy_align(template_size, 4);
        grub_check_free(template_buf);
    }

    if (persistent_size > 0 && persistent_buf)
    {
        headlen = ventoy_cpio_newc_fill_head(buf, persistent_size, persistent_buf, "ventoy/ventoy_persistent_map");
        buf += headlen + ventoy_align(persistent_size, 4);
        grub_check_free(persistent_buf);
    }

    if (injection_size > 0 && injection_buf)
    {
        headlen = ventoy_cpio_newc_fill_head(buf, injection_size, injection_buf, "ventoy/ventoy_injection");
        buf += headlen + ventoy_align(injection_size, 4);

        grub_free(injection_buf);
        injection_buf = NULL;
    }

    if (dud_size > 0)
    {
        for (i = 0; i < dudnode->dudnum; i++)
        {
            pos = grub_strrchr(dudnode->dudpath[i].path, '.');
            grub_snprintf(tmpname, sizeof(tmpname), "ventoy/ventoy_dud%d%s", i, (pos ? pos : ".iso"));
            dud_size = dudnode->files[i].size;
            headlen = ventoy_cpio_newc_fill_head(buf, dud_size, dudnode->files[i].buf, tmpname);
            buf += headlen + ventoy_align(dud_size, 4);
        }
    }

    /* step2: insert os param to cpio */
    headlen = ventoy_cpio_newc_fill_head(buf, 0, NULL, "ventoy/ventoy_os_param");
    padlen = sizeof(ventoy_os_param);
    g_ventoy_cpio_size = (grub_uint32_t)(buf - g_ventoy_cpio_buf) + headlen + padlen + initrd_head_len;
    mod = g_ventoy_cpio_size % 2048;
    if (mod)
    {
        g_ventoy_cpio_size += 2048 - mod;
        padlen += 2048 - mod;
    }

    /* update os param data size, the data will be updated before chain boot */
    ventoy_cpio_newc_fill_int(padlen, ((cpio_newc_header *)buf)->c_filesize, 8);
    g_ventoy_runtime_buf = (grub_uint8_t *)buf + headlen;

    /* step3: fill initrd cpio head, the file size will be updated before chain boot */
    g_ventoy_initrd_head = (cpio_newc_header *)(g_ventoy_runtime_buf + padlen);
    ventoy_cpio_newc_fill_head(g_ventoy_initrd_head, 0, NULL, "initrd000.xx");

    grub_file_close(file);
    grub_file_close(archfile);

    if (grub_strcmp(args[3], "busybox=64") == 0)
    {
        debug("cpio busybox proc %s\n", args[3]);
        ventoy_cpio_busybox64((cpio_newc_header *)g_ventoy_cpio_buf, "64h");
    }
    else if (grub_strcmp(args[3], "busybox=a64") == 0)
    {
        debug("cpio busybox proc %s\n", args[3]);
        ventoy_cpio_busybox64((cpio_newc_header *)g_ventoy_cpio_buf, "a64");
    }
    else if (grub_strcmp(args[3], "busybox=m64") == 0)
    {
        debug("cpio busybox proc %s\n", args[3]);
        ventoy_cpio_busybox64((cpio_newc_header *)g_ventoy_cpio_buf, "m64");
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_trailer_cpio(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int mod;
    int bufsize;
    int namelen;
    int offset;
    char *name;
    grub_uint8_t *bufend;
    cpio_newc_header *head;
    grub_file_t file;
    const grub_uint8_t trailler[124] = {
        0x30, 0x37, 0x30, 0x37, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
        0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
        0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x31, 0x30, 0x30,
        0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
        0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
        0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
        0x30, 0x30, 0x30, 0x30, 0x30, 0x42, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x54, 0x52,
        0x41, 0x49, 0x4C, 0x45, 0x52, 0x21, 0x21, 0x21, 0x00, 0x00, 0x00, 0x00 
    };

    (void)ctxt;
    (void)argc;

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s", args[0], args[1]);
    if (!file)
    {
        return 1;
    }

    grub_memset(g_ventoy_runtime_buf, 0, sizeof(ventoy_os_param));
    ventoy_fill_os_param(file, (ventoy_os_param *)g_ventoy_runtime_buf);

    grub_file_close(file);

    grub_memcpy(g_ventoy_initrd_head, trailler, sizeof(trailler));
    bufend = (grub_uint8_t *)g_ventoy_initrd_head + sizeof(trailler);

    bufsize = (int)(bufend - g_ventoy_cpio_buf);
    mod = bufsize % 512;
    if (mod)
    {
        grub_memset(bufend, 0, 512 - mod);
        bufsize += 512 - mod;
    }

    if (argc > 1 && grub_strcmp(args[2], "noinit") == 0)
    {
        head = (cpio_newc_header *)g_ventoy_cpio_buf;
        name = (char *)(head + 1);

        while (grub_strcmp(name, "TRAILER!!!"))
        {
            if (grub_strcmp(name, "init") == 0)
            {
                grub_memcpy(name, "xxxx", 4);
            }
            else if (grub_strcmp(name, "linuxrc") == 0)
            {
                grub_memcpy(name, "vtoyxrc", 7);
            }
            else if (grub_strcmp(name, "sbin") == 0)
            {
                grub_memcpy(name, "vtoy", 4);
            }
            else if (grub_strcmp(name, "sbin/init") == 0)
            {
                grub_memcpy(name, "vtoy/vtoy", 9);
            }

            namelen = ventoy_cpio_newc_get_int(head->c_namesize);
            offset = sizeof(cpio_newc_header) + namelen;
            offset = ventoy_align(offset, 4);
            offset += ventoy_cpio_newc_get_int(head->c_filesize);
            offset = ventoy_align(offset, 4);
            
            head = (cpio_newc_header *)((char *)head + offset);
            name = (char *)(head + 1);
        }
    }

    ventoy_memfile_env_set("ventoy_cpio", g_ventoy_cpio_buf, (ulonglong)bufsize);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}
