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

static int ld_cache_lib_check(const char *lib, int *flag)
{
    if (((*flag) & LIB_FLAG_GTK3) == 0)
    {
        if (strncmp(lib, "libgtk-3.so", 11) == 0)
        {
            vlog("LIB:<%s>\n", lib);
            *flag |= LIB_FLAG_GTK3;
            return 0;
        }
    }
    
    if (((*flag) & LIB_FLAG_GTK2) == 0)
    {
        if (strncmp(lib, "libgtk-x11-2.0.so", 17) == 0)
        {
            vlog("LIB:<%s>\n", lib);
            *flag |= LIB_FLAG_GTK2;
            return 0;
        }
    }
    
    if (((*flag) & LIB_FLAG_GTK4) == 0)
    {
        if (strncmp(lib, "libgtk-4.so", 11) == 0)
        {
            vlog("LIB:<%s>\n", lib);
            *flag |= LIB_FLAG_GTK4;
            return 0;
        }
    }
    
    if (((*flag) & LIB_FLAG_QT4) == 0)
    {
        if (strncmp(lib, "libQt4", 6) == 0)
        {
            vlog("LIB:<%s>\n", lib);
            *flag |= LIB_FLAG_QT4;
            return 0;
        }
    }
    
    if (((*flag) & LIB_FLAG_QT5) == 0)
    {
        if (strncmp(lib, "libQt5", 6) == 0)
        {
            vlog("LIB:<%s>\n", lib);
            *flag |= LIB_FLAG_QT5;
            return 0;
        }
    }
    
    if (((*flag) & LIB_FLAG_QT6) == 0)
    {
        if (strncmp(lib, "libQt6", 6) == 0)
        {
            vlog("LIB:<%s>\n", lib);
            *flag |= LIB_FLAG_QT6;
            return 0;
        }
    }
    
    if (((*flag) & LIB_FLAG_GLADE2) == 0)
    {
        if (strncmp(lib, "libglade-2", 10) == 0)
        {
            vlog("LIB:<%s>\n", lib);
            *flag |= LIB_FLAG_GLADE2;
            return 0;
        }
    }

    return 0;
}

int parse_ld_cache(int *flag)
{
    int fd;
    int format;
    unsigned int i;
    struct stat st;
    size_t offset = 0;
    size_t cache_size = 0;
    const char *cache_data = NULL;
    struct cache_file *cache = NULL;
    struct cache_file_new *cache_new = NULL;

    *flag = 0;
    
    fd = open(LD_CACHE_FILE, O_RDONLY);
    if (fd < 0)
    {
        vlog("failed to open %s err:%d\n", LD_CACHE_FILE, errno);
        return 1;
    }

    if (fstat(fd, &st) < 0 || st.st_size == 0)
    {
        close(fd);
        return 1;
    }

    cache = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (cache == MAP_FAILED)
    {
        close(fd);
        return 1;
    }

    cache_size = st.st_size;
    if (cache_size < sizeof (struct cache_file))
    {
        vlog("File is not a cache file.\n");
        munmap (cache, cache_size);
        close(fd);
        return 1;
    }

    if (memcmp(cache->magic, CACHEMAGIC, sizeof CACHEMAGIC - 1))
    {
        /* This can only be the new format without the old one.  */
        cache_new = (struct cache_file_new *) cache;

        if (memcmp(cache_new->magic, CACHEMAGIC_NEW, sizeof CACHEMAGIC_NEW - 1) ||
            memcmp (cache_new->version, CACHE_VERSION, sizeof CACHE_VERSION - 1))
        {
            munmap (cache, cache_size);
            close(fd);
            return 1;
        }
          
        format = 1;
        /* This is where the strings start.  */
        cache_data = (const char *) cache_new;
    }
    else
    {
        /* Check for corruption, avoiding overflow.  */
        if ((cache_size - sizeof (struct cache_file)) / sizeof (struct file_entry) < cache->nlibs)
        {
            vlog("File is not a cache file.\n");
            munmap (cache, cache_size);
            close(fd);
            return 1;
        }

        offset = ALIGN_CACHE(sizeof (struct cache_file) + (cache->nlibs * sizeof (struct file_entry)));
        
        /* This is where the strings start.  */
        cache_data = (const char *) &cache->libs[cache->nlibs];

        /* Check for a new cache embedded in the old format.  */
        if (cache_size > (offset + sizeof (struct cache_file_new)))
        {
            cache_new = (struct cache_file_new *) ((void *)cache + offset);

            if (memcmp(cache_new->magic, CACHEMAGIC_NEW, sizeof CACHEMAGIC_NEW - 1) == 0 &&
                memcmp(cache_new->version, CACHE_VERSION, sizeof CACHE_VERSION - 1) == 0)
            {
                cache_data = (const char *) cache_new;
                format = 1;
            }
        }
    }

    if (format == 0)
    {
        vlog("%d libs found in cache format 0\n", cache->nlibs);
        for (i = 0; i < cache->nlibs; i++)
        {
            ld_cache_lib_check(cache_data + cache->libs[i].key, flag);
        }
    }
    else if (format == 1)
    {
        vlog("%d libs found in cache format 1\n", cache_new->nlibs);

        for (i = 0; i < cache_new->nlibs; i++)
        {
            ld_cache_lib_check(cache_data + cache_new->libs[i].key, flag);
        }
    }

    vlog("ldconfig lib flags 0x%x\n", *flag);
    vlog("lib flags GLADE2:[%s] GTK2:[%s] GTK3:[%s] GTK4:[%s] QT4:[%s] QT5:[%s] QT6:[%s]\n", 
        INT2STR_YN((*flag) & LIB_FLAG_GLADE2), INT2STR_YN((*flag) & LIB_FLAG_GTK2),
        INT2STR_YN((*flag) & LIB_FLAG_GTK3), INT2STR_YN((*flag) & LIB_FLAG_GTK4),
        INT2STR_YN((*flag) & LIB_FLAG_QT4), INT2STR_YN((*flag) & LIB_FLAG_QT5),
        INT2STR_YN((*flag) & LIB_FLAG_QT6));

    munmap (cache, cache_size);
    close (fd);
    return 0;
}

