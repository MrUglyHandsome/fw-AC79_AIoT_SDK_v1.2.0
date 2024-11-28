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
 * Desc: wifi config related report log
 */

#include "lightduer_ds_log_wifi_config.h"
#include "lightduer_ds_log.h"
#include "lightduer_lib.h"
#include "lightduer_log.h"
#include "lightduer_random.h"

static int s_wifi_cfg_id = 0;

duer_status_t duer_ds_log_wifi_cfg(duer_ds_log_wifi_cfg_code_t log_code,
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

    baidu_json_AddNumberToObjectCS(log_message, "id", s_wifi_cfg_id);

    switch (log_code) {
    case DUER_DS_LOG_WIFI_CFG_START:
    case DUER_DS_LOG_WIFI_CFG_CONNECTING:
    case DUER_DS_LOG_WIFI_CFG_CONNECTED:
    case DUER_DS_LOG_WIFI_CFG_FINISHED:
        log_level = DUER_DS_LOG_LEVEL_INFO;
        break;
    case DUER_DS_LOG_WIFI_CFG_SCAN_RESLUTS:
    case DUER_DS_LOG_WIFI_CFG_LOCKED:
        log_level = DUER_DS_LOG_LEVEL_DEBUG;
        break;
    case DUER_DS_LOG_WIFI_CFG_TIMEOUT:
    case DUER_DS_LOG_WIFI_CFG_FAILED:
        log_level = DUER_DS_LOG_LEVEL_ERROR;
        break;
    default:
        DUER_LOGW("unkown log code 0x%0x", log_code);
        return DUER_ERR_FAILED;
    }

    return duer_ds_log_important(log_level, DUER_DS_LOG_MODULE_WIFICFG, log_code, log_message);
}

duer_status_t duer_ds_log_wifi_cfg_start(void)
{
    if (s_wifi_cfg_id == 0) {
        s_wifi_cfg_id = duer_random();
    } else {
        s_wifi_cfg_id++;
    }

    if (s_wifi_cfg_id == 0) {
        s_wifi_cfg_id++;
    }

    return duer_ds_log_wifi_cfg(DUER_DS_LOG_WIFI_CFG_START, NULL);
}

duer_status_t duer_ds_log_wifi_cfg_scan_resluts(const char **ssids, duer_size_t num)
{
    baidu_json *message = NULL;
    baidu_json *list = NULL;

    CHECK_WIFI_CFG_ID()

    if (ssids == NULL || num == 0) {
        DUER_LOGE("invalid agruments");
        return DUER_ERR_FAILED;
    }

    message = baidu_json_CreateObject();
    if (message == NULL) {
        DUER_LOGE("message json create fail");
        return DUER_ERR_MEMORY_OVERFLOW;
    }

    list = baidu_json_CreateStringArray(ssids, num);
    if (list == NULL) {
        DUER_LOGE("list json create fail");
        baidu_json_Delete(message);
        return DUER_ERR_FAILED;
    }

    baidu_json_AddItemToObjectCS(message, "list", list);

    return duer_ds_log_wifi_cfg(DUER_DS_LOG_WIFI_CFG_SCAN_RESLUTS, message);
}

duer_status_t duer_ds_log_wifi_cfg_finished(duer_wifi_cfg_err_code_t code,
        duer_wifi_cfg_method_t method)
{
    baidu_json *message = NULL;

    CHECK_WIFI_CFG_ID()

    message = baidu_json_CreateObject();
    if (message == NULL) {
        DUER_LOGE("message json create fail");
        return DUER_ERR_MEMORY_OVERFLOW;
    }

    baidu_json_AddNumberToObjectCS(message, "code", code);
    if (code == DUER_WIFI_CFG_SUCCESS) {
        baidu_json_AddNumberToObjectCS(message, "method", method);
    }

    return duer_ds_log_wifi_cfg(DUER_DS_LOG_WIFI_CFG_FINISHED, message);
}

duer_status_t duer_ds_log_wifi_cfg_failed(duer_wifi_cfg_err_code_t err_code, int rssi)
{
    baidu_json *message = NULL;

    CHECK_WIFI_CFG_ID()

    message = baidu_json_CreateObject();
    if (message == NULL) {
        DUER_LOGE("message json create fail");
        return DUER_ERR_MEMORY_OVERFLOW;
    }

    baidu_json_AddNumberToObjectCS(message, "code", err_code);
    if (rssi != 0) {
        baidu_json_AddNumberToObjectCS(message, "rssi", rssi);
    }

    return duer_ds_log_wifi_cfg(DUER_DS_LOG_WIFI_CFG_FAILED, message);
}

int duer_ds_log_wifi_cfg_get_id(void)
{
    return s_wifi_cfg_id;
}
