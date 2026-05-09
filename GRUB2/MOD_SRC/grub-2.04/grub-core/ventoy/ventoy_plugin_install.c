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
install_template *g_install_template_head = NULL;
dud *g_dud_head = NULL;

static int ventoy_plugin_is_parent(const char *pat, int patlen, const char *isopath)
{
    if (patlen > 1)
    {
        if (isopath[patlen] == '/' && ventoy_strncmp(pat, isopath, patlen) == 0 &&
            grub_strchr(isopath + patlen + 1, '/') == NULL)
        {
            return 1;
        }
    }
    else
    {
        if (pat[0] == '/' && grub_strchr(isopath + 1, '/') == NULL)
        {
            return 1;
        }
    }

    return 0;
}

int ventoy_plugin_check_path(const char *path, const char *file)
{
    if (file[0] != '/')
    {
        grub_printf("%s is NOT begin with '/' \n", file);
        return 1;
    }

    if (grub_strchr(file, '\\'))
    {
        grub_printf("%s contains invalid '\\' \n", file);
        return 1;
    }
    
    if (grub_strstr(file, "//"))
    {
        grub_printf("%s contains invalid double slash\n", file);
        return 1;
    }

    if (grub_strstr(file, "../"))
    {
        grub_printf("%s contains invalid '../' \n", file);
        return 1;
    }

    if (!ventoy_check_file_exist("%s%s", path, file))
    {
        grub_printf("%s%s does NOT exist\n", path, file);
        return 1;
    }

    return 0;
}

int ventoy_plugin_check_fullpath
(
    VTOY_JSON *json, 
    const char *isodisk, 
    const char *key,
    int *pathnum
)
{
    int rc = 0;
    int ret = 0;
    int cnt = 0;
    VTOY_JSON *node = json;
    VTOY_JSON *child = NULL;
    
    while (node)
    {
        if (0 == grub_strcmp(key, node->pcName))
        {
            break;
        }
        node = node->pstNext;
    }

    if (!node)
    {
        return 1;
    }

    if (JSON_TYPE_STRING == node->enDataType)
    {
        cnt = 1;
        ret = ventoy_plugin_check_path(isodisk, node->unData.pcStrVal);
        grub_printf("%s: %s [%s]\n", key, node->unData.pcStrVal, ret ? "FAIL" : "OK");
    }
    else if (JSON_TYPE_ARRAY == node->enDataType)
    {
        for (child = node->pstChild; child; child = child->pstNext)
        {
            if (JSON_TYPE_STRING != child->enDataType)
            {
                grub_printf("Non string json type\n");
            }
            else
            {
                rc = ventoy_plugin_check_path(isodisk, child->unData.pcStrVal);
                grub_printf("%s: %s [%s]\n", key, child->unData.pcStrVal, rc ? "FAIL" : "OK");
                ret += rc;
                cnt++;
            }
        }
    }

    *pathnum = cnt;
    return ret;
}

int ventoy_plugin_parse_fullpath
(
    VTOY_JSON *json, 
    const char *isodisk, 
    const char *key, 
    file_fullpath **fullpath,
    int *pathnum
)
{
    int rc = 1;
    int count = 0;
    VTOY_JSON *node = json;
    VTOY_JSON *child = NULL;
    file_fullpath *path = NULL;
    
    while (node)
    {
        if (0 == grub_strcmp(key, node->pcName))
        {
            break;
        }
        node = node->pstNext;
    }

    if (!node)
    {
        return 1;
    }

    if (JSON_TYPE_STRING == node->enDataType)
    {
        debug("%s is string type data\n", node->pcName);

        if ((node->unData.pcStrVal[0] != '/') || (!ventoy_check_file_exist("%s%s", isodisk, node->unData.pcStrVal)))
        {
            debug("%s%s file not found\n", isodisk, node->unData.pcStrVal);
            return 1;
        }
        
        path = (file_fullpath *)grub_zalloc(sizeof(file_fullpath));
        if (path)
        {
            grub_snprintf(path->path, sizeof(path->path), "%s", node->unData.pcStrVal);
            *fullpath = path;
            *pathnum = 1;
            rc = 0;
        }
    }
    else if (JSON_TYPE_ARRAY == node->enDataType)
    {
        for (child = node->pstChild; child; child = child->pstNext)
        {
            if ((JSON_TYPE_STRING != child->enDataType) || (child->unData.pcStrVal[0] != '/'))
            {
                debug("Invalid data type:%d\n", child->enDataType);
                return 1;
            }
            count++;
        }
        debug("%s is array type data, count=%d\n", node->pcName, count);
        
        path = (file_fullpath *)grub_zalloc(sizeof(file_fullpath) * count);
        if (path)
        {
            *fullpath = path;
            
            for (count = 0, child = node->pstChild; child; child = child->pstNext)
            {
                if (ventoy_check_file_exist("%s%s", isodisk, child->unData.pcStrVal))
                {
                    grub_snprintf(path->path, sizeof(path->path), "%s", child->unData.pcStrVal);
                    path++;
                    count++;
                }
            }

            *pathnum = count;
            rc = 0;
        }
    }

    return rc;
}

int ventoy_plugin_auto_install_check(VTOY_JSON *json, const char *isodisk)
{
    int pathnum = 0;
    int autosel = 0;
    int timeout = 0;
    char *pos = NULL;
    const char *iso = NULL;
    VTOY_JSON *pNode = NULL;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        grub_printf("Not array type %d\n", json->enDataType);
        return 1;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        if (pNode->enDataType != JSON_TYPE_OBJECT)
        {
            grub_printf("NOT object type\n");
        }
    
        if ((iso = vtoy_json_get_string_ex(pNode->pstChild, "image")) != NULL)
        {
            pos = grub_strchr(iso, '*');
            if (pos || 0 == ventoy_plugin_check_path(isodisk, iso))
            {
                grub_printf("image: %s [%s]\n", iso, (pos ? "*" : "OK"));                
                ventoy_plugin_check_fullpath(pNode->pstChild, isodisk, "template", &pathnum);
                
                if (JSON_SUCCESS == vtoy_json_get_int(pNode->pstChild, "autosel", &autosel))
                {
                    if (autosel >= 0 && autosel <= pathnum)
                    {
                        grub_printf("autosel: %d [OK]\n", autosel);
                    }
                    else
                    {
                        grub_printf("autosel: %d [FAIL]\n", autosel);
                    }
                }
                
                if (JSON_SUCCESS == vtoy_json_get_int(pNode->pstChild, "timeout", &timeout))
                {
                    if (timeout >= 0)
                    {
                        grub_printf("timeout: %d [OK]\n", timeout);
                    }
                    else
                    {
                        grub_printf("timeout: %d [FAIL]\n", timeout);
                    }
                }
            }
            else
            {
                grub_printf("image: %s [FAIL]\n", iso);
            }
        }
        else if ((iso = vtoy_json_get_string_ex(pNode->pstChild, "parent")) != NULL)
        {
            if (ventoy_is_dir_exist("%s%s", isodisk, iso))
            {
                grub_printf("parent: %s [OK]\n", iso);
                ventoy_plugin_check_fullpath(pNode->pstChild, isodisk, "template", &pathnum);
                
                if (JSON_SUCCESS == vtoy_json_get_int(pNode->pstChild, "autosel", &autosel))
                {
                    if (autosel >= 0 && autosel <= pathnum)
                    {
                        grub_printf("autosel: %d [OK]\n", autosel);
                    }
                    else
                    {
                        grub_printf("autosel: %d [FAIL]\n", autosel);
                    }
                }
                
                if (JSON_SUCCESS == vtoy_json_get_int(pNode->pstChild, "timeout", &timeout))
                {
                    if (timeout >= 0)
                    {
                        grub_printf("timeout: %d [OK]\n", timeout);
                    }
                    else
                    {
                        grub_printf("timeout: %d [FAIL]\n", timeout);
                    }
                }
            }
            else
            {
                grub_printf("parent: %s [FAIL]\n", iso);
            }
        }
        else
        {
            grub_printf("image not found\n");
        }
    }

    return 0;
}

int ventoy_plugin_auto_install_entry(VTOY_JSON *json, const char *isodisk)
{
    int type = 0;
    int pathnum = 0;
    int autosel = 0;
    int timeout = 0;
    const char *iso = NULL;
    VTOY_JSON *pNode = NULL;
    install_template *node = NULL;
    install_template *next = NULL;
    file_fullpath *templatepath = NULL;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        debug("Not array %d\n", json->enDataType);
        return 0;
    }

    if (g_install_template_head)
    {
        for (node = g_install_template_head; node; node = next)
        {
            next = node->next;
            grub_check_free(node->templatepath);
            grub_free(node);
        }

        g_install_template_head = NULL;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        type = auto_install_type_file;
        iso = vtoy_json_get_string_ex(pNode->pstChild, "image");
        if (!iso)
        {
            type = auto_install_type_parent;
            iso = vtoy_json_get_string_ex(pNode->pstChild, "parent");
        }
        
        if (iso && iso[0] == '/')
        {
            if (0 == ventoy_plugin_parse_fullpath(pNode->pstChild, isodisk, "template", &templatepath, &pathnum))
            {
                node = grub_zalloc(sizeof(install_template));
                if (node)
                {
                    node->type = type;
                    node->pathlen = grub_snprintf(node->isopath, sizeof(node->isopath), "%s", iso);
                    node->templatepath = templatepath;
                    node->templatenum = pathnum;

                    node->autosel = -1;
                    node->timeout = -1;
                    if (JSON_SUCCESS == vtoy_json_get_int(pNode->pstChild, "autosel", &autosel))
                    {
                        if (autosel >= 0 && autosel <= pathnum)
                        {
                            node->autosel = autosel;
                        }
                    }
                    
                    if (JSON_SUCCESS == vtoy_json_get_int(pNode->pstChild, "timeout", &timeout))
                    {
                        if (timeout >= 0)
                        {
                            node->timeout = timeout;
                        }
                    }

                    if (g_install_template_head)
                    {
                        node->next = g_install_template_head;
                    }
                    
                    g_install_template_head = node;
                }
            }
        }
    }

    return 0;
}

int ventoy_plugin_dud_check(VTOY_JSON *json, const char *isodisk)
{
    int pathnum = 0;
    char *pos = NULL;
    const char *iso = NULL;
    VTOY_JSON *pNode = NULL;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        grub_printf("Not array type %d\n", json->enDataType);
        return 1;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        if (pNode->enDataType != JSON_TYPE_OBJECT)
        {
            grub_printf("NOT object type\n");
        }
    
        iso = vtoy_json_get_string_ex(pNode->pstChild, "image");
        if (iso)
        {
            pos = grub_strchr(iso, '*');
            if (pos || 0 == ventoy_plugin_check_path(isodisk, iso))
            {
                grub_printf("image: %s [%s]\n", iso, (pos ? "*" : "OK"));
                ventoy_plugin_check_fullpath(pNode->pstChild, isodisk, "dud", &pathnum);
            }
            else
            {
                grub_printf("image: %s [FAIL]\n", iso);
            }
        }
        else
        {
            grub_printf("image not found\n");
        }
    }

    return 0;
}

int ventoy_plugin_dud_entry(VTOY_JSON *json, const char *isodisk)
{
    int pathnum = 0;
    const char *iso = NULL;
    VTOY_JSON *pNode = NULL;
    dud *node = NULL;
    dud *next = NULL;
    file_fullpath *dudpath = NULL;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        debug("Not array %d\n", json->enDataType);
        return 0;
    }

    if (g_dud_head)
    {
        for (node = g_dud_head; node; node = next)
        {
            next = node->next;
            grub_check_free(node->dudpath);
            grub_free(node);
        }

        g_dud_head = NULL;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        iso = vtoy_json_get_string_ex(pNode->pstChild, "image");
        if (iso && iso[0] == '/')
        {
            if (0 == ventoy_plugin_parse_fullpath(pNode->pstChild, isodisk, "dud", &dudpath, &pathnum))
            {
                node = grub_zalloc(sizeof(dud));
                if (node)
                {
                    node->pathlen = grub_snprintf(node->isopath, sizeof(node->isopath), "%s", iso);
                    node->dudpath = dudpath;
                    node->dudnum = pathnum;
                    node->files = grub_zalloc(sizeof(dudfile) * pathnum);

                    if (node->files)
                    {
                        if (g_dud_head)
                        {
                            node->next = g_dud_head;
                        }
                        
                        g_dud_head = node;
                    }
                    else
                    {
                        grub_free(node);
                    }
                }
            }
        }
    }

    return 0;
}
