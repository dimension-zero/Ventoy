/******************************************************************************
 * ventoy_plugin.c 
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
#include <grub/crypto.h>
#include <grub/time.h>
#include <grub/font.h>
#include <grub/video.h>
#include <grub/ventoy.h>
#include "ventoy_def.h"
#include "ventoy_plugin_priv.h"

GRUB_MOD_LICENSE ("GPLv3+");

char g_arch_mode_suffix[64];
char g_iso_disk_name[128];

void ventoy_plugin_dump_injection(void)
{
    injection_config *node = NULL;

    for (node = g_injection_head; node; node = node->next)
    {
        grub_printf("\n%s:<%s>\n", (node->type == injection_type_file) ? "IMAGE" : "PARENT", node->isopath);
        grub_printf("ARCHIVE:<%s>\n", node->archive);
    }

    return;
}


void ventoy_plugin_dump_auto_install(void)
{
    int i;
    install_template *node = NULL;

    for (node = g_install_template_head; node; node = node->next)
    {
        grub_printf("\n%s:<%s> <%d>\n", 
            (node->type == auto_install_type_file) ? "IMAGE" : "PARENT",
            node->isopath, node->templatenum);
        for (i = 0; i < node->templatenum; i++)
        {
            grub_printf("SCRIPT %d:<%s>\n", i, node->templatepath[i].path);            
        }
    }

    return;
}

void ventoy_plugin_dump_persistence(void)
{
    int rc;
    int i = 0;
    persistence_config *node = NULL;
    ventoy_img_chunk_list chunk_list;

    for (node = g_persistence_head; node; node = node->next)
    {
        grub_printf("\nIMAGE:<%s> <%d>\n", node->isopath, node->backendnum);

        for (i = 0; i < node->backendnum; i++)
        {
            grub_printf("PERSIST %d:<%s>", i, node->backendpath[i].path);            
            rc = ventoy_plugin_get_persistent_chunklist(node->isopath, i, &chunk_list);
            if (rc == 0)
            {
                grub_printf(" [ SUCCESS ]\n");
                grub_free(chunk_list.chunk);
            }
            else
            {
                grub_printf(" [ FAILED ]\n");
            }
        }
    }

    return;
}

install_template * ventoy_plugin_find_install_template(const char *isopath)
{
    int len;
    install_template *node = NULL;

    if (!g_install_template_head)
    {
        return NULL;
    }

    len = (int)grub_strlen(isopath);
    for (node = g_install_template_head; node; node = node->next)
    {
        if (node->type == auto_install_type_file)
        {
            if (node->pathlen == len && ventoy_strcmp(node->isopath, isopath) == 0)
            {
                return node;
            }
        }
    }
    
    for (node = g_install_template_head; node; node = node->next)
    {
        if (node->type == auto_install_type_parent)
        {
            if (node->pathlen < len && ventoy_plugin_is_parent(node->isopath, node->pathlen, isopath))
            {
                return node;
            }
        }
    }

    return NULL;
}

char * ventoy_plugin_get_cur_install_template(const char *isopath, install_template **cur)
{
    install_template *node = NULL;

    if (cur)
    {
        *cur = NULL;
    }

    node = ventoy_plugin_find_install_template(isopath);
    if ((!node) || (!node->templatepath))
    {
        return NULL;
    }

    if (node->cursel < 0 || node->cursel >= node->templatenum)
    {
        return NULL;
    }

    if (cur)
    {
        *cur = node;
    }

    return node->templatepath[node->cursel].path;
}

persistence_config * ventoy_plugin_find_persistent(const char *isopath)
{
    int len;
    persistence_config *node = NULL;

    if (!g_persistence_head)
    {
        return NULL;
    }

    len = (int)grub_strlen(isopath);
    for (node = g_persistence_head; node; node = node->next)
    {
        if ((len == node->pathlen) && (ventoy_strcmp(node->isopath, isopath) == 0))
        {
            return node;
        }
    }

    return NULL;
}

int ventoy_plugin_get_persistent_chunklist(const char *isopath, int index, ventoy_img_chunk_list *chunk_list)
{
    int rc = 1;
    int len = 0;
    char *path = NULL;
    grub_uint64_t start = 0;
    grub_file_t file = NULL;
    persistence_config *node = NULL;

    node = ventoy_plugin_find_persistent(isopath);
    if ((!node) || (!node->backendpath))
    {
        return 1;
    }

    if (index < 0)
    {
        index = node->cursel;
    }

    if (index < 0 || index >= node->backendnum)
    {
        return 1;
    }

    path = node->backendpath[index].path;

    if (node->backendpath[index].vlnk_add == 0)
    {
        len = grub_strlen(path);
        if (len > 9 && grub_strncmp(path + len - 9, ".vlnk.dat", 9) == 0)
        {
            ventoy_add_vlnk_file(NULL, path);
            node->backendpath[index].vlnk_add = 1;
        }
    }
    
    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s", g_iso_disk_name, path);
    if (!file)
    {
        debug("Failed to open file %s%s\n", g_iso_disk_name, path);
        goto end;
    }

    grub_memset(chunk_list, 0, sizeof(ventoy_img_chunk_list));
    chunk_list->chunk = grub_malloc(sizeof(ventoy_img_chunk) * DEFAULT_CHUNK_NUM);
    if (NULL == chunk_list->chunk)
    {
        goto end;
    }
    
    chunk_list->max_chunk = DEFAULT_CHUNK_NUM;
    chunk_list->cur_chunk = 0;

    start = file->device->disk->partition->start;
    ventoy_get_block_list(file, chunk_list, start);
    
    if (0 != ventoy_check_block_list(file, chunk_list, start, NULL, 0))
    {
        grub_free(chunk_list->chunk);
        chunk_list->chunk = NULL;
        goto end;
    }

    rc = 0;

end:
    if (file)
        grub_file_close(file);

    return rc;
}

const char * ventoy_plugin_get_injection(const char *isopath)
{
    int len;
    injection_config *node = NULL;

    if (!g_injection_head)
    {
        return NULL;
    }

    len = (int)grub_strlen(isopath);
    for (node = g_injection_head; node; node = node->next)
    {
        if (node->type == injection_type_file)
        {
            if (node->pathlen == len && ventoy_strcmp(node->isopath, isopath) == 0)
            {
                return node->archive;
            }
        }
    }
    
    for (node = g_injection_head; node; node = node->next)
    {
        if (node->type == injection_type_parent)
        {
            if (node->pathlen < len && ventoy_plugin_is_parent(node->isopath, node->pathlen, isopath))
            {
                return node->archive;
            }
        }
    }

    return NULL;
}

const char * ventoy_plugin_get_menu_alias(int type, const char *isopath)
{
    int len;
    menu_alias *node = NULL;

    if (!g_menu_alias_head)
    {
        return NULL;
    }

    len = (int)grub_strlen(isopath);
    for (node = g_menu_alias_head; node; node = node->next)
    {
        if (node->type == type && node->pathlen && 
            node->pathlen == len && ventoy_strcmp(node->isopath, isopath) == 0)
        {
            return node->alias;
        }
    }

    return NULL;
}

const menu_tip * ventoy_plugin_get_menu_tip(int type, const char *isopath)
{
    int len;
    menu_tip *node = NULL;

    if (!g_menu_tip_head)
    {
        return NULL;
    }

    len = (int)grub_strlen(isopath);
    for (node = g_menu_tip_head; node; node = node->next)
    {
        if (node->type == type && node->pathlen && 
            node->pathlen == len && ventoy_strcmp(node->isopath, isopath) == 0)
        {
            return node;
        }
    }

    return NULL;
}

const char * ventoy_plugin_get_menu_class(int type, const char *name, const char *path)
{
    int namelen;
    int pathlen;
    menu_class *node = NULL;

    if (!g_menu_class_head)
    {
        return NULL;
    }
    
    namelen = (int)grub_strlen(name); 
    pathlen = (int)grub_strlen(path); 
    
    if (vtoy_class_image_file == type)
    {
        for (node = g_menu_class_head; node; node = node->next)
        {
            if (node->type != type)
            {
                continue;
            }

            if (node->parent == 0)
            {
                if ((node->patlen < namelen) && grub_strstr(name, node->pattern))
                {
                    return node->class;
                }
            }
        }
        
        for (node = g_menu_class_head; node; node = node->next)
        {
            if (node->type != type)
            {
                continue;
            }

            if (node->parent)
            {
                if ((node->patlen < pathlen) && ventoy_plugin_is_parent(node->pattern, node->patlen, path))
                {
                    return node->class;
                }
            }
        }
    }
    else
    {
        for (node = g_menu_class_head; node; node = node->next)
        {
            if (node->type == type && node->patlen == namelen && grub_strncmp(name, node->pattern, namelen) == 0)
            {
                return node->class;
            }
        }
    }

    return NULL;
}

int ventoy_plugin_add_custom_boot(const char *vcfgpath)
{
    int len;
    custom_boot *node = NULL;
    
    node = grub_zalloc(sizeof(custom_boot));
    if (node)
    {
        node->type = vtoy_custom_boot_image_file;
        node->pathlen = grub_snprintf(node->path, sizeof(node->path), "%s", vcfgpath);
        grub_snprintf(node->cfg, sizeof(node->cfg), "%s", vcfgpath);

        /* .vcfg */
        len = node->pathlen - 5;
        node->path[len] = 0;
        node->pathlen = len;

        if (g_custom_boot_head)
        {
            node->next = g_custom_boot_head;
        }
        g_custom_boot_head = node;
    }
    
    return 0;
}

const char * ventoy_plugin_get_custom_boot(const char *isopath)
{
    int i;
    int len;
    custom_boot *node = NULL;

    if (!g_custom_boot_head)
    {
        return NULL;
    }

    len = (int)grub_strlen(isopath);
    
    for (node = g_custom_boot_head; node; node = node->next)
    {
        if (node->type == vtoy_custom_boot_image_file)
        {
            if (node->pathlen == len && grub_strncmp(isopath, node->path, len) == 0)
            {
                return node->cfg;                
            }
        }
        else
        {
            if (node->pathlen < len && isopath[node->pathlen] == '/' && 
                grub_strncmp(isopath, node->path, node->pathlen) == 0)
            {
                for (i = node->pathlen + 1; i < len; i++)
                {
                    if (isopath[i] == '/')
                    {
                        break;
                    }
                }

                if (i >= len)
                {
                    return node->cfg;                
                }
            }
        }
    }

    return NULL;
}

grub_err_t ventoy_cmd_dump_custom_boot(grub_extcmd_context_t ctxt, int argc, char **args)
{
    custom_boot *node = NULL;

    (void)argc;
    (void)ctxt;
    (void)args;

    for (node = g_custom_boot_head; node; node = node->next)
    {
        grub_printf("[%s] <%s>:<%s>\n", (node->type == vtoy_custom_boot_directory) ? "dir" : "file", 
            node->path, node->cfg);
    }

    return 0;
}

int ventoy_plugin_check_memdisk(const char *isopath)
{
    int len;
    auto_memdisk *node = NULL;

    if (!g_auto_memdisk_head)
    {
        return 0;
    }

    len = (int)grub_strlen(isopath);    
    for (node = g_auto_memdisk_head; node; node = node->next)
    {
        if (node->pathlen == len && ventoy_strncmp(node->isopath, isopath, len) == 0)
        {
            return 1;
        }
    }

    return 0;
}

int ventoy_plugin_get_image_list_index(int type, const char *name)
{
    int len;
    int index = 1;
    image_list *node = NULL;

    if (!g_image_list_head)
    {
        return 0;
    }

    len = (int)grub_strlen(name);    
    
    for (node = g_image_list_head; node; node = node->next, index++)
    {
        if (vtoy_class_directory == type)
        {
            if (len < node->pathlen && ventoy_strncmp(node->isopath, name, len) == 0)
            {
                return index;
            }
        }
        else
        {
            if (len == node->pathlen && ventoy_strncmp(node->isopath, name, len) == 0)
            {
                return index;
            }
        }
    }

    return 0;
}

int ventoy_plugin_find_conf_replace(const char *iso, conf_replace *nodes[VTOY_MAX_CONF_REPLACE])
{
    int n = 0;
    int len;
    conf_replace *node;

    if (!g_conf_replace_head)
    {
        return 0;
    }

    len = (int)grub_strlen(iso);
    
    for (node = g_conf_replace_head; node; node = node->next)
    {
        if (node->pathlen == len && ventoy_strncmp(node->isopath, iso, len) == 0)
        {
            nodes[n++] = node;
            if (n >= VTOY_MAX_CONF_REPLACE)
            {
                return n;
            }
        }
    }
    
    return n;
}

dud * ventoy_plugin_find_dud(const char *iso)
{
    int len;
    dud *node;

    if (!g_dud_head)
    {
        return NULL;
    }

    len = (int)grub_strlen(iso);
    for (node = g_dud_head; node; node = node->next)
    {
        if (node->pathlen == len && ventoy_strncmp(node->isopath, iso, len) == 0)
        {
            return node;
        }
    }
    
    return NULL;
}

int ventoy_plugin_load_dud(dud *node, const char *isopart)
{
    int i;
    char *buf;
    grub_file_t file;

    for (i = 0; i < node->dudnum; i++)
    {
        if (node->files[i].size > 0)
        {
            debug("file %d has been loaded\n", i);
            continue;
        }
    
        file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s", isopart, node->dudpath[i].path);
        if (file)
        {
            buf = grub_malloc(file->size);
            if (buf)
            {
                grub_file_read(file, buf, file->size);            
                node->files[i].size = (int)file->size;
                node->files[i].buf = buf;                
            }
            grub_file_close(file);
        }
    }

    return 0;
}

