/******************************************************************************
 * ventoy_util_windows_thread.c — writeback thread for HTTP edits
 *
 * Copyright (c) 2021, longpanda <admin@ventoy.net>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ventoy_define.h>
#include <ventoy_util.h>
#include <ventoy_disk.h>

static volatile int g_thread_stop = 0;
static HANDLE g_writeback_thread;
static HANDLE g_writeback_event;

DWORD WINAPI ventoy_local_thread_run(LPVOID lpParameter)
{
	ventoy_http_writeback_pf callback = (ventoy_http_writeback_pf)lpParameter;

    while (1)
    {
        WaitForSingleObject(g_writeback_event, INFINITE);
        if (g_thread_stop)
        {
            break;
        }
        else
        {
            callback();
        }
    }

    return 0;
}


void ventoy_set_writeback_event(void)
{
    SetEvent(g_writeback_event);
}


int ventoy_start_writeback_thread(ventoy_http_writeback_pf callback)
{
    g_thread_stop = 0;
    g_writeback_event = CreateEventA(NULL, FALSE, FALSE, "VTOYWRBK");
    g_writeback_thread = CreateThread(NULL, 0, ventoy_local_thread_run, callback, 0, NULL);

    return 0;
}


void ventoy_stop_writeback_thread(void)
{
    g_thread_stop = 1;
    ventoy_set_writeback_event();

    WaitForSingleObject(g_writeback_thread, INFINITE);

    CHECK_CLOSE_HANDLE(g_writeback_thread);
    CHECK_CLOSE_HANDLE(g_writeback_event);
}
