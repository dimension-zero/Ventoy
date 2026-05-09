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

int ventoy_api_preview_json(struct mg_connection *conn, VTOY_JSON *json)
{
    int i = 0;
    int pos = 0;
    int len = 0;
    int utf16enclen = 0;
    char *encodebuf = NULL;
    unsigned short *utf16buf = NULL;
    
    (void)json;

    /* We can not use json directly, because it will be formated in the JS. */

    len = ventoy_data_real_save_all(0);

    utf16buf = (unsigned short *)malloc(2 * len + 16);    
    if (!utf16buf)
    {
        goto json;
    }

    utf16enclen = (int)utf8_to_utf16((unsigned char *)JSON_SAVE_BUFFER, len, utf16buf, len + 2);

    encodebuf = (char *)malloc(utf16enclen * 4 + 16);
    if (!encodebuf)
    {
        goto json;
    }

    for (i = 0; i < utf16enclen; i++)
    {
        scnprintf(encodebuf + i * 4, 5, "%04X", utf16buf[i]);
    }

json:
    VTOY_JSON_FMT_BEGIN(pos, JSON_BUFFER, JSON_BUF_MAX);
    VTOY_JSON_FMT_OBJ_BEGIN();
    VTOY_JSON_FMT_STRN("json", (encodebuf ? encodebuf : ""));
    VTOY_JSON_FMT_OBJ_END();
    VTOY_JSON_FMT_END(pos);

    CHECK_FREE(encodebuf);
    CHECK_FREE(utf16buf);

    ventoy_json_buffer(conn, JSON_BUFFER, pos);
    return 0;
}


#if 0
#endif

int ventoy_data_save_all(void)
{
    ventoy_set_writeback_event();
    return 0;
}

int ventoy_data_real_save_all(int apilock)
{
    int i = 0;
    int pos = 0;
    char title[64];

    if (apilock)
    {
        pthread_mutex_lock(&g_api_mutex);        
    }

    ssprintf(pos, JSON_SAVE_BUFFER, JSON_BUF_MAX, "{\n");

    ventoy_save_plug(control);
    ventoy_save_plug(theme);
    ventoy_save_plug(menu_alias);
    ventoy_save_plug(menu_tip);
    ventoy_save_plug(menu_class);
    ventoy_save_plug(auto_install);
    ventoy_save_plug(persistence);
    ventoy_save_plug(injection);
    ventoy_save_plug(conf_replace);
    ventoy_save_plug(password);
    ventoy_save_plug(image_list);
    ventoy_save_plug(auto_memdisk);
    ventoy_save_plug(dud);
    
    if (JSON_SAVE_BUFFER[pos - 1] == '\n' && JSON_SAVE_BUFFER[pos - 2] == ',')
    {
        JSON_SAVE_BUFFER[pos - 2] = '\n';
        pos--;
    }
    ssprintf(pos, JSON_SAVE_BUFFER, JSON_BUF_MAX, "}\n");

    if (apilock)
    {
        pthread_mutex_unlock(&g_api_mutex);        
    }

    return pos;
}

int ventoy_http_writeback(void)
{
    int ret;
    int pos;
    char filename[128];

    ventoy_get_json_path(filename, NULL);

    pos = ventoy_data_real_save_all(1);
        
    #ifdef VENTOY_SIM
    printf("%s", JSON_SAVE_BUFFER);
    #endif
    
    ret = ventoy_write_buf_to_file(filename, JSON_SAVE_BUFFER, pos);
    if (ret)
    {
        vlog("Failed to write ventoy.json file.\n");
        g_sysinfo.config_save_error = 1;
    }

    return 0;
}

