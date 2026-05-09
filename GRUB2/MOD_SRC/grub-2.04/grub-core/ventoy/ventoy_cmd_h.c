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

grub_err_t ventoy_cmd_img_hook_root(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    ventoy_env_hook_root(1);

    return 0;
}

grub_err_t ventoy_cmd_img_unhook_root(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    ventoy_env_hook_root(0);

    return 0;
}

#ifdef GRUB_MACHINE_EFI
grub_err_t ventoy_cmd_check_secureboot_var(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int ret = 1;
    grub_uint8_t *var;
    grub_size_t size;
    grub_efi_guid_t global = GRUB_EFI_GLOBAL_VARIABLE_GUID;

    (void)ctxt;
    (void)argc;
    (void)args;

    var = grub_efi_get_variable("SecureBoot", &global, &size);
    if (var && *var == 1)
    {
        return 0;
    }

    return ret;
}
#else
grub_err_t ventoy_cmd_check_secureboot_var(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;
    return 1;
}
#endif

grub_err_t ventoy_cmd_img_check_range(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    int ret = 1;
    grub_file_t file;
    grub_uint64_t FileSectors = 0;
    ventoy_gpt_info *gpt = NULL;
    ventoy_part_table *pt = NULL;
    grub_uint8_t zeroguid[16] = {0};

    (void)ctxt;
    (void)argc;

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (!file)
    {
        debug("failed to open file %s\n", args[0]);
        return 1;
    }

    if (file->size % 512)
    {
        debug("unaligned file size: %llu\n", (ulonglong)file->size);
        goto out;
    }

    gpt = grub_zalloc(sizeof(ventoy_gpt_info));
    if (!gpt)
    {
        goto out;
    }

    FileSectors = file->size / 512;

    grub_file_read(file, gpt, sizeof(ventoy_gpt_info));
    if (grub_strncmp(gpt->Head.Signature, "EFI PART", 8) == 0)
    {
        debug("This is EFI partition table\n");

        for (i = 0; i < 128; i++)
        {
            if (grub_memcmp(gpt->PartTbl[i].PartGuid, zeroguid, 16))
            {
                if (FileSectors < gpt->PartTbl[i].LastLBA)
                {
                    debug("out of range: part[%d] LastLBA:%llu FileSectors:%llu\n", i,
                        (ulonglong)gpt->PartTbl[i].LastLBA, (ulonglong)FileSectors);
                    goto out;
                }
            }
        }
    }
    else
    {
        debug("This is MBR partition table\n");

        for (i = 0; i < 4; i++)
        {
            pt = gpt->MBR.PartTbl + i;
            if (FileSectors < pt->StartSectorId + pt->SectorCount)
            {
                debug("out of range: part[%d] LastLBA:%llu FileSectors:%llu\n", i,
                       (ulonglong)(pt->StartSectorId + pt->SectorCount),
                       (ulonglong)FileSectors);
                goto out;
            }
        }
    }

    ret = 0;

out:
    grub_file_close(file);
    grub_check_free(gpt);
    grub_errno = GRUB_ERR_NONE;
    return ret;
}

grub_err_t ventoy_cmd_clear_key(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    int ret;

    (void)ctxt;
    (void)argc;
    (void)args;

    for (i = 0; i < 500; i++)
    {
        ret = grub_getkey_noblock();
        if (ret == GRUB_TERM_NO_KEY)
        {
            break;
        }
    }

    if (i >= 500)
    {
        grub_cls();
        grub_printf("\n\n Still have key input after clear.\n");
        grub_refresh();
        grub_sleep(5);
    }

    return 0;
}

grub_err_t ventoy_cmd_acpi_param(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    int buflen;
    int datalen;
    int loclen;
    int img_chunk_num;
    int image_sector_size;
    char cmd[64];
    ventoy_chain_head *chain;
    ventoy_img_chunk *chunk;
    ventoy_os_param *osparam;
    ventoy_image_location *location;
    ventoy_image_disk_region *region;
    struct grub_acpi_table_header *acpi;

    (void)ctxt;

    if (argc != 2)
    {
        return 1;
    }

    debug("ventoy_cmd_acpi_param %s %s\n", args[0], args[1]);

    chain = (ventoy_chain_head *)(ulong)grub_strtoul(args[0], NULL, 16);
    if (!chain)
    {
        return 1;
    }

    image_sector_size = (int)grub_strtol(args[1], NULL, 10);

    if (grub_memcmp(&g_ventoy_guid, &(chain->os_param.guid), 16))
    {
        debug("Invalid ventoy guid 0x%x\n", chain->os_param.guid.data1);
        return 1;
    }

    img_chunk_num = chain->img_chunk_num;

    loclen = sizeof(ventoy_image_location) + (img_chunk_num - 1) * sizeof(ventoy_image_disk_region);
    datalen = sizeof(ventoy_os_param) + loclen;

    buflen = sizeof(struct grub_acpi_table_header) + datalen;
    acpi = grub_zalloc(buflen);
    if (!acpi)
    {
        return 1;
    }

    /* Step1: Fill acpi table header */
    grub_memcpy(acpi->signature, "VTOY", 4);
    acpi->length = buflen;
    acpi->revision = 1;
    grub_memcpy(acpi->oemid, "VENTOY", 6);
    grub_memcpy(acpi->oemtable, "OSPARAMS", 8);
    acpi->oemrev = 1;
    acpi->creator_id[0] = 1;
    acpi->creator_rev = 1;

    /* Step2: Fill data */
    osparam = (ventoy_os_param *)(acpi + 1);
    grub_memcpy(osparam, &chain->os_param, sizeof(ventoy_os_param));
    osparam->vtoy_img_location_addr = 0;
    osparam->vtoy_img_location_len  = loclen;
    osparam->chksum = 0;
    osparam->chksum = 0x100 - grub_byte_checksum(osparam, sizeof(ventoy_os_param));

    location = (ventoy_image_location *)(osparam + 1);
    grub_memcpy(&location->guid, &osparam->guid, sizeof(ventoy_guid));
    location->image_sector_size = image_sector_size;
    location->disk_sector_size  = chain->disk_sector_size;
    location->region_count = img_chunk_num;

    region = location->regions;
    chunk = (ventoy_img_chunk *)((char *)chain + chain->img_chunk_offset);
    if (512 == image_sector_size)
    {
        for (i = 0; i < img_chunk_num; i++)
        {
            region->image_sector_count = chunk->disk_end_sector - chunk->disk_start_sector + 1;
            region->image_start_sector = chunk->img_start_sector * 4;
            region->disk_start_sector  = chunk->disk_start_sector;
            region++;
            chunk++;
        }
    }
    else
    {
        for (i = 0; i < img_chunk_num; i++)
        {
            region->image_sector_count = chunk->img_end_sector - chunk->img_start_sector + 1;
            region->image_start_sector = chunk->img_start_sector;
            region->disk_start_sector  = chunk->disk_start_sector;
            region++;
            chunk++;
        }
    }

    /* Step3: Fill acpi checksum */
    acpi->checksum = 0;
    acpi->checksum = 0x100 - grub_byte_checksum(acpi, acpi->length);

    /* load acpi table */
    grub_snprintf(cmd, sizeof(cmd), "acpi mem:0x%lx:size:%d", (ulong)acpi, acpi->length);
    grub_script_execute_sourcecode(cmd);

    grub_free(acpi);

    VENTOY_CMD_RETURN(0);
}

grub_err_t ventoy_cmd_push_last_entry(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    g_ventoy_last_entry_back = g_ventoy_last_entry;
    g_ventoy_last_entry = -1;

    return 0;
}

grub_err_t ventoy_cmd_pop_last_entry(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    g_ventoy_last_entry = g_ventoy_last_entry_back;

    return 0;
}

int ventoy_lib_module_callback(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    const char *pos = filename + 1;

    if (info->dir)
    {
        while (*pos)
        {
            if (*pos == '.')
            {
                if ((*(pos - 1) >= '0' && *(pos - 1) <= '9') && (*(pos + 1) >= '0' && *(pos + 1) <= '9'))
                {
                    grub_strncpy((char *)data, filename, 128);
                    return 1;
                }
            }
            pos++;
        }
    }

    return 0;
}

grub_err_t ventoy_cmd_lib_module_ver(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int rc = 1;
    char *device_name = NULL;
    grub_device_t dev = NULL;
    grub_fs_t fs = NULL;
    char buf[128] = {0};

    (void)ctxt;

    if (argc != 3)
    {
        debug("ventoy_cmd_lib_module_ver, invalid param num %d\n", argc);
        return 1;
    }

    debug("ventoy_cmd_lib_module_ver %s %s %s\n", args[0], args[1], args[2]);

    device_name = grub_file_get_device_name(args[0]);
    if (!device_name)
    {
        debug("grub_file_get_device_name failed, %s\n", args[0]);
        goto end;
    }

    dev = grub_device_open(device_name);
    if (!dev)
    {
        debug("grub_device_open failed, %s\n", device_name);
        goto end;
    }

    fs = grub_fs_probe(dev);
    if (!fs)
    {
        debug("grub_fs_probe failed, %s\n", device_name);
        goto end;
    }

    fs->fs_dir(dev, args[1], ventoy_lib_module_callback, buf);

    if (buf[0])
    {
        ventoy_set_env(args[2], buf);
    }

    rc = 0;

end:

    check_free(device_name, grub_free);
    check_free(dev, grub_device_close);

    return rc;
}

int ventoy_load_part_table(const char *diskname)
{
    char name[64];
    int ret;
    grub_disk_t disk;
    grub_device_t dev;

    g_ventoy_part_info = grub_zalloc(sizeof(ventoy_gpt_info));
    if (!g_ventoy_part_info)
    {
        return 1;
    }

    disk = grub_disk_open(diskname);
    if (!disk)
    {
        debug("Failed to open disk %s\n", diskname);
        return 1;
    }

    g_ventoy_disk_size = disk->total_sectors * (1U << disk->log_sector_size);

    g_ventoy_disk_bios_id = disk->id;

    grub_disk_read(disk, 0, 0, sizeof(ventoy_gpt_info), g_ventoy_part_info);
    grub_disk_close(disk);

    grub_snprintf(name, sizeof(name), "%s,1", diskname);
    dev = grub_device_open(name);
    if (dev)
    {
        /* Check for official Ventoy device */
        ret = ventoy_check_official_device(dev);
        grub_device_close(dev);

        if (ret)
        {
            return 1;
        }
    }

    g_ventoy_disk_part_size[0] = ventoy_get_vtoy_partsize(0);
    g_ventoy_disk_part_size[1] = ventoy_get_vtoy_partsize(1);

    return 0;
}

void ventoy_prompt_end(void)
{
    int op = 0;
    char c;

    grub_printf("\n\n\n");
    grub_printf(" 1 --- Exit grub\n");
    grub_printf(" 2 --- Reboot\n");
    grub_printf(" 3 --- Shut down\n");
    grub_printf("Please enter your choice: ");
    grub_refresh();

    while (1)
    {
        c = grub_getkey();
        if (c >= '1' && c <= '3')
        {
            if (op == 0)
            {
                op = c - '0';
                grub_printf("%c", c);
                grub_refresh();
            }
        }
        else if (c == '\r' || c == '\n')
        {
            if (op)
            {
                if (op == 1)
                {
                    grub_exit();
                }
                else if (op == 2)
                {
                    grub_reboot();
                }
                else if (op == 3)
                {
                    grub_script_execute_sourcecode("halt");
                }
            }
        }
        else if (c == '\b')
        {
            if (op)
            {
                op = 0;
                grub_printf("\rPlease enter your choice:   ");
                grub_printf("\rPlease enter your choice: ");
                grub_refresh();
            }
        }
    }
}

grub_err_t ventoy_cmd_load_part_table(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int ret;

    (void)argc;
    (void)ctxt;

    ret = ventoy_load_part_table(args[0]);
    if (ret)
    {
        ventoy_prompt_end();
    }

    g_ventoy_disk_part_size[0] = ventoy_get_vtoy_partsize(0);
    g_ventoy_disk_part_size[1] = ventoy_get_vtoy_partsize(1);

    return 0;
}

grub_err_t ventoy_cmd_check_custom_boot(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int ret = 1;
    const char *vcfg = NULL;

    (void)argc;
    (void)ctxt;

    vcfg = ventoy_plugin_get_custom_boot(args[0]);
    if (vcfg)
    {
        debug("custom boot <%s>:<%s>\n", args[0], vcfg);
        grub_env_set(args[1], vcfg);
        ret = 0;
    }
    else
    {
        debug("custom boot <%s>:<NOT FOUND>\n", args[0]);
    }

    grub_errno = 0;
    return ret;
}


grub_err_t ventoy_cmd_part_exist(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int id;
    grub_uint8_t zeroguid[16] = {0};

    (void)argc;
    (void)ctxt;

    id = (int)grub_strtoul(args[0], NULL, 10);
    grub_errno = 0;

    if (grub_memcmp(g_ventoy_part_info->Head.Signature, "EFI PART", 8) == 0)
    {
        if (id >= 1 && id <= 128)
        {
            if (grub_memcmp(g_ventoy_part_info->PartTbl[id - 1].PartGuid, zeroguid, 16))
            {
                return 0;
            }
        }
    }
    else
    {
        if (id >= 1 && id <= 4)
        {
            if (g_ventoy_part_info->MBR.PartTbl[id - 1].FsFlag)
            {
                return 0;
            }
        }
    }

    return 1;
}

grub_err_t ventoy_cmd_get_fs_label(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int rc = 1;
    char *device_name = NULL;
    grub_device_t dev = NULL;
    grub_fs_t fs = NULL;
    char *label = NULL;

    (void)ctxt;

    debug("get fs label for %s\n", args[0]);

    if (argc != 2)
    {
        debug("ventoy_cmd_get_fs_label, invalid param num %d\n", argc);
        return 1;
    }

    device_name = grub_file_get_device_name(args[0]);
    if (!device_name)
    {
        debug("grub_file_get_device_name failed, %s\n", args[0]);
        goto end;
    }

    dev = grub_device_open(device_name);
    if (!dev)
    {
        debug("grub_device_open failed, %s\n", device_name);
        goto end;
    }

    fs = grub_fs_probe(dev);
    if (NULL == fs || NULL == fs->fs_label)
    {
        debug("grub_fs_probe failed, %s %p %p\n", device_name, fs, fs->fs_label);
        goto end;
    }

    fs->fs_label(dev, &label);
    if (label)
    {
        debug("label=<%s>\n", label);
        ventoy_set_env(args[1], label);
        grub_free(label);
