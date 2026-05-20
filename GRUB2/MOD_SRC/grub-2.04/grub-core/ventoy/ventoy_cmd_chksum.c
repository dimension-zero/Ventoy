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
int ventoy_chksum_pathcmp(int chktype, char *rlpath, char *rdpath)
{
    char *pos1 = NULL;
    char *pos2 = NULL;

    if (chktype == 2)
    {
        pos1 = ventoy_str_basename(rlpath);
        pos2 = ventoy_str_basename(rdpath);
        return grub_strcmp(pos1, pos2);
    }
    else if (chktype == 3 || chktype == 4)
    {
        if (grub_strcmp(rlpath, rdpath) == 0 || grub_strcmp(rlpath + 1, rdpath) == 0)
        {
            return 0;
        }
    }

    return 1;
}

static int ventoy_find_checksum
(
    grub_file_t file,
    const char *uname,
    int retlen,
    char *path,
    int chktype,
    char *chksum
)
{
    int ulen;
    char *pos = NULL;
    char *pos1 = NULL;
    char *pos2 = NULL;
    char *buf = NULL;
    char *currline = NULL;
    char *nextline = NULL;

    ulen = (int)grub_strlen(uname);

    /* read file to buffer */
    buf = grub_malloc(file->size + 4);
    if (!buf)
    {
        return 1;
    }
    grub_file_read(file, buf, file->size);
    buf[file->size] = 0;

    /* parse each line */
    for (currline = buf; currline; currline = nextline)
    {
        nextline = ventoy_get_line(currline);
        VTOY_SKIP_SPACE(currline);

        if (grub_strncasecmp(currline, uname, ulen) == 0)
        {
            pos = grub_strchr(currline, '=');
            pos1 = grub_strchr(currline, '(');
            pos2 = grub_strchr(currline, ')');

            if (pos && pos1 && pos2)
            {
                *pos2 = 0;
                if (ventoy_chksum_pathcmp(chktype, path, pos1 + 1) == 0)
                {
                    VTOY_SKIP_SPACE_NEXT(pos, 1);
                    grub_memcpy(chksum, pos, retlen);
                    goto end;
                }
            }
        }
        else if (ventoy_str_len_alnum(currline, retlen))
        {
            VTOY_SKIP_SPACE_NEXT_EX(pos, currline, retlen);
            if (ventoy_chksum_pathcmp(chktype, path, pos) == 0)
            {
                grub_memcpy(chksum, currline, retlen);
                goto end;
            }
        }
    }

end:
    grub_free(buf);
    return 0;
}

int ventoy_check_chkfile(const char *isopart, char *path, const char *lchkname, grub_file_t *pfile)
{
    int ret = 0;
    int cnt = 0;
    char c = 0;
    char *pos = NULL;
    grub_file_t file = NULL;

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s.%s", isopart, path, lchkname);
    if (file)
    {
        VTOY_GOTO_END(1);
    }

    cnt = ventoy_str_chrcnt(path, '/');
    if (cnt > 1)
    {
        pos = grub_strrchr(path, '/');
        c = *pos;
        *pos = 0;

        file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s/VENTOY_CHECKSUM", isopart, path);
        if (file)
        {
            *pos = c;
            VTOY_GOTO_END(2);
        }
        *pos = c;
    }

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s/VENTOY_CHECKSUM", isopart);
    if (file)
    {
        ret = (cnt > 1) ? 3 : 4;
    }

end:

    if (pfile)
    {
        *pfile = file;
    }
    else
    {
        check_free(file, grub_file_close);
    }
    return ret;
}

grub_err_t ventoy_cmd_cmp_checksum(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int index = 0;
    int chktype = 0;
    char *pos = NULL;
    grub_file_t file = NULL;
    const char *calc_value = NULL;
    const char *isopart = NULL;
    char fchksum[64];
    char readchk[256] = {0};
    char filebuf[512] = {0};
    char uchkname[16];

    (void)ctxt;

    index = (int)grub_strtol(args[0], NULL, 10);
    if (argc != 2 || index < 0 || index >= VTOY_CHKSUM_NUM)
    {
        return 1;
    }

    grub_strncpy(uchkname, g_lower_chksum_name[index], sizeof(uchkname));
    ventoy_str_toupper(uchkname);

    isopart = grub_env_get("vtoy_iso_part");
    calc_value = grub_env_get("VT_LAST_CHECK_SUM");

    chktype = ventoy_check_chkfile(isopart, args[1], g_lower_chksum_name[index], &file);
    if (chktype <= 0)
    {
        grub_printf("\n\nNo checksum file found.\n");
        goto end;
    }

    if (chktype == 1)
    {
        grub_snprintf(fchksum, sizeof(fchksum), ".%s", g_lower_chksum_name[index]);
        grub_memset(filebuf, 0, sizeof(filebuf));
        grub_file_read(file, filebuf, 511);

        pos = grub_strchr(filebuf, '=');
        if (pos)
        {
            VTOY_SKIP_SPACE_NEXT(pos, 1);
            grub_memcpy(readchk, pos, g_chksum_retlen[index]);
        }
        else
        {
            grub_memcpy(readchk, filebuf, g_chksum_retlen[index]);
        }
    }
    else if (chktype == 3 || chktype == 4)
    {
        grub_snprintf(fchksum, sizeof(fchksum), "global VENTOY_CHECKSUM");
        ventoy_find_checksum(file, uchkname, g_chksum_retlen[index], args[1], chktype, readchk);
        if (readchk[0] == 0)
        {
            grub_printf("\n\n%s value not found in %s.\n", uchkname, fchksum);
            goto end;
        }
    }
    else
    {
        grub_snprintf(fchksum, sizeof(fchksum), "local VENTOY_CHECKSUM");
        ventoy_find_checksum(file, uchkname, g_chksum_retlen[index], args[1], chktype, readchk);
        if (readchk[0] == 0)
        {
            grub_file_close(file);
            file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s/VENTOY_CHECKSUM", isopart);
            if (file)
            {
                grub_snprintf(fchksum, sizeof(fchksum), "global VENTOY_CHECKSUM");
                ventoy_find_checksum(file, uchkname, g_chksum_retlen[index], args[1], 3, readchk);
                if (readchk[0] == 0)
                {
                    grub_printf("\n\n%s value not found in both local and global VENTOY_CHECKSUM.\n", uchkname);
                    goto end;
                }
            }
        }
    }

    if (grub_strcasecmp(calc_value, readchk) == 0)
    {
        grub_printf("\n\nCheck %s value with %s file.  [ SUCCESS ]\n", uchkname, fchksum);
    }
    else
    {
        grub_printf("\n\nCheck %s value with %s file.  [ ERROR ]\n", uchkname, fchksum);
        grub_printf("The %s value in %s file is:\n%s\n", uchkname, fchksum, readchk);
    }

end:
    grub_refresh();
    check_free(file, grub_file_close);
    VENTOY_CMD_RETURN(0);
}

static int ventoy_find_all_checksum
(
    grub_file_t file,
    char *path,
    int chktype,
    int exists[VTOY_CHKSUM_NUM],
    int *ptotexist
)
{
    int i;
    int ulen;
    int tot = 0;
    char c = 0;
    char *pos = NULL;
    char *pos1 = NULL;
    char *pos2 = NULL;
    char *buf = NULL;
    char *currline = NULL;
    char *nextline = NULL;
    const char *uname = NULL;

    tot = *ptotexist;

    /* read file to buffer */
    buf = grub_malloc(file->size + 4);
    if (!buf)
    {
        return 1;
    }
    grub_file_read(file, buf, file->size);
    buf[file->size] = 0;

    /* parse each line */
    for (currline = buf; currline; currline = nextline)
    {
        nextline = ventoy_get_line(currline);
        VTOY_SKIP_SPACE(currline);

        for (i = 0; i < VTOY_CHKSUM_NUM; i++)
        {
            if (exists[i])
            {
                continue;
            }

            uname = g_lower_chksum_name[i];
            ulen = g_lower_chksum_namelen[i];

            if (grub_strncasecmp(currline, uname, ulen) == 0)
            {
                pos = grub_strchr(currline, '=');
                pos1 = grub_strchr(currline, '(');
                pos2 = grub_strchr(currline, ')');

                if (pos && pos1 && pos2)
                {
                    c = *pos2;
                    *pos2 = 0;
                    if (ventoy_chksum_pathcmp(chktype, path, pos1 + 1) == 0)
                    {
                        exists[i] = 1;
                        tot++;
                    }
                    *pos2 = c;
                }
            }
            else if (ventoy_str_len_alnum(currline, g_chksum_retlen[i]))
            {
                VTOY_SKIP_SPACE_NEXT_EX(pos, currline, g_chksum_retlen[i]);
                if (ventoy_chksum_pathcmp(chktype, path, pos) == 0)
                {
                    exists[i] = 1;
                    tot++;
                }
            }

            if (tot >= VTOY_CHKSUM_NUM)
            {
                goto end;
            }
        }
    }

end:

    *ptotexist = tot;
    grub_free(buf);
    return 0;
}

grub_err_t ventoy_cmd_vtoychksum_exist(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i = 0;
    int cnt = 0;
    char c = 0;
    int tip = 0;
    char *pos = NULL;
    grub_file_t file = NULL;
    const char *isopart = NULL;
    int exists[VTOY_CHKSUM_NUM] = { 0, 0, 0, 0 };
    int totexist = 0;

    (void)argc;
    (void)ctxt;

    isopart = grub_env_get("vtoy_iso_part");

    for (i = 0; i < VTOY_CHKSUM_NUM; i++)
    {
        if (ventoy_check_file_exist("%s%s.%s", isopart, args[0], g_lower_chksum_name[i]))
        {
            exists[i] = 1;
            totexist++;
        }
    }

    if (totexist == VTOY_CHKSUM_NUM)
    {
        goto end;
    }

    cnt = ventoy_str_chrcnt(args[0], '/');
    if (cnt > 1)
    {
        pos = grub_strrchr(args[0], '/');
        c = *pos;
        *pos = 0;
        file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s/VENTOY_CHECKSUM", isopart, args[0]);
        *pos = c;

        if (file)
        {
            if (tip == 0 && file->size > (32 * VTOY_SIZE_1KB))
            {
                tip = 1;
                grub_printf("Reading checksum file...\n");
                grub_refresh();
            }

            debug("parse local VENTOY_CHECKSUM\n");
            ventoy_find_all_checksum(file, args[0], 2, exists, &totexist);
            grub_file_close(file);
        }
    }

    if (totexist == VTOY_CHKSUM_NUM)
    {
        goto end;
    }

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s/VENTOY_CHECKSUM", isopart);
    if (file)
    {
        if (tip == 0 && file->size > (32 * VTOY_SIZE_1KB))
        {
            tip = 1;
            grub_printf("Reading checksum file...\n");
            grub_refresh();
        }

        debug("parse global VENTOY_CHECKSUM\n");
        ventoy_find_all_checksum(file, args[0], (cnt > 1) ? 3 : 4, exists, &totexist);
        grub_file_close(file);
    }

end:

    ventoy_env_int_set("VT_EXIST_MD5", exists[0]);
    ventoy_env_int_set("VT_EXIST_SHA1", exists[1]);
    ventoy_env_int_set("VT_EXIST_SHA256", exists[2]);
    ventoy_env_int_set("VT_EXIST_SHA512", exists[3]);

    VENTOY_CMD_RETURN(0);
}


const char * ventoy_menu_lang_read_hook(struct grub_env_var *var, const char *val)
{
    (void)var;
    return ventoy_get_vmenu_title(val);
}

