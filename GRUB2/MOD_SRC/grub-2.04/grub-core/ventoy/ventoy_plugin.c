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
}

static plugin_entry g_plugin_entries[] = 
{
    { "control", ventoy_plugin_control_entry, ventoy_plugin_control_check, 0 },
    { "theme", ventoy_plugin_theme_entry, ventoy_plugin_theme_check, 0 },
    { "auto_install", ventoy_plugin_auto_install_entry, ventoy_plugin_auto_install_check, 0 },
    { "persistence", ventoy_plugin_persistence_entry, ventoy_plugin_persistence_check, 0 },
    { "menu_alias", ventoy_plugin_menualias_entry, ventoy_plugin_menualias_check, 0 },
    { "menu_tip", ventoy_plugin_menutip_entry, ventoy_plugin_menutip_check, 0 },
    { "menu_class", ventoy_plugin_menuclass_entry, ventoy_plugin_menuclass_check, 0 },
    { "injection", ventoy_plugin_injection_entry, ventoy_plugin_injection_check, 0 },
    { "auto_memdisk", ventoy_plugin_auto_memdisk_entry, ventoy_plugin_auto_memdisk_check, 0 },
    { "image_list", ventoy_plugin_image_list_entry, ventoy_plugin_image_list_check, 0 },
    { "image_blacklist", ventoy_plugin_image_list_entry, ventoy_plugin_image_list_check, 0 },
    { "conf_replace", ventoy_plugin_conf_replace_entry, ventoy_plugin_conf_replace_check, 0 },
    { "dud", ventoy_plugin_dud_entry, ventoy_plugin_dud_check, 0 },
    { "password", ventoy_plugin_pwd_entry, ventoy_plugin_pwd_check, 0 },
    { "custom_boot", ventoy_plugin_custom_boot_entry, ventoy_plugin_custom_boot_check, 0 },
};

static int ventoy_parse_plugin_config(VTOY_JSON *json, const char *isodisk)
{
    int i;
    char key[128];
    VTOY_JSON *cur = NULL;

    grub_snprintf(g_iso_disk_name, sizeof(g_iso_disk_name), "%s", isodisk);

    for (cur = json; cur; cur = cur->pstNext)
    {
        for (i = 0; i < (int)ARRAY_SIZE(g_plugin_entries); i++)
        {
            grub_snprintf(key, sizeof(key), "%s_%s", g_plugin_entries[i].key, g_arch_mode_suffix);
            if (g_plugin_entries[i].flag == 0 && grub_strcmp(key, cur->pcName) == 0)
            {
                debug("Plugin entry for %s\n", g_plugin_entries[i].key);
                g_plugin_entries[i].entryfunc(cur, isodisk);
                g_plugin_entries[i].flag = 1;
                break;
            }
        }
    }

    
    for (cur = json; cur; cur = cur->pstNext)
    {
        for (i = 0; i < (int)ARRAY_SIZE(g_plugin_entries); i++)
        {
            if (g_plugin_entries[i].flag == 0 && grub_strcmp(g_plugin_entries[i].key, cur->pcName) == 0)
            {
                debug("Plugin entry for %s\n", g_plugin_entries[i].key);
                g_plugin_entries[i].entryfunc(cur, isodisk);
                g_plugin_entries[i].flag = 1;
                break;
            }
        }
    }

    return 0;
}

grub_err_t ventoy_cmd_load_plugin(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int ret = 0;
    int offset = 0;
    char *buf = NULL;
    grub_uint8_t *code = NULL;
    grub_file_t file;
    VTOY_JSON *json = NULL;
    
    (void)ctxt;
    (void)argc;

    grub_env_set("VTOY_TIP_LEFT", "10%");
    grub_env_set("VTOY_TIP_TOP", "80%+5");
    grub_env_set("VTOY_TIP_COLOR", "blue");
    grub_env_set("VTOY_TIP_ALIGN", "left");

    file = ventoy_grub_file_open(GRUB_FILE_TYPE_LINUX_INITRD, "%s/ventoy/ventoy.json", args[0]);
    if (!file)
    {
        return GRUB_ERR_NONE;
    }

    debug("json configuration file size %d\n", (int)file->size);

    buf = grub_malloc(file->size + 1);
    if (!buf)
    {
        grub_file_close(file);
        return 1;
    }
    
    buf[file->size] = 0;
    grub_file_read(file, buf, file->size);
    grub_file_close(file);

    json = vtoy_json_create();
    if (!json)
    {
        return 1;
    }

    code = (grub_uint8_t *)buf;
    if (code[0] == 0xef && code[1] == 0xbb && code[2] == 0xbf)
    {
        offset = 3; /* Skip UTF-8 BOM */
    }
    else if ((code[0] == 0xff && code[1] == 0xfe) || (code[0] == 0xfe && code[1] == 0xff))
    {
        grub_env_set("VTOY_PLUGIN_SYNTAX_ERROR", "1");
        grub_env_export("VTOY_PLUGIN_SYNTAX_ERROR");

        grub_env_set("VTOY_PLUGIN_ENCODE_ERROR", "1");
        grub_env_export("VTOY_PLUGIN_ENCODE_ERROR");

        debug("Failed to parse json string %d\n", ret);
        grub_free(buf);
        return 1;
    }
    
    ret = vtoy_json_parse(json, buf + offset);
    if (ret)
    {
        grub_env_set("VTOY_PLUGIN_SYNTAX_ERROR", "1");
        grub_env_export("VTOY_PLUGIN_SYNTAX_ERROR");
        
        debug("Failed to parse json string %d\n", ret);
        grub_free(buf);
        return 1;
    }

    ventoy_parse_plugin_config(json->pstChild, args[0]);

    vtoy_json_destroy(json);

    grub_free(buf);

    if (g_boot_pwd.type)
    {        
        grub_printf("\n\n======= %s ======\n\n", grub_env_get("VTOY_TEXT_MENU_VER"));
        if (ventoy_check_password(&g_boot_pwd, 3))
        {
            grub_printf("\n!!! Password check failed, will exit after 5 seconds. !!!\n");
            grub_refresh();
            grub_sleep(5);
            grub_exit();
        }
    }

    if (g_menu_tip_head)
    {
        grub_env_set("VTOY_MENU_TIP_ENABLE", "1");
    }
    else
    {
        grub_env_unset("VTOY_MENU_TIP_ENABLE");
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

