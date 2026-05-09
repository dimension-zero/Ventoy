/******************************************************************************
 * ventoy_json_get.c — read-only accessors over a parsed VTOY_JSON tree
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

VTOY_JSON *vtoy_json_find_item
(
    VTOY_JSON *pstJson,
    JSON_TYPE  enDataType,
    const char *szKey
)
{
    while (NULL != pstJson)
    {
        if ((enDataType == pstJson->enDataType) &&
            (0 == strcmp(szKey, pstJson->pcName)))
        {
            return pstJson;
        }
        pstJson = pstJson->pstNext;
    }

    return NULL;
}

int vtoy_json_scan_parse
(
    const VTOY_JSON    *pstJson,
    uint32_t            uiParseNum,
    VTOY_JSON_PARSE_S  *pstJsonParse
)
{
    uint32_t i = 0;
    const VTOY_JSON *pstJsonCur = NULL;
    VTOY_JSON_PARSE_S *pstCurParse = NULL;

    for (pstJsonCur = pstJson; NULL != pstJsonCur; pstJsonCur = pstJsonCur->pstNext)
    {
        if ((JSON_TYPE_OBJECT == pstJsonCur->enDataType) ||
            (JSON_TYPE_ARRAY == pstJsonCur->enDataType))
        {
            continue;
        }

        for (i = 0, pstCurParse = NULL; i < uiParseNum; i++)
        {
            if (0 == strcmp(pstJsonParse[i].pcKey, pstJsonCur->pcName))
            {
                pstCurParse = pstJsonParse + i;
                break;
            }
        }

        if (NULL == pstCurParse)
        {
            continue;
        }

        switch (pstJsonCur->enDataType)
        {
            case JSON_TYPE_NUMBER:
            {
                if (sizeof(uint32_t) == pstCurParse->uiBufSize)
                {
                    *(uint32_t *)(pstCurParse->pDataBuf) = (uint32_t)pstJsonCur->unData.lValue;
                }
                else if (sizeof(uint16_t) == pstCurParse->uiBufSize)
                {
                    *(uint16_t *)(pstCurParse->pDataBuf) = (uint16_t)pstJsonCur->unData.lValue;
                }
                else if (sizeof(uint8_t) == pstCurParse->uiBufSize)
                {
                    *(uint8_t *)(pstCurParse->pDataBuf) = (uint8_t)pstJsonCur->unData.lValue;
                }
                else if ((pstCurParse->uiBufSize > sizeof(uint64_t)))
                {
                    scnprintf((char *)pstCurParse->pDataBuf, pstCurParse->uiBufSize, "%llu",
                        (unsigned long long)(pstJsonCur->unData.lValue));
                }
                else
                {
                    vdebug("Invalid number data buf size %u.\n", pstCurParse->uiBufSize);
                }
                break;
            }
            case JSON_TYPE_STRING:
            {
                scnprintf((char *)pstCurParse->pDataBuf, pstCurParse->uiBufSize, "%s", pstJsonCur->unData.pcStrVal);
                break;
            }
            case JSON_TYPE_BOOL:
            {
                *(uint8_t *)(pstCurParse->pDataBuf) = (pstJsonCur->unData.lValue) > 0 ? 1 : 0;
                break;
            }
            default :
            {
                break;
            }
        }
    }

    return JSON_SUCCESS;
}

int vtoy_json_scan_array
(
     VTOY_JSON *pstJson,
     const char *szKey,
     VTOY_JSON **ppstArrayItem
)
{
    VTOY_JSON *pstJsonItem = NULL;

    pstJsonItem = vtoy_json_find_item(pstJson, JSON_TYPE_ARRAY, szKey);
    if (NULL == pstJsonItem)
    {
        vdebug("Key %s is not found in json data.\n", szKey);
        return JSON_NOT_FOUND;
    }

    *ppstArrayItem = pstJsonItem;

    return JSON_SUCCESS;
}

int vtoy_json_scan_array_ex
(
     VTOY_JSON *pstJson,
     const char *szKey,
     VTOY_JSON **ppstArrayItem
)
{
    VTOY_JSON *pstJsonItem = NULL;

    pstJsonItem = vtoy_json_find_item(pstJson, JSON_TYPE_ARRAY, szKey);
    if (NULL == pstJsonItem)
    {
        vdebug("Key %s is not found in json data.\n", szKey);
        return JSON_NOT_FOUND;
    }

    *ppstArrayItem = pstJsonItem->pstChild;

    return JSON_SUCCESS;
}

int vtoy_json_scan_object
(
     VTOY_JSON *pstJson,
     const char *szKey,
     VTOY_JSON **ppstObjectItem
)
{
    VTOY_JSON *pstJsonItem = NULL;

    pstJsonItem = vtoy_json_find_item(pstJson, JSON_TYPE_OBJECT, szKey);
    if (NULL == pstJsonItem)
    {
        vdebug("Key %s is not found in json data.\n", szKey);
        return JSON_NOT_FOUND;
    }

    *ppstObjectItem = pstJsonItem;

    return JSON_SUCCESS;
}

int vtoy_json_get_int
(
    VTOY_JSON *pstJson,
    const char *szKey,
    int *piValue
)
{
    VTOY_JSON *pstJsonItem = NULL;

    pstJsonItem = vtoy_json_find_item(pstJson, JSON_TYPE_NUMBER, szKey);
    if (NULL == pstJsonItem)
    {
        //vdebug("Key %s is not found in json data.\n", szKey);
        return JSON_NOT_FOUND;
    }

    *piValue = (int)pstJsonItem->unData.lValue;

    return JSON_SUCCESS;
}

int vtoy_json_get_uint
(
    VTOY_JSON *pstJson,
    const char *szKey,
    uint32_t *puiValue
)
{
    VTOY_JSON *pstJsonItem = NULL;

    pstJsonItem = vtoy_json_find_item(pstJson, JSON_TYPE_NUMBER, szKey);
    if (NULL == pstJsonItem)
    {
        vdebug("Key %s is not found in json data.\n", szKey);
        return JSON_NOT_FOUND;
    }

    *puiValue = (uint32_t)pstJsonItem->unData.lValue;

    return JSON_SUCCESS;
}

int vtoy_json_get_uint64
(
    VTOY_JSON *pstJson,
    const char *szKey,
    uint64_t *pui64Value
)
{
    VTOY_JSON *pstJsonItem = NULL;

    pstJsonItem = vtoy_json_find_item(pstJson, JSON_TYPE_NUMBER, szKey);
    if (NULL == pstJsonItem)
    {
        vdebug("Key %s is not found in json data.\n", szKey);
        return JSON_NOT_FOUND;
    }

    *pui64Value = (uint64_t)pstJsonItem->unData.lValue;

    return JSON_SUCCESS;
}

int vtoy_json_get_bool
(
    VTOY_JSON *pstJson,
    const char *szKey,
    uint8_t *pbValue
)
{
    VTOY_JSON *pstJsonItem = NULL;

    pstJsonItem = vtoy_json_find_item(pstJson, JSON_TYPE_BOOL, szKey);
    if (NULL == pstJsonItem)
    {
        vdebug("Key %s is not found in json data.\n", szKey);
        return JSON_NOT_FOUND;
    }

    *pbValue = pstJsonItem->unData.lValue > 0 ? 1 : 0;

    return JSON_SUCCESS;
}

int vtoy_json_get_string
(
     VTOY_JSON *pstJson,
     const char *szKey,
     uint32_t  uiBufLen,
     char *pcBuf
)
{
    VTOY_JSON *pstJsonItem = NULL;

    pstJsonItem = vtoy_json_find_item(pstJson, JSON_TYPE_STRING, szKey);
    if (NULL == pstJsonItem)
    {
        //vdebug("Key %s is not found in json data.\n", szKey);
        return JSON_NOT_FOUND;
    }

    scnprintf(pcBuf, uiBufLen, "%s", pstJsonItem->unData.pcStrVal);

    return JSON_SUCCESS;
}

const char * vtoy_json_get_string_ex(VTOY_JSON *pstJson,  const char *szKey)
{
    VTOY_JSON *pstJsonItem = NULL;

    if ((NULL == pstJson) || (NULL == szKey))
    {
        return NULL;
    }

    pstJsonItem = vtoy_json_find_item(pstJson, JSON_TYPE_STRING, szKey);
    if (NULL == pstJsonItem)
    {
        //vdebug("Key %s is not found in json data.\n", szKey);
        return NULL;
    }

    return pstJsonItem->unData.pcStrVal;
}
