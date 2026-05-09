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

int ventoy_load_old_json(const char *filename)
{
    int ret = 0;
    int offset = 0;
    int buflen = 0;
    char *buffer = NULL;
    unsigned char *start = NULL;
    VTOY_JSON *json = NULL;
    VTOY_JSON *node = NULL;
    VTOY_JSON *next = NULL;

    ret = ventoy_read_file_to_buf(filename, 4, (void **)&buffer, &buflen);
    if (ret)
    {
        vlog("Failed to read old ventoy.json file.\n");
        return 1;
    }
    buffer[buflen] = 0;

    start = (unsigned char *)buffer;

    if (start[0] == 0xef && start[1] == 0xbb && start[2] == 0xbf)
    {
        offset = 3;
    }
    else if ((start[0] == 0xff && start[1] == 0xfe) || (start[0] == 0xfe && start[1] == 0xff))
    {
        vlog("ventoy.json is in UCS-2 encoding, ignore it.\n");
        free(buffer);
        return 1;
    }

    json = vtoy_json_create();
    if (!json)
    {
        free(buffer);
        return 1;
    }
    
    if (vtoy_json_parse_ex(json, buffer + offset, buflen - offset) == JSON_SUCCESS)
    {
        vlog("parse ventoy.json success\n");

        for (node = json->pstChild; node; node = node->pstNext)
        for (next = node->pstNext; next; next = next->pstNext)
        {
            if (node->pcName && next->pcName && strcmp(node->pcName, next->pcName) == 0)
            {
                vlog("ventoy.json contains duplicate key <%s>.\n", node->pcName);
                g_sysinfo.invalid_config = 1;
                ret = 1;
                goto end;
            }
        }

        for (node = json->pstChild; node; node = node->pstNext)
        {
            ventoy_parse_json(control);
            ventoy_parse_json(theme);
            ventoy_parse_json(menu_alias);
            ventoy_parse_json(menu_tip);
            ventoy_parse_json(menu_class);
            ventoy_parse_json(auto_install);
            ventoy_parse_json(persistence);
            ventoy_parse_json(injection);
            ventoy_parse_json(conf_replace);
            ventoy_parse_json(password);
            ventoy_parse_json(image_list);
            ventoy_parse_json(image_blacklist);
            ventoy_parse_json(auto_memdisk);
            ventoy_parse_json(dud);
        }
    }
    else
    {
        vlog("ventoy.json has syntax error.\n");    
        g_sysinfo.syntax_error = 1;
        ret = 1;
    }

end:
    vtoy_json_destroy(json);

    free(buffer);
    return ret;
}

