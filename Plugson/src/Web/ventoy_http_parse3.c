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
int ventoy_parse_conf_replace(VTOY_JSON *json, void *p)
{
    int img = 0;
    const char *path = NULL;
    const char *org = NULL;
    const char *new = NULL;
    data_conf_replace *data = (data_conf_replace *)p;
    conf_replace_node *tail = NULL;
    conf_replace_node *pnode = NULL;
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

        path = vtoy_json_get_string_ex(node->pstChild, "iso");
        org = vtoy_json_get_string_ex(node->pstChild, "org");
        new = vtoy_json_get_string_ex(node->pstChild, "new");

        img = 0;
        vtoy_json_get_int(node->pstChild, "img", &img);

        if (path && org && new)
        {
            pnode = zalloc(sizeof(conf_replace_node));
            if (pnode)
            {
                strlcpy(pnode->path, path);
                strlcpy(pnode->org, org);
                strlcpy(pnode->new, new);
                if (img == 1)
                {
                    pnode->image = img;                    
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
    }    

    return 0;
}
int ventoy_parse_password(VTOY_JSON *json, void *p)
{
    int type;
    const char *bootpwd = NULL;
    const char *isopwd= NULL;
    const char *wimpwd= NULL;
    const char *imgpwd= NULL;
    const char *efipwd= NULL;
    const char *vhdpwd= NULL;
    const char *vtoypwd= NULL;
    const char *path = NULL;
    const char *pwd = NULL;
    data_password *data = (data_password *)p;
    menu_password *tail = NULL;
    menu_password *pnode = NULL;
    VTOY_JSON *node = NULL;
    VTOY_JSON *menupwd = NULL;

    if (json->enDataType != JSON_TYPE_OBJECT)
    {
        return 0;
    }

    bootpwd = vtoy_json_get_string_ex(json->pstChild, "bootpwd");
    isopwd = vtoy_json_get_string_ex(json->pstChild, "isopwd");
    wimpwd = vtoy_json_get_string_ex(json->pstChild, "wimpwd");
    imgpwd = vtoy_json_get_string_ex(json->pstChild, "imgpwd");
    efipwd = vtoy_json_get_string_ex(json->pstChild, "efipwd");
    vhdpwd = vtoy_json_get_string_ex(json->pstChild, "vhdpwd");
    vtoypwd = vtoy_json_get_string_ex(json->pstChild, "vtoypwd");


    if (bootpwd) strlcpy(data->bootpwd, bootpwd);
    if (isopwd) strlcpy(data->isopwd, isopwd);
    if (wimpwd) strlcpy(data->wimpwd, wimpwd);
    if (imgpwd) strlcpy(data->imgpwd, imgpwd);
    if (efipwd) strlcpy(data->efipwd, efipwd);
    if (vhdpwd) strlcpy(data->vhdpwd, vhdpwd);
    if (vtoypwd) strlcpy(data->vtoypwd, vtoypwd);
    
    
    menupwd = vtoy_json_find_item(json->pstChild, JSON_TYPE_ARRAY, "menupwd");
    if (!menupwd)
    {
        return 0;
    }

    for (node = menupwd->pstChild; node; node = node->pstNext)
    {
        if (node->enDataType != JSON_TYPE_OBJECT)
        {
            continue;
        }

        type = 0;
        path = vtoy_json_get_string_ex(node->pstChild, "file");
        if (!path)
        {
            path = vtoy_json_get_string_ex(node->pstChild, "parent");
            type = 1;
        }
        pwd = vtoy_json_get_string_ex(node->pstChild, "pwd");

        if (path && pwd)
        {
            pnode = zalloc(sizeof(menu_password));
            if (pnode)
            {
                pnode->type = type;
                strlcpy(pnode->path, path);
                strlcpy(pnode->pwd, pwd);
                
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

int ventoy_parse_image_list_real(VTOY_JSON *json, int type, void *p)
{
    VTOY_JSON *node = NULL;
    data_image_list *data = (data_image_list *)p;
    path_node *tail = NULL;
    path_node *pnode = NULL;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        return 0;
    }

    data->type = type;

    for (node = json->pstChild; node; node = node->pstNext)
    {
        if (node->enDataType == JSON_TYPE_STRING)
        {
            pnode = zalloc(sizeof(path_node));
            if (pnode)
            {
                strlcpy(pnode->path, node->unData.pcStrVal);
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
int ventoy_parse_image_blacklist(VTOY_JSON *json, void *p)
{
     return ventoy_parse_image_list_real(json, 1, p);
}
int ventoy_parse_image_list(VTOY_JSON *json, void *p)
{
    return ventoy_parse_image_list_real(json, 0, p);
}

int ventoy_parse_auto_memdisk(VTOY_JSON *json, void *p)
{
    VTOY_JSON *node = NULL;
    data_auto_memdisk *data = (data_auto_memdisk *)p;
    path_node *tail = NULL;
    path_node *pnode = NULL;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        return 0;
    }

    for (node = json->pstChild; node; node = node->pstNext)
    {
        if (node->enDataType == JSON_TYPE_STRING)
        {
            pnode = zalloc(sizeof(path_node));
            if (pnode)
            {
                strlcpy(pnode->path, node->unData.pcStrVal);
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
int ventoy_parse_dud(VTOY_JSON *json, void *p)
{
    int count = 0;
    const char *path = NULL;
    const char *file = NULL;
    data_dud *data = (data_dud *)p;
    dud_node *tail = NULL;
    dud_node *pnode = NULL;
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
                
        file = vtoy_json_get_string_ex(node->pstChild, "dud");
        if (file)
        {
            pnode = zalloc(sizeof(dud_node));
            if (pnode)
            {
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

        filelist = vtoy_json_find_item(node->pstChild, JSON_TYPE_ARRAY, "dud");
        if (!filelist)
        {
            continue;
        }

        pnode = zalloc(sizeof(dud_node));
        if (!pnode)
        {
            continue;
        }

        strlcpy(pnode->path, path);

        for (filenode = filelist->pstChild; filenode; filenode = filenode->pstNext)
        {
            if (filenode->enDataType != JSON_TYPE_STRING)
            {
                continue;
            }

            pathnode = zalloc(sizeof(path_node));
            if (pathnode)
            {
                strlcpy(pathnode->path, filenode->unData.pcStrVal);
                count++;

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



#if 0
#endif


