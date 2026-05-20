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
void ventoy_data_default_control(data_control *data)
{
    memset(data, 0, sizeof(data_control));

    data->password_asterisk = 1;
    data->secondary_menu = 1;
    data->filter_dot_underscore = 1;
    data->max_search_level = -1;
    data->menu_timeout = 0;
    data->secondary_menu_timeout = 0;
    data->win11_bypass_check = 1;
    data->win11_bypass_nro = 1;
    
    strlcpy(data->default_kbd_layout, "QWERTY_USA");
    strlcpy(data->menu_language, "en_US");
}

int ventoy_data_cmp_control(data_control *data1, data_control *data2)
{
    if (data1->default_menu_mode != data2->default_menu_mode ||
        data1->treeview_style != data2->treeview_style ||
        data1->filter_dot_underscore != data2->filter_dot_underscore ||
        data1->sort_casesensitive != data2->sort_casesensitive ||
        data1->max_search_level != data2->max_search_level ||
        data1->vhd_no_warning != data2->vhd_no_warning ||
        data1->filter_iso != data2->filter_iso ||
        data1->filter_wim != data2->filter_wim ||
        data1->filter_efi != data2->filter_efi ||
        data1->filter_img != data2->filter_img ||
        data1->filter_vhd != data2->filter_vhd ||
        data1->filter_vtoy != data2->filter_vtoy ||
        data1->win11_bypass_check != data2->win11_bypass_check ||
        data1->win11_bypass_nro != data2->win11_bypass_nro ||
        data1->linux_remount != data2->linux_remount ||
        data1->password_asterisk != data2->password_asterisk ||
        data1->secondary_menu != data2->secondary_menu ||
        data1->menu_timeout != data2->menu_timeout ||
        data1->secondary_menu_timeout != data2->secondary_menu_timeout)
    {
        return 1;
    }

    if (strcmp(data1->default_search_root, data2->default_search_root) ||
        strcmp(data1->default_image, data2->default_image) ||
        strcmp(data1->default_kbd_layout, data2->default_kbd_layout) ||
        strcmp(data1->menu_language, data2->menu_language))
    {
        return 1;
    }

    return 0;
}

int ventoy_data_save_control(data_control *data, const char *title, char *buf, int buflen)
{
    int pos = 0;
    data_control *def = g_data_control + bios_max;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);

    VTOY_JSON_FMT_KEY_L(L1, title);
    VTOY_JSON_FMT_ARY_BEGIN_N();

    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_DEFAULT_MENU_MODE", default_menu_mode);        
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_TREE_VIEW_MENU_STYLE", treeview_style);
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_FILT_DOT_UNDERSCORE_FILE", filter_dot_underscore);
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_SORT_CASE_SENSITIVE",  sort_casesensitive);

    if (data->max_search_level >= 0)
    {
        VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_MAX_SEARCH_LEVEL",  max_search_level);            
    }
    
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_VHD_NO_WARNING",  vhd_no_warning);
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_FILE_FLT_ISO", filter_iso);
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_FILE_FLT_WIM", filter_wim);
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_FILE_FLT_EFI", filter_efi);
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_FILE_FLT_IMG", filter_img);
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_FILE_FLT_VHD", filter_vhd);
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_FILE_FLT_VTOY", filter_vtoy);
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_WIN11_BYPASS_CHECK",  win11_bypass_check);
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_WIN11_BYPASS_NRO",  win11_bypass_nro);
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_LINUX_REMOUNT",  linux_remount);
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_SECONDARY_BOOT_MENU",  secondary_menu);
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_SHOW_PASSWORD_ASTERISK",  password_asterisk);
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_MENU_TIMEOUT",  menu_timeout);
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_SECONDARY_TIMEOUT",  secondary_menu_timeout);

    VTOY_JSON_FMT_CTRL_STRN(L2, "VTOY_DEFAULT_KBD_LAYOUT", default_kbd_layout);        
    VTOY_JSON_FMT_CTRL_STRN(L2, "VTOY_MENU_LANGUAGE", menu_language);  

    if (strcmp(def->default_search_root, data->default_search_root))
    {
        VTOY_JSON_FMT_CTRL_STRN_STR(L2, "VTOY_DEFAULT_SEARCH_ROOT", ventoy_real_path(data->default_search_root));
    }
    
    if (strcmp(def->default_image, data->default_image))
    {
        VTOY_JSON_FMT_CTRL_STRN_STR(L2, "VTOY_DEFAULT_IMAGE", ventoy_real_path(data->default_image));
    }

    VTOY_JSON_FMT_ARY_ENDEX_LN(L1);
    VTOY_JSON_FMT_END(pos);

    return pos;
}

int ventoy_data_json_control(data_control *ctrl, char *buf, int buflen)
{
    int i = 0;
    int pos = 0;
    int valid = 0;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);
    VTOY_JSON_FMT_OBJ_BEGIN();

    VTOY_JSON_FMT_SINT("default_menu_mode", ctrl->default_menu_mode);
    VTOY_JSON_FMT_SINT("treeview_style",  ctrl->treeview_style);
    VTOY_JSON_FMT_SINT("filter_dot_underscore",  ctrl->filter_dot_underscore);
    VTOY_JSON_FMT_SINT("sort_casesensitive",  ctrl->sort_casesensitive);
    VTOY_JSON_FMT_SINT("max_search_level",  ctrl->max_search_level);    
    VTOY_JSON_FMT_SINT("vhd_no_warning",  ctrl->vhd_no_warning);
    
    VTOY_JSON_FMT_SINT("filter_iso", ctrl->filter_iso);
    VTOY_JSON_FMT_SINT("filter_wim", ctrl->filter_wim);
    VTOY_JSON_FMT_SINT("filter_efi", ctrl->filter_efi);
    VTOY_JSON_FMT_SINT("filter_img", ctrl->filter_img);
    VTOY_JSON_FMT_SINT("filter_vhd", ctrl->filter_vhd);
    VTOY_JSON_FMT_SINT("filter_vtoy", ctrl->filter_vtoy);
    VTOY_JSON_FMT_SINT("win11_bypass_check",  ctrl->win11_bypass_check);
    VTOY_JSON_FMT_SINT("win11_bypass_nro",  ctrl->win11_bypass_nro);
    VTOY_JSON_FMT_SINT("linux_remount",  ctrl->linux_remount);
    VTOY_JSON_FMT_SINT("secondary_menu",  ctrl->secondary_menu);
    VTOY_JSON_FMT_SINT("password_asterisk",  ctrl->password_asterisk);
    VTOY_JSON_FMT_SINT("menu_timeout",  ctrl->menu_timeout);
    VTOY_JSON_FMT_SINT("secondary_menu_timeout",  ctrl->secondary_menu_timeout);
    VTOY_JSON_FMT_STRN("default_kbd_layout",  ctrl->default_kbd_layout);
    VTOY_JSON_FMT_STRN("menu_language",  ctrl->menu_language);

    valid = 0;
    if (ctrl->default_search_root[0] && ventoy_is_directory_exist("%s%s", g_cur_dir, ctrl->default_search_root))
    {
        valid = 1;
    }
    VTOY_JSON_FMT_STRN("default_search_root",  ctrl->default_search_root);
    VTOY_JSON_FMT_SINT("default_search_root_valid", valid);

    
    valid = 0;
    if (ctrl->default_image[0] && ventoy_is_file_exist("%s%s", g_cur_dir, ctrl->default_image))
    {
        valid = 1;
    }
    VTOY_JSON_FMT_STRN("default_image", ctrl->default_image);
    VTOY_JSON_FMT_SINT("default_image_valid", valid);

    VTOY_JSON_FMT_KEY("menu_list");
    VTOY_JSON_FMT_ARY_BEGIN();

    for (i = 0; g_ventoy_menu_lang[i][0]; i++)
    {
        VTOY_JSON_FMT_ITEM(g_ventoy_menu_lang[i]);        
    }
    VTOY_JSON_FMT_ARY_ENDEX();
    
    VTOY_JSON_FMT_OBJ_END();
    VTOY_JSON_FMT_END(pos);

    return pos;
}

int ventoy_api_get_control(struct mg_connection *conn, VTOY_JSON *json)
{
    api_get_func(conn, json, control);
    return 0;
}

int ventoy_api_save_control(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    data_control *ctrl = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    ctrl = g_data_control + index;

    VTOY_JSON_INT("default_menu_mode", ctrl->default_menu_mode);
    VTOY_JSON_INT("treeview_style", ctrl->treeview_style);
    VTOY_JSON_INT("filter_dot_underscore", ctrl->filter_dot_underscore);
    VTOY_JSON_INT("sort_casesensitive", ctrl->sort_casesensitive);
    VTOY_JSON_INT("max_search_level", ctrl->max_search_level);
    VTOY_JSON_INT("vhd_no_warning", ctrl->vhd_no_warning);
    VTOY_JSON_INT("filter_iso", ctrl->filter_iso);
    VTOY_JSON_INT("filter_wim", ctrl->filter_wim);
    VTOY_JSON_INT("filter_efi", ctrl->filter_efi);
    VTOY_JSON_INT("filter_img", ctrl->filter_img);
    VTOY_JSON_INT("filter_vhd", ctrl->filter_vhd);
    VTOY_JSON_INT("filter_vtoy", ctrl->filter_vtoy);
    VTOY_JSON_INT("win11_bypass_check", ctrl->win11_bypass_check);
    VTOY_JSON_INT("win11_bypass_nro", ctrl->win11_bypass_nro);
    VTOY_JSON_INT("linux_remount", ctrl->linux_remount);
    VTOY_JSON_INT("secondary_menu", ctrl->secondary_menu);
    VTOY_JSON_INT("password_asterisk", ctrl->password_asterisk);
    VTOY_JSON_INT("menu_timeout", ctrl->menu_timeout);
    VTOY_JSON_INT("secondary_menu_timeout", ctrl->secondary_menu_timeout);

    VTOY_JSON_STR("default_image", ctrl->default_image);
    VTOY_JSON_STR("default_search_root", ctrl->default_search_root);
    VTOY_JSON_STR("menu_language", ctrl->menu_language);
    VTOY_JSON_STR("default_kbd_layout", ctrl->default_kbd_layout);
    
    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);
    return 0;
}


#if 0
#endif

