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

void ventoy_data_default_menu_alias(data_alias *data)
{
    memset(data, 0, sizeof(data_alias));
}

int ventoy_data_cmp_menu_alias(data_alias *data1, data_alias *data2)
{
    data_alias_node *list1 = NULL;
    data_alias_node *list2 = NULL;

    if (NULL == data1->list && NULL == data2->list)
    {
        return 0;
    }
    else if (data1->list && data2->list)
    {
        list1 = data1->list;
        list2 = data2->list;
    
        while (list1 && list2)
        {
            if ((list1->type != list2->type) || 
                strcmp(list1->path, list2->path) || 
                strcmp(list1->alias, list2->alias))
            {
                return 1;
            }
        
            list1 = list1->next;
            list2 = list2->next;
        }

        if (list1 == NULL && list2 == NULL)
        {
            return 0;
        }
        return 1;
    }
    else
    {
        return 1;
    }

    return 0;
}


int ventoy_data_save_menu_alias(data_alias *data, const char *title, char *buf, int buflen)
{
    int pos = 0;
    data_alias_node *node = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);

    VTOY_JSON_FMT_KEY_L(L1, title);
    VTOY_JSON_FMT_ARY_BEGIN_N();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN_LN(L2);

        if (node->type == path_type_file)
        {
            VTOY_JSON_FMT_STRN_PATH_LN(L3, "image", node->path);            
        }
        else
        {
            VTOY_JSON_FMT_STRN_PATH_LN(L3, "dir", node->path);
        }
        
        VTOY_JSON_FMT_STRN_EX_LN(L3, "alias", node->alias);
        
        VTOY_JSON_FMT_OBJ_ENDEX_LN(L2);
    }

    VTOY_JSON_FMT_ARY_ENDEX_LN(L1);
    VTOY_JSON_FMT_END(pos);

    return pos;
}


int ventoy_data_json_menu_alias(data_alias *data, char *buf, int buflen)
{
    int pos = 0;
    int valid = 0;
    data_alias_node *node = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);
    VTOY_JSON_FMT_ARY_BEGIN();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN();

        VTOY_JSON_FMT_UINT("type", node->type);
        VTOY_JSON_FMT_STRN("path", node->path);
        if (node->type == path_type_file)
        {
            valid = ventoy_check_fuzzy_path(node->path, 1);
        }
        else
        {
            valid = ventoy_is_directory_exist("%s%s", g_cur_dir, node->path);
        }
        
        VTOY_JSON_FMT_SINT("valid", valid);
        VTOY_JSON_FMT_STRN("alias", node->alias);
        
        VTOY_JSON_FMT_OBJ_ENDEX();
    }

    VTOY_JSON_FMT_ARY_END();
    VTOY_JSON_FMT_END(pos);

    return pos;
}

int ventoy_api_get_alias(struct mg_connection *conn, VTOY_JSON *json)
{
    api_get_func(conn, json, menu_alias);
    return 0;
}

int ventoy_api_save_alias(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);  
    return 0;
}

int ventoy_api_alias_add(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    int type = path_type_file;
    const char *path = NULL;
    const char *alias = NULL;
    data_alias_node *node = NULL;
    data_alias_node *cur = NULL;
    data_alias *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_menu_alias + index;

    vtoy_json_get_int(json, "type", &type);

    path = VTOY_JSON_STR_EX("path");
    alias = VTOY_JSON_STR_EX("alias");
    if (path && alias)
    {
        if (ventoy_is_real_exist_common(path, data->list, data_alias_node))
        {
            ventoy_json_result(conn, VTOY_JSON_DUPLICATE);
            return 0;
        }

        node = zalloc(sizeof(data_alias_node));
        if (node)
        {
            node->type = type;
            scnprintf(node->path, sizeof(node->path), "%s", path);
            scnprintf(node->alias, sizeof(node->alias), "%s", alias);
            
            vtoy_list_add(data->list, cur, node);
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);  
    return 0;
}

int ventoy_api_alias_del(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    data_alias_node *last = NULL;
    data_alias_node *node = NULL;
    data_alias *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_menu_alias + index;

    path = VTOY_JSON_STR_EX("path");
    if (path)
    {
        if (strcmp(path, VTOY_DEL_ALL_PATH) == 0)
        {
            vtoy_list_free(data_alias_node, data->list);          
        }
        else
        {
            vtoy_list_del(last, node, data->list, path);            
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);    
    return 0;
}

#if 0
#endif

void ventoy_data_default_menu_tip(data_tip *data)
{
    memset(data, 0, sizeof(data_tip));

    scnprintf(data->left, sizeof(data->left), "10%%");
    scnprintf(data->top, sizeof(data->top), "81%%");
    scnprintf(data->color, sizeof(data->color), "%s", "blue");
}

int ventoy_data_cmp_menu_tip(data_tip *data1, data_tip *data2)
{
    data_tip_node *list1 = NULL;
    data_tip_node *list2 = NULL;

    if (strcmp(data1->left, data2->left) || strcmp(data1->top, data2->top) || strcmp(data1->color, data2->color))
    {
        return 1;
    }

    if (NULL == data1->list && NULL == data2->list)
    {
        return 0;
    }
    else if (data1->list && data2->list)
    {
        list1 = data1->list;
        list2 = data2->list;
    
        while (list1 && list2)
        {
            if ((list1->type != list2->type) || 
                strcmp(list1->path, list2->path) || 
                strcmp(list1->tip, list2->tip))
            {
                return 1;
            }
        
            list1 = list1->next;
            list2 = list2->next;
        }

        if (list1 == NULL && list2 == NULL)
        {
            return 0;
        }
        return 1;
    }
    else
    {
        return 1;
    }

    return 0;
}


int ventoy_data_save_menu_tip(data_tip *data, const char *title, char *buf, int buflen)
{
    int pos = 0;
    data_tip_node *node = NULL;
    data_tip *def = g_data_menu_tip + bios_max;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);
    VTOY_JSON_FMT_KEY_L(L1, title);
    VTOY_JSON_FMT_OBJ_BEGIN_N();

    VTOY_JSON_FMT_DIFF_STRN(L2, "left", left);        
    VTOY_JSON_FMT_DIFF_STRN(L2, "top", top);        
    VTOY_JSON_FMT_DIFF_STRN(L2, "color", color);        

    if (data->list)
    {
        VTOY_JSON_FMT_KEY_L(L2, "tips");
        VTOY_JSON_FMT_ARY_BEGIN_N();

        for (node = data->list; node; node = node->next)
        {
            VTOY_JSON_FMT_OBJ_BEGIN_LN(L3);

            if (node->type == path_type_file)
            {
                VTOY_JSON_FMT_STRN_PATH_LN(L4, "image", node->path);            
            }
            else
            {
                VTOY_JSON_FMT_STRN_PATH_LN(L4, "dir", node->path);
            }
            VTOY_JSON_FMT_STRN_EX_LN(L4, "tip", node->tip);
            
            VTOY_JSON_FMT_OBJ_ENDEX_LN(L3);
        }

        VTOY_JSON_FMT_ARY_ENDEX_LN(L2);
    }
    
    VTOY_JSON_FMT_OBJ_ENDEX_LN(L1);
    
    VTOY_JSON_FMT_END(pos);

    return pos;
}


int ventoy_data_json_menu_tip(data_tip *data, char *buf, int buflen)
{
    int pos = 0;
    int valid = 0;
    data_tip_node *node = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);

    VTOY_JSON_FMT_OBJ_BEGIN();
    
    VTOY_JSON_FMT_STRN("left", data->left);
    VTOY_JSON_FMT_STRN("top", data->top);
    VTOY_JSON_FMT_STRN("color", data->color);

    VTOY_JSON_FMT_KEY("tips");
    VTOY_JSON_FMT_ARY_BEGIN();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN();

        VTOY_JSON_FMT_UINT("type", node->type);
        VTOY_JSON_FMT_STRN("path", node->path);
        if (node->type == path_type_file)
        {
            valid = ventoy_check_fuzzy_path(node->path, 1);
        }
        else
        {
            valid = ventoy_is_directory_exist("%s%s", g_cur_dir, node->path);
        }
        
        VTOY_JSON_FMT_SINT("valid", valid);
        VTOY_JSON_FMT_STRN("tip", node->tip);
        
        VTOY_JSON_FMT_OBJ_ENDEX();
    }

    VTOY_JSON_FMT_ARY_ENDEX();

    VTOY_JSON_FMT_OBJ_END();
    VTOY_JSON_FMT_END(pos);

    return pos;
}

int ventoy_api_get_tip(struct mg_connection *conn, VTOY_JSON *json)
{
    api_get_func(conn, json, menu_tip);
    return 0;
}

int ventoy_api_save_tip(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    data_tip *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_menu_tip + index;

    VTOY_JSON_STR("left", data->left);
    VTOY_JSON_STR("top", data->top);
    VTOY_JSON_STR("color", data->color);

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);   
    return 0;
}

int ventoy_api_tip_add(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    int type = path_type_file;
    const char *path = NULL;
    const char *tip = NULL;
    data_tip_node *node = NULL;
    data_tip_node *cur = NULL;
    data_tip *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_menu_tip + index;

    vtoy_json_get_int(json, "type", &type);

    path = VTOY_JSON_STR_EX("path");
    tip = VTOY_JSON_STR_EX("tip");
    if (path && tip)
    {
        if (ventoy_is_real_exist_common(path, data->list, data_tip_node))
        {
            ventoy_json_result(conn, VTOY_JSON_DUPLICATE);
            return 0;
        }
    
        node = zalloc(sizeof(data_tip_node));
        if (node)
        {
            node->type = type;
            scnprintf(node->path, sizeof(node->path), "%s", path);
            scnprintf(node->tip, sizeof(node->tip), "%s", tip);
            
            vtoy_list_add(data->list, cur, node);
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);   
    return 0;
}

int ventoy_api_tip_del(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    data_tip_node *last = NULL;
    data_tip_node *node = NULL;
    data_tip *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_menu_tip + index;

    path = VTOY_JSON_STR_EX("path");
    if (path)
    {
        if (strcmp(path, VTOY_DEL_ALL_PATH) == 0)
        {
            vtoy_list_free(data_tip_node, data->list);          
        }
        else
        {
            vtoy_list_del(last, node, data->list, path);  
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);   
    return 0;
}

#if 0
#endif

void ventoy_data_default_menu_class(data_class *data)
{
    memset(data, 0, sizeof(data_class));
}

int ventoy_data_cmp_menu_class(data_class *data1, data_class *data2)
{
    data_class_node *list1 = NULL;
    data_class_node *list2 = NULL;

    if (NULL == data1->list && NULL == data2->list)
    {
        return 0;
    }
    else if (data1->list && data2->list)
    {
        list1 = data1->list;
        list2 = data2->list;
    
        while (list1 && list2)
        {
            if ((list1->type != list2->type) || 
                strcmp(list1->path, list2->path) || 
                strcmp(list1->class, list2->class))
            {
                return 1;
            }
        
            list1 = list1->next;
            list2 = list2->next;
        }

        if (list1 == NULL && list2 == NULL)
        {
            return 0;
        }
        return 1;
    }
    else
    {
        return 1;
    }

    return 0;
}


int ventoy_data_save_menu_class(data_class *data, const char *title, char *buf, int buflen)
{
    int pos = 0;
    data_class_node *node = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);

    VTOY_JSON_FMT_KEY_L(L1, title);
    VTOY_JSON_FMT_ARY_BEGIN_N();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN_LN(L2);

        if (node->type == class_type_key)
        {
            VTOY_JSON_FMT_STRN_LN(L3, "key", node->path);            
        }
        else if (node->type == class_type_dir)
        {
            VTOY_JSON_FMT_STRN_PATH_LN(L3, "dir", node->path);
        }
        else
        {
            VTOY_JSON_FMT_STRN_PATH_LN(L3, "parent", node->path);
        }
        VTOY_JSON_FMT_STRN_LN(L3, "class", node->class);
        
        VTOY_JSON_FMT_OBJ_ENDEX_LN(L2);
    }

    VTOY_JSON_FMT_ARY_ENDEX_LN(L1);
    VTOY_JSON_FMT_END(pos);

    return pos;
}


int ventoy_data_json_menu_class(data_class *data, char *buf, int buflen)
{
    int pos = 0;
    int valid = 0;
    data_class_node *node = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);
    VTOY_JSON_FMT_ARY_BEGIN();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN();

        VTOY_JSON_FMT_UINT("type", node->type);
        VTOY_JSON_FMT_STRN("path", node->path);       

        if (node->type == class_type_key)
        {
            valid = 1;
        }
        else
        {
            valid = ventoy_is_directory_exist("%s%s", g_cur_dir, node->path);
        }
        VTOY_JSON_FMT_SINT("valid", valid);
        
        VTOY_JSON_FMT_STRN("class", node->class);
        
        VTOY_JSON_FMT_OBJ_ENDEX();
    }

    VTOY_JSON_FMT_ARY_END();
    VTOY_JSON_FMT_END(pos);

    return pos;
}


int ventoy_api_get_class(struct mg_connection *conn, VTOY_JSON *json)
{
    api_get_func(conn, json, menu_class);
    return 0;
}

int ventoy_api_save_class(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);  
    return 0;
}

int ventoy_api_class_add(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    int type = class_type_key;
    const char *path = NULL;
    const char *class = NULL;
    data_class_node *node = NULL;
    data_class_node *cur = NULL;
    data_class *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_menu_class + index;

    vtoy_json_get_int(json, "type", &type);

    path = VTOY_JSON_STR_EX("path");
    class = VTOY_JSON_STR_EX("class");
    if (path && class)
    {
        node = zalloc(sizeof(data_class_node));
        if (node)
        {
            node->type = type;
        
            scnprintf(node->path, sizeof(node->path), "%s", path);
            scnprintf(node->class, sizeof(node->class), "%s", class);
            
            vtoy_list_add(data->list, cur, node);
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET); 
    return 0;
}

int ventoy_api_class_del(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    data_class_node *last = NULL;
    data_class_node *node = NULL;
    data_class *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_menu_class + index;

    path = VTOY_JSON_STR_EX("path");
    if (path)
    {
        if (strcmp(path, VTOY_DEL_ALL_PATH) == 0)
        {
            vtoy_list_free(data_class_node, data->list);
        }
        else
        {
            vtoy_list_del(last, node, data->list, path);            
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);  
    return 0;
}

#if 0
#endif

