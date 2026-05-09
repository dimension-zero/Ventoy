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

}
int ventoy_parse_auto_install(VTOY_JSON *json, void *p)
{
    int type;
    int count;
    int timeout;
    int timeouten;
    int autosel;
    int autoselen;
    const char *path = NULL;
    const char *file = NULL;
    data_auto_install *data = (data_auto_install *)p;
    auto_install_node *tail = NULL;
    auto_install_node *pnode = NULL;
    path_node *pathnode = NULL;
    path_node *pathtail = NULL;
    VTOY_JSON *node = NULL;
    VTOY_JSON *filelist = NULL;
    VTOY_JSON *filenode = NULL;

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

        type = 0;
        path = vtoy_json_get_string_ex(node->pstChild, "image");
        if (!path)
        {
            path = vtoy_json_get_string_ex(node->pstChild, "parent");
            type = 1;
        }
        if (!path)
        {
            continue;
        }
                
        file = vtoy_json_get_string_ex(node->pstChild, "template");
        if (file)
        {
            pnode = zalloc(sizeof(auto_install_node));
            if (pnode)
            {
                pnode->type = type;
                pnode->autosel = 1;
                strlcpy(pnode->path, path);

                pathnode = zalloc(sizeof(path_node));
                if (pathnode)
                {
                    strlcpy(pathnode->path, file);
                    pnode->list = pathnode;
                }
                else
                {
                    free(pnode);
                }

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

            continue;
        }


        timeouten = autoselen = 0;
        if (JSON_SUCCESS == vtoy_json_get_int(node->pstChild, "timeout", &timeout))
        {
            timeouten = 1;
        }
        if (JSON_SUCCESS == vtoy_json_get_int(node->pstChild, "autosel", &autosel))
        {
            autoselen = 1;
        }

        filelist = vtoy_json_find_item(node->pstChild, JSON_TYPE_ARRAY, "template");
        if (!filelist)
        {
            continue;
        }

        pnode = zalloc(sizeof(auto_install_node));
        if (!pnode)
        {
            continue;
        }

        pnode->type = type;
        pnode->autoselen = autoselen;
        pnode->timeouten = timeouten;
        pnode->timeout = timeout;
        pnode->autosel = autosel;
        strlcpy(pnode->path, path);

        count = 0;
        for (filenode = filelist->pstChild; filenode; filenode = filenode->pstNext)
        {
            if (filenode->enDataType != JSON_TYPE_STRING)
            {
                continue;
            }

            pathnode = zalloc(sizeof(path_node));
            if (pathnode)
            {
                count++;
                strlcpy(pathnode->path, filenode->unData.pcStrVal);

                if (pnode->list)
                {
                    pathtail->next = pathnode;
                    pathtail = pathnode;
                }
                else
                {
                    pnode->list = pathtail = pathnode;
                }                
            }
        }

        if (count == 0)
        {
            free(pnode);
        }
        else
        {
            if (pnode->autoselen && pnode->autosel > count)
            {
                pnode->autosel = 1;
            }

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

    return 0;
}
int ventoy_parse_persistence(VTOY_JSON *json, void *p)
{
    int count;
    int timeout;
    int timeouten;
    int autosel;
    int autoselen;
    const char *path = NULL;
    const char *file = NULL;
    data_persistence *data = (data_persistence *)p;
    persistence_node *tail = NULL;
    persistence_node *pnode = NULL;
    path_node *pathnode = NULL;
    path_node *pathtail = NULL;
    VTOY_JSON *node = NULL;
    VTOY_JSON *filelist = NULL;
    VTOY_JSON *filenode = NULL;

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

        path = vtoy_json_get_string_ex(node->pstChild, "image");
        if (!path)
        {
            continue;
        }
                
        file = vtoy_json_get_string_ex(node->pstChild, "backend");
        if (file)
        {
            pnode = zalloc(sizeof(persistence_node));
            if (pnode)
            {
                pnode->type = 0;
                pnode->autosel = 1;
                strlcpy(pnode->path, path);

                pathnode = zalloc(sizeof(path_node));
                if (pathnode)
                {
                    strlcpy(pathnode->path, file);
                    pnode->list = pathnode;
                }
                else
                {
                    free(pnode);
                }

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

            continue;
        }


        timeouten = autoselen = 0;
        if (JSON_SUCCESS == vtoy_json_get_int(node->pstChild, "timeout", &timeout))
        {
            timeouten = 1;
        }
        if (JSON_SUCCESS == vtoy_json_get_int(node->pstChild, "autosel", &autosel))
        {
            autoselen = 1;
        }

        filelist = vtoy_json_find_item(node->pstChild, JSON_TYPE_ARRAY, "backend");
        if (!filelist)
        {
            continue;
        }

        pnode = zalloc(sizeof(persistence_node));
        if (!pnode)
        {
            continue;
        }

        pnode->type = 0;
        pnode->autoselen = autoselen;
        pnode->timeouten = timeouten;
        pnode->timeout = timeout;
        pnode->autosel = autosel;
        strlcpy(pnode->path, path);

        count = 0;
        for (filenode = filelist->pstChild; filenode; filenode = filenode->pstNext)
        {
            if (filenode->enDataType != JSON_TYPE_STRING)
            {
                continue;
            }

            pathnode = zalloc(sizeof(path_node));
            if (pathnode)
            {
                count++;
                strlcpy(pathnode->path, filenode->unData.pcStrVal);

                if (pnode->list)
                {
                    pathtail->next = pathnode;
                    pathtail = pathnode;
                }
                else
                {
                    pnode->list = pathtail = pathnode;
                }                
            }
        }

        if (count == 0)
        {
            free(pnode);
        }
        else
        {
            if (pnode->autoselen && pnode->autosel > count)
            {
                pnode->autosel = 1;
            }

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

    return 0;
}
int ventoy_parse_injection(VTOY_JSON *json, void *p)
{
    int type;
    const char *path = NULL;
    const char *archive = NULL;
    data_injection *data = (data_injection *)p;
    injection_node *tail = NULL;
    injection_node *pnode = NULL;
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

        type = 0;
        path = vtoy_json_get_string_ex(node->pstChild, "image");
        if (!path)
        {
            path = vtoy_json_get_string_ex(node->pstChild, "parent");
            type = 1;
        }
        archive = vtoy_json_get_string_ex(node->pstChild, "archive");

        if (path && archive)
        {
            pnode = zalloc(sizeof(injection_node));
            if (pnode)
            {
                pnode->type = type;
                strlcpy(pnode->path, path);
                strlcpy(pnode->archive, archive);
                
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
