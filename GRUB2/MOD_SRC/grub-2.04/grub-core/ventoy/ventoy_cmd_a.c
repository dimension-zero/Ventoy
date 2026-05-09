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

    if (file == NULL)
    {
        debug("failed to open file <%s>\n", "ventoy_efiboot.img.xz");
        return 1;
    }

    len = (int)file->size;

    data = (char *)grub_malloc(file->size);
    if (!data)
    {
        return 1;
    }

    grub_file_read(file, data, file->size);
    grub_file_close(file);

    grub_snprintf(exec, sizeof(exec), "loopback efiboot mem:0x%llx:size:%d", (ulonglong)(ulong)data, len);
    grub_script_execute_sourcecode(exec);

    file = grub_file_open("(efiboot)/EFI/BOOT/BOOTX64.EFI", GRUB_FILE_TYPE_LINUX_INITRD);
    offset = (grub_uint32_t)grub_iso9660_get_last_file_dirent_pos(file);
    grub_file_close(file);

    grub_script_execute_sourcecode("loopback -d efiboot");

    *buf = data;
    *datalen = len;
    *direntoff = offset + 2;

    return 0;
}

int ventoy_set_check_result(int ret, const char *msg)
{
    char buf[32];

    grub_snprintf(buf, sizeof(buf), "%d", (ret & 0x7FFF));
    grub_env_set("VTOY_CHKDEV_RESULT_STRING", buf);
    grub_env_export("VTOY_CHKDEV_RESULT_STRING");

    if (ret)
    {
        grub_cls();
        grub_printf(VTOY_WARNING"\n");
        grub_printf(VTOY_WARNING"\n");
        grub_printf(VTOY_WARNING"\n\n\n");

        grub_printf("This is NOT a standard Ventoy device and is NOT supported (%d).\n", ret);
        grub_printf("Error message: <%s>\n\n", msg);
        grub_printf("You should follow the instructions in https://www.ventoy.net to use Ventoy.\n");
        grub_refresh();
    }

    return ret;
}

int ventoy_check_official_device(grub_device_t dev)
{
    int workaround = 0;
    grub_file_t file;
    grub_uint64_t offset;
    char devname[64];
    grub_fs_t fs;
    grub_uint8_t mbr[512];
    grub_disk_t disk;
    grub_device_t dev2;
    char *label = NULL;
    struct grub_partition *partition;

    if (dev->disk == NULL || dev->disk->partition == NULL)
    {
        return ventoy_set_check_result(1 | 0x1000, "Internal Error");
    }

    if (0 == ventoy_check_file_exist("(%s,2)/ventoy/ventoy.cpio", dev->disk->name) ||
        0 == ventoy_check_file_exist("(%s,2)/grub/localboot.cfg", dev->disk->name) ||
        0 == ventoy_check_file_exist("(%s,2)/tool/mount.exfat-fuse_aarch64", dev->disk->name))
    {
        #ifndef GRUB_MACHINE_EFI
        if (0 == ventoy_check_file_exist("(ventoydisk)/ventoy/ventoy.cpio", dev->disk->name))
        {
            return ventoy_set_check_result(2 | 0x1000, "File ventoy/ventoy.cpio missing in VTOYEFI partition");
        }
        else if (0 == ventoy_check_file_exist("(ventoydisk)/grub/localboot.cfg", dev->disk->name))
        {
            return ventoy_set_check_result(2 | 0x1000, "File grub/localboot.cfg missing in VTOYEFI partition");
        }
        else if (0 == ventoy_check_file_exist("(ventoydisk)/tool/mount.exfat-fuse_aarch64", dev->disk->name))
        {
            return ventoy_set_check_result(2 | 0x1000, "File tool/mount.exfat-fuse_aarch64 missing in VTOYEFI partition");
        }
        else
        {
            workaround = 1;
        }
        #endif
    }

    /* We must have partition 2 */
    if (workaround)
    {
        file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", "(ventoydisk)/ventoy/ventoy.cpio");
    }
    else
    {
        file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "(%s,2)/ventoy/ventoy.cpio", dev->disk->name);
    }
    if (!file)
    {
        return ventoy_set_check_result(3 | 0x1000, "File ventoy/ventoy.cpio open failed in VTOYEFI partition");
    }

    if (NULL == grub_strstr(file->fs->name, "fat"))
    {
        grub_file_close(file);
        return ventoy_set_check_result(4 | 0x1000, "VTOYEFI partition is not FAT filesystem");
    }

    partition = dev->disk->partition;
    if (partition->number != 0 || partition->start != 2048)
    {
        return ventoy_set_check_result(5, "Ventoy partition is not start at 1MB");
    }

    if (workaround)
    {
        if (grub_strncmp(g_ventoy_part_info->Head.Signature, "EFI PART", 8) == 0)
        {
            ventoy_gpt_part_tbl *PartTbl = g_ventoy_part_info->PartTbl;
            if (PartTbl[1].StartLBA != PartTbl[0].LastLBA + 1 ||
                (PartTbl[1].LastLBA + 1 - PartTbl[1].StartLBA) != 65536)
            {
                grub_file_close(file);
                return ventoy_set_check_result(6, "Disk partition layout check failed.");
            }
        }
        else
        {
            ventoy_part_table *PartTbl = g_ventoy_part_info->MBR.PartTbl;
            if (PartTbl[1].StartSectorId != PartTbl[0].StartSectorId + PartTbl[0].SectorCount ||
                PartTbl[1].SectorCount != 65536)
            {
                grub_file_close(file);
                return ventoy_set_check_result(6, "Disk partition layout check failed.");
            }
        }
    }
    else
    {
        offset = partition->start + partition->len;
        partition = file->device->disk->partition;
        if ((partition->number != 1) || (partition->len != 65536) || (offset != partition->start))
        {
            grub_file_close(file);
            return ventoy_set_check_result(7, "Disk partition layout check failed.");
        }
    }

    grub_file_close(file);

    if (workaround == 0)
    {
        grub_snprintf(devname, sizeof(devname), "%s,2", dev->disk->name);
        dev2 = grub_device_open(devname);
        if (!dev2)
        {
            return ventoy_set_check_result(8, "Disk open failed");
        }

        fs = grub_fs_probe(dev2);
        if (!fs)
        {
            grub_device_close(dev2);
            return ventoy_set_check_result(9, "FS probe failed");
        }

        fs->fs_label(dev2, &label);
        if ((!label) || grub_strncmp("VTOYEFI", label, 7))
        {
            grub_device_close(dev2);
            return ventoy_set_check_result(10, "Partition name is not VTOYEFI");
        }

        grub_device_close(dev2);
    }

    /* MBR check */
    disk = grub_disk_open(dev->disk->name);
    if (!disk)
    {
        return ventoy_set_check_result(11, "Disk open failed");
    }

    grub_memset(mbr, 0, 512);
    grub_disk_read(disk, 0, 0, 512, mbr);
    grub_disk_close(disk);

    if (grub_memcmp(g_check_mbr_data, mbr, 0x30) || grub_memcmp(g_check_mbr_data + 0x30, mbr + 0x190, 16))
    {
        return ventoy_set_check_result(12, "MBR check failed");
    }

    return ventoy_set_check_result(0, NULL);
}

int ventoy_check_ignore_flag(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    if (0 == info->dir)
    {
        if (filename && filename[0] == '.' && 0 == grub_strncmp(filename, ".ventoyignore", 13))
        {
            *((int *)data) = 1;
            return 0;
        }
    }

    return 0;
}

grub_uint64_t ventoy_grub_get_file_size(const char *fmt, ...)
{
    grub_uint64_t size = 0;
    grub_file_t file;
    va_list ap;
    char fullpath[256] = {0};

    va_start (ap, fmt);
    grub_vsnprintf(fullpath, 255, fmt, ap);
    va_end (ap);

    file = grub_file_open(fullpath, VENTOY_FILE_TYPE);
    if (!file)
    {
        debug("grub_file_open failed <%s>\n", fullpath);
        grub_errno = 0;
        return 0;
    }

    size = file->size;
    grub_file_close(file);
    return size;
}

grub_file_t ventoy_grub_file_open(enum grub_file_type type, const char *fmt, ...)
{
    va_list ap;
    grub_file_t file;
    char fullpath[512] = {0};

    va_start (ap, fmt);
    grub_vsnprintf(fullpath, 511, fmt, ap);
    va_end (ap);

    file = grub_file_open(fullpath, type);
    if (!file)
    {
        debug("grub_file_open failed <%s> %d\n", fullpath, grub_errno);
        grub_errno = 0;
    }

    return file;
}

int ventoy_is_dir_exist(const char *fmt, ...)
{
    va_list ap;
    int len;
    char *pos = NULL;
    char buf[512] = {0};

    grub_snprintf(buf, sizeof(buf), "%s", "[ -d \"");
    pos = buf + 6;

    va_start (ap, fmt);
    len = grub_vsnprintf(pos, 511, fmt, ap);
    va_end (ap);

    grub_strncpy(pos + len, "\" ]", 3);

    debug("script exec %s\n", buf);

    if (0 == grub_script_execute_sourcecode(buf))
    {
        return 1;
    }

    return 0;
}

int ventoy_gzip_compress(void *mem_in, int mem_in_len, void *mem_out, int mem_out_len)
{
	mz_stream s;
    grub_uint8_t *outbuf;
    grub_uint8_t gzHdr[10] =
    {
		0x1F, 0x8B,	/* magic */
		8,		    /* z method */
		0,		    /* flags */
		0,0,0,0,	/* mtime */
		4,		    /* xfl */
		3,		    /* OS */
	};

	grub_memset(&s, 0, sizeof(mz_stream));

    mz_deflateInit2(&s, 1, MZ_DEFLATED, -MZ_DEFAULT_WINDOW_BITS, 6, MZ_DEFAULT_STRATEGY);

    outbuf = (grub_uint8_t *)mem_out;

    mem_out_len -= sizeof(gzHdr) + 8;
    grub_memcpy(outbuf, gzHdr, sizeof(gzHdr));
    outbuf += sizeof(gzHdr);

    s.avail_in = mem_in_len;
    s.next_in = mem_in;

    s.avail_out = mem_out_len;
    s.next_out = outbuf;

    mz_deflate(&s, MZ_FINISH);

    mz_deflateEnd(&s);

    outbuf += s.total_out;
    *(grub_uint32_t *)outbuf = grub_getcrc32c(0, outbuf, s.total_out);
    *(grub_uint32_t *)(outbuf + 4) = (grub_uint32_t)(s.total_out);

    return s.total_out + sizeof(gzHdr) + 8;
}


#if 0
ventoy grub cmds
#endif

