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

void ventoy_syslog(int level, const char *Fmt, ...)
{
    int buflen;
    char *buf = NULL;
    char log[512];
    va_list arg;
    time_t stamp;
    struct tm ttm;
    FILE *fp;

    (void)level;
    
    time(&stamp);
    localtime_r(&stamp, &ttm);

    if (g_log_buf)
    {
        buf = g_log_buf;
        buflen = MAX_LOG_BUF;
    }
    else
    {
        buf = log;
        buflen = sizeof(log);
    }

    va_start(arg, Fmt);
    vsnprintf(buf, buflen, Fmt, arg);
    va_end(arg);

    fp = fopen(g_log_file, "a+");
    if (fp)
    {
        fprintf(fp, "[%04u/%02u/%02u %02u:%02u:%02u] %s", 
           ttm.tm_year + 1900, ttm.tm_mon, ttm.tm_mday,
           ttm.tm_hour, ttm.tm_min, ttm.tm_sec,
           buf);
        fclose(fp);
    }

    #if 0
    printf("[%04u/%02u/%02u %02u:%02u:%02u] %s", 
           ttm.tm_year + 1900, ttm.tm_mon, ttm.tm_mday,
           ttm.tm_hour, ttm.tm_min, ttm.tm_sec,
           buf);
    #endif
}

int bit_from_machine(const char *machine)
{
    if (strstr(machine, "64"))
    {
        return 64;
    }
    else
    {
        return 32;
    }
}

int get_os_bit(int *bit)
{
    int ret;
    struct utsname unameData;

    memset(&unameData, 0, sizeof(unameData));
    ret = uname(&unameData);
    if (ret != 0)
    {
        vlog("uname error, code: %d\n", errno);
        return 1;
    }

    *bit = strstr(unameData.machine, "64") ? 64 : 32;
    vlog("uname -m <%s> %dbit\n", unameData.machine, *bit);
    
    return 0;
}

int read_file_1st_line(const char *file, char *buffer, int buflen)
{
    FILE *fp = NULL;

    fp = fopen(file, "r");
    if (fp == NULL)
    {
        vlog("Failed to open file %s code:%d", file, errno);
        return 1;
    }

    fgets(buffer, buflen, fp);
    fclose(fp);
    return 0;
}

static int read_pid_cmdline(long pid, char *Buffer, int BufLen)
{
    char path[256];
    
    snprintf(path, sizeof(path), "/proc/%ld/cmdline", pid);
    return read_file_1st_line(path, Buffer, BufLen);
}

static int is_dir_exist(const char *fmt, ...)
{
    va_list arg;
    struct stat st;
    char path[4096];

    va_start(arg, fmt);
    vsnprintf(path, sizeof(path), fmt, arg);
    va_end(arg);

    memset(&st, 0, sizeof(st));
    if (stat(path, &st) < 0)
    {
        return 0;
    }

    if (st.st_mode & S_IFDIR)
    {
        return 1;
    }

    return 0;
}

static void touch_new_file(char *filename)
{
    char *pos = NULL;
    FILE *fp = NULL;

    if (access(filename, F_OK) == -1)
    {
        for (pos = filename; *pos; pos++)
        {
            if (*pos == '/')
            {
                *pos = 0;
                if (!is_dir_exist("%s", filename))
                {
                    mkdir(filename, 0755);
                }
                *pos = '/';
            }
        }
    
        fp = fopen(filename, "w+");
        if (fp)
        {
            fclose(fp);
        }        
    }    
}

static int find_exe_path(const char *exe, char *pathbuf, int buflen)
{
    int i;
    char path[PATH_MAX];
    char *tmpptr = NULL;
    char *saveptr = NULL;
    char *newenv = NULL;
    const char *env = getenv("PATH");

    if (NULL == env)
    {
        return 0;
    }

    newenv = strdup(env);
    if (!newenv)
    {
        return 0;
    }

    tmpptr = newenv;
    while (NULL != (tmpptr = strtok_r(tmpptr, ":", &saveptr)))
	{
        snprintf(path, sizeof(path), "%s/%s", tmpptr, exe);
        if (access(path, F_OK) != -1)
        {
            snprintf(pathbuf, buflen, "%s", path);
            free(newenv);
            return 1;
        }
		tmpptr = NULL;
    }
    
    free(newenv);
    return 0;
}

void dump_args(const char *prefix, char **argv)
{
    int i = 0;
    
    vlog("=========%s ARGS BEGIN===========\n", prefix);
    while (argv[i])
    {
        vlog("argv[%d]=<%s>\n", i, argv[i]);
        i++;
    }
    vlog("=========%s ARGS END===========\n", prefix);
}

int pre_check(void)
{
    int ret;
    int bit;
    int buildbit;
    const char *env = NULL;

    env = getenv("DISPLAY");
    if (NULL == env || env[0] != ':')
    {
        vlog("DISPLAY not exist(%p). Not in X environment.\n", env);
        return 1;
    }

    ret = get_os_bit(&bit);
    if (ret)
    {
        vlog("Failed to get os bit.\n");
        return 1;
    }

    buildbit = strstr(VTOY_GUI_ARCH, "64") ? 64 : 32;
    vlog("Build bit is %d (%s)\n", buildbit, VTOY_GUI_ARCH);

    if (bit != buildbit)
    {
        vlog("Current system is %d bit (%s). Please run the correct VentoyGUI.\n", bit, VTOY_GUI_ARCH);
        return 1;
    }

    return 0;
}

static char * find_argv(int argc, char **argv, char *key)
{
    int i;
    int len;

    len = (int)strlen(key);
    for (i = 0; i < argc; i++)
    {
        if (strncmp(argv[i], key, len) == 0)
        {
            return argv[i];
        }
    }

    return NULL;
}

static int adjust_cur_dir(char *argv0)
{
    int ret = 2;
    char c;
    char *pos = NULL;
    char *end = NULL;

    if (argv0[0] == '.')
    {
        return 1;
    }

    for (pos = argv0; pos && *pos; pos++)
    {
        if (*pos == '/')
        {
            end = pos;
        }
    }

    if (end)
    {
        c = *end;
        *end = 0;
        ret = chdir(argv0);
        *end = c;
    }

    return ret;
}



static char **recover_environ_param(char *env)
{
    int i = 0;
    int j = 0;
    int k = 0;
    int cnt = 0;
    char **newenvs = NULL;

    for (i = 0; env[i]; i++)
    {
        if (env[i] == '\n')
        {
            cnt++;
        }
    }

    newenvs = malloc(sizeof(char *) * (cnt + 1));
    if (!newenvs)
    {
        vlog("malloc new envs fail %d\n", cnt + 1);
        return NULL;
    }
    memset(newenvs, 0, sizeof(char *) * (cnt + 1));

    for (j = i = 0; env[i]; i++)
    {
        if (env[i] == '\n')
        {
            env[i] = 0;
            newenvs[k++] = env + j;
            j = i + 1;
        }
    }

    vlog("recover environ %d %d\n", cnt, k);
    return newenvs;
}

static int restart_main(int argc, char **argv, char *guiexe)
{
    int i = 0;
    int j = 0;
    char *para = NULL;
    char **envs = NULL;
    char *newargv[MAX_PARAS + 1] = { NULL };

    para = find_argv(argc, argv, VTOY_ENV_STR);
    if (!para)
    {
        vlog("failed to find %s\n", VTOY_ENV_STR);
        return 1;
    }

    newargv[j++] = guiexe;
    for (i = 1; i < argc && j < MAX_PARAS; i++)
    {
        if (strncmp(argv[i], "_vtoy_", 6) != 0)
        {
            newargv[j++] = argv[i];
        }
    }

    envs = recover_environ_param(para + strlen(VTOY_ENV_STR));
    if (envs)
    {
        vlog("recover success, argc=%d evecve <%s>\n", j, guiexe);
        dump_args("EXECVE", newargv);
        execve(guiexe, newargv, envs); 
    }
    else
    {
        vlog("recover failed, argc=%d evecv <%s>\n", j, guiexe);
        execv(guiexe, newargv); 
    }

    return 1;
}

static char *create_environ_param(const char *prefix, char **envs)
{
    int i = 0;
    int cnt = 0;
    int envlen = 0;
    int prelen = 0;
    char *cur = NULL;
    char *para = NULL;

    prelen = strlen(prefix);
    for (i = 0; envs[i]; i++)
    {
        cnt++;
        envlen += strlen(envs[i]) + 1;
    }

    para = malloc(prelen + envlen);
    if (!para)
    {
        vlog("failed to malloc env str %d\n", prelen + envlen);
        return NULL;
    }

    cur = para;
    memcpy(cur, prefix, prelen);
    cur += prelen;

    for (i = 0; envs[i]; i++)
    {
        envlen = strlen(envs[i]);
        memcpy(cur, envs[i], envlen);

        cur[envlen] = '\n';
        cur += envlen + 1;
    }

    vlog("create environment param %d\n", cnt);
    return para;
}

static int restart_by_pkexec(int argc, char **argv, const char *curpath, const char *exe)
{
    int i = 0;
    int j = 0;
    char envcount[64];
    char path[PATH_MAX];
    char pkexec[PATH_MAX];
    char exepara[PATH_MAX];
    char *newargv[MAX_PARAS + 1] = { NULL };

    vlog("try restart self by pkexec ...\n");

    if (find_exe_path("pkexec", pkexec, sizeof(pkexec)))
    {
        vlog("Find pkexec at <%s>\n", pkexec);
    }
    else
    {
        vlog("pkexec not found\n");
        return 1;
    }

    if (argv[0][0] != '/')
    {
        snprintf(path, sizeof(path), "%s/%s", curpath, argv[0]);
    }
    else
    {
        snprintf(path, sizeof(path), "%s", argv[0]);
    }
    snprintf(exepara, sizeof(exepara), "%s%s", VTOY_GUI_PATH, exe);

    newargv[j++] = pkexec;
    newargv[j++] = path;
    for (i = 1; i < argc && j < MAX_PARAS; i++)
    {
        if (strcmp(argv[i], "--xdg") == 0)
        {
            continue;
        }
        newargv[j++] = argv[i];
    }

    if (j < MAX_PARAS)
    {
        newargv[j++] = create_environ_param(VTOY_ENV_STR, environ);        
    }

    if (j < MAX_PARAS)
    {
        newargv[j++] = exepara;        
    }
    
    if (g_xdg_log && j + 1 < MAX_PARAS)
    {
        newargv[j++] = "-l";        
        newargv[j++] = g_log_file;        
    }
    
    if (g_xdg_ini && j + 1 < MAX_PARAS)
    {
        newargv[j++] = "-i";        
        newargv[j++] = g_ini_file;        
    }

    dump_args("PKEXEC", newargv);
    execv(pkexec, newargv);

    return 1;
}

int real_main(int argc, char **argv)
{
    int ret;
    int euid;
    char *exe = NULL;
    const char *env = NULL;
    char path[PATH_MAX];
    char curpath[PATH_MAX];

    ret = adjust_cur_dir(argv[0]);

    vlog("\n");
    vlog("=========================================================\n");
    vlog("=========================================================\n");
    vlog("=============== VentoyGui %s ===============\n", VTOY_GUI_ARCH);
    vlog("=========================================================\n");
    vlog("=========================================================\n");
    vlog("log file is <%s>\n", g_log_file);

    euid = geteuid();
    getcwd(curpath, sizeof(curpath));

    vlog("pid:%ld ppid:%ld uid:%d euid:%d\n", (long)getpid(), (long)getppid(), getuid(), euid);
    vlog("adjust dir:%d  current path:%s\n", ret, curpath);
    dump_args("RAW", argv);

    if (access("./boot/boot.img", F_OK) == -1)
    {
        vlog("Please run under the correct directory!\n");
        return 1;
    }

    exe = find_argv(argc, argv, VTOY_GUI_PATH);
    if (exe)
    {
        if (euid != 0)
        {
            vlog("Invalid euid %d when restart.\n", euid);
            return 1;
        }

        return restart_main(argc, argv, exe + strlen(VTOY_GUI_PATH));
    }
    else
    {
        if (pre_check())
        {
            return 1;
        }

        if (detect_gui_exe_path(argc, argv, curpath, path, sizeof(path)))
        {
            return 1;
        }

        if (strstr(path, "gtk"))
        {
            env = getenv("XDG_SESSION_TYPE");
            vlog("=== XDG_SESSION_TYPE is <%s> ===\n", env ? env : "NULL");
            
            if (env && strncasecmp(env, "wayland", 7) == 0)
            {
                vlog("Force GDK_BACKEND from %s to x11 for better compatibility\n", env);
                setenv("GDK_BACKEND", "x11", 1);
            }
        }

        if (euid == 0)
        {
            vlog("We have root privileges, just exec %s\n", path);
            argv[0] = path;
            execv(argv[0], argv);
        }
        else
        {
            vlog("EUID check failed.\n");

            /* try pkexec */
            restart_by_pkexec(argc, argv, curpath, path);

            vlog("### Please run with root privileges. ###\n");
            return 1;
        }
    }

    return 1;
}

int main(int argc, char **argv)
{
    int i;
    int ret;
    const char *env = NULL;

    snprintf(g_log_file, sizeof(g_log_file), "log.txt");
    for (i = 0; i < argc; i++)
    {
        if (argv[i] && argv[i + 1] && strcmp(argv[i], "-l") == 0)
        {
            touch_new_file(argv[i + 1]);
            snprintf(g_log_file, sizeof(g_log_file), "%s", argv[i + 1]);
        }
        else if (argv[i] && argv[i + 1] && strcmp(argv[i], "-i") == 0)
        {
            touch_new_file(argv[i + 1]);
        }
        else if (argv[i] && strcmp(argv[i], "--xdg") == 0)
        {
            env = getenv("XDG_CACHE_HOME");
            if (env)
            {
                g_xdg_log = 1;
                snprintf(g_log_file, sizeof(g_log_file), "%s/ventoy/ventoy.log", env);
                touch_new_file(g_log_file);
            }
            else
            {
                env = getenv("HOME");
                if (env && is_dir_exist("%s/.cache", env))
                {
                    g_xdg_log = 1;
                    snprintf(g_log_file, sizeof(g_log_file), "%s/.cache/ventoy/ventoy.log", env);
                    touch_new_file(g_log_file);
                }
            }
            
            env = getenv("XDG_CONFIG_HOME");
            if (env)
            {
                g_xdg_ini = 1;
                snprintf(g_ini_file, sizeof(g_ini_file), "%s/ventoy/Ventoy2Disk.ini", env);
                touch_new_file(g_ini_file);
            }
            else
            {
                env = getenv("HOME");
                if (env && is_dir_exist("%s/.config", env))
                {
                    g_xdg_ini = 1;
                    snprintf(g_ini_file, sizeof(g_ini_file), "%s/.config/ventoy/Ventoy2Disk.ini", env);
                    touch_new_file(g_ini_file);
                }
            }
        }
    }

    g_log_buf = malloc(MAX_LOG_BUF);
    if (!g_log_buf)
    {
        vlog("Failed to malloc log buffer %d\n", MAX_LOG_BUF);
        return 1;
    }

    ret = real_main(argc, argv);
    free(g_log_buf);
    return ret;
}

