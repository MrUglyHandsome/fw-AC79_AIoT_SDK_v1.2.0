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
/**
 * File: baidu_ca_ap_info_adp.c
 * Auth: Gang Chen (chengang12@baidu.com)
 * Desc: Adapt the AP informatin functions to esp32.
 */

#ifdef DUER_PLATFORM_ESP32
#include "baidu_ca_adapter_internal.h"
#include "esp_wifi.h"

static const int8_t WIFI_SIGNAL_STRENGTH_OK = -70;

duer_status_t wifi_status_get(void)
{
    wifi_ap_record_t ap_info;
    esp_err_t rs = ESP_OK;
    duer_status_t ret = DUER_OK;

    rs = esp_wifi_sta_get_ap_info(&ap_info);
    if (rs == ESP_OK) {
        if (ap_info.rssi < WIFI_SIGNAL_STRENGTH_OK) {
            ret = DUER_ERR_WIFI_SIGNAL_WEAK;
        }
    } else if (rs == ESP_ERR_WIFI_NOT_CONNECT) {
        ret = DUER_ERR_WIFI_DISCONNECTED;
    } else {
        ret = DUER_ERR_FAILED;
    }

    return ret;
}
#endif
