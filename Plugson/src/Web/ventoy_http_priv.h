/* ventoy_http_priv.h — cross-module declarations for ventoy_http */
#ifndef __VENTOY_HTTP_PRIV_H__
#define __VENTOY_HTTP_PRIV_H__

#include <ventoy_define.h>
#include <ventoy_json.h>
#include <ventoy_util.h>
#include <ventoy_disk.h>
#include <ventoy_http.h>

#if defined(_MSC_VER) || defined(WIN32)
#else
#include <pthread.h>
#endif

/* ---- macros ---- */
#define VTOY_DEL_ALL_PATH   "4119ae33-98ea-448e-b9c0-569aafcf1fb4"
#define JSON_BUFFER g_pub_json_buffer
#define JSON_SAVE_BUFFER g_pub_save_buffer
#define ventoy_is_real_exist_common(xpath, xnode, xtype) \
    ventoy_path_is_real_exist(xpath, xnode, offsetof(xtype, path), offsetof(xtype, next))

/* ---- globals (defined in ventoy_http.c) ---- */
extern const char *g_json_title_postfix[];
extern const char *g_ventoy_kbd_layout[];
extern int g_json_exist[plugin_type_max][bios_max];
extern const char *g_plugin_name[];
extern char g_ventoy_menu_lang[MAX_LANGUAGE][8];
extern char g_pub_path[2 * MAX_PATH];
extern data_control g_data_control[];
extern data_theme g_data_theme[];
extern data_alias g_data_menu_alias[];
extern data_tip   g_data_menu_tip[];
extern data_class g_data_menu_class[];
extern data_image_list g_data_image_list[];
extern data_image_list *g_data_image_blacklist;
extern data_auto_memdisk g_data_auto_memdisk[];
extern data_password g_data_password[];
extern data_conf_replace g_data_conf_replace[];
extern data_injection g_data_injection[];
extern data_auto_install g_data_auto_install[];
extern data_persistence g_data_persistence[];
extern data_dud g_data_dud[];
extern char *g_pub_json_buffer;
extern char *g_pub_save_buffer;
extern pthread_mutex_t g_api_mutex;
extern struct mg_context *g_ventoy_http_ctx;

/* ---- function prototypes ---- */
int ventoy_is_kbd_valid(const char *key);
const char * ventoy_real_path(const char *org);
int ventoy_json_result(struct mg_connection *conn, const char *err);
int ventoy_json_buffer(struct mg_connection *conn, const char *json_buf, int json_len);
void ventoy_free_path_node_list(path_node *list);
int ventoy_path_is_real_exist(const char *path, void *head, size_t pathoff, size_t nextoff);
path_node * ventoy_path_node_add_array(VTOY_JSON *array);
int ventoy_check_fuzzy_path(char *path, int prefix);
int ventoy_path_list_cmp(path_node *list1, path_node *list2);
int ventoy_api_device_info(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_sysinfo(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_handshake(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_check_exist(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_check_exist2(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_check_fuzzy(struct mg_connection *conn, VTOY_JSON *json);
void ventoy_data_default_control(data_control *data);
int ventoy_data_cmp_control(data_control *data1, data_control *data2);
int ventoy_data_save_control(data_control *data, const char *title, char *buf, int buflen);
int ventoy_data_json_control(data_control *ctrl, char *buf, int buflen);
int ventoy_api_get_control(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_save_control(struct mg_connection *conn, VTOY_JSON *json);
void ventoy_data_default_theme(data_theme *data);
int ventoy_data_cmp_theme(data_theme *data1, data_theme *data2);
int ventoy_data_save_theme(data_theme *data, const char *title, char *buf, int buflen);
int ventoy_data_json_theme(data_theme *data, char *buf, int buflen);
int ventoy_api_get_theme(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_save_theme(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_theme_add_file(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_theme_del_file(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_theme_add_font(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_theme_del_font(struct mg_connection *conn, VTOY_JSON *json);
void ventoy_data_default_menu_alias(data_alias *data);
int ventoy_data_cmp_menu_alias(data_alias *data1, data_alias *data2);
int ventoy_data_save_menu_alias(data_alias *data, const char *title, char *buf, int buflen);
int ventoy_data_json_menu_alias(data_alias *data, char *buf, int buflen);
int ventoy_api_get_alias(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_save_alias(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_alias_add(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_alias_del(struct mg_connection *conn, VTOY_JSON *json);
void ventoy_data_default_menu_tip(data_tip *data);
int ventoy_data_cmp_menu_tip(data_tip *data1, data_tip *data2);
int ventoy_data_save_menu_tip(data_tip *data, const char *title, char *buf, int buflen);
int ventoy_data_json_menu_tip(data_tip *data, char *buf, int buflen);
int ventoy_api_get_tip(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_save_tip(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_tip_add(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_tip_del(struct mg_connection *conn, VTOY_JSON *json);
void ventoy_data_default_menu_class(data_class *data);
int ventoy_data_cmp_menu_class(data_class *data1, data_class *data2);
int ventoy_data_save_menu_class(data_class *data, const char *title, char *buf, int buflen);
int ventoy_data_json_menu_class(data_class *data, char *buf, int buflen);
int ventoy_api_get_class(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_save_class(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_class_add(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_class_del(struct mg_connection *conn, VTOY_JSON *json);
void ventoy_data_default_auto_memdisk(data_auto_memdisk *data);
int ventoy_data_cmp_auto_memdisk(data_auto_memdisk *data1, data_auto_memdisk *data2);
int ventoy_data_save_auto_memdisk(data_auto_memdisk *data, const char *title, char *buf, int buflen);
int ventoy_data_json_auto_memdisk(data_auto_memdisk *data, char *buf, int buflen);
int ventoy_api_get_auto_memdisk(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_save_auto_memdisk(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_auto_memdisk_add(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_auto_memdisk_del(struct mg_connection *conn, VTOY_JSON *json);
void ventoy_data_default_image_list(data_image_list *data);
int ventoy_data_cmp_image_list(data_image_list *data1, data_image_list *data2);
int ventoy_data_save_image_list(data_image_list *data, const char *title, char *buf, int buflen);
int ventoy_data_json_image_list(data_image_list *data, char *buf, int buflen);
int ventoy_api_get_image_list(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_save_image_list(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_image_list_add(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_image_list_del(struct mg_connection *conn, VTOY_JSON *json);
void ventoy_data_default_password(data_password *data);
int ventoy_data_cmp_password(data_password *data1, data_password *data2);
int ventoy_data_save_password(data_password *data, const char *title, char *buf, int buflen);
int ventoy_data_json_password(data_password *data, char *buf, int buflen);
int ventoy_api_get_password(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_save_password(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_password_add(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_password_del(struct mg_connection *conn, VTOY_JSON *json);
void ventoy_data_default_conf_replace(data_conf_replace *data);
int ventoy_data_cmp_conf_replace(data_conf_replace *data1, data_conf_replace *data2);
int ventoy_data_save_conf_replace(data_conf_replace *data, const char *title, char *buf, int buflen);
int ventoy_data_json_conf_replace(data_conf_replace *data, char *buf, int buflen);
int ventoy_api_get_conf_replace(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_save_conf_replace(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_conf_replace_add(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_conf_replace_del(struct mg_connection *conn, VTOY_JSON *json);
void ventoy_data_default_dud(data_dud *data);
int ventoy_data_cmp_dud(data_dud *data1, data_dud *data2);
int ventoy_data_save_dud(data_dud *data, const char *title, char *buf, int buflen);
int ventoy_data_json_dud(data_dud *data, char *buf, int buflen);
int ventoy_api_get_dud(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_save_dud(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_dud_add(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_dud_del(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_dud_add_inner(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_dud_del_inner(struct mg_connection *conn, VTOY_JSON *json);
void ventoy_data_default_auto_install(data_auto_install *data);
int ventoy_data_cmp_auto_install(data_auto_install *data1, data_auto_install *data2);
int ventoy_data_save_auto_install(data_auto_install *data, const char *title, char *buf, int buflen);
int ventoy_data_json_auto_install(data_auto_install *data, char *buf, int buflen);
int ventoy_api_get_auto_install(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_save_auto_install(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_auto_install_add(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_auto_install_del(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_auto_install_add_inner(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_auto_install_del_inner(struct mg_connection *conn, VTOY_JSON *json);
void ventoy_data_default_persistence(data_persistence *data);
int ventoy_data_cmp_persistence(data_persistence *data1, data_persistence *data2);
int ventoy_data_save_persistence(data_persistence *data, const char *title, char *buf, int buflen);
int ventoy_data_json_persistence(data_persistence *data, char *buf, int buflen);
int ventoy_api_get_persistence(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_save_persistence(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_persistence_add(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_persistence_del(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_persistence_add_inner(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_persistence_del_inner(struct mg_connection *conn, VTOY_JSON *json);
void ventoy_data_default_injection(data_injection *data);
int ventoy_data_cmp_injection(data_injection *data1, data_injection *data2);
int ventoy_data_save_injection(data_injection *data, const char *title, char *buf, int buflen);
int ventoy_data_json_injection(data_injection *data, char *buf, int buflen);
int ventoy_api_get_injection(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_save_injection(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_injection_add(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_injection_del(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_api_preview_json(struct mg_connection *conn, VTOY_JSON *json);
int ventoy_data_save_all(void);
int ventoy_data_real_save_all(int apilock);
int ventoy_http_writeback(void);
int ventoy_json_handler(struct mg_connection *conn, VTOY_JSON *json, char *jsonstr);
int ventoy_request_handler(struct mg_connection *conn);
const char *ventoy_web_openfile(const struct mg_connection *conn, const char *path, size_t *data_len);
int ventoy_parse_control(VTOY_JSON *json, void *p);
int ventoy_parse_theme(VTOY_JSON *json, void *p);
int ventoy_parse_menu_alias(VTOY_JSON *json, void *p);
int ventoy_parse_menu_tip(VTOY_JSON *json, void *p);
int ventoy_parse_menu_class(VTOY_JSON *json, void *p);
int ventoy_parse_auto_install(VTOY_JSON *json, void *p);
int ventoy_parse_persistence(VTOY_JSON *json, void *p);
int ventoy_parse_injection(VTOY_JSON *json, void *p);
int ventoy_parse_conf_replace(VTOY_JSON *json, void *p);
int ventoy_parse_password(VTOY_JSON *json, void *p);
int ventoy_parse_image_list_real(VTOY_JSON *json, int type, void *p);
int ventoy_parse_image_blacklist(VTOY_JSON *json, void *p);
int ventoy_parse_image_list(VTOY_JSON *json, void *p);
int ventoy_parse_auto_memdisk(VTOY_JSON *json, void *p);
int ventoy_parse_dud(VTOY_JSON *json, void *p);
int ventoy_load_old_json(const char *filename);
int ventoy_http_start(const char *ip, const char *port);
int ventoy_http_stop(void);
int ventoy_http_init(void);
void ventoy_http_exit(void);

#endif /* __VENTOY_HTTP_PRIV_H__ */
