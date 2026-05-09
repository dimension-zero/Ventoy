/*
 * ventoy_gui_priv.h — internal prototypes shared between ventoy_gui.c
 * (entrypoint), ventoy_gui_ldcache.c, and ventoy_gui_detect.c.
 */
#ifndef __VENTOY_GUI_PRIV_H__
#define __VENTOY_GUI_PRIV_H__

int parse_ld_cache(int *flag);
int detect_gui_exe_path(int argc, char **argv, const char *curpath, char *pathbuf, int buflen);

#endif
