/******************************************************************************
 * VentoyJson.c — lifecycle (create / parse-entry / destroy) + standalone test
 *
 * Copyright (c) 2021, longpanda <admin@ventoy.net>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef FOR_VTOY_JSON_CHECK
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#else
#include <Windows.h>
#include "Ventoy2Disk.h"
#endif

#include "VentoyJson.h"

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

    pstJson = (VTOY_JSON *)malloc(sizeof(VTOY_JSON));
    if (NULL == pstJson)
    {
        return NULL;
    }
    memset(pstJson, 0, sizeof(VTOY_JSON));
    return pstJson;
}

int vtoy_json_parse(VTOY_JSON *pstJson, const char *szJsonData)
{
    UINT32 uiMemSize = 0;
    int Ret = JSON_SUCCESS;
    char *pcNewBuf = NULL;
    const char *pcEnd = NULL;

	uiMemSize = (UINT32)strlen(szJsonData) + 1;
    pcNewBuf = (char *)malloc(uiMemSize);
    if (NULL == pcNewBuf)
    {
        Log("Failed to alloc new buf.");
        return JSON_FAILED;
    }
    memcpy(pcNewBuf, szJsonData, uiMemSize);
    pcNewBuf[uiMemSize - 1] = 0;

    Ret = vtoy_json_parse_value(pcNewBuf, (char *)szJsonData, pstJson, szJsonData, &pcEnd);
    if (JSON_SUCCESS != Ret)
    {
        Log("Failed to parse json data start=%p, end=%p", szJsonData, pcEnd);
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


#ifdef FOR_VTOY_JSON_CHECK

int main(int argc, char**argv)
{
    int ret = 1;
    int FileSize;
    FILE *fp;
    void *Data = NULL;
    VTOY_JSON *json = NULL;

    fp = fopen(argv[1], "rb");
    if (!fp)
    {
        Log("Failed to open %s\n", argv[1]);
        goto out;
    }

    fseek(fp, 0, SEEK_END);
    FileSize = (int)ftell(fp);
    fseek(fp, 0, SEEK_SET);

    Data = malloc(FileSize + 4);
    if (!Data)
    {
        Log("Failed to malloc %d\n", FileSize + 4);
        goto out;
    }
    *((char *)Data + FileSize) = 0;

    fread(Data, 1, FileSize, fp);

    json = vtoy_json_create();
    if (!json)
    {
        Log("Failed vtoy_json_create\n");
        goto out;
    }

    if (vtoy_json_parse(json, (char *)Data) != JSON_SUCCESS)
    {
        goto out;
    }

    ret = 0;

out:
    if (fp) fclose(fp);
    if (Data) free(Data);
    if (json) vtoy_json_destroy(json);

    printf("\n");
    return ret;
}

#endif
