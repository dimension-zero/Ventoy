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
vtoy_password g_boot_pwd;
vtoy_password g_file_type_pwd[img_type_max];
menu_password *g_pwd_head = NULL;
persistence_config *g_persistence_head = NULL;


static int ventoy_plugin_parse_pwdstr(char *pwdstr, vtoy_password *pwd)
{
    int i;
    int len;
    char ch;
    char *pos;
    char bytes[3];
    vtoy_password tmpPwd;
    
    len = (int)grub_strlen(pwdstr);
    if (len > 64)
    {
        if (NULL == pwd) grub_printf("Password too long %d\n", len);
        return 1;
    }

    grub_memset(&tmpPwd, 0, sizeof(tmpPwd));

    if (grub_strncmp(pwdstr, "txt#", 4) == 0)
    {
        tmpPwd.type = VTOY_PASSWORD_TXT;
        grub_snprintf(tmpPwd.text, sizeof(tmpPwd.text), "%s", pwdstr + 4);
    }
    else if (grub_strncmp(pwdstr, "md5#", 4) == 0)
    {
        if ((len - 4) == 32)
        {
            for (i = 0; i < 16; i++)
            {
                bytes[0] = pwdstr[4 + i * 2];
                bytes[1] = pwdstr[4 + i * 2 + 1];
                bytes[2] = 0;
                
                if (grub_isxdigit(bytes[0]) && grub_isxdigit(bytes[1]))
                {
                    tmpPwd.md5[i] = (grub_uint8_t)grub_strtoul(bytes, NULL, 16);
                }
                else
                {
                    if (NULL == pwd) grub_printf("Invalid md5 hex format %s %d\n", pwdstr, i);
                    return 1;
                }
            }
            tmpPwd.type = VTOY_PASSWORD_MD5;
        }
        else if ((len - 4) > 32)
        {
            pos = grub_strchr(pwdstr + 4, '#');
            if (!pos)
            {
                if (NULL == pwd) grub_printf("Invalid md5 password format %s\n", pwdstr);
                return 1;
            }

            if (len - 1 - ((long)pos - (long)pwdstr) != 32)
            {
                if (NULL == pwd) grub_printf("Invalid md5 salt password format %s\n", pwdstr);
                return 1;
            }
        
            ch = *pos;
            *pos = 0;
            grub_snprintf(tmpPwd.salt, sizeof(tmpPwd.salt), "%s", pwdstr + 4);
            *pos = ch;

            pos++;
            for (i = 0; i < 16; i++)
            {
                bytes[0] = pos[i * 2];
                bytes[1] = pos[i * 2 + 1];
                bytes[2] = 0;
                
                if (grub_isxdigit(bytes[0]) && grub_isxdigit(bytes[1]))
                {
                    tmpPwd.md5[i] = (grub_uint8_t)grub_strtoul(bytes, NULL, 16);
                }
                else
                {
                    if (NULL == pwd) grub_printf("Invalid md5 hex format %s %d\n", pwdstr, i);
                    return 1;
                }
            }

            tmpPwd.type = VTOY_PASSWORD_SALT_MD5;
        }
        else
        {
            if (NULL == pwd) grub_printf("Invalid md5 password format %s\n", pwdstr);
            return 1;
        }
    }
    else
    {
        if (NULL == pwd) grub_printf("Invalid password format %s\n", pwdstr);
        return 1;
    }

    if (pwd)
    {
        grub_memcpy(pwd, &tmpPwd, sizeof(tmpPwd));
    }

    return 0;
}

static int ventoy_plugin_get_pwd_type(const char *pwd)
{
    int i;
    char pwdtype[64];

    for (i = 0; pwd && i < (int)ARRAY_SIZE(g_menu_prefix); i++)
    {
        grub_snprintf(pwdtype, sizeof(pwdtype), "%spwd", g_menu_prefix[i]);
        if (grub_strcmp(pwdtype, pwd) == 0)
        {
            return img_type_start + i; 
        }
    }
    
    return -1;
}

int ventoy_plugin_pwd_entry(VTOY_JSON *json, const char *isodisk)
{
    int type = -1;
    const char *iso = NULL;
    const char *pwd = NULL;
    VTOY_JSON *pNode = NULL;
    VTOY_JSON *pCNode = NULL;
    menu_password *node = NULL;
    menu_password *tail = NULL;
    menu_password *next = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_OBJECT)
    {
        debug("Not object %d\n", json->enDataType);
        return 0;
    }

    if (g_pwd_head)
    {
        for (node = g_pwd_head; node; node = next)
        {
            next = node->next;
            grub_free(node);
        }

        g_pwd_head = NULL;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        if (pNode->pcName && grub_strcmp("bootpwd", pNode->pcName) == 0)
        {
            ventoy_plugin_parse_pwdstr(pNode->unData.pcStrVal, &g_boot_pwd);
        }
        else if ((type = ventoy_plugin_get_pwd_type(pNode->pcName)) >= 0)
        {
            ventoy_plugin_parse_pwdstr(pNode->unData.pcStrVal, g_file_type_pwd + type);
        }
        else if (pNode->pcName && grub_strcmp("menupwd", pNode->pcName) == 0)
        {
            for (pCNode = pNode->pstChild; pCNode; pCNode = pCNode->pstNext)
            {
                if (pCNode->enDataType != JSON_TYPE_OBJECT)
                {
                    continue;
                }

                type = vtoy_menu_pwd_file;
                iso = vtoy_json_get_string_ex(pCNode->pstChild, "file");
                if (!iso)
                {
                    type = vtoy_menu_pwd_parent;
                    iso = vtoy_json_get_string_ex(pCNode->pstChild, "parent");                    
                }                
                
                pwd = vtoy_json_get_string_ex(pCNode->pstChild, "pwd");
                if (iso && pwd && iso[0] == '/')
                {
                    node = grub_zalloc(sizeof(menu_password));
                    if (node)
                    {
                        node->type = type;
                        node->pathlen = grub_snprintf(node->isopath, sizeof(node->isopath), "%s", iso);

                        if (ventoy_plugin_parse_pwdstr((char *)pwd, &(node->password)))
                        {
                            grub_free(node);
                            continue;
                        }

                        if (g_pwd_head)
                        {
                            tail->next = node;
                        }
                        else
                        {
                            g_pwd_head = node;
                        }
                        tail = node;
                    }
                }
            }
        }
    }

    return 0;
}

int ventoy_plugin_pwd_check(VTOY_JSON *json, const char *isodisk)
{
    int type = -1;
    char *pos = NULL;
    const char *iso = NULL;
    const char *pwd = NULL;
    VTOY_JSON *pNode = NULL;
    VTOY_JSON *pCNode = NULL;

    if (json->enDataType != JSON_TYPE_OBJECT)
    {
        grub_printf("Not object %d\n", json->enDataType);
        return 0;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        if (pNode->pcName && grub_strcmp("bootpwd", pNode->pcName) == 0)
        {
            if (0 == ventoy_plugin_parse_pwdstr(pNode->unData.pcStrVal, NULL))
            {
                grub_printf("bootpwd:<%s>\n", pNode->unData.pcStrVal);
            }
            else
            {
                grub_printf("Invalid bootpwd.\n");
            }
        }
        else if ((type = ventoy_plugin_get_pwd_type(pNode->pcName)) >= 0)
        {
            if (0 == ventoy_plugin_parse_pwdstr(pNode->unData.pcStrVal, NULL))
            {
                grub_printf("%s:<%s>\n", pNode->pcName, pNode->unData.pcStrVal);
            }
            else
            {
                grub_printf("Invalid pwd <%s>\n", pNode->unData.pcStrVal);
            }
        }
        else if (pNode->pcName && grub_strcmp("menupwd", pNode->pcName) == 0)
        {
            grub_printf("\n");
            for (pCNode = pNode->pstChild; pCNode; pCNode = pCNode->pstNext)
            {
                if (pCNode->enDataType != JSON_TYPE_OBJECT)
                {
                    grub_printf("Not object %d\n", pCNode->enDataType);
                    continue;
                }

                if ((iso = vtoy_json_get_string_ex(pCNode->pstChild, "file")) != NULL)
                {
                    pos = grub_strchr(iso, '*');
                    if (pos || 0 == ventoy_plugin_check_path(isodisk, iso))
                    {
                        pwd = vtoy_json_get_string_ex(pCNode->pstChild, "pwd");

                        if (0 == ventoy_plugin_parse_pwdstr((char *)pwd, NULL))
                        {
                            grub_printf("file:<%s> [%s]\n", iso, (pos ? "*" : "OK"));
                            grub_printf("pwd:<%s>\n\n", pwd);
                        }
                        else
                        {
                            grub_printf("Invalid password for <%s>\n", iso);
                        }
                    }
                    else
                    {
                        grub_printf("<%s%s> not found\n", isodisk, iso);
                    }
                }
                else if ((iso = vtoy_json_get_string_ex(pCNode->pstChild, "parent")) != NULL)
                {
                    if (ventoy_is_dir_exist("%s%s", isodisk, iso))
                    {
                        pwd = vtoy_json_get_string_ex(pCNode->pstChild, "pwd");
                        if (0 == ventoy_plugin_parse_pwdstr((char *)pwd, NULL))
                        {
                            grub_printf("dir:<%s> [%s]\n", iso, (pos ? "*" : "OK"));
                            grub_printf("pwd:<%s>\n\n", pwd);
                        }
                        else
                        {
                            grub_printf("Invalid password for <%s>\n", iso);
                        }
                    }
                    else
                    {
                        grub_printf("<%s%s> not found\n", isodisk, iso);
                    }
                }
                else
                {
                    grub_printf("No file item found in json.\n");
                }
            }
        }
    }

    return 0;
}

int ventoy_plugin_persistence_check(VTOY_JSON *json, const char *isodisk)
{
    int autosel = 0;
    int timeout = 0;
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
                ventoy_plugin_check_fullpath(pNode->pstChild, isodisk, "backend", &pathnum);

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
        else
        {
            grub_printf("image not found\n");
        }
    }

    return 0;
}

int ventoy_plugin_persistence_entry(VTOY_JSON *json, const char *isodisk)
{
    int autosel = 0;
    int timeout = 0;
    int pathnum = 0;
    const char *iso = NULL;
    VTOY_JSON *pNode = NULL;
    persistence_config *node = NULL;
    persistence_config *next = NULL;
    file_fullpath *backendpath = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        debug("Not array %d\n", json->enDataType);
        return 0;
    }

    if (g_persistence_head)
    {
        for (node = g_persistence_head; node; node = next)
        {
            next = node->next;
            grub_check_free(node->backendpath);
            grub_free(node);
        }

        g_persistence_head = NULL;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        iso = vtoy_json_get_string_ex(pNode->pstChild, "image");
        if (iso && iso[0] == '/')
        {
            if (0 == ventoy_plugin_parse_fullpath(pNode->pstChild, isodisk, "backend", &backendpath, &pathnum))
            {
                node = grub_zalloc(sizeof(persistence_config));
                if (node)
                {
                    node->pathlen = grub_snprintf(node->isopath, sizeof(node->isopath), "%s", iso);
                    node->backendpath = backendpath;
                    node->backendnum = pathnum;

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

                    if (g_persistence_head)
                    {
                        node->next = g_persistence_head;
                    }
                    
                    g_persistence_head = node;
                }
            }
        }
    }

    return 0;
}
