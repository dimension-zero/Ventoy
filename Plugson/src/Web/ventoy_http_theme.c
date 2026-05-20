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
void ventoy_data_default_theme(data_theme *data)
{
    memset(data, 0, sizeof(data_theme));
    strlcpy(data->gfxmode, "1024x768");
    scnprintf(data->ventoy_left, sizeof(data->ventoy_left), "5%%");
    scnprintf(data->ventoy_top, sizeof(data->ventoy_top), "95%%");
    scnprintf(data->ventoy_color, sizeof(data->ventoy_color), "%s", "#0000ff");
}

int ventoy_data_cmp_theme(data_theme *data1, data_theme *data2)
{
    if (data1->display_mode != data2->display_mode ||
        strcmp(data1->ventoy_left, data2->ventoy_left) ||
        strcmp(data1->ventoy_top, data2->ventoy_top) ||
        strcmp(data1->gfxmode, data2->gfxmode) ||
        strcmp(data1->ventoy_color, data2->ventoy_color)
        )
    {
        return 1;
    }

    if (ventoy_path_list_cmp(data1->filelist, data2->filelist))
    {
        return 1;
    }

    if (ventoy_path_list_cmp(data1->fontslist, data2->fontslist))
    {
        return 1;
    }

    return 0;
}


int ventoy_data_save_theme(data_theme *data, const char *title, char *buf, int buflen)
{
    int pos = 0;
    path_node *node = NULL;
    data_theme *def = g_data_theme + bios_max;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);

    VTOY_JSON_FMT_KEY_L(L1, title);
    VTOY_JSON_FMT_OBJ_BEGIN_N();

    if (data->filelist)
    {
        if (data->filelist->next)
        {
            VTOY_JSON_FMT_KEY_L(L2, "file");
            VTOY_JSON_FMT_ARY_BEGIN_N();

            for (node = data->filelist; node; node = node->next)
            {
                VTOY_JSON_FMT_ITEM_PATH_LN(L3, node->path);
            }

            VTOY_JSON_FMT_ARY_ENDEX_LN(L2);
        
            if (def->default_file != data->default_file)
            {
                VTOY_JSON_FMT_SINT_LN(L2, "default_file", data->default_file);                
            }
            
            if (def->resolution_fit != data->resolution_fit)
            {
                VTOY_JSON_FMT_SINT_LN(L2, "resolution_fit", data->resolution_fit);                
            }
        }
        else
        {
            VTOY_JSON_FMT_STRN_PATH_LN(L2, "file", data->filelist->path);
        }
    }

    if (data->display_mode != def->display_mode)
    {
        if (display_mode_cli == data->display_mode)
        {
            VTOY_JSON_FMT_STRN_LN(L2, "display_mode", "CLI");
        }
        else if (display_mode_serial == data->display_mode)
        {
            VTOY_JSON_FMT_STRN_LN(L2, "display_mode", "serial");
        }
        else if (display_mode_ser_console == data->display_mode)
        {
            VTOY_JSON_FMT_STRN_LN(L2, "display_mode", "serial_console");
        }
        else
        {
            VTOY_JSON_FMT_STRN_LN(L2, "display_mode", "GUI");
        }
    }

    VTOY_JSON_FMT_DIFF_STRN(L2, "gfxmode", gfxmode);
    
    VTOY_JSON_FMT_DIFF_STRN(L2, "ventoy_left", ventoy_left);
    VTOY_JSON_FMT_DIFF_STRN(L2, "ventoy_top", ventoy_top);
    VTOY_JSON_FMT_DIFF_STRN(L2, "ventoy_color", ventoy_color);
    
    if (data->fontslist)
    {
        VTOY_JSON_FMT_KEY_L(L2, "fonts");
        VTOY_JSON_FMT_ARY_BEGIN_N();

        for (node = data->fontslist; node; node = node->next)
        {
			VTOY_JSON_FMT_ITEM_PATH_LN(L3, node->path);
        }
        
        VTOY_JSON_FMT_ARY_ENDEX_LN(L2);
    }
    
    VTOY_JSON_FMT_OBJ_ENDEX_LN(L1);
    VTOY_JSON_FMT_END(pos);

    return pos;
}


int ventoy_data_json_theme(data_theme *data, char *buf, int buflen)
{
    int pos = 0;
    path_node *node = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);
    VTOY_JSON_FMT_OBJ_BEGIN();

    VTOY_JSON_FMT_SINT("default_file",  data->default_file);
    VTOY_JSON_FMT_SINT("resolution_fit",  data->resolution_fit);
    VTOY_JSON_FMT_SINT("display_mode",  data->display_mode);
    VTOY_JSON_FMT_STRN("gfxmode", data->gfxmode);
    
    VTOY_JSON_FMT_STRN("ventoy_color", data->ventoy_color);
    VTOY_JSON_FMT_STRN("ventoy_left", data->ventoy_left);
    VTOY_JSON_FMT_STRN("ventoy_top", data->ventoy_top);
    
    VTOY_JSON_FMT_KEY("filelist");
    VTOY_JSON_FMT_ARY_BEGIN();
    for (node = data->filelist; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN();
        VTOY_JSON_FMT_STRN("path", node->path);
        VTOY_JSON_FMT_SINT("valid", ventoy_is_file_exist("%s%s", g_cur_dir, node->path));
        VTOY_JSON_FMT_OBJ_ENDEX();
    }
    VTOY_JSON_FMT_ARY_ENDEX();
    
    VTOY_JSON_FMT_KEY("fontslist");
    VTOY_JSON_FMT_ARY_BEGIN();
    for (node = data->fontslist; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN();
        VTOY_JSON_FMT_STRN("path", node->path);
        VTOY_JSON_FMT_SINT("valid", ventoy_is_file_exist("%s%s", g_cur_dir, node->path));
        VTOY_JSON_FMT_OBJ_ENDEX();
    }
    VTOY_JSON_FMT_ARY_ENDEX();
    
    VTOY_JSON_FMT_OBJ_END();
    VTOY_JSON_FMT_END(pos);

    return pos;
}

int ventoy_api_get_theme(struct mg_connection *conn, VTOY_JSON *json)
{
    api_get_func(conn, json, theme);
    return 0;
}

int ventoy_api_save_theme(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    data_theme *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_theme + index;

    VTOY_JSON_INT("default_file", data->default_file);
    VTOY_JSON_INT("resolution_fit", data->resolution_fit);
    VTOY_JSON_INT("display_mode", data->display_mode);
    VTOY_JSON_STR("gfxmode", data->gfxmode);
    VTOY_JSON_STR("ventoy_left", data->ventoy_left);
    VTOY_JSON_STR("ventoy_top", data->ventoy_top);
    VTOY_JSON_STR("ventoy_color", data->ventoy_color);

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);  
    return 0;
}


int ventoy_api_theme_add_file(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    path_node *node = NULL;
    path_node *cur = NULL;
    data_theme *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_theme + index;

    path = VTOY_JSON_STR_EX("path");
    if (path)
    {
        if (ventoy_is_real_exist_common(path, data->filelist, path_node))
        {
            ventoy_json_result(conn, VTOY_JSON_DUPLICATE);
            return 0;
        }

        node = zalloc(sizeof(path_node));
        if (node)
        {
            scnprintf(node->path, sizeof(node->path), "%s", path);

            vtoy_list_add(data->filelist, cur, node);            
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET); 
    return 0;
}

int ventoy_api_theme_del_file(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    path_node *node = NULL;
    path_node *last = NULL;
    data_theme *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_theme + index;

    path = VTOY_JSON_STR_EX("path");
    if (path)
    {
        if (strcmp(path, VTOY_DEL_ALL_PATH) == 0)
        {
            vtoy_list_free(path_node, data->filelist);
        }
        else
        {
            vtoy_list_del(last, node, data->filelist, path);
        }    
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);  
    return 0;
}

int ventoy_api_theme_add_font(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    path_node *node = NULL;
    path_node *cur = NULL;
    data_theme *data = NULL;

    vtoy_json_get_int(json, "index", &index);
    data = g_data_theme + index;

    path = VTOY_JSON_STR_EX("path");
    if (path)
    {
        if (ventoy_is_real_exist_common(path, data->fontslist, path_node))
        {
            ventoy_json_result(conn, VTOY_JSON_DUPLICATE);
            return 0;
        }

        node = zalloc(sizeof(path_node));
        if (node)
        {
            scnprintf(node->path, sizeof(node->path), "%s", path);
            vtoy_list_add(data->fontslist, cur, node);
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);   
    return 0;
}


int ventoy_api_theme_del_font(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    path_node *node = NULL;
    path_node *last = NULL;
    data_theme *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_theme + index;

    path = VTOY_JSON_STR_EX("path");
    if (path)
    {
        if (strcmp(path, VTOY_DEL_ALL_PATH) == 0)
        {
            vtoy_list_free(path_node, data->fontslist);
        }
        else
        {
            vtoy_list_del(last, node, data->fontslist, path);            
        }    
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);   
    return 0;
}

#if 0
#endif

