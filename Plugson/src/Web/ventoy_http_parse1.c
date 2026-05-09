/******************************************************************************
 * ventoy_http.c  ---- ventoy http
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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <errno.h>
#include <time.h>

#if defined(_MSC_VER) || defined(WIN32)
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <linux/fs.h>
#include <linux/limits.h>
#include <dirent.h>
#include <pthread.h>
#endif

#include <ventoy_define.h>
#include <ventoy_json.h>
#include <ventoy_util.h>
#include <ventoy_disk.h>
#include <ventoy_http.h>
#include "ventoy_http_priv.h"
#include "fat_filelib.h"


#define ventoy_is_real_exist_common(xpath, xnode, xtype) \
    ventoy_path_is_real_exist(xpath, xnode, offsetof(xtype, path), offsetof(xtype, next))

int ventoy_parse_control(VTOY_JSON *json, void *p)
{
    int i;
    VTOY_JSON *node = NULL;
    VTOY_JSON *child = NULL;
    data_control *data = (data_control *)p;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        return 0;
    }

    for (node = json->pstChild; node; node = node->pstNext)
    {
        if (node->enDataType == JSON_TYPE_OBJECT)
        {
            child = node->pstChild;
            
            if (child->enDataType != JSON_TYPE_STRING)
            {
                continue;
            }

            if (strcmp(child->pcName, "VTOY_DEFAULT_MENU_MODE") == 0)
            {
                CONTROL_PARSE_INT_DEF_0(child, data->default_menu_mode);
            }
            else if (strcmp(child->pcName, "VTOY_WIN11_BYPASS_CHECK") == 0)
            {
                CONTROL_PARSE_INT_DEF_1(child, data->win11_bypass_check);
            }
            else if (strcmp(child->pcName, "VTOY_WIN11_BYPASS_NRO") == 0)
            {
                CONTROL_PARSE_INT_DEF_1(child, data->win11_bypass_nro);
            }
            else if (strcmp(child->pcName, "VTOY_LINUX_REMOUNT") == 0)
            {
                CONTROL_PARSE_INT_DEF_0(child, data->linux_remount);
            }
            else if (strcmp(child->pcName, "VTOY_SECONDARY_BOOT_MENU") == 0)
            {
                CONTROL_PARSE_INT_DEF_1(child, data->secondary_menu);
            }
            else if (strcmp(child->pcName, "VTOY_SHOW_PASSWORD_ASTERISK") == 0)
            {
                CONTROL_PARSE_INT_DEF_1(child, data->password_asterisk);
            }
            else if (strcmp(child->pcName, "VTOY_TREE_VIEW_MENU_STYLE") == 0)
            {
                CONTROL_PARSE_INT_DEF_0(child, data->treeview_style);
            }
            else if (strcmp(child->pcName, "VTOY_FILT_DOT_UNDERSCORE_FILE") == 0)
            {
                CONTROL_PARSE_INT_DEF_1(child, data->filter_dot_underscore);
            }
            else if (strcmp(child->pcName, "VTOY_SORT_CASE_SENSITIVE") == 0)
            {
                CONTROL_PARSE_INT_DEF_0(child, data->sort_casesensitive);
            }
            else if (strcmp(child->pcName, "VTOY_MAX_SEARCH_LEVEL") == 0)
            {
                if (strcmp(child->unData.pcStrVal, "max") == 0)
                {
                    data->max_search_level = -1;
                }
                else
                {
                    data->max_search_level = (int)strtol(child->unData.pcStrVal, NULL, 10);
                }
            }
            else if (strcmp(child->pcName, "VTOY_DEFAULT_SEARCH_ROOT") == 0)
            {
                strlcpy(data->default_search_root, child->unData.pcStrVal);
            }
            else if (strcmp(child->pcName, "VTOY_DEFAULT_IMAGE") == 0)
            {
                strlcpy(data->default_image, child->unData.pcStrVal);
            }
            else if (strcmp(child->pcName, "VTOY_DEFAULT_KBD_LAYOUT") == 0)
            {
                for (i = 0; g_ventoy_kbd_layout[i]; i++)
                {
                    if (strcmp(child->unData.pcStrVal, g_ventoy_kbd_layout[i]) == 0)
                    {
                        strlcpy(data->default_kbd_layout, child->unData.pcStrVal);
                        break;
                    }
                }
            }
            else if (strcmp(child->pcName, "VTOY_MENU_LANGUAGE") == 0)
            {
                for (i = 0; g_ventoy_menu_lang[i][0]; i++)
                {
                    if (strcmp(child->unData.pcStrVal, g_ventoy_menu_lang[i]) == 0)
                    {
                        strlcpy(data->menu_language, child->unData.pcStrVal);
                        break;
                    }
                }
            }
            else if (strcmp(child->pcName, "VTOY_MENU_TIMEOUT") == 0)
            {
                data->menu_timeout = (int)strtol(child->unData.pcStrVal, NULL, 10);
            }
            else if (strcmp(child->pcName, "VTOY_SECONDARY_TIMEOUT") == 0)
            {
                data->secondary_menu_timeout = (int)strtol(child->unData.pcStrVal, NULL, 10);
            }
            else if (strcmp(child->pcName, "VTOY_VHD_NO_WARNING") == 0)
            {
                CONTROL_PARSE_INT_DEF_0(child, data->vhd_no_warning);
            }
            else if (strcmp(child->pcName, "VTOY_FILE_FLT_ISO") == 0)
            {
                CONTROL_PARSE_INT_DEF_0(child, data->filter_iso);
            }
            else if (strcmp(child->pcName, "VTOY_FILE_FLT_IMG") == 0)
            {
                CONTROL_PARSE_INT_DEF_0(child, data->filter_img);
            }
            else if (strcmp(child->pcName, "VTOY_FILE_FLT_EFI") == 0)
            {
                CONTROL_PARSE_INT_DEF_0(child, data->filter_efi);
            }
            else if (strcmp(child->pcName, "VTOY_FILE_FLT_WIM") == 0)
            {
                CONTROL_PARSE_INT_DEF_0(child, data->filter_wim);
            }
            else if (strcmp(child->pcName, "VTOY_FILE_FLT_VHD") == 0)
            {
                CONTROL_PARSE_INT_DEF_0(child, data->filter_vhd);
            }
            else if (strcmp(child->pcName, "VTOY_FILE_FLT_VTOY") == 0)
            {
                CONTROL_PARSE_INT_DEF_0(child, data->filter_vtoy);
            }
            
        }
    }

    return 0;
}
int ventoy_parse_theme(VTOY_JSON *json, void *p)
{
    const char *dismode = NULL;
    VTOY_JSON *child = NULL;
    VTOY_JSON *node = NULL;
    path_node *tail = NULL;
    path_node *pnode = NULL;
    data_theme *data = (data_theme *)p;

    if (json->enDataType != JSON_TYPE_OBJECT)
    {
        return 0;
    }

    child = json->pstChild;

    dismode = vtoy_json_get_string_ex(child, "display_mode");
    vtoy_json_get_string(child, "ventoy_left", sizeof(data->ventoy_left), data->ventoy_left);
    vtoy_json_get_string(child, "ventoy_top", sizeof(data->ventoy_top), data->ventoy_top);
    vtoy_json_get_string(child, "ventoy_color", sizeof(data->ventoy_color), data->ventoy_color);
    
    vtoy_json_get_int(child, "default_file", &(data->default_file));    
    vtoy_json_get_int(child, "resolution_fit", &(data->resolution_fit));    
    vtoy_json_get_string(child, "gfxmode", sizeof(data->gfxmode), data->gfxmode);
    vtoy_json_get_string(child, "serial_param", sizeof(data->serial_param), data->serial_param);

    if (dismode)
    {
        if (strcmp(dismode, "CLI") == 0)
        {
            data->display_mode = display_mode_cli;
        }
        else if (strcmp(dismode, "serial") == 0)
        {
            data->display_mode = display_mode_serial;
        }
        else if (strcmp(dismode, "serial_console") == 0)
        {
            data->display_mode = display_mode_ser_console;
        }
        else
        {
            data->display_mode = display_mode_gui;
        }
    }

    node = vtoy_json_find_item(child, JSON_TYPE_STRING, "file");
    if (node)
    {
        data->default_file = 0;
        data->resolution_fit = 0;

        pnode = zalloc(sizeof(path_node));
        if (pnode)
        {
            strlcpy(pnode->path, node->unData.pcStrVal);
            data->filelist = pnode;
        }
    }
    else
    {
        node = vtoy_json_find_item(child, JSON_TYPE_ARRAY, "file");
        if (node)
        {
            for (node = node->pstChild; node; node = node->pstNext)
            {
                if (node->enDataType == JSON_TYPE_STRING)
                {
                    pnode = zalloc(sizeof(path_node));
                    if (pnode)
                    {
                        strlcpy(pnode->path, node->unData.pcStrVal);
                        if (data->filelist)
                        {
                            tail->next = pnode;
                            tail = pnode;
                        }
                        else
                        {
                            data->filelist = tail = pnode;
                        }
                    }
                }
            }
        }
    }

    
    node = vtoy_json_find_item(child, JSON_TYPE_ARRAY, "fonts");
    if (node)
    {
        for (node = node->pstChild; node; node = node->pstNext)
        {
            if (node->enDataType == JSON_TYPE_STRING)
            {
                pnode = zalloc(sizeof(path_node));
                if (pnode)
                {
                    strlcpy(pnode->path, node->unData.pcStrVal);
                    if (data->fontslist)
                    {
                        tail->next = pnode;
                        tail = pnode;
                    }
                    else
                    {
                        data->fontslist = tail = pnode;
                    }
                }
            }
        }
    }

    return 0;
}
int ventoy_parse_menu_alias(VTOY_JSON *json, void *p)
{
    int type;
    const char *path = NULL;
    const char *alias = NULL;
    data_alias *data = (data_alias *)p;
    data_alias_node *tail = NULL;
    data_alias_node *pnode = NULL;
    VTOY_JSON *node = NULL;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        return 0;
    }

    for (node = json->pstChild; node; node = node->pstNext)
    {
        if (node->enDataType != JSON_TYPE_OBJECT)
        {
            continue;
        }

        type = path_type_file;
        path = vtoy_json_get_string_ex(node->pstChild, "image");
        if (!path)
        {
            path = vtoy_json_get_string_ex(node->pstChild, "dir");
            type = path_type_dir;
        }
        alias = vtoy_json_get_string_ex(node->pstChild, "alias");

        if (path && alias)
        {
            pnode = zalloc(sizeof(data_alias_node));
            if (pnode)
            {
                pnode->type = type;
                strlcpy(pnode->path, path);
                strlcpy(pnode->alias, alias);
                
                if (data->list)
                {
                    tail->next = pnode;
                    tail = pnode;
                }
                else
                {
                    data->list = tail = pnode;
                }
            }
        }
    }    

    return 0;
}

int ventoy_parse_menu_tip(VTOY_JSON *json, void *p)
{
    int type;
    const char *path = NULL;
    const char *tip = NULL;
    data_tip *data = (data_tip *)p;
    data_tip_node *tail = NULL;
    data_tip_node *pnode = NULL;
    VTOY_JSON *node = NULL;
    VTOY_JSON *tips = NULL;

    if (json->enDataType != JSON_TYPE_OBJECT)
    {
        return 0;
    }

    vtoy_json_get_string(json->pstChild, "left", sizeof(data->left), data->left);
    vtoy_json_get_string(json->pstChild, "top", sizeof(data->top), data->top);
    vtoy_json_get_string(json->pstChild, "color", sizeof(data->color), data->color);

    tips = vtoy_json_find_item(json->pstChild, JSON_TYPE_ARRAY, "tips");
    if (!tips)
    {
        return 0;
    }

    for (node = tips->pstChild; node; node = node->pstNext)
    {
        if (node->enDataType != JSON_TYPE_OBJECT)
        {
            continue;
        }

        type = path_type_file;
        path = vtoy_json_get_string_ex(node->pstChild, "image");
        if (!path)
        {
            path = vtoy_json_get_string_ex(node->pstChild, "dir");
            type = path_type_dir;
        }
        tip = vtoy_json_get_string_ex(node->pstChild, "tip");

        if (path && tip)
        {
            pnode = zalloc(sizeof(data_tip_node));
            if (pnode)
            {
                pnode->type = type;
                strlcpy(pnode->path, path);
                strlcpy(pnode->tip, tip);
                
                if (data->list)
                {
                    tail->next = pnode;
                    tail = pnode;
                }
                else
                {
                    data->list = tail = pnode;
                }
            }
        }
    }    

    return 0;
}
int ventoy_parse_menu_class(VTOY_JSON *json, void *p)
{
    int type;
    const char *path = NULL;
    const char *class = NULL;
    data_class *data = (data_class *)p;
    data_class_node *tail = NULL;
    data_class_node *pnode = NULL;
    VTOY_JSON *node = NULL;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        return 0;
    }

    for (node = json->pstChild; node; node = node->pstNext)
    {
        if (node->enDataType != JSON_TYPE_OBJECT)
        {
            continue;
        }

        type = class_type_key;
        path = vtoy_json_get_string_ex(node->pstChild, "key");
        if (!path)
        {
            type = class_type_dir;
            path = vtoy_json_get_string_ex(node->pstChild, "dir");
            if (!path)
            {
                type = class_type_parent;
                path = vtoy_json_get_string_ex(node->pstChild, "parent");
            }
        }
        class = vtoy_json_get_string_ex(node->pstChild, "class");

        if (path && class)
        {
            pnode = zalloc(sizeof(data_class_node));
            if (pnode)
            {
                pnode->type = type;
                strlcpy(pnode->path, path);
                strlcpy(pnode->class, class);
                
                if (data->list)
                {
                    tail->next = pnode;
                    tail = pnode;
                }
                else
                {
                    data->list = tail = pnode;
                }
            }
        }
    }    

    return 0;
