/******************************************************************************
 * ventoy_json_parse.c — recursive-descent parser internals
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

static char *vtoy_json_skip(const char *pcData)
{
    while ((NULL != pcData) && ('\0' != *pcData) && (*pcData <= 32))
    {
        pcData++;
    }

    return (char *)pcData;
}

static int vtoy_json_parse_number
(
    VTOY_JSON *pstJson,
    const char *pcData,
    const char **ppcEnd
)
{
    unsigned long Value;

    Value = strtoul(pcData, (char **)ppcEnd, 10);
    if (*ppcEnd == pcData)
    {
        vdebug("Failed to parse json number %s.\n", pcData);
        return JSON_FAILED;
    }

    pstJson->enDataType = JSON_TYPE_NUMBER;
    pstJson->unData.lValue = Value;

    return JSON_SUCCESS;
}

static int vtoy_json_parse_string
(
    char *pcNewStart,
    char *pcRawStart,
    VTOY_JSON *pstJson,
    const char *pcData,
    const char **ppcEnd
)
{
    uint32_t uiLen = 0;
    const char *pcPos = NULL;
    const char *pcTmp = pcData + 1;

    *ppcEnd = pcData;

    if ('\"' != *pcData)
    {
        return JSON_FAILED;
    }

    pcPos = strchr(pcTmp, '\"');
    if ((NULL == pcPos) || (pcPos < pcTmp))
    {
        vdebug("Invalid string %s.\n", pcData);
        return JSON_FAILED;
    }

    if (*(pcPos - 1) == '\\')
    {
        for (pcPos++; *pcPos; pcPos++)
        {
            if (*pcPos == '"' && *(pcPos - 1) != '\\')
            {
                break;
            }
        }

        if (*pcPos == 0 || pcPos < pcTmp)
        {
            vdebug("Invalid quotes string %s.", pcData);
            return JSON_FAILED;
        }
    }

    *ppcEnd = pcPos + 1;
    uiLen = (uint32_t)(unsigned long)(pcPos - pcTmp);

    pstJson->enDataType = JSON_TYPE_STRING;
    pstJson->unData.pcStrVal = pcNewStart + (pcTmp - pcRawStart);
    pstJson->unData.pcStrVal[uiLen] = '\0';

    return JSON_SUCCESS;
}

static int vtoy_json_parse_array
(
    char *pcNewStart,
    char *pcRawStart,
    VTOY_JSON *pstJson,
    const char *pcData,
    const char **ppcEnd
)
{
    int Ret = JSON_SUCCESS;
    VTOY_JSON *pstJsonChild = NULL;
    VTOY_JSON *pstJsonItem = NULL;
    const char *pcTmp = pcData + 1;

    *ppcEnd = pcData;
    pstJson->enDataType = JSON_TYPE_ARRAY;

    if ('[' != *pcData)
    {
        return JSON_FAILED;
    }

    pcTmp = vtoy_json_skip(pcTmp);

    if (']' == *pcTmp)
    {
        *ppcEnd = pcTmp + 1;
        return JSON_SUCCESS;
    }

    JSON_NEW_ITEM(pstJson->pstChild, JSON_FAILED);

    Ret = vtoy_json_parse_value(pcNewStart, pcRawStart, pstJson->pstChild, pcTmp, ppcEnd);
    if (JSON_SUCCESS != Ret)
    {
        vdebug("Failed to parse array child.\n");
        return JSON_FAILED;
    }

    pstJsonChild = pstJson->pstChild;
    pcTmp = vtoy_json_skip(*ppcEnd);
    while ((NULL != pcTmp) && (',' == *pcTmp))
    {
        JSON_NEW_ITEM(pstJsonItem, JSON_FAILED);
        pstJsonChild->pstNext = pstJsonItem;
        pstJsonItem->pstPrev = pstJsonChild;
        pstJsonChild = pstJsonItem;

        Ret = vtoy_json_parse_value(pcNewStart, pcRawStart, pstJsonChild, vtoy_json_skip(pcTmp + 1), ppcEnd);
        if (JSON_SUCCESS != Ret)
        {
            vdebug("Failed to parse array child.\n");
            return JSON_FAILED;
        }
        pcTmp = vtoy_json_skip(*ppcEnd);
    }

    if ((NULL != pcTmp) && (']' == *pcTmp))
    {
        *ppcEnd = pcTmp + 1;
        return JSON_SUCCESS;
    }
    else
    {
        *ppcEnd = pcTmp;
        return JSON_FAILED;
    }
}

static int vtoy_json_parse_object
(
    char *pcNewStart,
    char *pcRawStart,
    VTOY_JSON *pstJson,
    const char *pcData,
    const char **ppcEnd
)
{
    int Ret = JSON_SUCCESS;
    VTOY_JSON *pstJsonChild = NULL;
    VTOY_JSON *pstJsonItem = NULL;
    const char *pcTmp = pcData + 1;

    *ppcEnd = pcData;
    pstJson->enDataType = JSON_TYPE_OBJECT;

    if ('{' != *pcData)
    {
        return JSON_FAILED;
    }

    pcTmp = vtoy_json_skip(pcTmp);
    if ('}' == *pcTmp)
    {
        *ppcEnd = pcTmp + 1;
        return JSON_SUCCESS;
    }

    JSON_NEW_ITEM(pstJson->pstChild, JSON_FAILED);

    Ret = vtoy_json_parse_string(pcNewStart, pcRawStart, pstJson->pstChild, pcTmp, ppcEnd);
    if (JSON_SUCCESS != Ret)
    {
        vdebug("Failed to parse array child.\n");
        return JSON_FAILED;
    }

    pstJsonChild = pstJson->pstChild;
    pstJsonChild->pcName = pstJsonChild->unData.pcStrVal;
    pstJsonChild->unData.pcStrVal = NULL;

    pcTmp = vtoy_json_skip(*ppcEnd);
    if ((NULL == pcTmp) || (':' != *pcTmp))
    {
        *ppcEnd = pcTmp;
        return JSON_FAILED;
    }

    Ret = vtoy_json_parse_value(pcNewStart, pcRawStart, pstJsonChild, vtoy_json_skip(pcTmp + 1), ppcEnd);
    if (JSON_SUCCESS != Ret)
    {
        vdebug("Failed to parse array child.\n");
        return JSON_FAILED;
    }

    pcTmp = vtoy_json_skip(*ppcEnd);
    while ((NULL != pcTmp) && (',' == *pcTmp))
    {
        JSON_NEW_ITEM(pstJsonItem, JSON_FAILED);
        pstJsonChild->pstNext = pstJsonItem;
        pstJsonItem->pstPrev = pstJsonChild;
        pstJsonChild = pstJsonItem;

        Ret = vtoy_json_parse_string(pcNewStart, pcRawStart, pstJsonChild, vtoy_json_skip(pcTmp + 1), ppcEnd);
        if (JSON_SUCCESS != Ret)
        {
            vdebug("Failed to parse array child.\n");
            return JSON_FAILED;
        }

        pcTmp = vtoy_json_skip(*ppcEnd);
        pstJsonChild->pcName = pstJsonChild->unData.pcStrVal;
        pstJsonChild->unData.pcStrVal = NULL;
        if ((NULL == pcTmp) || (':' != *pcTmp))
        {
            *ppcEnd = pcTmp;
            return JSON_FAILED;
        }

        Ret = vtoy_json_parse_value(pcNewStart, pcRawStart, pstJsonChild, vtoy_json_skip(pcTmp + 1), ppcEnd);
        if (JSON_SUCCESS != Ret)
        {
            vdebug("Failed to parse array child.\n");
            return JSON_FAILED;
        }

        pcTmp = vtoy_json_skip(*ppcEnd);
    }

    if ((NULL != pcTmp) && ('}' == *pcTmp))
    {
        *ppcEnd = pcTmp + 1;
        return JSON_SUCCESS;
    }
    else
    {
        *ppcEnd = pcTmp;
        return JSON_FAILED;
    }
}

int vtoy_json_parse_value
(
    char *pcNewStart,
    char *pcRawStart,
    VTOY_JSON *pstJson,
    const char *pcData,
    const char **ppcEnd
)
{
    pcData = vtoy_json_skip(pcData);

    switch (*pcData)
    {
        case 'n':
        {
            if (0 == strncmp(pcData, "null", 4))
            {
                pstJson->enDataType = JSON_TYPE_NULL;
                *ppcEnd = pcData + 4;
                return JSON_SUCCESS;
            }
            break;
        }
        case 'f':
        {
            if (0 == strncmp(pcData, "false", 5))
            {
                pstJson->enDataType = JSON_TYPE_BOOL;
                pstJson->unData.lValue = 0;
                *ppcEnd = pcData + 5;
                return JSON_SUCCESS;
            }
            break;
        }
        case 't':
        {
            if (0 == strncmp(pcData, "true", 4))
            {
                pstJson->enDataType = JSON_TYPE_BOOL;
                pstJson->unData.lValue = 1;
                *ppcEnd = pcData + 4;
                return JSON_SUCCESS;
            }
            break;
        }
        case '\"':
        {
            return vtoy_json_parse_string(pcNewStart, pcRawStart, pstJson, pcData, ppcEnd);
        }
        case '[':
        {
            return vtoy_json_parse_array(pcNewStart, pcRawStart, pstJson, pcData, ppcEnd);
        }
        case '{':
        {
            return vtoy_json_parse_object(pcNewStart, pcRawStart, pstJson, pcData, ppcEnd);
        }
        case '-':
        {
            return vtoy_json_parse_number(pstJson, pcData, ppcEnd);
        }
        default :
        {
            if (*pcData >= '0' && *pcData <= '9')
            {
                return vtoy_json_parse_number(pstJson, pcData, ppcEnd);
            }
        }
    }

    *ppcEnd = pcData;
    vdebug("Invalid json data %u.\n", (uint8_t)(*pcData));
    return JSON_FAILED;
}
