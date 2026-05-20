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
#include "fat_filelib.h"

#include "ventoy_http_priv.h"
void ventoy_data_default_injection(data_injection *data)
{
    memset(data, 0, sizeof(data_injection));
}

int ventoy_data_cmp_injection(data_injection *data1, data_injection *data2)
{
    injection_node *list1 = NULL;
    injection_node *list2 = NULL;

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
                strcmp(list1->archive, list2->archive))
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


int ventoy_data_save_injection(data_injection *data, const char *title, char *buf, int buflen)
{
    int pos = 0;
    injection_node *node = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);

    VTOY_JSON_FMT_KEY_L(L1, title);
    VTOY_JSON_FMT_ARY_BEGIN_N();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN_LN(L2);

        if (node->type == 0)
        {
            VTOY_JSON_FMT_STRN_PATH_LN(L3, "image", node->path);            
        }
        else
        {
            VTOY_JSON_FMT_STRN_PATH_LN(L3, "parent", node->path);
        }
        VTOY_JSON_FMT_STRN_PATH_LN(L3, "archive", node->archive);
        
        VTOY_JSON_FMT_OBJ_ENDEX_LN(L2);
    }

    VTOY_JSON_FMT_ARY_ENDEX_LN(L1);
    VTOY_JSON_FMT_END(pos);

    return pos;
}


int ventoy_data_json_injection(data_injection *data, char *buf, int buflen)
{
    int pos = 0;
    int valid = 0;
    injection_node *node = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);
    VTOY_JSON_FMT_ARY_BEGIN();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN();

        VTOY_JSON_FMT_UINT("type", node->type);
        VTOY_JSON_FMT_STRN("path", node->path);       

        if (node->type == 0)
        {
            valid = ventoy_check_fuzzy_path(node->path, 1);
        }
        else
        {
            valid = ventoy_is_directory_exist("%s%s", g_cur_dir, node->path);
        }
        VTOY_JSON_FMT_SINT("valid", valid);
        
        VTOY_JSON_FMT_STRN("archive", node->archive);

        valid = ventoy_is_file_exist("%s%s", g_cur_dir, node->archive);
        VTOY_JSON_FMT_SINT("archive_valid", valid);
        
        VTOY_JSON_FMT_OBJ_ENDEX();
    }

    VTOY_JSON_FMT_ARY_END();
    VTOY_JSON_FMT_END(pos);

    return pos;
}


int ventoy_api_get_injection(struct mg_connection *conn, VTOY_JSON *json)
{
    api_get_func(conn, json, injection);
    return 0;
}

int ventoy_api_save_injection(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);   
    return 0;
}

int ventoy_api_injection_add(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    int type = 0;
    const char *path = NULL;
    const char *archive = NULL;
    injection_node *node = NULL;
    injection_node *cur = NULL;
    data_injection *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_injection + index;

    vtoy_json_get_int(json, "type", &type);

    path = VTOY_JSON_STR_EX("path");
    archive = VTOY_JSON_STR_EX("archive");
    if (path && archive)
    {
        if (ventoy_is_real_exist_common(path, data->list, injection_node))
        {
            ventoy_json_result(conn, VTOY_JSON_DUPLICATE);
            return 0;
        }
    
        node = zalloc(sizeof(injection_node));
        if (node)
        {
            node->type = type;
        
            scnprintf(node->path, sizeof(node->path), "%s", path);
            scnprintf(node->archive, sizeof(node->archive), "%s", archive);
            
            vtoy_list_add(data->list, cur, node);
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);   
    return 0;
}

int ventoy_api_injection_del(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    injection_node *last = NULL;
    injection_node *node = NULL;
    data_injection *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_injection + index;

    path = VTOY_JSON_STR_EX("path");
    if (path)
    {
        if (strcmp(path, VTOY_DEL_ALL_PATH) == 0)
        {
            vtoy_list_free(injection_node, data->list);
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

