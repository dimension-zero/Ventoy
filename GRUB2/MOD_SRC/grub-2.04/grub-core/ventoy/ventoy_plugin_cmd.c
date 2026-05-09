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
VTOY_JSON *g_menu_lang_json = NULL;
char g_cur_menu_language[32] = {0};
char g_push_menu_language[32] = {0};

static const vtoy_password * ventoy_plugin_get_password(const char *isopath)
{
    int i;
    int len;
    const char *pos = NULL;
    menu_password *node = NULL;

    if (!isopath)
    {
        return NULL;
    }

    if (g_pwd_head)
    {
        len = (int)grub_strlen(isopath);    
        for (node = g_pwd_head; node; node = node->next)
        {
            if (node->type == vtoy_menu_pwd_file)
            {
                if (node->pathlen == len && ventoy_strncmp(node->isopath, isopath, len) == 0)
                {
                    return &(node->password);
                }
            }
        }

        for (node = g_pwd_head; node; node = node->next)
        {   
            if (node->type == vtoy_menu_pwd_parent)
            {
                if (node->pathlen < len && ventoy_plugin_is_parent(node->isopath, node->pathlen, isopath))
                {
                    return &(node->password);
                }
            }
        }
    }

    while (*isopath)
    {
        if (*isopath == '.')
        {
            pos = isopath;
        }
        isopath++;
    }

    if (pos)
    {
        for (i = 0; i < (int)ARRAY_SIZE(g_menu_prefix); i++)
        {
            if (g_file_type_pwd[i].type && 0 == grub_strcasecmp(pos + 1, g_menu_prefix[i]))
            {
                return g_file_type_pwd + i;
            }
        }
    }

    return NULL;
}

grub_err_t ventoy_cmd_check_password(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int ret;
    const vtoy_password *pwd = NULL;
    
    (void)ctxt;
    (void)argc;

    pwd = ventoy_plugin_get_password(args[0]);
    if (pwd)
    {
        if (0 == ventoy_check_password(pwd, 1))
        {
            ret = 1;
        }
        else
        {
            ret = 0;
        }
    }
    else
    {
        ret = 1;
    }

    grub_errno = 0;
    return ret;
}

grub_err_t ventoy_cmd_plugin_check_json(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i = 0;
    int ret = 0;
    char *buf = NULL;
    char key[128];
    grub_file_t file;
    VTOY_JSON *node = NULL;
    VTOY_JSON *json = NULL;
    
    (void)ctxt;

    if (argc != 3)
    {
        return 0;
    }

    file = ventoy_grub_file_open(GRUB_FILE_TYPE_LINUX_INITRD, "%s/ventoy/ventoy.json", args[0]);
    if (!file)
    {
        grub_printf("Plugin json file /ventoy/ventoy.json does NOT exist.\n");
        grub_printf("Attention: directory name and filename are both case-sensitive.\n");
        goto end;
    }

    buf = grub_malloc(file->size + 1);
    if (!buf)
    {
        grub_printf("Failed to malloc memory %lu.\n", (ulong)(file->size + 1));
        goto end;
    }
    
    buf[file->size] = 0;
    grub_file_read(file, buf, file->size);

    json = vtoy_json_create();
    if (!json)
    {
        grub_printf("Failed to create json\n");
        goto end;
    }

    ret = vtoy_json_parse(json, buf);
    if (ret)
    {
        grub_printf("Syntax error detected in ventoy.json, please check it.\n");
        goto end;
    }

    grub_snprintf(key, sizeof(key), "%s_%s", args[1], g_arch_mode_suffix);
    for (node = json->pstChild; node; node = node->pstNext)
    {
        if (grub_strcmp(node->pcName, key) == 0)
        {
            break;
        }
    }

    if (!node)
    {
        for (node = json->pstChild; node; node = node->pstNext)
        {
            if (grub_strcmp(node->pcName, args[1]) == 0)
            {
                break;
            }
        }

        if (!node)
        {
            grub_printf("%s is NOT found in ventoy.json\n", args[1]);
            goto end;            
        }
    }

    for (i = 0; i < (int)ARRAY_SIZE(g_plugin_entries); i++)
    {
        if (grub_strcmp(g_plugin_entries[i].key, args[1]) == 0)
        {
            if (g_plugin_entries[i].checkfunc)
            {
                ret = g_plugin_entries[i].checkfunc(node, args[2]);                
            }
            break;
        }
    }
    
end:
    check_free(file, grub_file_close);
    check_free(json, vtoy_json_destroy);
    grub_check_free(buf);

    return 0;
}

grub_err_t ventoy_cmd_select_theme_cfg(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int pos = 0;
    int bufsize = 0;
    char *name = NULL;
    char *buf = NULL;
    theme_list *node = NULL;

    (void)argc;
    (void)args;
    (void)ctxt;

    if (g_theme_single_file[0])
    {
        return 0;
    }

    if (g_theme_num < 2)
    {
        return 0;
    }

    bufsize = (g_theme_num + 1) * 1024;
    buf = grub_malloc(bufsize);
    if (!buf)
    {
        return 0;
    }
    
    for (node = g_theme_head; node; node = node->next)
    {
        name = grub_strstr(node->theme.path, ")/");
        if (name)
        {
            name++;
        }
        else
        {
            name = node->theme.path;
        }
    
        pos += grub_snprintf(buf + pos, bufsize - pos, 
            "menuentry \"%s\" --class=debug_theme_item --class=debug_theme_select --class=F5tool {\n"
                "vt_set_theme_path \"%s\"\n"
            "}\n",
            name, node->theme.path);
    }

    pos += grub_snprintf(buf + pos, bufsize - pos, 
            "menuentry \"$VTLANG_RETURN_PREVIOUS\" --class=vtoyret VTOY_RET {\n"
                "echo 'Return ...'\n"
            "}\n");

    grub_script_execute_sourcecode(buf);
    grub_free(buf);
    
    return 0;
}

extern char g_ventoy_theme_path[256];

grub_err_t ventoy_cmd_set_theme(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_uint32_t i = 0;
    grub_uint32_t mod = 0;
    grub_uint32_t theme_num = 0;
    theme_list *node = g_theme_head;
    struct grub_datetime datetime;
    struct grub_video_mode_info info;
    char buf[64];
    char **pThemePath = NULL;

    (void)argc;
    (void)args;
    (void)ctxt;

    if (g_theme_single_file[0])
    {
        debug("single theme %s\n", g_theme_single_file);
        grub_env_set("theme", g_theme_single_file);
        goto end;
    }

    debug("g_theme_num = %d\n", g_theme_num);
    
    if (g_theme_num == 0)
    {
        goto end;
    }
    
    if (g_theme_id > 0 && g_theme_id <= g_theme_num)
    {
        for (i = 0; i < (grub_uint32_t)(g_theme_id - 1) && node; i++)
        {
            node = node->next;
        }

        grub_env_set("theme", node->theme.path);
        goto end;
    }

    pThemePath = (char **)grub_zalloc(sizeof(char *) * g_theme_num);
    if (!pThemePath)
    {
        goto end;
    }

    if (g_theme_res_fit)
    {
        if (grub_video_get_info(&info) == GRUB_ERR_NONE)
        {
            debug("get video info success %ux%u\n", info.width, info.height);
            grub_snprintf(buf, sizeof(buf), "%ux%u", info.width, info.height);
            for (node = g_theme_head; node; node = node->next)
            {
                if (grub_strstr(node->theme.path, buf))
                {
                    pThemePath[theme_num++] = node->theme.path;
                }
            }
        }
    }

    if (theme_num == 0)
    {
        for (node = g_theme_head; node; node = node->next)
        {
            pThemePath[theme_num++] = node->theme.path;
        }
    }

    if (theme_num == 1)
    {
        mod = 0;
        debug("Only 1 theme match, no need to random.\n");
    }
    else
    {
        grub_memset(&datetime, 0, sizeof(datetime));
        grub_get_datetime(&datetime);

        if (g_theme_random == vtoy_theme_random_boot_second)
        {
            grub_divmod32((grub_uint32_t)datetime.second, theme_num, &mod);
        }
        else if (g_theme_random == vtoy_theme_random_boot_day)
        {
            grub_divmod32((grub_uint32_t)datetime.day, theme_num, &mod);
        }
        else if (g_theme_random == vtoy_theme_random_boot_month)
        {
            grub_divmod32((grub_uint32_t)datetime.month, theme_num, &mod);
        }

        debug("%04d/%02d/%02d %02d:%02d:%02d theme_num:%d mod:%d\n",
              datetime.year, datetime.month, datetime.day,
              datetime.hour, datetime.minute, datetime.second,
              theme_num, mod);
    }

    if (argc > 0 && grub_strcmp(args[0], "switch") == 0)
    {
        grub_snprintf(g_ventoy_theme_path, sizeof(g_ventoy_theme_path), "%s", pThemePath[mod]);        
    }
    else
    {        
        debug("random theme %s\n", pThemePath[mod]);
        grub_env_set("theme", pThemePath[mod]);
    }
    g_ventoy_menu_refresh = 1;

end:

    grub_check_free(pThemePath);
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_set_theme_path(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)argc;
    (void)ctxt;

    if (argc == 0)
    {
        g_ventoy_theme_path[0] = 0;
    }
    else
    {
        grub_snprintf(g_ventoy_theme_path, sizeof(g_ventoy_theme_path), "%s", args[0]);
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

const char *ventoy_get_vmenu_title(const char *vMenu)
{
    return vtoy_json_get_string_ex(g_menu_lang_json->pstChild, vMenu);
}

int ventoy_plugin_load_menu_lang(int init, const char *lang)
{
    int ret = 1;
    grub_file_t file = NULL;
    char *buf = NULL;

    if (grub_strcmp(lang, g_cur_menu_language) == 0)
    {
        debug("Same menu lang %s\n", lang);
        return 0;
    }
    grub_snprintf(g_cur_menu_language, sizeof(g_cur_menu_language), "%s", lang);

    debug("Load menu lang %s\n", g_cur_menu_language);

    if (g_menu_lang_json)
    {
        vtoy_json_destroy(g_menu_lang_json);
        g_menu_lang_json = NULL;
    }

    g_menu_lang_json = vtoy_json_create();
    if (!g_menu_lang_json)
    {
        goto end;
    }

    file = ventoy_grub_file_open(GRUB_FILE_TYPE_LINUX_INITRD, "(vt_menu_tarfs)/menu/%s.json", lang);
    if (!file)
    {
        goto end;
    }

    buf = grub_malloc(file->size + 1);
    if (!buf)
    {
        grub_printf("Failed to malloc memory %lu.\n", (ulong)(file->size + 1));
        goto end;
    }

    buf[file->size] = 0;
    grub_file_read(file, buf, file->size);

    vtoy_json_parse(g_menu_lang_json, buf);

    if (g_default_menu_mode == 0)
    {
        grub_snprintf(g_ventoy_hotkey_tip, sizeof(g_ventoy_hotkey_tip), "%s", ventoy_get_vmenu_title("VTLANG_STR_HOTKEY_TREE"));
    }
    else
    {
        grub_snprintf(g_ventoy_hotkey_tip, sizeof(g_ventoy_hotkey_tip), "%s", ventoy_get_vmenu_title("VTLANG_STR_HOTKEY_LIST"));
    }

    if (init == 0)
    {
        ventoy_menu_push_key(GRUB_TERM_ESC);
        ventoy_menu_push_key(GRUB_TERM_ESC);
        g_ventoy_menu_refresh = 1;        
    }
    ret = 0;

end:

    check_free(file, grub_file_close);
    grub_check_free(buf);

    return ret;
}

grub_err_t ventoy_cmd_cur_menu_lang(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;

    if (argc > 0)
    {
        grub_env_set(args[0], g_cur_menu_language);
    }
    else
    {
        grub_printf("%s\n", g_cur_menu_language);
        grub_printf("%s\n", g_ventoy_hotkey_tip);
        grub_refresh();        
    }

    VENTOY_CMD_RETURN(0);
}

grub_err_t ventoy_cmd_push_menulang(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)argc;
    (void)ctxt;

    if (g_push_menu_language[0] == 0)
    {
        grub_memcpy(g_push_menu_language, g_cur_menu_language, sizeof(g_push_menu_language));
        ventoy_plugin_load_menu_lang(0, args[0]);
    }

    VENTOY_CMD_RETURN(0);
}

grub_err_t ventoy_cmd_pop_menulang(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)argc;
    (void)ctxt;
    (void)args;

    if (g_push_menu_language[0])
    {
        ventoy_plugin_load_menu_lang(0, g_push_menu_language);
        g_push_menu_language[0] = 0;
    }

    VENTOY_CMD_RETURN(0);
}


