/******************************************************************************
 * ventoy_cmd.c
 *
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
#include <grub/misc.h>
#include <grub/kernel.h>
#ifdef GRUB_MACHINE_EFI
#include <grub/efi/api.h>
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

GRUB_MOD_LICENSE ("GPLv3+");

grub_uint8_t g_check_mbr_data[] = {
    0xEB, 0x63, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x56, 0x54, 0x00, 0x47, 0x65, 0x00, 0x48, 0x44, 0x00, 0x52, 0x64, 0x00, 0x20, 0x45, 0x72, 0x0D,
};

initrd_info *g_initrd_img_list = NULL;
initrd_info *g_initrd_img_tail = NULL;
int g_initrd_img_count = 0;
int g_valid_initrd_count = 0;
int g_default_menu_mode = 0;
int g_filt_dot_underscore_file = 0;
int g_filt_trash_dir = 1;
int g_sort_case_sensitive = 0;
int g_tree_view_menu_style = 0;
grub_file_t g_old_file;
int g_ventoy_last_entry_back;

char g_iso_path[256];
char g_img_swap_tmp_buf[1024];
img_info g_img_swap_tmp;
img_info *g_ventoy_img_list = NULL;

int g_ventoy_img_count = 0;

grub_device_t g_enum_dev = NULL;
grub_fs_t g_enum_fs = NULL;
int g_img_max_search_level = -1;
img_iterator_node g_img_iterator_head;
img_iterator_node *g_img_iterator_tail = NULL;

grub_uint8_t g_ventoy_break_level = 0;
grub_uint8_t g_ventoy_debug_level = 0;
grub_uint8_t g_ventoy_chain_type = 0;

grub_uint8_t *g_ventoy_cpio_buf = NULL;
grub_uint32_t g_ventoy_cpio_size = 0;
cpio_newc_header *g_ventoy_initrd_head = NULL;
grub_uint8_t *g_ventoy_runtime_buf = NULL;

int g_plugin_image_list = 0;

ventoy_grub_param *g_grub_param = NULL;

ventoy_guid  g_ventoy_guid = VENTOY_GUID;

ventoy_img_chunk_list g_img_chunk_list;

int g_wimboot_enable = 0;
ventoy_img_chunk_list g_wimiso_chunk_list;
char *g_wimiso_path = NULL;
grub_uint32_t g_wimiso_size = 0;

int g_vhdboot_enable = 0;

grub_uint64_t g_svd_replace_offset = 0;

int g_conf_replace_count = 0;
grub_uint64_t g_conf_replace_offset[VTOY_MAX_CONF_REPLACE] = { 0 };
conf_replace *g_conf_replace_node[VTOY_MAX_CONF_REPLACE] = { NULL };
grub_uint8_t *g_conf_replace_new_buf[VTOY_MAX_CONF_REPLACE] = { NULL };
int g_conf_replace_new_len[VTOY_MAX_CONF_REPLACE] = { 0 };
int g_conf_replace_new_len_align[VTOY_MAX_CONF_REPLACE] = { 0 };

int g_ventoy_disk_bios_id = 0;
ventoy_gpt_info *g_ventoy_part_info = NULL;
grub_uint64_t g_ventoy_disk_size = 0;
grub_uint64_t g_ventoy_disk_part_size[2];

char *g_tree_script_buf = NULL;
int g_tree_script_pos = 0;
int g_tree_script_pre = 0;

char *g_list_script_buf = NULL;
int g_list_script_pos = 0;

char *g_part_list_buf = NULL;
int g_part_list_pos = 0;
grub_uint64_t g_part_end_max = 0;

int g_video_mode_max = 0;
int g_video_mode_num = 0;
ventoy_video_mode *g_video_mode_list = NULL;

int g_enumerate_time_checked = 0;
grub_uint64_t g_enumerate_start_time_ms;
grub_uint64_t g_enumerate_finish_time_ms;
int g_vtoy_file_flt[VTOY_FILE_FLT_BUTT] = {0};

char g_iso_vd_id_publisher[130];
char g_iso_vd_id_prepare[130];
char g_iso_vd_id_application[130];

int g_pager_flag = 0;
char g_old_pager[32];

const char *g_menu_class[img_type_max] =
{
    "vtoyiso", "vtoywim", "vtoyefi", "vtoyimg", "vtoyvhd", "vtoyvtoy"
};

const char *g_menu_prefix[img_type_max] =
{
    "iso", "wim", "efi", "img", "vhd", "vtoy"
};

const char *g_lower_chksum_name[VTOY_CHKSUM_NUM] = { "md5", "sha1", "sha256", "sha512" };
int g_lower_chksum_namelen[VTOY_CHKSUM_NUM] = { 3, 4, 6, 6 };
int g_chksum_retlen[VTOY_CHKSUM_NUM] = { 32, 40, 64, 128 };

int g_vtoy_secondary_need_recover = 0;

int g_vtoy_load_prompt = 0;
char g_vtoy_prompt_msg[64];

char g_json_case_mis_path[32];

ventoy_vlnk_part *g_vlnk_part_list = NULL;

    else if (0 == grub_strcmp(args[1], "le"))
    {
        grub_errno = (value_long1 <= value_long2) ? GRUB_ERR_NONE : GRUB_ERR_TEST_FAILURE;
    }
    else
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s {Int1} { eq ne gt lt ge le } {Int2}", cmd_raw_name);
    }

    return grub_errno;
}

grub_err_t ventoy_cmd_device(grub_extcmd_context_t ctxt, int argc, char **args)
{
    char *pos = NULL;
    char buf[128] = {0};

    if (argc != 2)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s path var", cmd_raw_name);
    }

    grub_strncpy(buf, (args[0][0] == '(') ? args[0] + 1 : args[0], sizeof(buf) - 1);
    pos = grub_strstr(buf, ",");
    if (pos)
    {
        *pos = 0;
    }

    grub_env_set(args[1], buf);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_check_compatible(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    char buf[256];
    grub_disk_t disk;
    char *pos = NULL;
    const char *files[] = { "ventoy.dat", "VENTOY.DAT" };

    (void)ctxt;

    if (argc != 1)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s  (loop)", cmd_raw_name);
    }

    for (i = 0; i < (int)ARRAY_SIZE(files); i++)
    {
        grub_snprintf(buf, sizeof(buf) - 1, "[ -e \"%s/%s\" ]", args[0], files[i]);
        if (0 == grub_script_execute_sourcecode(buf))
        {
            debug("file %s exist, ventoy_compatible YES\n", buf);
            grub_env_set("ventoy_compatible", "YES");
            VENTOY_CMD_RETURN(GRUB_ERR_NONE);
        }
        else
        {
            debug("file %s NOT exist\n", buf);
        }
    }

    grub_snprintf(buf, sizeof(buf) - 1, "%s", args[0][0] == '(' ? (args[0] + 1) : args[0]);
    pos = grub_strstr(buf, ")");
    if (pos)
    {
        *pos = 0;
    }

    disk = grub_disk_open(buf);
    if (disk)
    {
        grub_disk_read(disk, 16 << 2, 0, 1024, g_img_swap_tmp_buf);
        grub_disk_close(disk);

        g_img_swap_tmp_buf[703] = 0;
        for (i = 318; i < 703; i++)
        {
            if (g_img_swap_tmp_buf[i] == 'V' &&
                0 == grub_strncmp(g_img_swap_tmp_buf + i, VENTOY_COMPATIBLE_STR, VENTOY_COMPATIBLE_STR_LEN))
            {
                debug("Ventoy compatible string exist at  %d, ventoy_compatible YES\n", i);
                grub_env_set("ventoy_compatible", "YES");
                VENTOY_CMD_RETURN(GRUB_ERR_NONE);
            }
        }
    }
    else
    {
        debug("failed to open disk <%s>\n", buf);
    }

    grub_env_set("ventoy_compatible", "NO");
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

int ventoy_cmp_img(img_info *img1, img_info *img2)
{
    char *s1, *s2;
    int c1 = 0;
    int c2 = 0;

    if (g_plugin_image_list == VENTOY_IMG_WHITE_LIST)
    {
        return (img1->plugin_list_index - img2->plugin_list_index);
    }

    for (s1 = img1->name, s2 = img2->name; *s1 && *s2; s1++, s2++)
    {
        c1 = *s1;
        c2 = *s2;

        if (0 == g_sort_case_sensitive)
        {
            if (grub_islower(c1))
            {
                c1 = c1 - 'a' + 'A';
            }

            if (grub_islower(c2))
            {
                c2 = c2 - 'a' + 'A';
            }
        }

        if (c1 != c2)
        {
            break;
        }
    }

    return (c1 - c2);
}

int ventoy_cmp_subdir(img_iterator_node *node1, img_iterator_node *node2)
{
    int i = 0;
    int c1 = 0;
    int c2 = 0;
    int len = 0;
    char *s1, *s2;

    if (g_plugin_image_list == VENTOY_IMG_WHITE_LIST)
    {
        return (node1->plugin_list_index - node2->plugin_list_index);
    }

    s1 = node1->dir;
    s2 = node2->dir;
    len = grub_min(node1->dirlen, node2->dirlen);

    for (i = 0; i < len - 1; i++)
    {
        c1 = *s1++;
        c2 = *s2++;

        if (0 == g_sort_case_sensitive)
        {
            if (grub_islower(c1))
            {
                c1 = c1 - 'a' + 'A';
            }

            if (grub_islower(c2))
            {
                c2 = c2 - 'a' + 'A';
            }
        }

        if (c1 != c2)
        {
            return (c1 - c2);
        }
    }

    if (len == node1->dirlen)
    {
        c1 = 0;
    }

    if (len == node2->dirlen)
    {
        c2 = 0;
    }

    return (c1 - c2);
}

void ventoy_swap_img(img_info *img1, img_info *img2)
{
    grub_memcpy(&g_img_swap_tmp, img1, sizeof(img_info));

    grub_memcpy(img1, img2, sizeof(img_info));
    img1->next = g_img_swap_tmp.next;
    img1->prev = g_img_swap_tmp.prev;

    g_img_swap_tmp.next = img2->next;
    g_img_swap_tmp.prev = img2->prev;
    grub_memcpy(img2, &g_img_swap_tmp, sizeof(img_info));
}

int ventoy_img_name_valid(const char *filename, grub_size_t namelen)
{
    (void)namelen;

    if (g_filt_dot_underscore_file && filename[0] == '.' && filename[1] == '_')
    {
        return 0;
    }

    return 1;
}

int ventoy_vlnk_iterate_partition(struct grub_disk *disk, const grub_partition_t partition, void *data)
{
    ventoy_vlnk_part *node = NULL;
    grub_uint32_t SelfSig;
    grub_uint32_t *pSig = (grub_uint32_t *)data;

    /* skip Ventoy partition 1/2 */
    grub_memcpy(&SelfSig, g_ventoy_part_info->MBR.BootCode + 0x1b8, 4);
    if (partition->number < 2 && SelfSig == *pSig)
    {
        return 0;
    }

    node = grub_zalloc(sizeof(ventoy_vlnk_part));
    if (node)
    {
        node->disksig = *pSig;
        node->partoffset = (partition->start << GRUB_DISK_SECTOR_BITS);
        grub_snprintf(node->disk, sizeof(node->disk) - 1, "%s", disk->name);
        grub_snprintf(node->device, sizeof(node->device) - 1, "%s,%d", disk->name, partition->number + 1);

        node->next = g_vlnk_part_list;
        g_vlnk_part_list = node;
    }

    return 0;
}

int ventoy_vlnk_iterate_disk(const char *name, void *data)
{
    grub_disk_t disk;
    grub_uint32_t sig;

    (void)data;

    disk = grub_disk_open(name);
    if (disk)
    {
        grub_disk_read(disk, 0, 0x1b8, 4, &sig);
        grub_partition_iterate(disk, ventoy_vlnk_iterate_partition, &sig);
        grub_disk_close(disk);
    }

    return 0;
}

int ventoy_vlnk_probe_fs(ventoy_vlnk_part *cur)
{
    const char *fs[ventoy_fs_max + 1] =
    {
        "exfat", "ntfs", "ext2", "xfs", "udf", "fat", "btrfs", NULL
    };

    if (!cur->dev)
    {
        cur->dev = grub_device_open(cur->device);
    }

    if (cur->dev)
    {
        cur->fs = grub_fs_list_probe(cur->dev, fs);
    }

    return 0;
}

int ventoy_check_vlnk_data(ventoy_vlnk *vlnk, int print, char *dst, int size)
{
    int diskfind = 0;
    int partfind = 0;
    int filefind = 0;
    char *disk, *device;
    grub_uint32_t readcrc, calccrc;
    ventoy_vlnk_part *cur;
    grub_fs_t fs = NULL;

    if (grub_memcmp(&(vlnk->guid), &g_ventoy_guid, sizeof(ventoy_guid)))
    {
        if (print)
        {
            grub_printf("VLNK invalid guid\n");
            grub_refresh();
        }
        return 1;
    }

    readcrc = vlnk->crc32;
    vlnk->crc32 = 0;
    calccrc = grub_getcrc32c(0, vlnk, sizeof(ventoy_vlnk));
    if (readcrc != calccrc)
    {
        if (print)
        {
            grub_printf("VLNK invalid crc 0x%08x 0x%08x\n", calccrc, readcrc);
            grub_refresh();
        }
        return 1;
    }

    if (!g_vlnk_part_list)
    {
        grub_disk_dev_iterate(ventoy_vlnk_iterate_disk, NULL);
    }

    for (cur = g_vlnk_part_list; cur && filefind == 0; cur = cur->next)
    {
        if (cur->disksig == vlnk->disk_signature)
        {
            diskfind = 1;
            disk = cur->disk;
            if (cur->partoffset == vlnk->part_offset)
            {
                partfind = 1;
                device = cur->device;

                if (cur->probe == 0)
                {
                    cur->probe = 1;
                    ventoy_vlnk_probe_fs(cur);
                }

                if (!fs)
                {
                    fs = cur->fs;
                }

                if (cur->fs)
                {
                    struct grub_file file;

                    grub_memset(&file, 0, sizeof(file));
                    file.device = cur->dev;
                    if (cur->fs->fs_open(&file, vlnk->filepath) == GRUB_ERR_NONE)
                    {
                        filefind = 1;
                        cur->fs->fs_close(&file);
                        grub_snprintf(dst, size - 1, "(%s)%s", cur->device, vlnk->filepath);
                    }
                    else
                    {
                        grub_errno = 0;
                    }
                }
            }
        }
    }

    if (print)
    {
        grub_printf("\n==== VLNK Information ====\n"
                    "Disk Signature: %08x\n"
                    "Partition Offset: %llu\n"
                    "File Path: <%s>\n\n",
                    vlnk->disk_signature, (ulonglong)vlnk->part_offset, vlnk->filepath);

        if (diskfind)
        {
            grub_printf("Disk Find: [ YES ] [ %s ]\n", disk);
        }
        else
        {
            grub_printf("Disk Find: [ NO ]\n");
        }

        if (partfind)
        {
            grub_printf("Part Find: [ YES ] [ %s ] [ %s ]\n", device, fs ? fs->name : "N/A");
        }
        else
        {
            grub_printf("Part Find: [ NO ]\n");
        }
        grub_printf("File Find: [ %s ]\n", filefind ? "YES" : "NO");
        if (filefind)
        {
            grub_printf("VLNK File: <%s>\n", dst);
        }

        grub_printf("\n");
        grub_refresh();
    }

    return (1 - filefind);
}

int ventoy_add_vlnk_file(char *dir, const char *name)
{
    int rc = 1;
    char src[512];
    char dst[512];
    grub_file_t file = NULL;
    ventoy_vlnk vlnk;

    if (!dir)
    {
        grub_snprintf(src, sizeof(src), "%s%s", g_iso_path, name);
    }
    else if (dir[0] == '/')
    {
        grub_snprintf(src, sizeof(src), "%s%s%s", g_iso_path, dir, name);
    }
    else
    {
        grub_snprintf(src, sizeof(src), "%s/%s%s", g_iso_path, dir, name);
    }

    file = grub_file_open(src, VENTOY_FILE_TYPE);
    if (!file)
    {
        return 1;
    }

    grub_memset(&vlnk, 0, sizeof(vlnk));
    grub_file_read(file, &vlnk, sizeof(vlnk));
    grub_file_close(file);

    if (ventoy_check_vlnk_data(&vlnk, 0, dst, sizeof(dst)) == 0)
    {
        rc = grub_file_add_vlnk(src, dst);
    }

    return rc;
}

int ventoy_collect_img_files(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    //int i = 0;
    int type = 0;
    int ignore = 0;
    int index = 0;
    int vlnk = 0;
    grub_size_t len;
    img_info *img;
    img_info *tail;
    const menu_tip *tip;
    img_iterator_node *tmp;
    img_iterator_node *new_node;
    img_iterator_node *node = (img_iterator_node *)data;

    if (g_enumerate_time_checked == 0)
    {
        g_enumerate_finish_time_ms = grub_get_time_ms();
        if ((g_enumerate_finish_time_ms - g_enumerate_start_time_ms) >= 3000)
        {
            grub_cls();
            grub_printf("\n\n Ventoy scanning files, please wait...\n");
            grub_refresh();
            g_enumerate_time_checked = 1;
        }
    }

    len = grub_strlen(filename);

    if (info->dir)
    {
        if (node->level + 1 > g_img_max_search_level)
        {
            return 0;
        }

        if ((len == 1 && filename[0] == '.') ||
            (len == 2 && filename[0] == '.' && filename[1] == '.'))
        {
            return 0;
        }

        if (!ventoy_img_name_valid(filename, len))
        {
            return 0;
        }

        if (g_filt_trash_dir)
        {
            if (0 == grub_strncmp(filename, ".trash-", 7) ||
                0 == grub_strcmp(filename, ".Trashes") ||
                0 == grub_strncmp(filename, "$RECYCLE.BIN", 12))
            {
                return 0;
            }
        }

        if (g_plugin_image_list == VENTOY_IMG_WHITE_LIST)
        {
            grub_snprintf(g_img_swap_tmp_buf, sizeof(g_img_swap_tmp_buf), "%s%s/", node->dir, filename);
            index = ventoy_plugin_get_image_list_index(vtoy_class_directory, g_img_swap_tmp_buf);
            if (index == 0)
            {
                debug("Directory %s not found in image_list plugin config...\n", g_img_swap_tmp_buf);
                return 0;
            }
        }

        new_node = grub_zalloc(sizeof(img_iterator_node));
        if (new_node)
        {
            new_node->level = node->level + 1;
            new_node->plugin_list_index = index;
            new_node->dirlen = grub_snprintf(new_node->dir, sizeof(new_node->dir), "%s%s/", node->dir, filename);

            g_enum_fs->fs_dir(g_enum_dev, new_node->dir, ventoy_check_ignore_flag, &ignore);
            if (ignore)
            {
                debug("Directory %s ignored...\n", new_node->dir);
                grub_free(new_node);
                return 0;
            }

            new_node->tail = node->tail;

            new_node->parent = node;
            if (!node->firstchild)
            {
                node->firstchild = new_node;
            }

            if (g_img_iterator_tail)
            {
                g_img_iterator_tail->next = new_node;
                g_img_iterator_tail = new_node;
            }
            else
            {
                g_img_iterator_head.next = new_node;
                g_img_iterator_tail = new_node;
            }
        }
    }
    else
    {
        debug("Find a file %s\n", filename);
        if (len < 4)
        {
            return 0;
        }

        if (FILE_FLT(ISO) && 0 == grub_strcasecmp(filename + len - 4, ".iso"))
        {
            type = img_type_iso;
        }
        else if (FILE_FLT(WIM) && g_wimboot_enable && (0 == grub_strcasecmp(filename + len - 4, ".wim")))
        {
            type = img_type_wim;
        }
        else if (FILE_FLT(VHD) && g_vhdboot_enable && (0 == grub_strcasecmp(filename + len - 4, ".vhd") ||
                (len >= 5 && 0 == grub_strcasecmp(filename + len - 5, ".vhdx"))))
        {
            type = img_type_vhd;
        }
        #ifdef GRUB_MACHINE_EFI
        else if (FILE_FLT(EFI) && 0 == grub_strcasecmp(filename + len - 4, ".efi"))
        {
            type = img_type_efi;
        }
        #endif
        else if (FILE_FLT(IMG) && 0 == grub_strcasecmp(filename + len - 4, ".img"))
        {
            if (len == 18 && grub_strncmp(filename, "ventoy_", 7) == 0)
            {
                if (grub_strncmp(filename + 7, "wimboot", 7) == 0 ||
                    grub_strncmp(filename + 7, "vhdboot", 7) == 0)
                {
                    return 0;
                }
            }
            type = img_type_img;
        }
        else if (FILE_FLT(VTOY) && len >= 5 && 0 == grub_strcasecmp(filename + len - 5, ".vtoy"))
        {
            type = img_type_vtoy;
        }
        else if (len >= 9 && 0 == grub_strcasecmp(filename + len - 5, ".vcfg"))
        {
            if (filename[len - 9] == '.' || (len >= 10 && filename[len - 10] == '.'))
            {
                grub_snprintf(g_img_swap_tmp_buf, sizeof(g_img_swap_tmp_buf), "%s%s", node->dir, filename);
                ventoy_plugin_add_custom_boot(g_img_swap_tmp_buf);
            }
            return 0;
        }
        else
        {
            return 0;
        }

        if (g_filt_dot_underscore_file && filename[0] == '.' && filename[1] == '_')
        {
            return 0;
        }

        if (g_plugin_image_list)
        {
            grub_snprintf(g_img_swap_tmp_buf, sizeof(g_img_swap_tmp_buf), "%s%s", node->dir, filename);
            index = ventoy_plugin_get_image_list_index(vtoy_class_image_file, g_img_swap_tmp_buf);
            if (VENTOY_IMG_WHITE_LIST == g_plugin_image_list && index == 0)
            {
                debug("File %s not found in image_list plugin config...\n", g_img_swap_tmp_buf);
                return 0;
            }
            else if (VENTOY_IMG_BLACK_LIST == g_plugin_image_list && index > 0)
            {
                debug("File %s found in image_blacklist plugin config %d ...\n", g_img_swap_tmp_buf, index);
                return 0;
            }
        }

        if (info->size == VTOY_FILT_MIN_FILE_SIZE || info->size == 0)
        {
            if (grub_file_is_vlnk_suffix(filename, len))
            {
                vlnk = 1;
                if (ventoy_add_vlnk_file(node->dir, filename) != 0)
                {
                    return 0;
                }
            }
        }

        img = grub_zalloc(sizeof(img_info));
        if (img)
        {
            img->type = type;
            img->plugin_list_index = index;
            grub_snprintf(img->name, sizeof(img->name), "%s", filename);

            img->pathlen = grub_snprintf(img->path, sizeof(img->path), "%s%s", node->dir, img->name);

            img->size = info->size;
            if (vlnk || 0 == img->size)
            {
                if (node->dir[0] == '/')
                {
                    img->size = ventoy_grub_get_file_size("%s%s%s", g_iso_path, node->dir, filename);
                }
                else
                {
                    img->size = ventoy_grub_get_file_size("%s/%s%s", g_iso_path, node->dir, filename);
                }
            }

            if (img->size < VTOY_FILT_MIN_FILE_SIZE)
            {
                debug("img <%s> size too small %llu\n", img->name, (ulonglong)img->size);
                grub_free(img);
                return 0;
            }

            if (g_ventoy_img_list)
            {
                tail = *(node->tail);
                img->prev = tail;
                tail->next = img;
            }
            else
            {
                g_ventoy_img_list = img;
            }

            img->id = g_ventoy_img_count;
            img->parent = node;
            if (node && NULL == node->firstiso)
            {
                node->firstiso = img;
            }

            node->isocnt++;
            tmp = node->parent;
            while (tmp)
            {
                tmp->isocnt++;
                tmp = tmp->parent;
            }

            *((img_info **)(node->tail)) = img;
            g_ventoy_img_count++;

            img->alias = ventoy_plugin_get_menu_alias(vtoy_alias_image_file, img->path);

            tip = ventoy_plugin_get_menu_tip(vtoy_tip_image_file, img->path);
            if (tip)
            {
                img->tip1 = tip->tip1;
                img->tip2 = tip->tip2;
            }

            img->class = ventoy_plugin_get_menu_class(vtoy_class_image_file, img->name, img->path);
            if (!img->class)
            {
                img->class = g_menu_class[type];
            }
            img->menu_prefix = g_menu_prefix[type];

            if (img_type_iso == type)
            {
                if (ventoy_plugin_check_memdisk(img->path))
                {
                    img->menu_prefix = "miso";
                }
            }
            else if (img_type_img == type)
            {
                if (ventoy_plugin_check_memdisk(img->path))
                {
                    img->menu_prefix = "mimg";
                }
            }

            debug("Add %s%s to list %d\n", node->dir, filename, g_ventoy_img_count);
        }
    }

    return 0;
}

int ventoy_fill_data(grub_uint32_t buflen, char *buffer)
{
    int len = GRUB_UINT_MAX;
    const char *value = NULL;
    char name[32] = {0};
    char plat[32] = {0};
    char guidstr[32] = {0};
    ventoy_guid guid = VENTOY_GUID;
    const char *fmt1 = NULL;
    const char *fmt2 = NULL;
    const char *fmt3 = NULL;
    grub_uint32_t *puint = (grub_uint32_t *)name;
    grub_uint32_t *puint2 = (grub_uint32_t *)plat;
    const char fmtdata[]={ 0x39, 0x35, 0x25, 0x00, 0x35, 0x00, 0x23, 0x30, 0x30, 0x30, 0x30, 0x66, 0x66, 0x00 };
    const char fmtcode[]={
        0x22, 0x0A, 0x2B, 0x20, 0x68, 0x62, 0x6F, 0x78, 0x20, 0x7B, 0x0A, 0x20, 0x20, 0x74, 0x6F, 0x70,
        0x20, 0x3D, 0x20, 0x25, 0x73, 0x0A, 0x20, 0x20, 0x6C, 0x65, 0x66, 0x74, 0x20, 0x3D, 0x20, 0x25,
        0x73, 0x0A, 0x20, 0x20, 0x2B, 0x20, 0x6C, 0x61, 0x62, 0x65, 0x6C, 0x20, 0x7B, 0x74, 0x65, 0x78,
        0x74, 0x20, 0x3D, 0x20, 0x22, 0x25, 0x73, 0x20, 0x25, 0x73, 0x25, 0x73, 0x22, 0x20, 0x63, 0x6F,
        0x6C, 0x6F, 0x72, 0x20, 0x3D, 0x20, 0x22, 0x25, 0x73, 0x22, 0x20, 0x61, 0x6C, 0x69, 0x67, 0x6E,
        0x20, 0x3D, 0x20, 0x22, 0x6C, 0x65, 0x66, 0x74, 0x22, 0x7D, 0x0A, 0x7D, 0x0A, 0x22, 0x00
    };

    grub_memset(name, 0, sizeof(name));
    puint[0] = grub_swap_bytes32(0x56454e54);
    puint[3] = grub_swap_bytes32(0x4f4e0000);
    puint[2] = grub_swap_bytes32(0x45525349);
    puint[1] = grub_swap_bytes32(0x4f595f56);
    value = ventoy_get_env(name);

    grub_memset(name, 0, sizeof(name));
    puint[1] = grub_swap_bytes32(0x5f544f50);
    puint[0] = grub_swap_bytes32(0x56544c45);
    fmt1 = ventoy_get_env(name);
    if (!fmt1)
    {
        fmt1 = fmtdata;
    }

    grub_memset(name, 0, sizeof(name));
    puint[1] = grub_swap_bytes32(0x5f4c4654);
    puint[0] = grub_swap_bytes32(0x56544c45);
    fmt2 = ventoy_get_env(name);

    grub_memset(name, 0, sizeof(name));
    puint[1] = grub_swap_bytes32(0x5f434c52);
    puint[0] = grub_swap_bytes32(0x56544c45);
    fmt3 = ventoy_get_env(name);

    grub_memcpy(guidstr, &guid, sizeof(guid));

    puint2[0] = grub_swap_bytes32(g_ventoy_plat_data);

    /* Easter egg :) It will be appreciated if you reserve it, but NOT mandatory. */
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wformat-nonliteral"
    len = grub_snprintf(buffer, buflen, fmtcode,
                        fmt1 ? fmt1 : fmtdata,
                        fmt2 ? fmt2 : fmtdata + 4,
                        value ? value : "", plat, guidstr,
                        fmt3 ? fmt3 : fmtdata + 6);
    #pragma GCC diagnostic pop

    grub_memset(name, 0, sizeof(name));
    puint[0] = grub_swap_bytes32(0x76746f79);
    puint[2] = grub_swap_bytes32(0x656e7365);
    puint[1] = grub_swap_bytes32(0x5f6c6963);
    ventoy_set_env(name, guidstr);

    return len;
}
