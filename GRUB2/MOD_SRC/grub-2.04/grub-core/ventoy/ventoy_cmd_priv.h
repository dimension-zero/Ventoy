/* ventoy_cmd_priv.h — cross-module declarations for ventoy_cmd */
#ifndef __VENTOY_CMD_PRIV_H__
#define __VENTOY_CMD_PRIV_H__

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
#include <grub/efi/api.h>
#ifdef GRUB_MACHINE_EFI
#include <grub/efi/efi.h>
#endif
#include <grub/time.h>
#include <grub/video.h>
#include <grub/acpi.h>
#include <grub/charset.h>
#include <grub/crypto.h>
#include <grub/lib/crc.h>
#include <grub/random.h>
#include <grub/ventoy.h>
#include "ventoy_def.h"
#include "miniz.h"

/* ---- file-scope globals (all defined in ventoy_cmd.c) ---- */
extern grub_uint8_t g_check_mbr_data[64];
extern grub_file_t g_old_file;
extern int g_ventoy_last_entry_back;
extern char *g_list_script_buf;
extern int g_list_script_pos;
extern char *g_part_list_buf;
extern int g_part_list_pos;
extern grub_uint64_t g_part_end_max;
extern int g_video_mode_max;
extern int g_video_mode_num;
extern ventoy_video_mode *g_video_mode_list;
extern int g_enumerate_time_checked;
extern grub_uint64_t g_enumerate_start_time_ms;
extern grub_uint64_t g_enumerate_finish_time_ms;
extern char g_iso_vd_id_publisher[130];
extern char g_iso_vd_id_prepare[130];
extern char g_iso_vd_id_application[130];
extern int g_pager_flag;
extern char g_old_pager[32];
extern const char *g_lower_chksum_name[VTOY_CHKSUM_NUM];
extern int g_lower_chksum_namelen[VTOY_CHKSUM_NUM];
extern int g_chksum_retlen[VTOY_CHKSUM_NUM];
extern int g_vtoy_secondary_need_recover;
extern int g_vtoy_load_prompt;
extern char g_vtoy_prompt_msg[64];
extern char g_json_case_mis_path[32];
extern ventoy_vlnk_part *g_vlnk_part_list;
extern char g_fake_vlnk_src[512];
extern char g_fake_vlnk_dst[512];
extern grub_uint64_t g_fake_vlnk_size;
extern const char* g_chunk_err_msg[VTOY_CHUNK_ERR_MAX];

/* ---- function prototypes ---- */
int ventoy_get_fs_type(const char *fs);
int ventoy_string_check(const char *str, grub_char_check_func check);
grub_ssize_t ventoy_fs_read(grub_file_t file, char *buf, grub_size_t len);
int ventoy_control_get_flag(const char *key);
grub_err_t ventoy_fs_close(grub_file_t file);
int ventoy_video_hook(const struct grub_video_mode_info *info, void *hook_arg);
int ventoy_video_mode_cmp(ventoy_video_mode *v1, ventoy_video_mode *v2);
int ventoy_enum_video_mode(void);
int ventoy_pre_parse_data(char *src, int size);
grub_file_t ventoy_wrapper_open(grub_file_t rawFile, enum grub_file_type type);
int ventoy_check_decimal_var(const char *name, long *value);
grub_uint64_t ventoy_get_vtoy_partsize(int part);
int ventoy_load_efiboot_template(char **buf, int *datalen, int *direntoff);
int ventoy_set_check_result(int ret, const char *msg);
int ventoy_check_official_device(grub_device_t dev);
int ventoy_check_ignore_flag(const char *filename, const struct grub_dirhook_info *info, void *data);
grub_uint64_t ventoy_grub_get_file_size(const char *fmt, ...);
grub_file_t ventoy_grub_file_open(enum grub_file_type type, const char *fmt, ...);
int ventoy_is_dir_exist(const char *fmt, ...);
int ventoy_gzip_compress(void *mem_in, int mem_in_len, void *mem_out, int mem_out_len);
grub_err_t ventoy_cmd_debug(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_break(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_strstr(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_strbegin(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_strcasebegin(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_incr(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_mod(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_file_size(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_load_wimboot(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_concat_efi_iso(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_set_wim_prompt(grub_extcmd_context_t ctxt, int argc, char **args);
int ventoy_need_prompt_load_file(void);
grub_ssize_t ventoy_load_file_with_prompt(grub_file_t file, void *buf, grub_ssize_t size);
grub_err_t ventoy_cmd_load_file_to_mem(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_load_img_memdisk(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_iso9660_is_joliet(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_iso9660_nojoliet(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_is_udf(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_cmp(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_device(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_check_compatible(grub_extcmd_context_t ctxt, int argc, char **args);
int ventoy_cmp_img(img_info *img1, img_info *img2);
int ventoy_cmp_subdir(img_iterator_node *node1, img_iterator_node *node2);
void ventoy_swap_img(img_info *img1, img_info *img2);
int ventoy_img_name_valid(const char *filename, grub_size_t namelen);
int ventoy_vlnk_iterate_partition(struct grub_disk *disk, const grub_partition_t partition, void *data);
int ventoy_vlnk_iterate_disk(const char *name, void *data);
int ventoy_vlnk_probe_fs(ventoy_vlnk_part *cur);
int ventoy_check_vlnk_data(ventoy_vlnk *vlnk, int print, char *dst, int size);
int ventoy_add_vlnk_file(char *dir, const char *name);
int ventoy_collect_img_files(const char *filename, const struct grub_dirhook_info *info, void *data);
int ventoy_fill_data(grub_uint32_t buflen, char *buffer);
int ventoy_get_password(char buf[], unsigned buf_size);
int ventoy_check_password(const vtoy_password *pwd, int retry);
img_info * ventoy_get_min_iso(img_iterator_node *node);
img_iterator_node * ventoy_get_min_child(img_iterator_node *node);
int ventoy_dynamic_tree_menu(img_iterator_node *node);
int ventoy_set_default_menu(void);
grub_err_t ventoy_cmd_clear_img(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_img_name(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_ext_select_img_path(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_set_fake_vlnk(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_reset_fake_vlnk(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_chosen_img_path(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_list_img(grub_extcmd_context_t ctxt, int argc, char **args);
int ventoy_get_disk_guid(const char *filename, grub_uint8_t *guid, grub_uint8_t *signature);
grub_uint32_t ventoy_get_iso_boot_catlog(grub_file_t file);
grub_uint32_t ventoy_get_bios_eltorito_rba(grub_file_t file, grub_uint32_t sector);
int ventoy_has_efi_eltorito(grub_file_t file, grub_uint32_t sector);
void ventoy_fill_os_param(grub_file_t file, ventoy_os_param *param);
const char * ventoy_get_chunk_err_msg(grub_uint32_t err);
int ventoy_get_block_list(grub_file_t file, ventoy_img_chunk_list *chunklist, grub_disk_addr_t start);
grub_err_t ventoy_cmd_img_sector(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_select_conf_replace(grub_extcmd_context_t ctxt, int argc, char **args);
int ventoy_var_expand(int *record, int *flag, const char *var, char *expand, int len);
int ventoy_auto_install_var_expand(install_template *node);
grub_err_t ventoy_cmd_sel_auto_install(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_sel_persistence(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_dump_img_sector(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_test_block_list(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_add_replace_file(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_get_replace_file_cnt(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_dump_menu(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_dump_img_list(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_dump_injection(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_dump_auto_install(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_dump_persistence(grub_extcmd_context_t ctxt, int argc, char **args);
int ventoy_check_mode_by_name(char *filename, const char *suffix);
grub_err_t ventoy_cmd_check_mode(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_dynamic_menu(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_file_exist_nocase(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_find_bootable_hdd(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_read_1st_line(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_img_part_info(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_file_strstr(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_parse_volume(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_parse_create_date(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_img_hook_root(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_img_unhook_root(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_check_secureboot_var(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_check_secureboot_var(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_img_check_range(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_clear_key(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_acpi_param(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_push_last_entry(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_pop_last_entry(grub_extcmd_context_t ctxt, int argc, char **args);
int ventoy_lib_module_callback(const char *filename, const struct grub_dirhook_info *info, void *data);
grub_err_t ventoy_cmd_lib_module_ver(grub_extcmd_context_t ctxt, int argc, char **args);
int ventoy_load_part_table(const char *diskname);
void ventoy_prompt_end(void);
grub_err_t ventoy_cmd_load_part_table(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_check_custom_boot(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_part_exist(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_get_fs_label(grub_extcmd_context_t ctxt, int argc, char **args);
int ventoy_fs_enum_1st_file(const char *filename, const struct grub_dirhook_info *info, void *data);
int ventoy_fs_enum_1st_dir(const char *filename, const struct grub_dirhook_info *info, void *data);
grub_err_t ventoy_fs_enum_1st_child(int argc, char **args, grub_fs_dir_hook_t hook);
grub_err_t ventoy_cmd_fs_enum_1st_file(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_fs_enum_1st_dir(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_basename(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_basefile(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_enum_video_mode(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t vt_cmd_update_cur_video_mode(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_get_video_mode(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_get_efivdisk_offset(grub_extcmd_context_t ctxt, int argc, char **args);
int ventoy_collect_replace_initrd(const char *filename, const struct grub_dirhook_info *info, void *data);
grub_err_t ventoy_cmd_search_replace_initrd(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_push_pager(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_pop_pager(grub_extcmd_context_t ctxt, int argc, char **args);
int ventoy_chk_case_file(const char *filename, const struct grub_dirhook_info *info, void *data);
int ventoy_chk_case_dir(const char *filename, const struct grub_dirhook_info *info, void *data);
grub_err_t ventoy_cmd_chk_json_pathcase(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t grub_cmd_gptpriority(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t grub_cmd_syslinux_nojoliet(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t grub_cmd_vlnk_dump_part(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t grub_cmd_is_vlnk_name(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t grub_cmd_get_vlnk_dst(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t grub_cmd_check_vlnk(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_iso_vd_id_clear(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_iso_vd_id_parse(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_iso_vd_id_begin(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_fn_mutex_lock(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_dump_rsv_page(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_need_secondary_menu(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_show_secondary_menu(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_secondary_recover_mode(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_fs_ignore_case(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_init_menu_lang(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_load_menu_lang(grub_extcmd_context_t ctxt, int argc, char **args);
int ventoy_chksum_pathcmp(int chktype, char *rlpath, char *rdpath);
int ventoy_check_chkfile(const char *isopart, char *path, const char *lchkname, grub_file_t *pfile);
grub_err_t ventoy_cmd_cmp_checksum(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_vtoychksum_exist(grub_extcmd_context_t ctxt, int argc, char **args);
const char * ventoy_menu_lang_read_hook(struct grub_env_var *var, const char *val);
int ventoy_env_init(void);
int ventoy_register_all_cmd(void);
int ventoy_unregister_all_cmd(void);

#endif /* __VENTOY_CMD_PRIV_H__ */
