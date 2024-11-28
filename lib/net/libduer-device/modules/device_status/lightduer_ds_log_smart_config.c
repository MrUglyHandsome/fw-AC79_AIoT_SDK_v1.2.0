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
 * Auth: Chen Xihao(chenxihao@baidu.com)
 * Desc: SmartConfig related report log
 */

#include "lightduer_ds_log_smart_config.h"
#include "lightduer_ds_log_wifi_config.h"
#include "lightduer_ds_log.h"
#include "lightduer_log.h"

duer_status_t duer_ds_log_smart_cfg(duer_ds_log_smart_cfg_code_t log_code,
                                    const baidu_json *message)
{
    duer_ds_log_level_enum_t log_level = DUER_DS_LOG_LEVEL_INFO;
    baidu_json *log_message = NULL;

    CHECK_WIFI_CFG_ID()

    if (!message) {
        log_message = baidu_json_CreateObject();
        if (log_message == NULL) {
            DUER_LOGE("message json create fail");
            return DUER_ERR_MEMORY_OVERFLOW;
        }
    } else {
        log_message = (baidu_json *)message;
    }

    baidu_json_AddNumberToObjectCS(log_message, "id", duer_ds_log_wifi_cfg_get_id());

    switch (log_code) {
    case DUER_DS_LOG_SMART_CFG_START:
    case DUER_DS_LOG_SMART_CFG_LOCK_CHANNEL:
    case DUER_DS_LOG_SMART_CFG_LEAD_COMPLETED:
    case DUER_DS_LOG_SMART_CFG_FINISHED:
        log_level = DUER_DS_LOG_LEVEL_INFO;
        break;
    case DUER_DS_LOG_SMART_CFG_FAILED:
        log_level = DUER_DS_LOG_LEVEL_ERROR;
        break;
    default:
        DUER_LOGW("unkown log code 0x%0x", log_code);
        return DUER_ERR_FAILED;
    }

    return duer_ds_log_important(log_level, DUER_DS_LOG_MODULE_SMARTCFG, log_code, log_message);
}

duer_status_t duer_ds_log_smart_cfg_lead_completed(duer_size_t data_len)
{
    baidu_json *message = NULL;

    CHECK_WIFI_CFG_ID()

    message = baidu_json_CreateObject();
    if (message == NULL) {
        DUER_LOGE("message json create fail");
        return DUER_ERR_MEMORY_OVERFLOW;
    }

    baidu_json_AddNumberToObjectCS(message, "data_len", data_len);

    return duer_ds_log_smart_cfg(DUER_DS_LOG_SMART_CFG_LEAD_COMPLETED, message);
}

duer_status_t duer_ds_log_smart_cfg_finished(duer_ds_log_smart_cfg_info_t *info)
{
    baidu_json *message = NULL;

    CHECK_WIFI_CFG_ID()

    if (info == NULL) {
        DUER_LOGE("invalid argmuent");
        return DUER_ERR_FAILED;
    }

    message = baidu_json_CreateObject();
    if (message == NULL) {
        DUER_LOGE("message json create fail");
        return DUER_ERR_MEMORY_OVERFLOW;
    }

    baidu_json_AddNumberToObjectCS(message, "packet_counts", info->packet_counts);
    baidu_json_AddNumberToObjectCS(message, "data_counts", info->data_counts);
    baidu_json_AddNumberToObjectCS(message, "data_invalids", info->data_invalids);
    baidu_json_AddNumberToObjectCS(message, "decode_counts", info->decode_counts);

    return duer_ds_log_smart_cfg(DUER_DS_LOG_SMART_CFG_FINISHED, message);
}
