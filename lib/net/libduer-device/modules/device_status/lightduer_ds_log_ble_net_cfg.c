/**
 * Copyright (2019) Baidu Inc. All rights reserved.
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
 * Desc: ble network config related report log
 */

#include "lightduer_ds_log_ble_net_cfg.h"
#include "lightduer_ds_log_wifi_config.h"
#include "lightduer_ds_log.h"
#include "lightduer_log.h"

duer_status_t duer_ds_log_ble_net_cfg(duer_ds_log_ble_net_cfg_code_t log_code)
{
    duer_ds_log_level_enum_t log_level = DUER_DS_LOG_LEVEL_INFO;
    baidu_json *log_message = NULL;

    CHECK_WIFI_CFG_ID()

    log_message = baidu_json_CreateObject();
    if (log_message == NULL) {
        DUER_LOGE("message json create fail");
        return DUER_ERR_MEMORY_OVERFLOW;
    }

    baidu_json_AddNumberToObjectCS(log_message, "id", duer_ds_log_wifi_cfg_get_id());

    switch (log_code) {
    case DUER_DS_LOG_BLE_NET_CFG_START:
    case DUER_DS_LOG_BLE_NET_CFG_CONNECT:
    case DUER_DS_LOG_BLE_NET_CFG_FIR_PCK:
    case DUER_DS_LOG_BLE_NET_CFG_SEC_PCK:
    case DUER_DS_LOG_BLE_NET_CFG_LAST_PCK:
    case DUER_DS_LOG_BLE_NET_CFG_DISCONNECT:
        log_level = DUER_DS_LOG_LEVEL_INFO;
        break;
    case DUER_DS_LOG_BLE_NET_CFG_BLE_START_FAILED:
    case DUER_DS_LOG_BLE_NET_CFG_SEQUENCE_ERROR:
    case DUER_DS_LOG_BLE_NET_CFG_AGGR_FRAG_ERROR:
    case DUER_DS_LOG_BLE_NET_CFG_UNKOWN_MSG_TYPE:
    case DUER_DS_LOG_BLE_NET_CFG_ECDH_INIT_FAILED:
    case DUER_DS_LOG_BLE_NET_CFG_ECDH_MAKE_PUBKEY_FAILED:
    case DUER_DS_LOG_BLE_NET_CFG_ECDH_CAL_SHARE_KEY_FAILED:
    case DUER_DS_LOG_BLE_NET_CFG_SCAN_WIFI_FAILED:
    case DUER_DS_LOG_BLE_NET_CFG_SCAN_NO_AP:
    case DUER_DS_LOG_BLE_NET_CFG_PARSE_WIFI_INFO_FAILED:
        log_level = DUER_DS_LOG_LEVEL_ERROR;
        break;
    default:
        DUER_LOGW("unkown log code 0x%0x", log_code);
        return DUER_ERR_FAILED;
    }

    return duer_ds_log_important(log_level, DUER_DS_LOG_MODULE_BLE_CFG, log_code, log_message);
}
