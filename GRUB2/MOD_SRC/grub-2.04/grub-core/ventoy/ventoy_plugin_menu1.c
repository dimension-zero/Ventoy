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
menu_tip *g_menu_tip_head = NULL;
menu_alias *g_menu_alias_head = NULL;
injection_config *g_injection_head = NULL;


int ventoy_plugin_menualias_check(VTOY_JSON *json, const char *isodisk)
{
    int type;
    const char *path = NULL;
    const char *alias = NULL;
    VTOY_JSON *pNode = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        grub_printf("Not array %d\n", json->enDataType);
        return 1;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        type = vtoy_alias_image_file;
        path = vtoy_json_get_string_ex(pNode->pstChild, "image");
        if (!path)
        {
            path = vtoy_json_get_string_ex(pNode->pstChild, "dir");
            type = vtoy_alias_directory;
        }
        
        alias = vtoy_json_get_string_ex(pNode->pstChild, "alias");
        if (path && path[0] == '/' && alias)
        {
            if (vtoy_alias_image_file == type)
            {
                if (grub_strchr(path, '*'))
                {
                    grub_printf("image: <%s> [ * ]\n", path);
                }
                else if (ventoy_check_file_exist("%s%s", isodisk, path))
                {
                    grub_printf("image: <%s> [ OK ]\n", path);
                }
                else
                {
                    grub_printf("image: <%s> [ NOT EXIST ]\n", path);
                }
            }
            else
            {
                if (ventoy_is_dir_exist("%s%s", isodisk, path))
                {
                    grub_printf("dir: <%s> [ OK ]\n", path);
                }
                else
                {
                    grub_printf("dir: <%s> [ NOT EXIST ]\n", path);
                }
            }

            grub_printf("alias: <%s>\n\n", alias);
        }
    }

    return 0;
}

int ventoy_plugin_menualias_entry(VTOY_JSON *json, const char *isodisk)
{
    int type;
    const char *path = NULL;
    const char *alias = NULL;
    VTOY_JSON *pNode = NULL;
    menu_alias *node = NULL;
    menu_alias *next = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        debug("Not array %d\n", json->enDataType);
        return 0;
    }

    if (g_menu_alias_head)
    {
        for (node = g_menu_alias_head; node; node = next)
        {
            next = node->next;
            grub_free(node);
        }

        g_menu_alias_head = NULL;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        type = vtoy_alias_image_file;
        path = vtoy_json_get_string_ex(pNode->pstChild, "image");
        if (!path)
        {
            path = vtoy_json_get_string_ex(pNode->pstChild, "dir");
            type = vtoy_alias_directory;
        }
        
        alias = vtoy_json_get_string_ex(pNode->pstChild, "alias");
        if (path && path[0] == '/' && alias)
        {
            node = grub_zalloc(sizeof(menu_alias));
            if (node)
            {
                node->type = type;
                node->pathlen = grub_snprintf(node->isopath, sizeof(node->isopath), "%s", path);
                grub_snprintf(node->alias, sizeof(node->alias), "%s", alias);

                if (g_menu_alias_head)
                {
                    node->next = g_menu_alias_head;
                }
                
                g_menu_alias_head = node;
            }
        }
    }

    return 0;
}

int ventoy_plugin_menutip_check(VTOY_JSON *json, const char *isodisk)
{
    int type;
    const char *path = NULL;
    const char *tip = NULL;
    VTOY_JSON *pNode = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_OBJECT)
    {
        grub_printf("Not object %d\n", json->enDataType);
        return 1;
    }

    tip = vtoy_json_get_string_ex(json->pstChild, "left");
    if (tip)
    {
        grub_printf("left: <%s>\n", tip);
    }
    
    tip = vtoy_json_get_string_ex(json->pstChild, "top");
    if (tip)
    {
        grub_printf("top: <%s>\n", tip);
    }
    
    tip = vtoy_json_get_string_ex(json->pstChild, "color");
    if (tip)
    {
        grub_printf("color: <%s>\n", tip);
    }

    pNode = vtoy_json_find_item(json->pstChild, JSON_TYPE_ARRAY, "tips");
    for (pNode = pNode->pstChild; pNode; pNode = pNode->pstNext)
    {
        type = vtoy_tip_image_file;
        path = vtoy_json_get_string_ex(pNode->pstChild, "image");
        if (!path)
        {
            path = vtoy_json_get_string_ex(pNode->pstChild, "dir");
            type = vtoy_tip_directory;
        }
        
        if (path && path[0] == '/')
        {
            if (vtoy_tip_image_file == type)
            {
                if (grub_strchr(path, '*'))
                {
                    grub_printf("image: <%s> [ * ]\n", path);
                }
                else if (ventoy_check_file_exist("%s%s", isodisk, path))
                {
                    grub_printf("image: <%s> [ OK ]\n", path);
                }
                else
                {
                    grub_printf("image: <%s> [ NOT EXIST ]\n", path);
                }
            }
            else
            {
                if (ventoy_is_dir_exist("%s%s", isodisk, path))
                {
                    grub_printf("dir: <%s> [ OK ]\n", path);
                }
                else
                {
                    grub_printf("dir: <%s> [ NOT EXIST ]\n", path);
                }
            }

            tip = vtoy_json_get_string_ex(pNode->pstChild, "tip");
            if (tip)
            {
                grub_printf("tip: <%s>\n", tip);
            }
            else
            {
                tip = vtoy_json_get_string_ex(pNode->pstChild, "tip1");
                if (tip)
                    grub_printf("tip1: <%s>\n", tip);
                else
                    grub_printf("tip1: <NULL>\n");
                
                tip = vtoy_json_get_string_ex(pNode->pstChild, "tip2");
                if (tip)
                    grub_printf("tip2: <%s>\n", tip);
                else
                    grub_printf("tip2: <NULL>\n");
            }
        }
        else
        {
            grub_printf("image: <%s> [ INVALID ]\n", path);
        }
    }

    return 0;
}

int ventoy_plugin_menutip_entry(VTOY_JSON *json, const char *isodisk)
{
    int type;
    const char *path = NULL;
    const char *tip = NULL;
    VTOY_JSON *pNode = NULL;
    menu_tip *node = NULL;
    menu_tip *next = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_OBJECT)
    {
        debug("Not object %d\n", json->enDataType);
        return 0;
    }

    pNode = vtoy_json_find_item(json->pstChild, JSON_TYPE_ARRAY, "tips");
    if (pNode == NULL)
    {
        debug("Not tips found\n");
        return 0;
    }

    if (g_menu_tip_head)
    {
        for (node = g_menu_tip_head; node; node = next)
        {
            next = node->next;
            grub_free(node);
        }

        g_menu_tip_head = NULL;
    }

    tip = vtoy_json_get_string_ex(json->pstChild, "left");
    if (tip)
    {
        grub_env_set("VTOY_TIP_LEFT", tip);
    }
    
    tip = vtoy_json_get_string_ex(json->pstChild, "top");
    if (tip)
    {
        grub_env_set("VTOY_TIP_TOP", tip);
    }
    
    tip = vtoy_json_get_string_ex(json->pstChild, "color");
    if (tip)
    {
        grub_env_set("VTOY_TIP_COLOR", tip);
    }

    for (pNode = pNode->pstChild; pNode; pNode = pNode->pstNext)
    {
        type = vtoy_tip_image_file;
        path = vtoy_json_get_string_ex(pNode->pstChild, "image");
        if (!path)
        {
            path = vtoy_json_get_string_ex(pNode->pstChild, "dir");
            type = vtoy_tip_directory;
        }
        
        if (path && path[0] == '/')
        {
            node = grub_zalloc(sizeof(menu_tip));
            if (node)
            {
                node->type = type;
                node->pathlen = grub_snprintf(node->isopath, sizeof(node->isopath), "%s", path);

                tip = vtoy_json_get_string_ex(pNode->pstChild, "tip");
                if (tip)
                {
                    grub_snprintf(node->tip1, 1000, "%s", tip);
                }
                else
                {
                    tip = vtoy_json_get_string_ex(pNode->pstChild, "tip1");
                    if (tip)
                        grub_snprintf(node->tip1, 1000, "%s", tip);

                    tip = vtoy_json_get_string_ex(pNode->pstChild, "tip2");
                    if (tip)
                        grub_snprintf(node->tip2, 1000, "%s", tip);
                }

                if (g_menu_tip_head)
                {
                    node->next = g_menu_tip_head;
                }
                
                g_menu_tip_head = node;
            }
        }
    }

    return 0;
}

int ventoy_plugin_injection_check(VTOY_JSON *json, const char *isodisk)
{
    int type = 0;
    const char *path = NULL;
    const char *archive = NULL;
    VTOY_JSON *pNode = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        grub_printf("Not array %d\n", json->enDataType);
        return 0;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        type = injection_type_file;
        path = vtoy_json_get_string_ex(pNode->pstChild, "image");
        if (!path)
        {
            type = injection_type_parent;
            path = vtoy_json_get_string_ex(pNode->pstChild, "parent");
            if (!path)
            {
                grub_printf("image/parent not found\n");
                continue;
            }
        }

        archive = vtoy_json_get_string_ex(pNode->pstChild, "archive");
        if (!archive)
        {
            grub_printf("archive not found\n");
            continue;
        }

        if (type == injection_type_file)
        {
            if (grub_strchr(path, '*'))
            {
                grub_printf("image: <%s> [*]\n", path);
            }
            else
            {
                grub_printf("image: <%s> [%s]\n", path, ventoy_check_file_exist("%s%s", isodisk, path) ? "OK" : "NOT EXIST");            
            }
        }
        else
        {
            grub_printf("parent: <%s> [%s]\n", path, 
                ventoy_is_dir_exist("%s%s", isodisk, path) ? "OK" : "NOT EXIST");
        }

        grub_printf("archive: <%s> [%s]\n\n", archive, ventoy_check_file_exist("%s%s", isodisk, archive) ? "OK" : "NOT EXIST");
    }

    return 0;
}

int ventoy_plugin_injection_entry(VTOY_JSON *json, const char *isodisk)
{
    int type = 0;
    const char *path = NULL;
    const char *archive = NULL;
    VTOY_JSON *pNode = NULL;
    injection_config *node = NULL;
    injection_config *next = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        debug("Not array %d\n", json->enDataType);
        return 0;
    }

    if (g_injection_head)
    {
        for (node = g_injection_head; node; node = next)
        {
            next = node->next;
            grub_free(node);
        }

        g_injection_head = NULL;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        type = injection_type_file;
        path = vtoy_json_get_string_ex(pNode->pstChild, "image");
        if (!path)
        {
            type = injection_type_parent;
            path = vtoy_json_get_string_ex(pNode->pstChild, "parent");
        }
        
        archive = vtoy_json_get_string_ex(pNode->pstChild, "archive");
        if (path && path[0] == '/' && archive && archive[0] == '/')
        {
            node = grub_zalloc(sizeof(injection_config));
            if (node)
            {
                node->type = type;
                node->pathlen = grub_snprintf(node->isopath, sizeof(node->isopath), "%s", path);
                grub_snprintf(node->archive, sizeof(node->archive), "%s", archive);

                if (g_injection_head)
                {
                    node->next = g_injection_head;
                }
                
                g_injection_head = node;
            }
        }
    }

    return 0;
}
