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

void ventoy_data_default_auto_install(data_auto_install *data)
{
    memset(data, 0, sizeof(data_auto_install));
}

int ventoy_data_cmp_auto_install(data_auto_install *data1, data_auto_install *data2)
{
    auto_install_node *list1 = NULL;
    auto_install_node *list2 = NULL;

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
            if (list1->timeout != list2->timeout ||
                list1->autosel != list2->autosel ||
                strcmp(list1->path, list2->path))
            {
                return 1;
            }

            /* no need to compare auto install list with default */
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


int ventoy_data_save_auto_install(data_auto_install *data, const char *title, char *buf, int buflen)
{
    int pos = 0;
    auto_install_node *node = NULL;
    path_node *pathnode = NULL;
    
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

        
        VTOY_JSON_FMT_KEY_L(L3, "template");
        VTOY_JSON_FMT_ARY_BEGIN_N();
        for (pathnode = node->list; pathnode; pathnode = pathnode->next)
        {
            VTOY_JSON_FMT_ITEM_PATH_LN(L4, pathnode->path);
        }
        VTOY_JSON_FMT_ARY_ENDEX_LN(L3);

        if (node->timeouten)
        {
            VTOY_JSON_FMT_SINT_LN(L3, "timeout", node->timeout);
        }

        if (node->autoselen)
        {
            VTOY_JSON_FMT_SINT_LN(L3, "autosel", node->autosel);                
        }

        VTOY_JSON_FMT_OBJ_ENDEX_LN(L2);
    }

    VTOY_JSON_FMT_ARY_ENDEX_LN(L1);
    VTOY_JSON_FMT_END(pos);

    return pos;
}


int ventoy_data_json_auto_install(data_auto_install *data, char *buf, int buflen)
{
    int pos = 0;
    int valid = 0;
    auto_install_node *node = NULL;
    path_node *pathnode = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);
    VTOY_JSON_FMT_ARY_BEGIN();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN();

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
        VTOY_JSON_FMT_SINT("type", node->type);
        
        VTOY_JSON_FMT_BOOL("timeouten", node->timeouten);
        VTOY_JSON_FMT_BOOL("autoselen", node->autoselen);
        
        VTOY_JSON_FMT_SINT("autosel", node->autosel);
        VTOY_JSON_FMT_SINT("timeout", node->timeout);

        VTOY_JSON_FMT_KEY("list");
        VTOY_JSON_FMT_ARY_BEGIN();
        for (pathnode = node->list; pathnode; pathnode = pathnode->next)
        {
            VTOY_JSON_FMT_OBJ_BEGIN();
            VTOY_JSON_FMT_STRN("path", pathnode->path);

            valid = ventoy_is_file_exist("%s%s", g_cur_dir, pathnode->path);
            VTOY_JSON_FMT_SINT("valid", valid);
            VTOY_JSON_FMT_OBJ_ENDEX();
        }
        VTOY_JSON_FMT_ARY_ENDEX(); 

        
        VTOY_JSON_FMT_OBJ_ENDEX();
    }

    VTOY_JSON_FMT_ARY_END();
    VTOY_JSON_FMT_END(pos);

    return pos;
}

int ventoy_api_get_auto_install(struct mg_connection *conn, VTOY_JSON *json)
{
    api_get_func(conn, json, auto_install);
    return 0;
}

int ventoy_api_save_auto_install(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int id = -1;
    int cnt = 0;
    int index = 0;
    uint8_t timeouten = 0;
    uint8_t autoselen = 0;
    auto_install_node *node = NULL;
    data_auto_install *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    vtoy_json_get_int(json, "id", &id);

    vtoy_json_get_bool(json, "timeouten", &timeouten);
    vtoy_json_get_bool(json, "autoselen", &autoselen);
    
    data = g_data_auto_install + index;

    if (id >= 0)
    {
        for (node = data->list; node; node = node->next)
        {
            if (cnt == id)
            {
                node->timeouten = (int)timeouten;
                node->autoselen = (int)autoselen;
                VTOY_JSON_INT("timeout", node->timeout);
                VTOY_JSON_INT("autosel", node->autosel);
                break;
            }
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET); 
    return 0;
}


int ventoy_api_auto_install_add(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    int type = 0;
    const char *path = NULL;
    auto_install_node *node = NULL;
    auto_install_node *cur = NULL;
    data_auto_install *data = NULL;
    VTOY_JSON *array = NULL;
    
    vtoy_json_get_int(json, "type", &type);
    vtoy_json_get_int(json, "index", &index);
    data = g_data_auto_install + index;

    array = vtoy_json_find_item(json, JSON_TYPE_ARRAY, "template");
    path = VTOY_JSON_STR_EX("path");
    if (path && array)
    {
        if (ventoy_is_real_exist_common(path, data->list, auto_install_node))
        {
            ventoy_json_result(conn, VTOY_JSON_DUPLICATE);
            return 0;
        }     
    
        node = zalloc(sizeof(auto_install_node));
        if (node)
        {
            node->type = type;
            node->timeouten = 0;
            node->autoselen = 0;
            node->autosel = 1;
            node->timeout = 0;
            scnprintf(node->path, sizeof(node->path), "%s", path);
            node->list = ventoy_path_node_add_array(array);
            
            vtoy_list_add(data->list, cur, node);
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);  
    return 0;
}

int ventoy_api_auto_install_del(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    auto_install_node *last = NULL;
    auto_install_node *next = NULL;
    auto_install_node *node = NULL;
    data_auto_install *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_auto_install + index;

    path = VTOY_JSON_STR_EX("path");
    if (path)
    {
        if (strcmp(path, VTOY_DEL_ALL_PATH) == 0)
        {
            for (node = data->list; node; node = next)
            {
                next = node->next;
                ventoy_free_path_node_list(node->list);
                free(node);
            }
            data->list = NULL;
        }
        else
        {
            vtoy_list_del_ex(last, node, data->list, path, ventoy_free_path_node_list);            
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);   
    return 0;
}

int ventoy_api_auto_install_add_inner(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    const char *outpath = NULL;
    path_node *pcur = NULL;
    path_node *pnode = NULL;
    auto_install_node *node = NULL;
    data_auto_install *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_auto_install + index;

    path = VTOY_JSON_STR_EX("path");
    outpath = VTOY_JSON_STR_EX("outpath");
    if (path && outpath)
    {
        for (node = data->list; node; node = node->next)
        {
            if (strcmp(outpath, node->path) == 0)
            {
                pnode = zalloc(sizeof(path_node));
                if (pnode)
                {
                    scnprintf(pnode->path, sizeof(pnode->path), "%s", path);
                    vtoy_list_add(node->list, pcur, pnode);
                }
            
                break;
            }
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);   
    return 0;
}

int ventoy_api_auto_install_del_inner(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    const char *outpath = NULL;
    path_node *plast = NULL;
    path_node *pnode = NULL;
    auto_install_node *node = NULL;
    data_auto_install *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_auto_install + index;

    path = VTOY_JSON_STR_EX("path");
    outpath = VTOY_JSON_STR_EX("outpath");
    if (path && outpath)
    {
        for (node = data->list; node; node = node->next)
        {
            if (strcmp(outpath, node->path) == 0)
            {
                vtoy_list_del(plast, pnode, node->list, path);
                break;
            }
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);  
    return 0;
}


#if 0
#endif


void ventoy_data_default_persistence(data_persistence *data)
{
    memset(data, 0, sizeof(data_persistence));
}

int ventoy_data_cmp_persistence(data_persistence *data1, data_persistence *data2)
{
    persistence_node *list1 = NULL;
    persistence_node *list2 = NULL;

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
            if (list1->timeout != list2->timeout ||
                list1->autosel != list2->autosel ||
                strcmp(list1->path, list2->path))
            {
                return 1;
            }

            /* no need to compare auto install list with default */
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


int ventoy_data_save_persistence(data_persistence *data, const char *title, char *buf, int buflen)
{
    int pos = 0;
    persistence_node *node = NULL;
    path_node *pathnode = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);

    VTOY_JSON_FMT_KEY_L(L1, title);
    VTOY_JSON_FMT_ARY_BEGIN_N();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN_LN(L2);
        VTOY_JSON_FMT_STRN_PATH_LN(L3, "image", node->path);            
        VTOY_JSON_FMT_KEY_L(L3, "backend");
        VTOY_JSON_FMT_ARY_BEGIN_N();
        for (pathnode = node->list; pathnode; pathnode = pathnode->next)
        {
            VTOY_JSON_FMT_ITEM_PATH_LN(L4, pathnode->path);
        }
        VTOY_JSON_FMT_ARY_ENDEX_LN(L3);

        if (node->timeouten)
        {
            VTOY_JSON_FMT_SINT_LN(L3, "timeout", node->timeout);
        }

        if (node->autoselen)
        {
            VTOY_JSON_FMT_SINT_LN(L3, "autosel", node->autosel);                
        }

        VTOY_JSON_FMT_OBJ_ENDEX_LN(L2);
    }

    VTOY_JSON_FMT_ARY_ENDEX_LN(L1);
    VTOY_JSON_FMT_END(pos);

    return pos;
}


int ventoy_data_json_persistence(data_persistence *data, char *buf, int buflen)
{
    int pos = 0;
    int valid = 0;
    persistence_node *node = NULL;
    path_node *pathnode = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);
    VTOY_JSON_FMT_ARY_BEGIN();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN();

        VTOY_JSON_FMT_STRN("path", node->path);

        valid = ventoy_check_fuzzy_path(node->path, 1);            
        VTOY_JSON_FMT_SINT("valid", valid);
        VTOY_JSON_FMT_SINT("type", node->type);
        
        VTOY_JSON_FMT_BOOL("timeouten", node->timeouten);
        VTOY_JSON_FMT_BOOL("autoselen", node->autoselen);
        
        VTOY_JSON_FMT_SINT("autosel", node->autosel);
        VTOY_JSON_FMT_SINT("timeout", node->timeout);

        VTOY_JSON_FMT_KEY("list");
        VTOY_JSON_FMT_ARY_BEGIN();
        for (pathnode = node->list; pathnode; pathnode = pathnode->next)
        {
            VTOY_JSON_FMT_OBJ_BEGIN();
            VTOY_JSON_FMT_STRN("path", pathnode->path);

            valid = ventoy_is_file_exist("%s%s", g_cur_dir, pathnode->path);
            VTOY_JSON_FMT_SINT("valid", valid);
            VTOY_JSON_FMT_OBJ_ENDEX();
        }
        VTOY_JSON_FMT_ARY_ENDEX(); 

        
        VTOY_JSON_FMT_OBJ_ENDEX();
    }

    VTOY_JSON_FMT_ARY_END();
    VTOY_JSON_FMT_END(pos);

    return pos;
}

int ventoy_api_get_persistence(struct mg_connection *conn, VTOY_JSON *json)
{
    api_get_func(conn, json, persistence);
    return 0;
}

int ventoy_api_save_persistence(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int id = -1;
    int cnt = 0;
    int index = 0;
    uint8_t timeouten = 0;
    uint8_t autoselen = 0;
    persistence_node *node = NULL;
    data_persistence *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    vtoy_json_get_int(json, "id", &id);

    vtoy_json_get_bool(json, "timeouten", &timeouten);
    vtoy_json_get_bool(json, "autoselen", &autoselen);
    
    data = g_data_persistence + index;

    if (id >= 0)
    {
        for (node = data->list; node; node = node->next)
        {
            if (cnt == id)
            {
                node->timeouten = (int)timeouten;
                node->autoselen = (int)autoselen;
                VTOY_JSON_INT("timeout", node->timeout);
                VTOY_JSON_INT("autosel", node->autosel);
                break;
            }
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);  
    return 0;
}


int ventoy_api_persistence_add(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    persistence_node *node = NULL;
    persistence_node *cur = NULL;
    data_persistence *data = NULL;
    VTOY_JSON *array = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_persistence + index;

    array = vtoy_json_find_item(json, JSON_TYPE_ARRAY, "backend");
    path = VTOY_JSON_STR_EX("path");
    if (path && array)
    {
        if (ventoy_is_real_exist_common(path, data->list, persistence_node))
        {
            ventoy_json_result(conn, VTOY_JSON_DUPLICATE);
            return 0;
        }
        
        node = zalloc(sizeof(persistence_node));
        if (node)
        {
            node->timeouten = 0;
            node->autoselen = 0;
            node->autosel = 1;
            node->timeout = 0;
            scnprintf(node->path, sizeof(node->path), "%s", path);
            node->list = ventoy_path_node_add_array(array);
            
            vtoy_list_add(data->list, cur, node);
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET); 
    return 0;
}

int ventoy_api_persistence_del(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    persistence_node *last = NULL;
    persistence_node *next = NULL;
    persistence_node *node = NULL;
    data_persistence *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_persistence + index;

    path = VTOY_JSON_STR_EX("path");
    if (path)
    {
        if (strcmp(path, VTOY_DEL_ALL_PATH) == 0)
        {
            for (node = data->list; node; node = next)
            {
                next = node->next;
                ventoy_free_path_node_list(node->list);
                free(node);
            }
            data->list = NULL;
        }
        else
        {
            vtoy_list_del_ex(last, node, data->list, path, ventoy_free_path_node_list);            
        }    
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);  
    return 0;
}

int ventoy_api_persistence_add_inner(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    const char *outpath = NULL;
    path_node *pcur = NULL;
    path_node *pnode = NULL;
    persistence_node *node = NULL;
    data_persistence *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_persistence + index;

    path = VTOY_JSON_STR_EX("path");
    outpath = VTOY_JSON_STR_EX("outpath");
    if (path && outpath)
    {
        for (node = data->list; node; node = node->next)
        {
            if (strcmp(outpath, node->path) == 0)
            {
                pnode = zalloc(sizeof(path_node));
                if (pnode)
                {
                    scnprintf(pnode->path, sizeof(pnode->path), "%s", path);
                    vtoy_list_add(node->list, pcur, pnode);
                }
            
                break;
            }
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);   
    return 0;
}

int ventoy_api_persistence_del_inner(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    const char *outpath = NULL;
    path_node *plast = NULL;
    path_node *pnode = NULL;
    persistence_node *node = NULL;
    data_persistence *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_persistence + index;

    path = VTOY_JSON_STR_EX("path");
    outpath = VTOY_JSON_STR_EX("outpath");
    if (path && outpath)
    {
        for (node = data->list; node; node = node->next)
        {
            if (strcmp(outpath, node->path) == 0)
            {
                vtoy_list_del(plast, pnode, node->list, path);
                break;
            }
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);  
    return 0;
}


#if 0
#endif

