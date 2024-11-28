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
 * Desc: airkiss related report log
 */

#include "lightduer_ds_log_airkiss.h"
#include "lightduer_ds_log_wifi_config.h"
#include "lightduer_ds_log.h"
#include "lightduer_log.h"

duer_status_t duer_ds_log_airkiss(duer_ds_log_airkiss_code_t log_code,
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
    case DUER_DS_LOG_AIRKISS_START:
    case DUER_DS_LOG_AIRKISS_FINISHED:
        log_level = DUER_DS_LOG_LEVEL_INFO;
        break;
    case DUER_DS_LOG_AIRKISS_FAILED:
        log_level = DUER_DS_LOG_LEVEL_ERROR;
        break;
    default:
        DUER_LOGW("unkown log code 0x%0x", log_code);
        return DUER_ERR_FAILED;
    }

    return duer_ds_log_important(log_level, DUER_DS_LOG_MODULE_AIRKISS, log_code, log_message);
}
