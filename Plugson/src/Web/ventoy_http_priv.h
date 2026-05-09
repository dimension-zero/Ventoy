/*
 * ventoy_http_priv.h — internal cross-TU declarations for ventoy_http.c split.
 * Globals defined in ventoy_http.c (master) are externed here for the 14
 * subsystem TUs that share the same global state.
 */
#ifndef __VENTOY_HTTP_PRIV_H__
#define __VENTOY_HTTP_PRIV_H__

#include <pthread.h>

extern const char *g_json_title_postfix[bios_max + 1];
extern const char *g_ventoy_kbd_layout[];
extern int g_json_exist[plugin_type_max][bios_max];
extern const char *g_plugin_name[plugin_type_max];
extern char g_ventoy_menu_lang[MAX_LANGUAGE][8];
extern char g_pub_path[2 * MAX_PATH];
extern data_control g_data_control[bios_max + 1];
extern data_theme g_data_theme[bios_max + 1];
extern data_alias g_data_menu_alias[bios_max + 1];
extern data_tip   g_data_menu_tip[bios_max + 1];
extern data_class g_data_menu_class[bios_max + 1];
extern data_image_list g_data_image_list[bios_max + 1];
extern data_image_list *g_data_image_blacklist;
extern data_auto_memdisk g_data_auto_memdisk[bios_max + 1];
extern data_password g_data_password[bios_max + 1];
extern data_conf_replace g_data_conf_replace[bios_max + 1];
extern data_injection g_data_injection[bios_max + 1];
extern data_auto_install g_data_auto_install[bios_max + 1];
extern data_persistence g_data_persistence[bios_max + 1];
extern data_dud g_data_dud[bios_max + 1];
extern char *g_pub_json_buffer;
extern char *g_pub_save_buffer;
extern pthread_mutex_t g_api_mutex;
extern struct mg_context *g_ventoy_http_ctx;

#endif
