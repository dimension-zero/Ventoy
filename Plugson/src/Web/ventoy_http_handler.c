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


static JSON_CB g_ventoy_json_cb[] = 
{
    { "sysinfo",        ventoy_api_sysinfo            },    
    { "handshake",      ventoy_api_handshake          },    
    { "check_path",     ventoy_api_check_exist        },    
    { "check_path2",    ventoy_api_check_exist2       },    
    { "check_fuzzy",    ventoy_api_check_fuzzy        },
    
    { "device_info",    ventoy_api_device_info        },    
    
    { "get_control",    ventoy_api_get_control        },    
    { "save_control",   ventoy_api_save_control       },  
    
    { "get_theme",      ventoy_api_get_theme          },    
    { "save_theme",     ventoy_api_save_theme         },    
    { "theme_add_file", ventoy_api_theme_add_file     },    
    { "theme_del_file", ventoy_api_theme_del_file     },    
    { "theme_add_font", ventoy_api_theme_add_font     },    
    { "theme_del_font", ventoy_api_theme_del_font     },
    
    { "get_alias",      ventoy_api_get_alias          },
    { "save_alias",     ventoy_api_save_alias         },   
    { "alias_add",      ventoy_api_alias_add          },
    { "alias_del",      ventoy_api_alias_del          },
    
    { "get_tip",        ventoy_api_get_tip            },
    { "save_tip",       ventoy_api_save_tip           },   
    { "tip_add",        ventoy_api_tip_add            },
    { "tip_del",        ventoy_api_tip_del            },
    
    { "get_class",      ventoy_api_get_class          },
    { "save_class",     ventoy_api_save_class         },   
    { "class_add",      ventoy_api_class_add          },
    { "class_del",      ventoy_api_class_del          },
    
    { "get_auto_memdisk",  ventoy_api_get_auto_memdisk  },
    { "save_auto_memdisk", ventoy_api_save_auto_memdisk },   
    { "auto_memdisk_add",  ventoy_api_auto_memdisk_add  },
    { "auto_memdisk_del",  ventoy_api_auto_memdisk_del  },
    
    { "get_image_list",  ventoy_api_get_image_list  },
    { "save_image_list", ventoy_api_save_image_list },
    { "image_list_add",  ventoy_api_image_list_add  },
    { "image_list_del",  ventoy_api_image_list_del  },

    { "get_conf_replace",      ventoy_api_get_conf_replace      },
    { "save_conf_replace",     ventoy_api_save_conf_replace     },   
    { "conf_replace_add",      ventoy_api_conf_replace_add      },
    { "conf_replace_del",      ventoy_api_conf_replace_del      },
    
    { "get_dud",            ventoy_api_get_dud      },
    { "save_dud",           ventoy_api_save_dud     },   
    { "dud_add",            ventoy_api_dud_add      },
    { "dud_del",            ventoy_api_dud_del      },
    { "dud_add_inner",      ventoy_api_dud_add_inner      },
    { "dud_del_inner",      ventoy_api_dud_del_inner      },
    
    { "get_auto_install",            ventoy_api_get_auto_install      },
    { "save_auto_install",           ventoy_api_save_auto_install     },   
    { "auto_install_add",            ventoy_api_auto_install_add      },
    { "auto_install_del",            ventoy_api_auto_install_del      },
    { "auto_install_add_inner",      ventoy_api_auto_install_add_inner      },
    { "auto_install_del_inner",      ventoy_api_auto_install_del_inner      },
    
    { "get_persistence",            ventoy_api_get_persistence      },
    { "save_persistence",           ventoy_api_save_persistence     },   
    { "persistence_add",            ventoy_api_persistence_add      },
    { "persistence_del",            ventoy_api_persistence_del      },
    { "persistence_add_inner",      ventoy_api_persistence_add_inner      },
    { "persistence_del_inner",      ventoy_api_persistence_del_inner      },
    
    { "get_password",      ventoy_api_get_password      },
    { "save_password",     ventoy_api_save_password     },   
    { "password_add",      ventoy_api_password_add      },
    { "password_del",      ventoy_api_password_del      },
    
    { "get_injection",      ventoy_api_get_injection     },
    { "save_injection",     ventoy_api_save_injection     },   
    { "injection_add",      ventoy_api_injection_add      },
    { "injection_del",      ventoy_api_injection_del      },
    { "preview_json",       ventoy_api_preview_json       },
    
};

int ventoy_json_handler(struct mg_connection *conn, VTOY_JSON *json, char *jsonstr)
{
    int i;
    const char *method = NULL;

    method = vtoy_json_get_string_ex(json, "method");
    if (!method)
    {
        ventoy_json_result(conn, VTOY_JSON_SUCCESS_RET);
        return 0;
    }

    if (strcmp(method, "handshake") == 0)
    {
        ventoy_api_handshake(conn, json);
        return 0;
    }

    for (i = 0; i < (int)(sizeof(g_ventoy_json_cb) / sizeof(g_ventoy_json_cb[0])); i++)
    {
        if (strcmp(method, g_ventoy_json_cb[i].method) == 0)
        {
            g_ventoy_json_cb[i].callback(conn, json);
            break;
        }
    }

    return 0;
}

int ventoy_request_handler(struct mg_connection *conn)
{
    int post_data_len;
    int post_buf_len;
    VTOY_JSON *json = NULL;
    char *post_data_buf = NULL;
    const struct mg_request_info *ri = NULL;
    char stack_buf[512];
    
    ri = mg_get_request_info(conn);    

    if (strcmp(ri->uri, "/vtoy/json") == 0)
    {
        if (ri->content_length > 500)
        {
            post_data_buf = malloc((int)(ri->content_length + 4));
            post_buf_len  = (int)(ri->content_length + 1);
        }
        else
        {
            post_data_buf = stack_buf;
            post_buf_len = sizeof(stack_buf);
        }
        
        post_data_len = mg_read(conn, post_data_buf, post_buf_len);
        post_data_buf[post_data_len] = 0;

        json = vtoy_json_create();
        if (JSON_SUCCESS == vtoy_json_parse(json, post_data_buf))
        {
            pthread_mutex_lock(&g_api_mutex);
            ventoy_json_handler(conn, json->pstChild, post_data_buf);
            pthread_mutex_unlock(&g_api_mutex);
        }
        else
        {
            ventoy_json_result(conn, VTOY_JSON_INVALID_RET);
        }

        vtoy_json_destroy(json);

        if (post_data_buf != stack_buf)
        {
            free(post_data_buf);
        }
        return 1;
    }
    else
    {
        return 0;
    }
}

const char *ventoy_web_openfile(const struct mg_connection *conn, const char *path, size_t *data_len)
{
    ventoy_file *node = NULL;

    (void)conn;

    if (!path)
    {
        return NULL;
    }
    
    node = ventoy_tar_find_file(path);
    if (node)
    {
        *data_len = node->size;
        return node->addr;
    }
    else
    {
        return NULL;
    }
}

#if 0
#endif

