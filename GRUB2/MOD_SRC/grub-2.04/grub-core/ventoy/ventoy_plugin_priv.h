/*
 * ventoy_plugin_priv.h — internal cross-TU declarations shared between
 * ventoy_plugin.c (master), ventoy_plugin_control.c, ventoy_plugin_install.c,
 * ventoy_plugin_pwd.c, ventoy_plugin_menu1.c, ventoy_plugin_menu2.c,
 * ventoy_plugin_master.c (dumps + getters), ventoy_plugin_cmd.c.
 */
#ifndef __VENTOY_PLUGIN_PRIV_H__
#define __VENTOY_PLUGIN_PRIV_H__

extern char g_iso_disk_name[128];
extern vtoy_password g_boot_pwd;
extern vtoy_password g_file_type_pwd[img_type_max];
extern install_template *g_install_template_head;
extern dud *g_dud_head;
extern menu_password *g_pwd_head;
extern persistence_config *g_persistence_head;
extern menu_tip *g_menu_tip_head;
extern menu_alias *g_menu_alias_head;
extern menu_class *g_menu_class_head;
extern custom_boot *g_custom_boot_head;
extern injection_config *g_injection_head;
extern auto_memdisk *g_auto_memdisk_head;
extern image_list *g_image_list_head;
extern conf_replace *g_conf_replace_head;
extern VTOY_JSON *g_menu_lang_json;
extern int g_theme_id;
extern int g_theme_res_fit;
extern int g_theme_num;
extern theme_list *g_theme_head;
extern int g_theme_random;
extern char g_theme_single_file[256];
extern char g_cur_menu_language[32];
extern char g_push_menu_language[32];

int ventoy_plugin_is_parent(const char *pat, int patlen, const char *isopath);
int ventoy_plugin_check_path(const char *path, const char *file);
int ventoy_plugin_check_fullpath(VTOY_JSON *json, const char *isodisk, const char *key, int *pathnum);
int ventoy_plugin_parse_fullpath(VTOY_JSON *json, const char *isodisk, const char *key, file_fullpath **fullpath, int *pathnum);

int ventoy_plugin_control_check(VTOY_JSON *json, const char *isodisk);
int ventoy_plugin_control_entry(VTOY_JSON *json, const char *isodisk);
int ventoy_plugin_theme_check(VTOY_JSON *json, const char *isodisk);
int ventoy_plugin_theme_entry(VTOY_JSON *json, const char *isodisk);
int ventoy_plugin_auto_install_check(VTOY_JSON *json, const char *isodisk);
int ventoy_plugin_auto_install_entry(VTOY_JSON *json, const char *isodisk);
int ventoy_plugin_dud_check(VTOY_JSON *json, const char *isodisk);
int ventoy_plugin_dud_entry(VTOY_JSON *json, const char *isodisk);
int ventoy_plugin_pwd_check(VTOY_JSON *json, const char *isodisk);
int ventoy_plugin_pwd_entry(VTOY_JSON *json, const char *isodisk);
int ventoy_plugin_persistence_check(VTOY_JSON *json, const char *isodisk);
int ventoy_plugin_persistence_entry(VTOY_JSON *json, const char *isodisk);
int ventoy_plugin_menualias_check(VTOY_JSON *json, const char *isodisk);
int ventoy_plugin_menualias_entry(VTOY_JSON *json, const char *isodisk);
int ventoy_plugin_menutip_check(VTOY_JSON *json, const char *isodisk);
int ventoy_plugin_menutip_entry(VTOY_JSON *json, const char *isodisk);
int ventoy_plugin_injection_check(VTOY_JSON *json, const char *isodisk);
int ventoy_plugin_injection_entry(VTOY_JSON *json, const char *isodisk);
int ventoy_plugin_menuclass_check(VTOY_JSON *json, const char *isodisk);
int ventoy_plugin_menuclass_entry(VTOY_JSON *json, const char *isodisk);
int ventoy_plugin_custom_boot_check(VTOY_JSON *json, const char *isodisk);
int ventoy_plugin_custom_boot_entry(VTOY_JSON *json, const char *isodisk);
int ventoy_plugin_conf_replace_check(VTOY_JSON *json, const char *isodisk);
int ventoy_plugin_conf_replace_entry(VTOY_JSON *json, const char *isodisk);
int ventoy_plugin_auto_memdisk_check(VTOY_JSON *json, const char *isodisk);
int ventoy_plugin_auto_memdisk_entry(VTOY_JSON *json, const char *isodisk);
int ventoy_plugin_image_list_check(VTOY_JSON *json, const char *isodisk);
int ventoy_plugin_image_list_entry(VTOY_JSON *json, const char *isodisk);

#endif
