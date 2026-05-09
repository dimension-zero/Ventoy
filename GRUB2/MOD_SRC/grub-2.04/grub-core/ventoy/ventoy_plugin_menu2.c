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
menu_class *g_menu_class_head = NULL;
custom_boot *g_custom_boot_head = NULL;
auto_memdisk *g_auto_memdisk_head = NULL;
image_list *g_image_list_head = NULL;
conf_replace *g_conf_replace_head = NULL;


int ventoy_plugin_menuclass_entry(VTOY_JSON *json, const char *isodisk)
{
    int type;
    int parent = 0;
    const char *key = NULL;
    const char *class = NULL;
    VTOY_JSON *pNode = NULL;
    menu_class *tail = NULL;
    menu_class *node = NULL;
    menu_class *next = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        debug("Not array %d\n", json->enDataType);
        return 0;
    }

    if (g_menu_class_head)
    {
        for (node = g_menu_class_head; node; node = next)
        {
            next = node->next;
            grub_free(node);
        }

        g_menu_class_head = NULL;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        parent = 0;
        type = vtoy_class_image_file;
        key = vtoy_json_get_string_ex(pNode->pstChild, "key");
        if (!key)
        {
            key = vtoy_json_get_string_ex(pNode->pstChild, "parent");
            if (key)
            {
                parent = 1;
            }
            else
            {
                key = vtoy_json_get_string_ex(pNode->pstChild, "dir");
                type = vtoy_class_directory;
            }
        }
        
        class = vtoy_json_get_string_ex(pNode->pstChild, "class");
        if (key && class)
        {
            node = grub_zalloc(sizeof(menu_class));
            if (node)
            {
                node->type = type;
                node->parent = parent;
                node->patlen = grub_snprintf(node->pattern, sizeof(node->pattern), "%s", key);
                grub_snprintf(node->class, sizeof(node->class), "%s", class);

                if (g_menu_class_head)
                {
                    tail->next = node;
                }
                else
                {
                    g_menu_class_head = node;
                }
                tail = node;
            }
        }
    }

    return 0;
}

int ventoy_plugin_menuclass_check(VTOY_JSON *json, const char *isodisk)
{
    const char *name = NULL;
    const char *key = NULL;
    const char *class = NULL;
    VTOY_JSON *pNode = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        grub_printf("Not array %d\n", json->enDataType);
        return 1;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        name = "key";
        key = vtoy_json_get_string_ex(pNode->pstChild, "key");
        if (!key)
        {
            name = "parent";
            key = vtoy_json_get_string_ex(pNode->pstChild, "parent");
            if (!key)
            {
                name = "dir";       
                key = vtoy_json_get_string_ex(pNode->pstChild, "dir"); 
            }
        }
        
        class = vtoy_json_get_string_ex(pNode->pstChild, "class");
        if (key && class)
        {
            grub_printf("%s: <%s>\n", name,  key);
            grub_printf("class: <%s>\n\n", class);
        }
    }

    return 0;
}

int ventoy_plugin_custom_boot_entry(VTOY_JSON *json, const char *isodisk)
{
    int type;
    int len;
    const char *key = NULL;
    const char *cfg = NULL;
    VTOY_JSON *pNode = NULL;
    custom_boot *tail = NULL;
    custom_boot *node = NULL;
    custom_boot *next = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        debug("Not array %d\n", json->enDataType);
        return 0;
    }

    if (g_custom_boot_head)
    {
        for (node = g_custom_boot_head; node; node = next)
        {
            next = node->next;
            grub_free(node);
        }

        g_custom_boot_head = NULL;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        type = vtoy_custom_boot_image_file;
        key = vtoy_json_get_string_ex(pNode->pstChild, "file");
        if (!key)
        {
            key = vtoy_json_get_string_ex(pNode->pstChild, "dir");
            type = vtoy_custom_boot_directory;
        }
        
        cfg = vtoy_json_get_string_ex(pNode->pstChild, "vcfg");
        if (key && cfg)
        {
            node = grub_zalloc(sizeof(custom_boot));
            if (node)
            {
                node->type = type;
                node->pathlen = grub_snprintf(node->path, sizeof(node->path), "%s", key);
                len = (int)grub_snprintf(node->cfg, sizeof(node->cfg), "%s", cfg);

                if (len >= 5 && grub_strncmp(node->cfg + len - 5, ".vcfg", 5) == 0)
                {
                    if (g_custom_boot_head)
                    {
                        tail->next = node;
                    }
                    else
                    {
                        g_custom_boot_head = node;
                    }
                    tail = node;
                }
                else
                {
                    grub_free(node);
                }
            }
        }
    }

    return 0;
}

int ventoy_plugin_custom_boot_check(VTOY_JSON *json, const char *isodisk)
{
    int type;
    int len;
    const char *key = NULL;
    const char *cfg = NULL;
    VTOY_JSON *pNode = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        grub_printf("Not array %d\n", json->enDataType);
        return 1;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        type = vtoy_custom_boot_image_file;
        key = vtoy_json_get_string_ex(pNode->pstChild, "file");
        if (!key)
        {
            key = vtoy_json_get_string_ex(pNode->pstChild, "dir"); 
            type = vtoy_custom_boot_directory;
        }
        
        cfg = vtoy_json_get_string_ex(pNode->pstChild, "vcfg");
        len = (int)grub_strlen(cfg);
        if (key && cfg)
        {
            if (len < 5 || grub_strncmp(cfg + len - 5, ".vcfg", 5))
            {
                grub_printf("<%s> does not have \".vcfg\" suffix\n\n", cfg);
            }
            else
            {
                grub_printf("%s: <%s>\n", (type == vtoy_custom_boot_directory) ? "dir" : "file",  key);
                grub_printf("vcfg: <%s>\n\n", cfg);                
            }
        }
    }

    return 0;
}

int ventoy_plugin_conf_replace_entry(VTOY_JSON *json, const char *isodisk)
{
    int img = 0;
    const char *isof = NULL;
    const char *orgf = NULL;
    const char *newf = NULL;
    VTOY_JSON *pNode = NULL;
    conf_replace *tail = NULL;
    conf_replace *node = NULL;
    conf_replace *next = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        debug("Not array %d\n", json->enDataType);
        return 0;
    }

    if (g_conf_replace_head)
    {
        for (node = g_conf_replace_head; node; node = next)
        {
            next = node->next;
            grub_free(node);
        }

        g_conf_replace_head = NULL;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        isof = vtoy_json_get_string_ex(pNode->pstChild, "iso");
        orgf = vtoy_json_get_string_ex(pNode->pstChild, "org");
        newf = vtoy_json_get_string_ex(pNode->pstChild, "new");
        if (isof && orgf && newf && isof[0] == '/' && orgf[0] == '/' && newf[0] == '/')
        {
            node = grub_zalloc(sizeof(conf_replace));
            if (node)
            {
                if (JSON_SUCCESS == vtoy_json_get_int(pNode->pstChild, "img", &img))
                {
                    node->img = img;
                }

                node->pathlen = grub_snprintf(node->isopath, sizeof(node->isopath), "%s", isof);
                grub_snprintf(node->orgconf, sizeof(node->orgconf), "%s", orgf);
                grub_snprintf(node->newconf, sizeof(node->newconf), "%s", newf);

                if (g_conf_replace_head)
                {
                    tail->next = node;
                }
                else
                {
                    g_conf_replace_head = node;
                }
                tail = node;
            }
        }
    }

    return 0;
}

int ventoy_plugin_conf_replace_check(VTOY_JSON *json, const char *isodisk)
{
    int img = 0;
    const char *isof = NULL;
    const char *orgf = NULL;
    const char *newf = NULL;
    VTOY_JSON *pNode = NULL;
    grub_file_t file = NULL;
    char cmd[256];

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        grub_printf("Not array %d\n", json->enDataType);
        return 1;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        isof = vtoy_json_get_string_ex(pNode->pstChild, "iso");
        orgf = vtoy_json_get_string_ex(pNode->pstChild, "org");
        newf = vtoy_json_get_string_ex(pNode->pstChild, "new");
        if (isof && orgf && newf && isof[0] == '/' && orgf[0] == '/' && newf[0] == '/')
        {
            if (ventoy_check_file_exist("%s%s", isodisk, isof))
            {
                grub_printf("iso:<%s> [OK]\n", isof);
                
                grub_snprintf(cmd, sizeof(cmd), "loopback vtisocheck \"%s%s\"", isodisk, isof);
                grub_script_execute_sourcecode(cmd);

                file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "(vtisocheck)/%s", orgf);
                if (file)
                {
                    if (grub_strcmp(file->fs->name, "iso9660") == 0)
                    {
                        grub_printf("org:<%s> [OK]\n", orgf);
                    }
                    else
                    {
                        grub_printf("org:<%s> [Exist But NOT ISO9660]\n", orgf);
                    }
                    grub_file_close(file);
                }
                else
                {
                    grub_printf("org:<%s> [NOT Exist]\n", orgf);
                }
                
                grub_script_execute_sourcecode("loopback -d vtisocheck");
            }
            else if (grub_strchr(isof, '*'))
            {
                grub_printf("iso:<%s> [*]\n", isof);
                grub_printf("org:<%s>\n", orgf);
            }
            else
            {
                grub_printf("iso:<%s> [NOT Exist]\n", isof);
                grub_printf("org:<%s>\n", orgf);
            }

            file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s", isodisk, newf);
            if (file)
            {
                if (file->size > vtoy_max_replace_file_size)
                {
                    grub_printf("new:<%s> [Too Big %lu] \n", newf, (ulong)file->size);
                }
                else
                {
                    grub_printf("new1:<%s> [OK]\n", newf);                    
                }
                grub_file_close(file);
            }
            else
            {
                grub_printf("new:<%s> [NOT Exist]\n", newf);   
            }

            if (JSON_SUCCESS == vtoy_json_get_int(pNode->pstChild, "img", &img))
            {
                grub_printf("img:<%d>\n", img);           
            }
            
            grub_printf("\n");
        }
    }

    return 0;
}

int ventoy_plugin_auto_memdisk_entry(VTOY_JSON *json, const char *isodisk)
{
    VTOY_JSON *pNode = NULL;
    auto_memdisk *node = NULL;
    auto_memdisk *next = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        debug("Not array %d\n", json->enDataType);
        return 0;
    }

    if (g_auto_memdisk_head)
    {
        for (node = g_auto_memdisk_head; node; node = next)
        {
            next = node->next;
            grub_free(node);
        }

        g_auto_memdisk_head = NULL;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        if (pNode->enDataType == JSON_TYPE_STRING)
        {
            node = grub_zalloc(sizeof(auto_memdisk));
            if (node)
            {
                node->pathlen = grub_snprintf(node->isopath, sizeof(node->isopath), "%s", pNode->unData.pcStrVal);

                if (g_auto_memdisk_head)
                {
                    node->next = g_auto_memdisk_head;
                }
                
                g_auto_memdisk_head = node;
            }
        }
    }

    return 0;
}

int ventoy_plugin_auto_memdisk_check(VTOY_JSON *json, const char *isodisk)
{
    VTOY_JSON *pNode = NULL;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        grub_printf("Not array %d\n", json->enDataType);
        return 1;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        if (pNode->enDataType == JSON_TYPE_STRING)
        {
            grub_printf("<%s> ", pNode->unData.pcStrVal);

            if (grub_strchr(pNode->unData.pcStrVal, '*'))
            {
                grub_printf(" [*]\n");
            }
            else if (ventoy_check_file_exist("%s%s", isodisk, pNode->unData.pcStrVal))
            {
                grub_printf(" [OK]\n");
            }
            else
            {
                grub_printf(" [NOT EXIST]\n");
            }
        }
    }

    return 0;
}

int ventoy_plugin_image_list_entry(VTOY_JSON *json, const char *isodisk)
{
    VTOY_JSON *pNode = NULL;
    image_list *node = NULL;
    image_list *next = NULL;
    image_list *tail = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        debug("Not array %d\n", json->enDataType);
        return 0;
    }

    if (g_image_list_head)
    {
        for (node = g_image_list_head; node; node = next)
        {
            next = node->next;
            grub_free(node);
        }

        g_image_list_head = NULL;
    }

    if (grub_strncmp(json->pcName, "image_blacklist", 15) == 0)
    {
        g_plugin_image_list = VENTOY_IMG_BLACK_LIST;
    }
    else
    {
        g_plugin_image_list = VENTOY_IMG_WHITE_LIST;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        if (pNode->enDataType == JSON_TYPE_STRING)
        {
            node = grub_zalloc(sizeof(image_list));
            if (node)
            {
                node->pathlen = grub_snprintf(node->isopath, sizeof(node->isopath), "%s", pNode->unData.pcStrVal);

                if (g_image_list_head)
                {
                    tail->next = node;
                }
                else
                {
                    g_image_list_head = node;
                }
                tail = node;
            }
        }
    }

    return 0;
}

int ventoy_plugin_image_list_check(VTOY_JSON *json, const char *isodisk)
{
    VTOY_JSON *pNode = NULL;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        grub_printf("Not array %d\n", json->enDataType);
        return 1;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        if (pNode->enDataType == JSON_TYPE_STRING)
        {
            grub_printf("<%s> ", pNode->unData.pcStrVal);

            if (grub_strchr(pNode->unData.pcStrVal, '*'))
            {
                grub_printf(" [*]\n");
            }
            else if (ventoy_check_file_exist("%s%s", isodisk, pNode->unData.pcStrVal))
            {
                grub_printf(" [OK]\n");
            }
            else
            {
                grub_printf(" [NOT EXIST]\n");
            }
        }
    }

    return 0;
