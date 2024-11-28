/**
 * Copyright (2018) Baidu Inc. All rights reserved.
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
 * Auth: Su Hao(suhao@baidu.com)
 * Desc: Sonic decode related report log
 */

#include "lightduer_ds_log_sonic_decode.h"
#include "lightduer_ds_log_wifi_config.h"
#include "lightduer_ds_log.h"
#include "lightduer_log.h"
#include "lightduer_lib.h"
#include <math.h>

typedef struct _duer_ds_log_sonicdec_statistics_s {
    int frame_length;

    int block_errors;
    int fragment_counts;
    int fragment_errors;

    double volume_sum;
    double volume_max;
    int volume_counts;
} duer_dslog_sonicdec_t;

DUER_LOC_IMPL duer_dslog_sonicdec_t s_duer_dslog_sonicdec = {
    0
};

DUER_LOC_IMPL baidu_json *duer_ds_log_sonicdec_stat(void)
{
    baidu_json *message = NULL;
    baidu_json *decode = NULL;
    baidu_json *volume = NULL;
    baidu_json *audio = NULL;

    do {
        message = baidu_json_CreateObject();
        if (message == NULL) {
            DUER_LOGE("message json create fail");
            break;
        }

        decode = baidu_json_CreateObject();
        if (decode == NULL) {
            DUER_LOGE("decode json create fail");
            break;
        }

        baidu_json_AddNumberToObjectCS(decode, "frame_length", s_duer_dslog_sonicdec.frame_length);
        baidu_json_AddNumberToObjectCS(decode, "block_errors", s_duer_dslog_sonicdec.block_errors);
        baidu_json_AddNumberToObjectCS(decode, "fragment_counts", s_duer_dslog_sonicdec.fragment_counts);
        baidu_json_AddNumberToObjectCS(decode, "fragment_errors", s_duer_dslog_sonicdec.fragment_errors);

        baidu_json_AddItemToObjectCS(message, "decode", decode);

        audio = baidu_json_CreateObject();
        if (audio == NULL) {
            DUER_LOGE("decode json create fail");
            break;
        }

        volume = baidu_json_CreateObject();
        if (volume == NULL) {
            DUER_LOGE("decode json create fail");
            break;
        }

        baidu_json_AddNumberToObjectCS(volume, "max", s_duer_dslog_sonicdec.volume_max);
        baidu_json_AddNumberToObjectCS(volume, "avg", s_duer_dslog_sonicdec.volume_sum / s_duer_dslog_sonicdec.volume_counts);

        baidu_json_AddItemToObjectCS(audio, "volume", volume);

        baidu_json_AddItemToObjectCS(message, "audio", audio);
        audio = NULL;
    } while (DUER_FALSE);

    if (audio) {
        baidu_json_Delete(audio);
        audio = NULL;
    }

    return message;
}

DUER_LOC_IMPL duer_status_t duer_ds_log_sonicdec_do(int code,
        duer_ds_log_level_enum_t level,
        baidu_json *message)
{
    duer_status_t rs = DUER_OK;

#ifndef __TARGET_SONIC_CODEC_DEMO__
    CHECK_WIFI_CFG_ID()
#endif

    do {

        if (!message) {
            message = baidu_json_CreateObject();
            if (message == NULL) {
                DUER_LOGE("message json create fail");
                rs = DUER_ERR_MEMORY_OVERFLOW;
                break;
            }
        }

        baidu_json_AddNumberToObjectCS(message, "id", duer_ds_log_wifi_cfg_get_id());
    } while (DUER_FALSE);

    return duer_ds_log_important(level, DUER_DS_LOG_MODULE_SONIC, code, message);
}

DUER_INT_IMPL duer_status_t duer_ds_log_sonicdec_start(void)
{
    DUER_MEMSET(&s_duer_dslog_sonicdec, 0, sizeof(s_duer_dslog_sonicdec));
    return duer_ds_log_sonicdec_do(DUER_DS_LOG_SONICCODEC_DECODE_START,
                                   DUER_DS_LOG_LEVEL_INFO,
                                   NULL);
}

DUER_INT_IMPL duer_status_t duer_ds_log_sonicdec_audio_analysis(const duer_coef_t *data, duer_size_t size)
{
    duer_status_t rs = DUER_OK;
    duer_s32_t i;
    duer_coef_t sum = 0;
    double volume = 0;

    do {
        if (data == NULL || size == 0) {
            rs = DUER_ERR_INVALID_PARAMETER;
            break;
        }

        for (i = 0; i < size; i++) {
            sum += DUER_ABS(data[i]);
            DUER_LOGV("sum = "DUER_COEF_F", data[%d] = "DUER_COEF_F, sum, i, data[i]);
        }

#ifndef DUER_FIXED_POINT
        sum = DUER_QCONST_BITS(sum, 16);
#endif

        volume = sum;
        volume /= size;

        volume = 20 * log10(volume);

        DUER_LOGV("sum = "DUER_COEF_F", volume = %lf", sum, volume);

        s_duer_dslog_sonicdec.volume_sum += volume;
        s_duer_dslog_sonicdec.volume_counts++;

        if (s_duer_dslog_sonicdec.volume_max < volume) {
            s_duer_dslog_sonicdec.volume_max = volume;
        }
    } while (DUER_FALSE);

    return rs;
}

DUER_INT_IMPL duer_status_t duer_ds_log_sonicdec_fetched_start(void)
{
    return duer_ds_log_sonicdec_do(DUER_DS_LOG_SONICCODEC_DECODE_FETCHED_START,
                                   DUER_DS_LOG_LEVEL_INFO,
                                   NULL);
}

DUER_INT_IMPL duer_status_t duer_ds_log_sonicdec_block_error(void)
{
    s_duer_dslog_sonicdec.block_errors++;
    return duer_ds_log_sonicdec_do(DUER_DS_LOG_SONICCODEC_DECODE_BLOCK_ERROR,
                                   DUER_DS_LOG_LEVEL_DEBUG,
                                   NULL);
}

DUER_INT_IMPL duer_status_t duer_ds_log_sonicdec_fragment_fetched(void)
{
    s_duer_dslog_sonicdec.fragment_counts++;
    return duer_ds_log_sonicdec_do(DUER_DS_LOG_SONICCODEC_DECODE_FRAGMENT_FETCHED,
                                   DUER_DS_LOG_LEVEL_DEBUG,
                                   NULL);
}

DUER_INT_IMPL duer_status_t duer_ds_log_sonicdec_fragment_error(void)
{
    s_duer_dslog_sonicdec.fragment_errors++;
    return duer_ds_log_sonicdec_do(DUER_DS_LOG_SONICCODEC_DECODE_FRAGMENT_ERROR,
                                   DUER_DS_LOG_LEVEL_DEBUG,
                                   NULL);
}

DUER_INT_IMPL duer_status_t duer_ds_log_sonicdec_frame_complete(int length)
{
    s_duer_dslog_sonicdec.frame_length = length;
    return duer_ds_log_sonicdec_do(DUER_DS_LOG_SONICCODEC_DECODE_FRAME_COMPLETE,
                                   DUER_DS_LOG_LEVEL_DEBUG,
                                   NULL);
}

DUER_INT_IMPL duer_status_t duer_ds_log_sonicdec_finish(void)
{
    return duer_ds_log_sonicdec_do(DUER_DS_LOG_SONICCODEC_DECODE_FINISH,
                                   DUER_DS_LOG_LEVEL_INFO,
                                   duer_ds_log_sonicdec_stat());
}

DUER_INT_IMPL duer_status_t duer_ds_log_sonicdec_failed(void)
{
    return duer_ds_log_sonicdec_do(DUER_DS_LOG_SONICCODEC_DECODE_FAILED,
                                   DUER_DS_LOG_LEVEL_ERROR,
                                   NULL);
}

