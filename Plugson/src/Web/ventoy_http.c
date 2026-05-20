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
const char *g_json_title_postfix[bios_max + 1] = 
{
    "", "_legacy", "_uefi", "_ia32", "_aa64", "_mips", ""
};

const char *g_ventoy_kbd_layout[] =
{
    "QWERTY_USA", "AZERTY", "CZECH_QWERTY", "CZECH_QWERTZ", "DANISH", 
    "DVORAK_USA", "FRENCH", "GERMAN", "ITALIANO", "JAPAN_106", "LATIN_USA", 
    "PORTU_BRAZIL", "QWERTY_UK", "QWERTZ", "QWERTZ_HUN", "QWERTZ_SLOV_CROAT", 
    "SPANISH", "SWEDISH", "TURKISH_Q", "VIETNAMESE",
    NULL
};

#define VTOY_DEL_ALL_PATH   "4119ae33-98ea-448e-b9c0-569aafcf1fb4"

int g_json_exist[plugin_type_max][bios_max];
const char *g_plugin_name[plugin_type_max] = 
{
    "control", "theme", "menu_alias", "menu_tip", 
    "menu_class", "auto_install", "persistence", "injection", 
    "conf_replace", "password", "image_list", 
    "auto_memdisk", "dud"
};

char g_ventoy_menu_lang[MAX_LANGUAGE][8];

char g_pub_path[2 * MAX_PATH];
data_control g_data_control[bios_max + 1];
data_theme g_data_theme[bios_max + 1];
data_alias g_data_menu_alias[bios_max + 1];
data_tip   g_data_menu_tip[bios_max + 1];
data_class g_data_menu_class[bios_max + 1];
data_image_list g_data_image_list[bios_max + 1];
data_image_list *g_data_image_blacklist = g_data_image_list;
data_auto_memdisk g_data_auto_memdisk[bios_max + 1];
data_password g_data_password[bios_max + 1];
data_conf_replace g_data_conf_replace[bios_max + 1];
data_injection g_data_injection[bios_max + 1];
data_auto_install g_data_auto_install[bios_max + 1];
data_persistence g_data_persistence[bios_max + 1];
data_dud g_data_dud[bios_max + 1];

char *g_pub_json_buffer = NULL;
char *g_pub_save_buffer = NULL;
#define JSON_BUFFER g_pub_json_buffer
#define JSON_SAVE_BUFFER g_pub_save_buffer

pthread_mutex_t g_api_mutex;
struct mg_context *g_ventoy_http_ctx = NULL;

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


int ventoy_http_start(const char *ip, const char *port)
{
    int i = 0;
    int ret = 0;
    char addr[128];
    char filename[128];
    char backupname[128];
    struct mg_callbacks callbacks;
    const char *options[] = 
    {
	    "listening_ports",    "24681",
        "document_root",      "www",
        "index_files",        "index.html",
        "num_threads",        "16",
        "error_log_file",     LOG_FILE,
	    "request_timeout_ms", "10000",
	     NULL
    };

    for (i = 0; i <= bios_max; i++)
    {
        ventoy_data_default_control(g_data_control + i);
        ventoy_data_default_theme(g_data_theme + i);
        ventoy_data_default_menu_alias(g_data_menu_alias + i);
        ventoy_data_default_menu_class(g_data_menu_class + i);
        ventoy_data_default_menu_tip(g_data_menu_tip + i);        
        ventoy_data_default_auto_install(g_data_auto_install + i);
        ventoy_data_default_persistence(g_data_persistence + i);
        ventoy_data_default_injection(g_data_injection + i);
        ventoy_data_default_conf_replace(g_data_conf_replace + i);
        ventoy_data_default_password(g_data_password + i);
        ventoy_data_default_image_list(g_data_image_list + i);
        ventoy_data_default_auto_memdisk(g_data_auto_memdisk + i);
        ventoy_data_default_dud(g_data_dud + i);
    }

    ventoy_get_json_path(filename, backupname);
    if (ventoy_is_file_exist("%s", filename))
    {
        ventoy_copy_file(filename, backupname);
        ret = ventoy_load_old_json(filename);
        if (ret == 0)
        {
            ventoy_data_real_save_all(0);
        }
    }

    /* option */
    scnprintf(addr, sizeof(addr), "%s:%s", ip, port);
    options[1] = addr;

    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.begin_request = ventoy_request_handler;
#ifndef VENTOY_SIM
    callbacks.open_file = ventoy_web_openfile;
#endif
    g_ventoy_http_ctx = mg_start(&callbacks, NULL, options);

    ventoy_start_writeback_thread(ventoy_http_writeback);

    return g_ventoy_http_ctx ? 0 : 1;
}

int ventoy_http_stop(void)
{
    if (g_ventoy_http_ctx)
    {
        mg_stop(g_ventoy_http_ctx);        
    }

    ventoy_stop_writeback_thread();
    return 0;
}

int ventoy_http_init(void)
{
    int i = 0;
    
#ifdef VENTOY_SIM
    char *Buffer = NULL;
    int BufLen = 0;

    ventoy_read_file_to_buf("www/menulist", 4, (void **)&Buffer, &BufLen);
    if (Buffer)
    {
        for (i = 0; i < BufLen / 5; i++)
        {
            memcpy(g_ventoy_menu_lang[i], Buffer + i * 5, 5);
            g_ventoy_menu_lang[i][5] = 0;
        }
        free(Buffer);
    }
#else
    ventoy_file *file;
    
    file = ventoy_tar_find_file("www/menulist");
    if (file)
    {
        for (i = 0; i < file->size / 5; i++)
        {
            memcpy(g_ventoy_menu_lang[i], (char *)(file->addr) + i * 5, 5);
            g_ventoy_menu_lang[i][5] = 0;
        }
    }
#endif

    if (!g_pub_json_buffer)
    {
        g_pub_json_buffer = malloc(JSON_BUF_MAX * 2);
        g_pub_save_buffer = g_pub_json_buffer + JSON_BUF_MAX;
    }   


    pthread_mutex_init(&g_api_mutex, NULL);
    return 0;
}

void ventoy_http_exit(void)
{
    check_free(g_pub_json_buffer);
    g_pub_json_buffer = NULL;
    g_pub_save_buffer = NULL;
    
    pthread_mutex_destroy(&g_api_mutex);
}


