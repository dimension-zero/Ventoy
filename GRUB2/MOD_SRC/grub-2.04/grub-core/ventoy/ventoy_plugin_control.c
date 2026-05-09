/******************************************************************************
 * ventoy_plugin.c 
 *
 * Copyright (c) 2020, longpanda <admin@ventoy.net>
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
#include <grub/types.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/err.h>
#include <grub/dl.h>
#include <grub/disk.h>
#include <grub/device.h>
#include <grub/term.h>
#include <grub/partition.h>
#include <grub/file.h>
#include <grub/normal.h>
#include <grub/extcmd.h>
#include <grub/datetime.h>
#include <grub/i18n.h>
#include <grub/net.h>
#include <grub/crypto.h>
#include <grub/time.h>
#include <grub/font.h>
#include <grub/video.h>
#include <grub/ventoy.h>
#include "ventoy_def.h"
#include "ventoy_plugin_priv.h"

GRUB_MOD_LICENSE ("GPLv3+");

char g_arch_mode_suffix[64];
char g_iso_disk_name[128];

int g_theme_id = 0;
int g_theme_res_fit = 0;
int g_theme_num = 0;
theme_list *g_theme_head = NULL;
int g_theme_random = vtoy_theme_random_boot_second;
char g_theme_single_file[256];

int ventoy_plugin_control_check(VTOY_JSON *json, const char *isodisk)
{
    int rc = 0;
    VTOY_JSON *pNode = NULL;
    VTOY_JSON *pChild = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        grub_printf("Not array type %d\n", json->enDataType);
        return 1;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        if (pNode->enDataType == JSON_TYPE_OBJECT)
        {
            pChild = pNode->pstChild;
            if (pChild->enDataType == JSON_TYPE_STRING)
            {
                if (grub_strcmp(pChild->pcName, "VTOY_DEFAULT_IMAGE") == 0)
                {                    
                    grub_printf("%s: %s [%s]\n", pChild->pcName, pChild->unData.pcStrVal,
                        ventoy_check_file_exist("%s%s", isodisk, pChild->unData.pcStrVal) ? "OK" : "NOT EXIST");
                }
                else
                {
                    grub_printf("%s: %s\n", pChild->pcName, pChild->unData.pcStrVal);                    
                }
            }
            else
            {
                grub_printf("%s is NOT string type\n", pChild->pcName);
                rc = 1;
            }
        }
        else
        {
            grub_printf("%s is not an object\n", pNode->pcName);
            rc = 1;
        }
    }

    return rc;
}

int ventoy_plugin_control_entry(VTOY_JSON *json, const char *isodisk)
{
    VTOY_JSON *pNode = NULL;
    VTOY_JSON *pChild = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        debug("Not array %d\n", json->enDataType);
        return 0;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        if (pNode->enDataType == JSON_TYPE_OBJECT)
        {
            pChild = pNode->pstChild;
            if (pChild->enDataType == JSON_TYPE_STRING && pChild->pcName && pChild->unData.pcStrVal)
            {
                ventoy_set_env(pChild->pcName, pChild->unData.pcStrVal);
            }
        }
    }

    return 0;
}

int ventoy_plugin_theme_check(VTOY_JSON *json, const char *isodisk)
{
    int exist = 0;
    const char *value;
    VTOY_JSON *node;
    
    value = vtoy_json_get_string_ex(json->pstChild, "file");
    if (value)
    {
        grub_printf("file: %s\n", value);
        if (value[0] == '/')
        {
            exist = ventoy_check_file_exist("%s%s", isodisk, value);
        }
        else
        {
            exist = ventoy_check_file_exist("%s/ventoy/%s", isodisk, value);
        }
        
        if (exist == 0)
        {
            grub_printf("Theme file %s does NOT exist\n", value);
            return 1;
        }
    }
    else
    {
        node = vtoy_json_find_item(json->pstChild, JSON_TYPE_ARRAY, "file");
        if (node)
        {
            for (node = node->pstChild; node; node = node->pstNext)
            {
                value = node->unData.pcStrVal;
                grub_printf("file: %s\n", value);
                if (value[0] == '/')
                {
                    exist = ventoy_check_file_exist("%s%s", isodisk, value);
                }
                else
                {
                    exist = ventoy_check_file_exist("%s/ventoy/%s", isodisk, value);
                }

                if (exist == 0)
                {
                    grub_printf("Theme file %s does NOT exist\n", value);
                    return 1;
                }
            }

            value = vtoy_json_get_string_ex(json->pstChild, "random");
            if (value)
            {
                grub_printf("random: %s\n", value);
            }
        }
    }
    
    value = vtoy_json_get_string_ex(json->pstChild, "gfxmode");
    if (value)
    {
        grub_printf("gfxmode: %s\n", value);
    }
    
    value = vtoy_json_get_string_ex(json->pstChild, "display_mode");
    if (value)
    {
        grub_printf("display_mode: %s\n", value);
    }

    value = vtoy_json_get_string_ex(json->pstChild, "serial_param");
    if (value)
    {
        grub_printf("serial_param %s\n", value);
    }

    value = vtoy_json_get_string_ex(json->pstChild, "ventoy_left");
    if (value)
    {
        grub_printf("ventoy_left: %s\n", value);
    }
    
    value = vtoy_json_get_string_ex(json->pstChild, "ventoy_top");
    if (value)
    {
        grub_printf("ventoy_top: %s\n", value);
    }
    
    value = vtoy_json_get_string_ex(json->pstChild, "ventoy_color");
    if (value)
    {
        grub_printf("ventoy_color: %s\n", value);
    }

    node = vtoy_json_find_item(json->pstChild, JSON_TYPE_ARRAY, "fonts");
    if (node)
    {
        for (node = node->pstChild; node; node = node->pstNext)
        {
            if (node->enDataType == JSON_TYPE_STRING)
            {
                if (ventoy_check_file_exist("%s%s", isodisk, node->unData.pcStrVal))
                {
                    grub_printf("%s [OK]\n", node->unData.pcStrVal);
                }
                else
                {
                    grub_printf("%s [NOT EXIST]\n", node->unData.pcStrVal);
                }
            }
        }
    }
    else
    {
        grub_printf("fonts NOT found\n");
    }

    return 0;
}

int ventoy_plugin_theme_entry(VTOY_JSON *json, const char *isodisk)
{
    const char *value;
    char val[64];
    char filepath[256];
    VTOY_JSON *node = NULL;
    theme_list *tail = NULL;
    theme_list *themenode = NULL;

    value = vtoy_json_get_string_ex(json->pstChild, "file");
    if (value)
    {
        if (value[0] == '/')
        {
            grub_snprintf(filepath, sizeof(filepath), "%s%s", isodisk, value);
        }
        else
        {
            grub_snprintf(filepath, sizeof(filepath), "%s/ventoy/%s", isodisk, value);
        }
        
        if (ventoy_check_file_exist(filepath) == 0)
        {
            debug("Theme file %s does not exist\n", filepath);
            return 0;
        }

        debug("vtoy_theme %s\n", filepath);
        ventoy_env_export("vtoy_theme", filepath);
        grub_snprintf(g_theme_single_file, sizeof(g_theme_single_file), "%s", filepath);
    }
    else
    {
        node = vtoy_json_find_item(json->pstChild, JSON_TYPE_ARRAY, "file");
        if (node)
        {
            for (node = node->pstChild; node; node = node->pstNext)
            {
                value = node->unData.pcStrVal;
                if (value[0] == '/')
                {
                    grub_snprintf(filepath, sizeof(filepath), "%s%s", isodisk, value);
                }
                else
                {
                    grub_snprintf(filepath, sizeof(filepath), "%s/ventoy/%s", isodisk, value);
                }

                if (ventoy_check_file_exist(filepath) == 0)
                {
                    continue;
                }

                themenode = grub_zalloc(sizeof(theme_list));
                if (themenode)
                {
                    grub_snprintf(themenode->theme.path, sizeof(themenode->theme.path), "%s", filepath);
                    if (g_theme_head)
                    {
                        tail->next = themenode;
                    }
                    else
                    {
                        g_theme_head = themenode;
                    }
                    tail = themenode;
                    g_theme_num++;
                }
            }

            ventoy_env_export("vtoy_theme", "random");
            value = vtoy_json_get_string_ex(json->pstChild, "random");
            if (value)
            {
                if (grub_strcmp(value, "boot_second") == 0)
                {
                    g_theme_random = vtoy_theme_random_boot_second;
                }
                else if (grub_strcmp(value, "boot_day") == 0)
                {
                    g_theme_random = vtoy_theme_random_boot_day;
                }
                else if (grub_strcmp(value, "boot_month") == 0)
                {
                    g_theme_random = vtoy_theme_random_boot_month;
                }
            }
        }
    }

    grub_snprintf(val, sizeof(val), "%d", g_theme_num);
    grub_env_set("VTOY_THEME_COUNT", val);
    grub_env_export("VTOY_THEME_COUNT");
    if (g_theme_num > 0)
    {
        vtoy_json_get_int(json->pstChild, "default_file", &g_theme_id);
        if (g_theme_id == 0)
        {
            vtoy_json_get_int(json->pstChild, "resolution_fit", &g_theme_res_fit);
            if (g_theme_res_fit != 1)
            {
                g_theme_res_fit = 0;
            }

            grub_snprintf(val, sizeof(val), "%d", g_theme_res_fit);
            ventoy_env_export("vtoy_res_fit", val);
        }
        
        if (g_theme_id > g_theme_num || g_theme_id < 0)
        {
            g_theme_id = 0;
        }
    }
    
    value = vtoy_json_get_string_ex(json->pstChild, "gfxmode");
    if (value)
    {
        debug("vtoy_gfxmode %s\n", value);
        ventoy_env_export("vtoy_gfxmode", value);
    }
    
    value = vtoy_json_get_string_ex(json->pstChild, "display_mode");
    if (value)
    {
        debug("display_mode %s\n", value);
        ventoy_env_export("vtoy_display_mode", value);
    }
    
    value = vtoy_json_get_string_ex(json->pstChild, "serial_param");
    if (value)
    {
        debug("serial_param %s\n", value);
        ventoy_env_export("vtoy_serial_param", value);
    }

    value = vtoy_json_get_string_ex(json->pstChild, "ventoy_left");
    if (value)
    {
        ventoy_env_export(ventoy_left_key, value);
    }
    
    value = vtoy_json_get_string_ex(json->pstChild, "ventoy_top");
    if (value)
    {
        ventoy_env_export(ventoy_top_key, value);
    }
    
    value = vtoy_json_get_string_ex(json->pstChild, "ventoy_color");
    if (value)
    {
        ventoy_env_export(ventoy_color_key, value);
    }

    node = vtoy_json_find_item(json->pstChild, JSON_TYPE_ARRAY, "fonts");
    if (node)
    {
        for (node = node->pstChild; node; node = node->pstNext)
        {
            if (node->enDataType == JSON_TYPE_STRING && 
                ventoy_check_file_exist("%s%s", isodisk, node->unData.pcStrVal))
            {
                grub_snprintf(filepath, sizeof(filepath), "%s%s", isodisk, node->unData.pcStrVal);
                grub_font_load(filepath);
            }
        }
    }

    return 0;
}
