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

void ventoy_data_default_auto_memdisk(data_auto_memdisk *data)
{
    memset(data, 0, sizeof(data_auto_memdisk));
}

int ventoy_data_cmp_auto_memdisk(data_auto_memdisk *data1, data_auto_memdisk *data2)
{
    return ventoy_path_list_cmp(data1->list, data2->list);
}

int ventoy_data_save_auto_memdisk(data_auto_memdisk *data, const char *title, char *buf, int buflen)
{
    int pos = 0;
    path_node *node = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);

    VTOY_JSON_FMT_KEY_L(L1, title);
    VTOY_JSON_FMT_ARY_BEGIN_N();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_ITEM_PATH_LN(L2, node->path);
    }

    VTOY_JSON_FMT_ARY_ENDEX_LN(L1);
    VTOY_JSON_FMT_END(pos);

    return pos;
}

int ventoy_data_json_auto_memdisk(data_auto_memdisk *data, char *buf, int buflen)
{
    int pos = 0;
    int valid = 0;
    path_node *node = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);
    VTOY_JSON_FMT_ARY_BEGIN();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN();

        VTOY_JSON_FMT_STRN("path", node->path); 
        valid = ventoy_check_fuzzy_path(node->path, 1);
        VTOY_JSON_FMT_SINT("valid", valid);        
        
        VTOY_JSON_FMT_OBJ_ENDEX();
    }

    VTOY_JSON_FMT_ARY_END();
    VTOY_JSON_FMT_END(pos);

    return pos;
}

int ventoy_api_get_auto_memdisk(struct mg_connection *conn, VTOY_JSON *json)
{
    api_get_func(conn, json, auto_memdisk);
    return 0;
}

int ventoy_api_save_auto_memdisk(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    
    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);
    return 0;
}

int ventoy_api_auto_memdisk_add(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    path_node *node = NULL;
    path_node *cur = NULL;
    data_auto_memdisk *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_auto_memdisk + index;

    path = VTOY_JSON_STR_EX("path");
    if (path)
    {
        if (ventoy_is_real_exist_common(path, data->list, path_node))
        {
            ventoy_json_result(conn, VTOY_JSON_DUPLICATE);
            return 0;
        }

        node = zalloc(sizeof(path_node));
        if (node)
        {
            scnprintf(node->path, sizeof(node->path), "%s", path);
            vtoy_list_add(data->list, cur, node);
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);
    return 0;
}

int ventoy_api_auto_memdisk_del(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    path_node *last = NULL;
    path_node *node = NULL;
    data_auto_memdisk *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_auto_memdisk + index;

    path = VTOY_JSON_STR_EX("path");
    if (path)
    {
        if (strcmp(path, VTOY_DEL_ALL_PATH) == 0)
        {
            vtoy_list_free(path_node, data->list);
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

void ventoy_data_default_image_list(data_image_list *data)
{
    memset(data, 0, sizeof(data_image_list));
}

int ventoy_data_cmp_image_list(data_image_list *data1, data_image_list *data2)
{
    if (data1->type != data2->type)
    {
        if (data1->list || data2->list)
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }

    return ventoy_path_list_cmp(data1->list, data2->list);
}

int ventoy_data_save_image_list(data_image_list *data, const char *title, char *buf, int buflen)
{
    int pos = 0;
    int prelen;
    path_node *node = NULL;
    char newtitle[64];

    (void)title;

    if (!(data->list))
    {
        return 0;
    }

    prelen = (int)strlen("image_list");

    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);

    if (data->type == 0)
    {
        scnprintf(newtitle, sizeof(newtitle), "image_list%s", title + prelen);
    }
    else
    {
        scnprintf(newtitle, sizeof(newtitle), "image_blacklist%s", title + prelen);
    }
    VTOY_JSON_FMT_KEY_L(L1, newtitle);
    
    VTOY_JSON_FMT_ARY_BEGIN_N();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_ITEM_PATH_LN(L2, node->path);
    }

    VTOY_JSON_FMT_ARY_ENDEX_LN(L1);
    VTOY_JSON_FMT_END(pos);

    return pos;
}

int ventoy_data_json_image_list(data_image_list *data, char *buf, int buflen)
{
    int pos = 0;
    int valid = 0;
    path_node *node = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);
    
    VTOY_JSON_FMT_OBJ_BEGIN();
    VTOY_JSON_FMT_SINT("type", data->type);   
    
    VTOY_JSON_FMT_KEY("list");
    VTOY_JSON_FMT_ARY_BEGIN();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN();

        VTOY_JSON_FMT_STRN("path", node->path); 
        valid = ventoy_check_fuzzy_path(node->path, 1);
        VTOY_JSON_FMT_SINT("valid", valid);        
        
        VTOY_JSON_FMT_OBJ_ENDEX();
    }

    VTOY_JSON_FMT_ARY_ENDEX();
    VTOY_JSON_FMT_OBJ_END();
    
    VTOY_JSON_FMT_END(pos);

    return pos;
}

int ventoy_api_get_image_list(struct mg_connection *conn, VTOY_JSON *json)
{
    api_get_func(conn, json, image_list);
    return 0;
}

int ventoy_api_save_image_list(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    data_image_list *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_image_list + index;

    VTOY_JSON_INT("type", data->type);

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);   
    return 0;
}

int ventoy_api_image_list_add(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    path_node *node = NULL;
    path_node *cur = NULL;
    data_image_list *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_image_list + index;

    path = VTOY_JSON_STR_EX("path");
    if (path)
    {
        if (ventoy_is_real_exist_common(path, data->list, path_node))
        {
            ventoy_json_result(conn, VTOY_JSON_DUPLICATE);
            return 0;
        }

        node = zalloc(sizeof(path_node));
        if (node)
        {
            scnprintf(node->path, sizeof(node->path), "%s", path);
            vtoy_list_add(data->list, cur, node);
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);
    return 0;
}

int ventoy_api_image_list_del(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    path_node *last = NULL;
    path_node *node = NULL;
    data_image_list *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_image_list + index;

    path = VTOY_JSON_STR_EX("path");
    if (path)
    {
        if (strcmp(path, VTOY_DEL_ALL_PATH) == 0)
        {
            vtoy_list_free(path_node, data->list);
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

