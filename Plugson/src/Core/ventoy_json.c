/******************************************************************************
 * ventoy_json.c — lifecycle (create / parse-entry / destroy) + escape helper
 *
 * Copyright (c) 2021, longpanda <admin@ventoy.net>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#if defined(_MSC_VER) || defined(WIN32)
#else
#include <unistd.h>
#include <sys/types.h>
#include <linux/limits.h>
#endif
#include <ventoy_define.h>
#include <ventoy_util.h>
#include <ventoy_json.h>

static void vtoy_json_free(VTOY_JSON *pstJsonHead)
{
    VTOY_JSON *pstNext = NULL;

    while (NULL != pstJsonHead)
    {
        pstNext = pstJsonHead->pstNext;
        if ((pstJsonHead->enDataType < JSON_TYPE_BUTT) && (NULL != pstJsonHead->pstChild))
        {
            vtoy_json_free(pstJsonHead->pstChild);
        }

        free(pstJsonHead);
        pstJsonHead = pstNext;
    }

    return;
}

VTOY_JSON * vtoy_json_create(void)
{
    VTOY_JSON *pstJson = NULL;

    pstJson = (VTOY_JSON *)zalloc(sizeof(VTOY_JSON));
    if (NULL == pstJson)
    {
        return NULL;
    }

    return pstJson;
}

int vtoy_json_parse(VTOY_JSON *pstJson, const char *szJsonData)
{
    uint32_t uiMemSize = 0;
    int Ret = JSON_SUCCESS;
    char *pcNewBuf = NULL;
    const char *pcEnd = NULL;

    uiMemSize = (uint32_t)strlen(szJsonData) + 1;
    pcNewBuf = (char *)malloc(uiMemSize);
    if (NULL == pcNewBuf)
    {
        vdebug("Failed to alloc new buf.\n");
        return JSON_FAILED;
    }
    memcpy(pcNewBuf, szJsonData, uiMemSize);
    pcNewBuf[uiMemSize - 1] = 0;

    Ret = vtoy_json_parse_value(pcNewBuf, (char *)szJsonData, pstJson, szJsonData, &pcEnd);
    if (JSON_SUCCESS != Ret)
    {
        vdebug("Failed to parse json data start=%p, end=%p.\n", szJsonData, pcEnd);
        return JSON_FAILED;
    }

    return JSON_SUCCESS;
}

int vtoy_json_parse_ex(VTOY_JSON *pstJson, const char *szJsonData, int szLen)
{
    uint32_t uiMemSize = 0;
    int Ret = JSON_SUCCESS;
    char *pcNewBuf = NULL;
    const char *pcEnd = NULL;

    uiMemSize = (uint32_t)szLen;
    pcNewBuf = (char *)malloc(uiMemSize + 1);
    if (NULL == pcNewBuf)
    {
        vdebug("Failed to alloc new buf.\n");
        return JSON_FAILED;
    }
    memcpy(pcNewBuf, szJsonData, szLen);
    pcNewBuf[uiMemSize] = 0;

    Ret = vtoy_json_parse_value(pcNewBuf, (char *)szJsonData, pstJson, szJsonData, &pcEnd);
    if (JSON_SUCCESS != Ret)
    {
        vdebug("Failed to parse json data start=%p, end=%p\n", szJsonData, pcEnd);
        return JSON_FAILED;
    }

    return JSON_SUCCESS;
}

int vtoy_json_destroy(VTOY_JSON *pstJson)
{
    if (NULL == pstJson)
    {
        return JSON_SUCCESS;
    }

    if (NULL != pstJson->pstChild)
    {
        vtoy_json_free(pstJson->pstChild);
    }

    if (NULL != pstJson->pstNext)
    {
        vtoy_json_free(pstJson->pstNext);
    }

    free(pstJson);

    return JSON_SUCCESS;
}

int vtoy_json_escape_string(char *buf, int buflen, const char *str, int newline)
{
    char last = 0;
    int count = 0;

    *buf++ = '"';
    count++;

    while (*str)
    {
        if (*str == '"' && last != '\\')
        {
            *buf = '\\';
            count++;
            buf++;
        }

        *buf = *str;
        count++;
        buf++;

        last = *str;
        str++;
    }

    *buf++ = '"';
    count++;

    *buf++ = ',';
    count++;

    if (newline)
    {
        *buf++ = '\n';
        count++;
    }

    return count;
}
