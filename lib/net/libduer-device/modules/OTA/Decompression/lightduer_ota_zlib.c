/**
 * Copyright (2017) Baidu Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*
 * File: lightduer_ota_zlib.c
 * Auth: Zhong Shuai (zhongshuai@baidu.com)
 * Desc: OTA Zlib
 */

#include "lightduer_ota_zlib.h"
#include "lightduer_ota_unpacker.h"
#include "lightduer_ota_decompression.h"
#include "zlib.h"
#include <stdint.h>
/* #include <stdbool.h> */
#include "lightduer_log.h"
#include "lightduer_lib.h"
#include "lightduer_types.h"
#include "lightduer_mutex.h"
#include "lightduer_memory.h"

/* #define BUFFER_SIZE (4 * 1024) */
#define BUFFER_SIZE (2 * 1024)

static void *zlib_alloc(void *opaque, unsigned int items, unsigned int size)
{
    return (void *)DUER_CALLOC(items, size);
}

static void zlib_free(void *opaque, void *ptr)
{
    DUER_FREE(ptr);
}

static int zlib_init(duer_ota_decompression_t *decompression, void **pdata)
{
    int ret = DUER_OK;
    z_streamp zlib_stream;

    if (decompression == NULL || pdata == NULL) {
        DUER_LOGE("OTA Zlib: Argument error");

        duer_ota_decompression_report_err(decompression, "Argument Error", 0);

        return DUER_ERR_INVALID_PARAMETER;
    }

    do {
        zlib_stream = DUER_MALLOC(sizeof(*zlib_stream));
        if (zlib_stream == NULL) {
            DUER_LOGE("OTA Zlib: Malloc zlib stream");

            duer_ota_decompression_report_err(decompression, "Malloc zlib stream failed", 0);

            return DUER_ERR_MEMORY_OVERFLOW;
        }

        zlib_stream->zalloc   = zlib_alloc;
        zlib_stream->zfree    = zlib_free;
        zlib_stream->opaque   = Z_NULL;
        zlib_stream->avail_in = 0;
        zlib_stream->next_in  = Z_NULL;

        ret = inflateInit(zlib_stream);
        if (ret != Z_OK) {
            DUER_LOGE("OTA Zlib: Init Zlib stream failed ret: %d", ret);
            if (zlib_stream->msg == NULL) {
                duer_ota_decompression_report_err(decompression, "Init zlib stream failed", ret);
            } else {
                duer_ota_decompression_report_err(decompression, zlib_stream->msg, ret);
            }

            ret = DUER_ERR_FAILED;

            break;
        }

        *pdata = zlib_stream;

        return DUER_OK;
    } while (0);

    if (zlib_stream != NULL) {
        DUER_FREE(zlib_stream);
    }

    return ret;
}

static int zlib_unzip(duer_ota_decompression_t *decompression,
                      void *pdata,
                      char const *compressed_data,
                      size_t compressed_data_size,
                      void *custom_data,
                      DECOMPRESS_DATA_HANDLER_FUN data_handler)
{
    int ret = DUER_OK;
    int zlib_ret = 0;
    uint8_t *buf = NULL;
    /* uint8_t buf[BUFFER_SIZE]; */
    size_t decompress_data_size = 0;
    z_streamp zlib_stream;

    if (pdata == NULL
        || custom_data == NULL
        || compressed_data == NULL
        || compressed_data_size <= 0
        || data_handler == NULL) {
        DUER_LOGE("OTA Zlib: Argument error");

        duer_ota_decompression_report_err(decompression, "Argument error", ret);

        return DUER_ERR_INVALID_PARAMETER;
    }

    buf = DUER_MALLOC(BUFFER_SIZE);
    if (NULL == buf) {
        return DUER_ERR_MEMORY_OVERFLOW;
    }

    zlib_stream = (z_streamp)pdata;

    zlib_stream->next_in = (uint8_t *)compressed_data;
    zlib_stream->avail_in = compressed_data_size;

    do {
        zlib_stream->avail_out = BUFFER_SIZE;
        zlib_stream->next_out  = buf;

        zlib_ret = inflate(zlib_stream, Z_NO_FLUSH);
        switch (zlib_ret) {
        case Z_NEED_DICT:
            DUER_LOGE("OTA Zlib: Zlib need dictionary");

            duer_ota_decompression_report_err(decompression, "Zlib need dictionary", zlib_ret);

            break;
        case Z_DATA_ERROR:
            DUER_LOGE("OTA Zlib: Zlib data error");

            duer_ota_decompression_report_err(decompression, "Zlib data error", zlib_ret);

            break;
        case Z_MEM_ERROR:
            DUER_LOGE("OTA Zlib: Zlib memory error");

            duer_ota_decompression_report_err(decompression, "Zlib memory error", zlib_ret);

            break;
        case Z_OK:
            DUER_LOGD("OTA Zlib: Unzip success");

            break;
        case Z_STREAM_END:
            DUER_LOGI("OTA Zlib: Unzip finish");

            ret = duer_ota_unpacker_set_unpacker_mode(custom_data, UNZIP_DATA_DONE);
            if (ret != DUER_OK) {
                DUER_LOGE("OTA Zlib: Set UNZIP_DATA_DONE failed");

                duer_ota_decompression_report_err(decompression, "Set UNZIP_DATA_DONE failed", zlib_ret);
            }

            ret = DUER_OK;

            break;
        }

        if (zlib_ret != Z_OK && zlib_ret != Z_STREAM_END) {
            DUER_LOGE("OTA Zlib: Zlib detail error: %s", zlib_stream->msg);

            duer_ota_decompression_report_err(decompression, zlib_stream->msg, zlib_ret);

            (void)inflateEnd(zlib_stream);

            DUER_FREE(buf);

            return DUER_ERR_FAILED;
        }

        decompress_data_size = BUFFER_SIZE - zlib_stream->avail_out;

        ret = (*data_handler)(custom_data, (const char *)buf, decompress_data_size);
        if (ret != DUER_OK) {
            DUER_LOGE("OTA Zlib: Error handling decompression data");

            break;
        }
    } while (zlib_stream->avail_out == 0);

    DUER_FREE(buf);

    return ret;
}

static int zlib_recovery(duer_ota_decompression_t *decompression, void *pdata)
{
    DUER_LOGE("OTA Zlib: Do not support recovery");

    return DUER_ERR_FAILED;
}

static int zlib_sync(duer_ota_decompression_t *decompression, void *pdata)
{
    DUER_LOGE("OTA Zlib: Do not support sync");

    return DUER_ERR_FAILED;
}

static int zlib_unint(duer_ota_decompression_t *decompression, void *pdata)
{
    int ret = DUER_OK;
    z_streamp zlib_stream;

    if (pdata == NULL) {
        DUER_LOGE("OTA Zlib: Argument error");

        duer_ota_decompression_report_err(decompression, "Argument error", 0);

        return DUER_ERR_INVALID_PARAMETER;
    }

    zlib_stream = (z_streamp)pdata;

    ret = inflateEnd(zlib_stream);
    if (ret != Z_OK) {
        DUER_LOGE("OTA Zlib: Unint zlib stream failed ret:%d", ret);

        duer_ota_decompression_report_err(decompression, "Inflate end failed", ret);

        ret = DUER_ERR_FAILED;
    }

    return ret;
}

int duer_ota_init_zlib(void)
{
    int ret = DUER_OK;
    duer_ota_decompression_t *decompression = NULL;

    decompression = duer_ota_unpacker_create_decompression("Zliblite");
    if (decompression == NULL) {
        DUER_LOGE("OTA Zlib: Create decompression failed");

        return DUER_ERR_FAILED;
    }

    decompression->init     = zlib_init;
    decompression->unzip    = zlib_unzip;
    decompression->recovery = zlib_recovery;
    decompression->sync     = zlib_sync;
    decompression->unint    = zlib_unint;

    ret = duer_ota_unpacker_register_decompression(decompression, ZLIB);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Zlib: Register decompression failed");

        ret = duer_ota_unpacker_destroy_decompression(decompression);
        if (ret != DUER_OK) {
            DUER_LOGE("OTA Zlib: Destroy decompression object failed");

            return ret;
        }

        return DUER_ERR_FAILED;
    }

    return ret;
}
