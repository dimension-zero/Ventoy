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
#include "ventoy_cmd_priv.h"
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

static int
ventoy_password_get (char buf[], unsigned buf_size)
{
  unsigned i, cur_len = 0;
  int key;
  struct grub_term_coordinate *pos = grub_term_save_pos ();

    while (1)
    {
        key = grub_getkey ();
        if (key == '\n' || key == '\r')
            break;

        if (key == GRUB_TERM_ESC)
        {
            cur_len = 0;
            break;
        }

        if (key == '\b')
        {
            if (cur_len)
            {
                grub_term_restore_pos (pos);
                for (i = 0; i < cur_len; i++)
                    grub_xputs (" ");
                grub_term_restore_pos (pos);
                cur_len--;
                for (i = 0; i < cur_len; i++)
                    grub_xputs ("*");
                grub_refresh ();
            }
            continue;
        }

        if (!grub_isprint (key))
            continue;

        if (cur_len + 2 < buf_size)
            buf[cur_len++] = key;
        grub_xputs ("*");
        grub_refresh ();
    }

    grub_memset (buf + cur_len, 0, buf_size - cur_len);

    grub_xputs ("\n");
    grub_refresh ();
    grub_free (pos);

    return (key != GRUB_TERM_ESC);
}

int ventoy_get_password(char buf[], unsigned buf_size)
{
    const char *env = NULL;

    env = grub_env_get("VTOY_SHOW_PASSWORD_ASTERISK");
    if (env && env[0] == '0' && env[1] == 0)
    {
        return grub_password_get(buf, buf_size);
    }
    else
    {
        return ventoy_password_get(buf, buf_size);
    }
}

int ventoy_check_password(const vtoy_password *pwd, int retry)
{
    int offset;
    char input[256];
    grub_uint8_t md5[16];

    while (retry--)
    {
        grub_memset(input, 0, sizeof(input));

        grub_printf("Enter password: ");
        grub_refresh();

        if (pwd->type == VTOY_PASSWORD_TXT)
        {
            ventoy_get_password(input, 128);
            if (grub_strcmp(pwd->text, input) == 0)
            {
                return 0;
            }
        }
        else if (pwd->type == VTOY_PASSWORD_MD5)
        {
            ventoy_get_password(input, 128);
            grub_crypto_hash(GRUB_MD_MD5, md5, input, grub_strlen(input));
            if (grub_memcmp(pwd->md5, md5, 16) == 0)
            {
                return 0;
            }
        }
        else if (pwd->type == VTOY_PASSWORD_SALT_MD5)
        {
            offset = (int)grub_snprintf(input, 128, "%s", pwd->salt);
            ventoy_get_password(input + offset, 128);

            grub_crypto_hash(GRUB_MD_MD5, md5, input, grub_strlen(input));
            if (grub_memcmp(pwd->md5, md5, 16) == 0)
            {
                return 0;
            }
        }

        grub_printf("Invalid password!\n\n");
        grub_refresh();
    }

    return 1;
}

img_info * ventoy_get_min_iso(img_iterator_node *node)
{
    img_info *minimg = NULL;
    img_info *img = (img_info *)(node->firstiso);

    while (img && (img_iterator_node *)(img->parent) == node)
    {
        if (img->select == 0 && (NULL == minimg || ventoy_cmp_img(img, minimg) < 0))
        {
            minimg = img;
        }
        img = img->next;
    }

    if (minimg)
    {
        minimg->select = 1;
    }

    return minimg;
}

img_iterator_node * ventoy_get_min_child(img_iterator_node *node)
{
    img_iterator_node *Minchild = NULL;
    img_iterator_node *child = node->firstchild;

    while (child && child->parent == node)
    {
        if (child->select == 0 && (NULL == Minchild || ventoy_cmp_subdir(child, Minchild) < 0))
        {
            Minchild = child;
        }
        child = child->next;
    }

    if (Minchild)
    {
        Minchild->select = 1;
    }

    return Minchild;
}

