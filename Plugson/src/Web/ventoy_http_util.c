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

int ventoy_is_kbd_valid(const char *key)
{
    int i = 0;
    
    for (i = 0; g_ventoy_kbd_layout[i]; i++)
    {
        if (strcmp(g_ventoy_kbd_layout[i], key) == 0)
        {
            return 1;
        }
    }

    return 0;
}

const char * ventoy_real_path(const char *org)
{
    int count = 0;
    
    if (g_sysinfo.pathcase)
    {
        scnprintf(g_pub_path, MAX_PATH, "%s", org);
        count = ventoy_path_case(g_pub_path + 1, 1);
        if (count > 0)
        {
            return g_pub_path;
        }
        return org;
    }
    else
    {
        return org;
    }
}


int ventoy_json_result(struct mg_connection *conn, const char *err)
{
    mg_printf(conn, 
              "HTTP/1.1 200 OK \r\n"
              "Content-Type: application/json\r\n"
              "Content-Length: %d\r\n"
              "\r\n%s",
              (int)strlen(err), err);

    return 0;
}

int ventoy_json_buffer(struct mg_connection *conn, const char *json_buf, int json_len)
{
    mg_printf(conn, 
              "HTTP/1.1 200 OK \r\n"
              "Content-Type: application/json\r\n"
              "Content-Length: %d\r\n"
              "\r\n%s",
              json_len, json_buf); 

    return 0;
}

void ventoy_free_path_node_list(path_node *list)
{
    path_node *next = NULL;
    path_node *node = list;

    while (node)
    {
        next = node->next;
        free(node);
        node = next;
    }    
}

int ventoy_path_is_real_exist(const char *path, void *head, size_t pathoff, size_t nextoff)
{
    char *node = NULL;
    const char *nodepath = NULL;
    const char *realpath = NULL;
    char pathbuf[MAX_PATH];

    if (strchr(path, '*'))
    {
        return 0;
    }

    realpath = ventoy_real_path(path);
    scnprintf(pathbuf, sizeof(pathbuf), "%s", realpath);

    node = (char *)head;
    while (node)
    {
        nodepath = node + pathoff;
        if (NULL == strchr(nodepath, '*'))
        {
            realpath = ventoy_real_path(nodepath);
            if (strcmp(pathbuf, realpath) == 0)
            {
                return 1;
            }
        }
        
        memcpy(&node, node + nextoff, sizeof(node));
    }

    return 0;
}

static path_node * ventoy_path_node_add_array(VTOY_JSON *array)
{
    path_node *head = NULL;
    path_node *node = NULL;
    path_node *cur = NULL;
    VTOY_JSON *item = NULL;

    for (item = array->pstChild; item; item = item->pstNext)
    {
        node = zalloc(sizeof(path_node));
        if (node)
        {                      
            scnprintf(node->path, sizeof(node->path), "%s", item->unData.pcStrVal);
            vtoy_list_add(head, cur, node);
        }
    }

    return head;
}

int ventoy_check_fuzzy_path(char *path, int prefix)
{
    int rc;
    char c;
    char *cur = NULL;
    char *pos = NULL;
    
    if (!path)
    {
        return 0;
    }

    pos = strchr(path, '*');
    if (pos)
    {
        for (cur = pos; *cur; cur++)
        {
            if (*cur == '/')
            {
                return 0;
            }
        }
    
        while (pos != path)
        {
            if (*pos == '/')
            {
                break;
            }
            pos--;
        }

        if (*pos == '/')
        {
            if (pos != path)
            {
                c = *pos;
                *pos = 0;
                if (prefix)
                {
                    rc = ventoy_is_directory_exist("%s%s", g_cur_dir, path);
                }
                else
                {
                    rc = ventoy_is_directory_exist("%s", path);
                }
                *pos = c;

                if (rc == 0)
                {
                    return 0;
                }
            }

            return -1;
        }
        else
        {
            return 0;
        }
    }
    else
    {
        if (prefix)
        {
            return ventoy_is_file_exist("%s%s", g_cur_dir, path);                        
        }
        else
        {
            return ventoy_is_file_exist("%s", path);            
        }
    }
}

int ventoy_path_list_cmp(path_node *list1, path_node *list2)
{
    if (NULL == list1 && NULL == list2)
    {
        return 0;
    }
    else if (list1 && list2)
    {
        while (list1 && list2)
        {
            if (strcmp(list1->path, list2->path))
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
}


int ventoy_api_device_info(struct mg_connection *conn, VTOY_JSON *json)
{
    int pos = 0;
    
    (void)json;

    VTOY_JSON_FMT_BEGIN(pos, JSON_BUFFER, JSON_BUF_MAX);
    VTOY_JSON_FMT_OBJ_BEGIN();
    
    VTOY_JSON_FMT_STRN("dev_name", g_sysinfo.cur_model);
    VTOY_JSON_FMT_STRN("dev_capacity", g_sysinfo.cur_capacity);
    VTOY_JSON_FMT_STRN("dev_fs", g_sysinfo.cur_fsname);
    VTOY_JSON_FMT_STRN("ventoy_ver", g_sysinfo.cur_ventoy_ver);
    VTOY_JSON_FMT_SINT("part_style", g_sysinfo.cur_part_style);
    VTOY_JSON_FMT_SINT("secure_boot", g_sysinfo.cur_secureboot);

    VTOY_JSON_FMT_OBJ_END();
    VTOY_JSON_FMT_END(pos);

    ventoy_json_buffer(conn, JSON_BUFFER, pos);
    return 0;
}

int ventoy_api_sysinfo(struct mg_connection *conn, VTOY_JSON *json)
{
    int pos = 0;
    
    (void)json;

    VTOY_JSON_FMT_BEGIN(pos, JSON_BUFFER, JSON_BUF_MAX);
    VTOY_JSON_FMT_OBJ_BEGIN();
    VTOY_JSON_FMT_STRN("language", ventoy_get_os_language());
    VTOY_JSON_FMT_STRN("curdir", g_cur_dir);

    //read clear
    VTOY_JSON_FMT_SINT("syntax_error", g_sysinfo.syntax_error);
    g_sysinfo.syntax_error = 0;
    
    VTOY_JSON_FMT_SINT("invalid_config", g_sysinfo.invalid_config);
    g_sysinfo.invalid_config = 0;
    
    #if defined(_MSC_VER) || defined(WIN32)
    VTOY_JSON_FMT_STRN("os", "windows");
    #else
    VTOY_JSON_FMT_STRN("os", "linux");
    #endif

    VTOY_JSON_FMT_OBJ_END();
    VTOY_JSON_FMT_END(pos);

    ventoy_json_buffer(conn, JSON_BUFFER, pos);
    return 0;
}

int ventoy_api_handshake(struct mg_connection *conn, VTOY_JSON *json)
{
    int i = 0;
    int j = 0;
    int pos = 0;
    char key[128];
    
    (void)json;

    VTOY_JSON_FMT_BEGIN(pos, JSON_BUFFER, JSON_BUF_MAX);
    VTOY_JSON_FMT_OBJ_BEGIN();
    VTOY_JSON_FMT_SINT("status", 0);
    VTOY_JSON_FMT_SINT("save_error", g_sysinfo.config_save_error);
    g_sysinfo.config_save_error = 0;

    for (i = 0; i < plugin_type_max; i++)
    {
        scnprintf(key, sizeof(key), "exist_%s", g_plugin_name[i]);
        VTOY_JSON_FMT_KEY(key);
        VTOY_JSON_FMT_ARY_BEGIN();
        for (j = 0; j < bios_max; j++)
        {
            VTOY_JSON_FMT_ITEM_INT(g_json_exist[i][j]);
        }
        VTOY_JSON_FMT_ARY_ENDEX();
    }
    
    VTOY_JSON_FMT_OBJ_END();
    VTOY_JSON_FMT_END(pos);

    ventoy_json_buffer(conn, JSON_BUFFER, pos);
    return 0;
}

int ventoy_api_check_exist(struct mg_connection *conn, VTOY_JSON *json)
{
    int dir = 0;
    int pos = 0;
    int exist = 0;
    const char *path = NULL;

    path = vtoy_json_get_string_ex(json, "path");
    vtoy_json_get_int(json, "dir", &dir);

    if (path)
    {
        if (dir)
        {
            exist = ventoy_is_directory_exist("%s", path);
        }
        else
        {
            exist = ventoy_is_file_exist("%s", path);
        }
    }

    VTOY_JSON_FMT_BEGIN(pos, JSON_BUFFER, JSON_BUF_MAX);
    VTOY_JSON_FMT_OBJ_BEGIN();
    VTOY_JSON_FMT_SINT("exist", exist);
    VTOY_JSON_FMT_OBJ_END();
    VTOY_JSON_FMT_END(pos);

    ventoy_json_buffer(conn, JSON_BUFFER, pos);
    return 0;
}

int ventoy_api_check_exist2(struct mg_connection *conn, VTOY_JSON *json)
{
    int dir1 = 0;
    int dir2 = 0;
    int fuzzy1 = 0;
    int fuzzy2 = 0;
    int pos = 0;
    int exist1 = 0;
    int exist2 = 0;
    const char *path1 = NULL;
    const char *path2 = NULL;

    path1 = vtoy_json_get_string_ex(json, "path1");
    path2 = vtoy_json_get_string_ex(json, "path2");
    vtoy_json_get_int(json, "dir1", &dir1);
    vtoy_json_get_int(json, "dir2", &dir2);
    vtoy_json_get_int(json, "fuzzy1", &fuzzy1);
    vtoy_json_get_int(json, "fuzzy2", &fuzzy2);

    if (path1)
    {
        if (dir1)
        {
            exist1 = ventoy_is_directory_exist("%s", path1);
        }
        else
        {
            if (fuzzy1)
            {
                exist1 = ventoy_check_fuzzy_path((char *)path1, 0);                
            }
            else
            {
                exist1 = ventoy_is_file_exist("%s", path1);
            }
        }
    }
    
    if (path2)
    {
        if (dir2)
        {
            exist2 = ventoy_is_directory_exist("%s", path2);
        }
        else
        {
            if (fuzzy2)
            {
                exist2 = ventoy_check_fuzzy_path((char *)path2, 0);                
            }
            else
            {
                exist2 = ventoy_is_file_exist("%s", path2);
            }
        }
    }

    VTOY_JSON_FMT_BEGIN(pos, JSON_BUFFER, JSON_BUF_MAX);
    VTOY_JSON_FMT_OBJ_BEGIN();
    VTOY_JSON_FMT_SINT("exist1", exist1);
    VTOY_JSON_FMT_SINT("exist2", exist2);
    VTOY_JSON_FMT_OBJ_END();
    VTOY_JSON_FMT_END(pos);

    ventoy_json_buffer(conn, JSON_BUFFER, pos);
    return 0;
}

int ventoy_api_check_fuzzy(struct mg_connection *conn, VTOY_JSON *json)
{
    int pos = 0;
    int exist = 0;
    const char *path = NULL;

    path = vtoy_json_get_string_ex(json, "path");
    if (path)
    {
        exist = ventoy_check_fuzzy_path((char *)path, 0);
    }

    VTOY_JSON_FMT_BEGIN(pos, JSON_BUFFER, JSON_BUF_MAX);
    VTOY_JSON_FMT_OBJ_BEGIN();
    VTOY_JSON_FMT_SINT("exist", exist);
    VTOY_JSON_FMT_OBJ_END();
    VTOY_JSON_FMT_END(pos);

    ventoy_json_buffer(conn, JSON_BUFFER, pos);
    return 0;
}


#if 0
#endif
