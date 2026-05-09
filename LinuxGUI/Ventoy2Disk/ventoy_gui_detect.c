#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <dirent.h> 
#include <sys/utsname.h>
#include <sys/types.h>
#include <linux/limits.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include "ventoy_json.h"
#include "ventoy_gui_priv.h"

#define LIB_FLAG_GTK2   (1 << 0)
#define LIB_FLAG_GTK3   (1 << 1)
#define LIB_FLAG_GTK4   (1 << 2)
#define LIB_FLAG_QT4    (1 << 3)
#define LIB_FLAG_QT5    (1 << 4)
#define LIB_FLAG_QT6    (1 << 5)
#define LIB_FLAG_GLADE2 (1 << 30)

#define LIB_FLAG_GTK    (LIB_FLAG_GTK2 | LIB_FLAG_GTK3 | LIB_FLAG_GTK4)
#define LIB_FLAG_QT     (LIB_FLAG_QT4 | LIB_FLAG_QT5 | LIB_FLAG_QT6)

#define MAX_PARAS       64
#define MAX_LOG_BUF     (1024 * 1024)
#define VTOY_GUI_PATH   "_vtoy_gui_path_="
#define VTOY_ENV_STR    "_vtoy_env_str_="
#define LD_CACHE_FILE   "/etc/ld.so.cache"
#define INT2STR_YN(a)   ((a) == 0 ? "NO" : "YES")

static int g_xdg_log = 0;
static int g_xdg_ini = 0;
static char g_log_file[PATH_MAX];
static char g_ini_file[PATH_MAX];
static char *g_log_buf = NULL;
extern char ** environ;

#define CACHEMAGIC "ld.so-1.7.0"

struct file_entry
{
  int flags;		/* This is 1 for an ELF library.  */
  unsigned int key, value; /* String table indices.  */
};

struct cache_file
{
  char magic[sizeof CACHEMAGIC - 1];
  unsigned int nlibs;
  struct file_entry libs[0];
};

#define CACHEMAGIC_NEW "glibc-ld.so.cache"
#define CACHE_VERSION "1.1"
#define CACHEMAGIC_VERSION_NEW CACHEMAGIC_NEW CACHE_VERSION

struct file_entry_new
{
  int32_t flags;		/* This is 1 for an ELF library.  */
  uint32_t key, value;		/* String table indices.  */
  uint32_t osversion;		/* Required OS version.	 */
  uint64_t hwcap;		/* Hwcap entry.	 */
};

struct cache_file_new
{
  char magic[sizeof CACHEMAGIC_NEW - 1];
  char version[sizeof CACHE_VERSION - 1];
  uint32_t nlibs;		/* Number of entries.  */
  uint32_t len_strings;		/* Size of string table. */
  uint32_t unused[5];		/* Leave space for future extensions
				   and align to 8 byte boundary.  */
  struct file_entry_new libs[0]; /* Entries describing libraries.  */
  /* After this the string table of size len_strings is found.	*/
};

/* Used to align cache_file_new.  */
#define ALIGN_CACHE(addr)				\
(((addr) + __alignof__ (struct cache_file_new) -1)	\
 & (~(__alignof__ (struct cache_file_new) - 1)))

#define vlog(fmt, args...) ventoy_syslog(0, fmt, ##args)

static int is_gtk_env(void)
{
    const char *env = NULL;
    
    env = getenv("GNOME_SETUP_DISPLAY");
    if (env && env[0] == ':')
    {
        vlog("GNOME_SETUP_DISPLAY=%s\n", env);
        return 1;
    }
    
    env = getenv("DESKTOP_SESSION");
    if (env && strcasecmp(env, "xfce") == 0)
    {
        vlog("DESKTOP_SESSION=%s\n", env);
        return 1;
    }

    return 0;
}

static int is_qt_env(void)
{
    return 0;
}

static int detect_gtk_version(int libflag)
{
    int gtk2;
    int gtk3;
    int gtk4;
    int glade2;

    gtk2 = libflag & LIB_FLAG_GTK2;
    gtk3 = libflag & LIB_FLAG_GTK3;
    gtk4 = libflag & LIB_FLAG_GTK4;
    glade2 = libflag & LIB_FLAG_GLADE2;

    if (gtk2 > 0 && glade2 > 0 && (gtk3 == 0 && gtk4 == 0))
    {
        return 2;
    }

    if (gtk3 > 0 && (gtk2 == 0 && gtk4 == 0))
    {
        return 3;
    }
    
    if (gtk4 > 0 && (gtk2 == 0 && gtk3 == 0))
    {
        return 4;
    }

    if (gtk3 > 0)
    {
        return 3;
    }

    if (gtk4 > 0)
    {
        return 4;
    }

    if (gtk2 > 0 && glade2 > 0)
    {
        return 2;
    }

    return 0;
}

static int detect_qt_version(int libflag)
{
    int qt4;
    int qt5;
    int qt6;

    qt4 = libflag & LIB_FLAG_QT4;
    qt5 = libflag & LIB_FLAG_QT5;
    qt6 = libflag & LIB_FLAG_QT6;

    if (qt4 > 0 && (qt5 == 0 && qt6 == 0))
    {
        return 4;
    }

    if (qt5 > 0 && (qt4 == 0 && qt6 == 0))
    {
        return 5;
    }
    
    if (qt6 > 0 && (qt4 == 0 && qt5 == 0))
    {
        return 6;
    }

    if (qt5 > 0)
    {
        return 5;
    }

    if (qt6 > 0)
    {
        return 6;
    }

    if (qt4 > 0)
    {
        return 4;
    }

    return 0;
}
static int gui_type_check(VTOY_JSON *pstNode)
{
    FILE *fp = NULL;
    const char *env = NULL;
    const char *arch = NULL;
    const char *srctype = NULL;
    const char *srcname = NULL;
    const char *condition = NULL;
    const char *expression = NULL;
    char line[1024];
    
    arch = vtoy_json_get_string_ex(pstNode, "arch");
    srctype = vtoy_json_get_string_ex(pstNode, "type");
    srcname = vtoy_json_get_string_ex(pstNode, "name");
    condition = vtoy_json_get_string_ex(pstNode, "condition");
    expression = vtoy_json_get_string_ex(pstNode, "expression");
    
    if (srctype == NULL || srcname == NULL || condition == NULL)
    {
        return 0;
    }

    if (arch && NULL == strstr(arch, VTOY_GUI_ARCH))
    {
        return 0;
    }

    vlog("check <%s> <%s> <%s>\n", srctype, srcname, condition);

    if (strcmp(srctype, "file") == 0)
    {
        if (access(srcname, F_OK) == -1)
        {
            return 0;
        }
    
        if (strcmp(condition, "exist") == 0)
        {
            vlog("File %s exist\n", srcname);
            return 1;
        }
        else if (strcmp(condition, "contains") == 0)
        {
            fp = fopen(srcname, "r");
            if (fp == NULL)
            {
                return 0;
            }

            while (fgets(line, sizeof(line), fp))
            {
                if (strstr(line, expression))
                {
                    vlog("File %s contains %s\n", srcname, expression);
                    fclose(fp);
                    return 1;
                }
            }

            fclose(fp);
            return 0;
        }
    }
    else if (strcmp(srctype, "env") == 0)
    {
        env = getenv(srcname);
        if (env == NULL)
        {
            return 0;
        }

        if (strcmp(condition, "exist") == 0)
        {
            vlog("env %s exist\n", srcname);
            return 1;
        }
        else if (strcmp(condition, "equal") == 0)
        {
            if (strcmp(expression, env) == 0)
            {
                vlog("env %s is %s\n", srcname, env);
                return 1;
            }
            return 0;
        }
        else if (strcmp(condition, "contains") == 0)
        {
            if (strstr(env, expression))
            {
                vlog("env %s is %s contains %s\n", srcname, env, expression);
                return 1;
            }
            return 0;
        }
    }
    
    return 0;
}

static int read_file_to_buf(const char *FileName, int ExtLen, void **Bufer, int *BufLen)
{
    int FileSize;
    FILE *fp = NULL;
    void *Data = NULL;

    fp = fopen(FileName, "rb");
    if (fp == NULL)
    {
        vlog("Failed to open file %s", FileName);
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    FileSize = (int)ftell(fp);

    Data = malloc(FileSize + ExtLen);
    if (!Data)
    {
        fclose(fp);
        return 1;
    }

    fseek(fp, 0, SEEK_SET);
    fread(Data, 1, FileSize, fp);

    fclose(fp);

    *Bufer = Data;
    *BufLen = FileSize;

    return 0;
}

static int distro_check_gui_env(char *type, int len, int *pver)
{
    int size;
    int length;
    char *pBuf = NULL;
    VTOY_JSON *pstNode = NULL;
    VTOY_JSON *pstJson = NULL;

    vlog("distro_check_gui_env ...\n");

    if (access("./tool/distro_gui_type.json", F_OK) == -1)
    {
        vlog("distro_gui_type.json file not exist\n");
        return 0;
    }

    read_file_to_buf("./tool/distro_gui_type.json", 1, (void **)&pBuf, &size);
    pBuf[size] = 0;
    
    pstJson = vtoy_json_create();
    vtoy_json_parse(pstJson, pBuf);

    for (pstNode = pstJson->pstChild; pstNode; pstNode = pstNode->pstNext)
    {
        if (gui_type_check(pstNode->pstChild))
        {
            length = (int)snprintf(type, len, "%s", vtoy_json_get_string_ex(pstNode->pstChild, "gui"));
            *pver = type[length - 1] - '0';
            type[length - 1] = 0;
            break;
        }
    }

    vtoy_json_destroy(pstJson);
    return pstNode ? 1 : 0;
}

int detect_gui_exe_path(int argc, char **argv, const char *curpath, char *pathbuf, int buflen)
{
    int i;
    int ret;
    int ver;
    int libflag = 0;
    const char *guitype = NULL;
    char line[256];
    mode_t mode;
    struct stat filestat;

    for (i = 1; i < argc; i++)
    {
        if (argv[i] && strcmp(argv[i], "--gtk2") == 0)
        {
            guitype = "gtk";
            ver = 2;
        }
        else if (argv[i] && strcmp(argv[i], "--gtk3") == 0)
        {
            guitype = "gtk";
            ver = 3;
        }
        else if (argv[i] && strcmp(argv[i], "--gtk4") == 0)
        {
            guitype = "gtk";
            ver = 4;
        }
        else if (argv[i] && strcmp(argv[i], "--qt4") == 0)
        {
            guitype = "qt";
            ver = 4;
        }
        else if (argv[i] && strcmp(argv[i], "--qt5") == 0)
        {
            guitype = "qt";
            ver = 5;
        }
        else if (argv[i] && strcmp(argv[i], "--qt6") == 0)
        {
            guitype = "qt";
            ver = 6;
        }
    }

    if (guitype)
    {
        vlog("Get GUI type from param <%s%d>.\n", guitype, ver);
    }
    else if (access("./ventoy_gui_type", F_OK) != -1)
    {
        vlog("Get GUI type from ventoy_gui_type file.\n");
    
        line[0] = 0;
        read_file_1st_line("./ventoy_gui_type", line, sizeof(line));
        if (strncmp(line, "gtk2", 4) == 0)
        {
            guitype = "gtk";
            ver = 2;

            parse_ld_cache(&libflag);
            if ((libflag & LIB_FLAG_GLADE2) == 0)
            {
                vlog("libglade2 is necessary for GTK2, but not found.\n");
                return 1;
            }
        }
        else if (strncmp(line, "gtk3", 4) == 0)
        {
            guitype = "gtk";
            ver = 3;
        }
        else if (strncmp(line, "gtk4", 4) == 0)
        {
            guitype = "gtk";
            ver = 4;
        }
        else if (strncmp(line, "qt4", 3) == 0)
        {
            guitype = "qt";
            ver = 4;
        }
        else if (strncmp(line, "qt5", 3) == 0)
        {
            guitype = "qt";
            ver = 5;
        }
        else if (strncmp(line, "qt6", 3) == 0)
        {
            guitype = "qt";
            ver = 6;
        }
        else
        {
            vlog("Current X environment is NOT supported.\n");
            return 1;
        }
    }
    else
    {
        vlog("Now detect the GUI type ...\n");

        parse_ld_cache(&libflag);

        if ((LIB_FLAG_GTK & libflag) > 0 && (LIB_FLAG_QT & libflag) == 0)
        {
            guitype = "gtk";
            ver = detect_gtk_version(libflag);
        }
        else if ((LIB_FLAG_GTK & libflag) == 0 && (LIB_FLAG_QT & libflag) > 0)
        {
            guitype = "qt";
            ver = detect_qt_version(libflag);
        }
        else if ((LIB_FLAG_GTK & libflag) > 0 && (LIB_FLAG_QT & libflag) > 0)
        {
            if (distro_check_gui_env(line, sizeof(line), &ver))
            {
                guitype = line;
                vlog("distro_check_gui <%s%d> ...\n", line, ver);
            }
            else if (is_gtk_env())
            {
                guitype = "gtk";
                ver = detect_gtk_version(libflag);
            }
            else if (is_qt_env())
            {
                guitype = "qt";
                ver = detect_qt_version(libflag);
            }
            else
            {
                vlog("Can not distinguish GTK and QT, default use GTK.\n");
                guitype = "gtk";
                ver = detect_gtk_version(libflag);
            }
        }
        else
        {
            vlog("Current X environment is NOT supported.\n");
            return 1;
        }
    }

    snprintf(pathbuf, buflen, "%s/tool/%s/Ventoy2Disk.%s%d", curpath, VTOY_GUI_ARCH, guitype, ver);

    vlog("This is %s%d X environment.\n", guitype, ver);
    vlog("exe = %s\n", pathbuf);
    
    if (access(pathbuf, F_OK) == -1)
    {
        vlog("%s is not exist.\n", pathbuf);
        return 1;
    }

    if (access(pathbuf, X_OK) == -1)
    {
        vlog("execute permission check fail, try chmod.\n", pathbuf);
        if (stat(pathbuf, &filestat) == 0)
        {
            mode = filestat.st_mode | S_IXUSR | S_IXGRP | S_IXOTH;
            ret = chmod(pathbuf, mode);
            vlog("old mode=%o new mode=%o ret=%d\n", filestat.st_mode, mode, ret);
        }
    }
    else
    {
        vlog("execute permission check success.\n");
    }

    return 0;
}

