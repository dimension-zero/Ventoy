/* ventoy_windows_priv.h — cross-module declarations for ventoy_windows */
#ifndef __VENTOY_WINDOWS_PRIV_H__
#define __VENTOY_WINDOWS_PRIV_H__

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
#include <grub/time.h>
#include <grub/crypto.h>
#include <grub/ventoy.h>
#include "ventoy_def.h"

/* ---- file-scope globals ---- */
extern int g_iso_fs_type;
extern int g_wim_total_patch_count;
extern int g_wim_valid_patch_count;
extern wim_patch *g_wim_patch_head;
extern grub_uint64_t g_suppress_wincd_override_offset;
extern grub_uint32_t g_suppress_wincd_override_data;

/* ---- forward decls (resolved by lzx.c / xca.c) ---- */
grub_ssize_t lzx_decompress ( const void *data, grub_size_t len, void *buf );
grub_ssize_t xca_decompress ( const void *data, grub_size_t len, void *buf );

/* ---- function prototypes ---- */
wim_patch *ventoy_find_wim_patch(const char *path);
int ventoy_collect_wim_patch(const char *bcdfile);
grub_err_t ventoy_cmd_wim_patch_count(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_collect_wim_patch(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_dump_wim_patch(grub_extcmd_context_t ctxt, int argc, char **args);
int wim_name_cmp(const char *search, grub_uint16_t *name, grub_uint16_t namelen);
int ventoy_is_pe64(grub_uint8_t *buffer);
grub_err_t ventoy_cmd_is_pe64(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_sel_wimboot(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_wimdows_reset(grub_extcmd_context_t ctxt, int argc, char **args);
int ventoy_load_jump_exe(const char *path, grub_uint8_t **data, grub_uint32_t *size, wim_hash *hash);
int ventoy_get_override_info(grub_file_t file, wim_tail *wim_data);
int ventoy_read_resource(grub_file_t fp, wim_header *wimhdr, wim_resource_header *head, void **buffer);
wim_directory_entry * search_wim_dirent(wim_directory_entry *dir, const char *search_name);
wim_lookup_entry * ventoy_find_look_entry(wim_header *header, wim_lookup_entry *lookup, wim_hash *hash);
int parse_custom_setup_path(char *cmdline, const char **path, char *exefile);
wim_lookup_entry * ventoy_find_meta_entry(wim_header *header, wim_lookup_entry *lookup);
grub_uint64_t ventoy_get_stream_len(wim_directory_entry *dir);
int ventoy_update_stream_hash(wim_patch *patch, wim_directory_entry *dir);
int ventoy_update_all_hash(wim_patch *patch, void *meta_data, wim_directory_entry *dir);
int ventoy_cat_exe_file_data(wim_tail *wim_data, grub_uint32_t exe_len, grub_uint8_t *exe_data, int windatalen);
int ventoy_get_windows_rtdata_len(const char *iso, int *flag);
int ventoy_fill_windows_rtdata(void *buf, char *isopath, int dataflag);
int ventoy_update_before_chain(ventoy_os_param *param, char *isopath);
int ventoy_wimdows_locate_wim(const char *disk, wim_patch *patch, int windatalen);
grub_err_t ventoy_cmd_sel_winpe_wim(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_locate_wim_patch(grub_extcmd_context_t ctxt, int argc, char **args);
grub_uint32_t ventoy_get_override_chunk_num(void);
void ventoy_fill_suppress_wincd_override_data(void *override);
void ventoy_windows_fill_override_data_iso9660(    grub_uint64_t isosize, void *override);
void ventoy_windows_fill_override_data_udf(grub_file_t isofile, void *override);
grub_uint32_t ventoy_windows_get_virt_data_size(void);
void ventoy_windows_fill_virt_data(    grub_uint64_t isosize, ventoy_chain_head *chain);
int ventoy_windows_drive_map(ventoy_chain_head *chain, int vlnk);
int ventoy_suppress_windows_cd_prompt(void);
int ventoy_extract_init_exe(char *wimfile, grub_uint8_t **pexe_data, grub_uint32_t *pexe_len, char *exe_name);
grub_err_t ventoy_cmd_windows_wimboot_data(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_windows_chain_data(grub_extcmd_context_t ctxt, int argc, char **args);
grub_uint32_t ventoy_get_wim_iso_offset(const char *filepath);
int ventoy_get_wim_chunklist(grub_file_t wimfile, ventoy_img_chunk_list *wimchunk);
grub_err_t ventoy_cmd_is_standard_winiso(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_wim_check_bootable(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_vlnk_wim_chain_data(grub_file_t wimfile);
grub_err_t ventoy_normal_wim_chain_data(grub_file_t wimfile);
grub_err_t ventoy_cmd_wim_chain_data(grub_extcmd_context_t ctxt, int argc, char **args);
int ventoy_chain_file_size(const char *path);
int ventoy_chain_file_read(const char *path, int offset, int len, void *buf);

#endif /* __VENTOY_WINDOWS_PRIV_H__ */
