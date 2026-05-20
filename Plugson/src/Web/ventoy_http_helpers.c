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

path_node * ventoy_path_node_add_array(VTOY_JSON *array)
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


