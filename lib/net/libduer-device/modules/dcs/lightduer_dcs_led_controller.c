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
/**
 * File: lightduer_dcs_led_controller.h
 * Auth: Chen Xihao (chenxihao@baidu.com)
 * Desc: apply APIs to support DCS led_controller Interface.
 */

#include "lightduer_dcs.h"
#include "lightduer_dcs_router.h"
#include "lightduer_dcs_local.h"
#include "lightduer_connagent.h"
#include "lightduer_ds_log_dcs.h"

static const char *DCS_LED_CONTROLLER_NAMESPACE = "ai.dueros.device_interface.led_controller";
static const char *DCS_CLEAR_LED_NAME = "ClearLED";

static volatile duer_bool s_is_initialized = DUER_FALSE;

static duer_status_t duer_clear_led_cb(const baidu_json *directive)
{
    baidu_json *payload = NULL;
    baidu_json *token = NULL;

    payload = baidu_json_GetObjectItem(directive, DCS_PAYLOAD_KEY);
    if (!payload) {
        return DUER_MSG_RSP_BAD_REQUEST;
    }

    token = baidu_json_GetObjectItem(payload, DCS_TOKEN_KEY);
    if (!token || !token->valuestring) {
        return DUER_MSG_RSP_BAD_REQUEST;
    }

    duer_dcs_clear_indicator_handler();

    return DUER_OK;
}

void duer_dcs_led_controller_init(void)
{
    DUER_DCS_CRITICAL_ENTER();
    if (s_is_initialized) {
        DUER_DCS_CRITICAL_EXIT();
        return;
    }

    duer_directive_list res[] = {
        {DCS_CLEAR_LED_NAME, duer_clear_led_cb},
    };

    duer_add_dcs_directive_internal(res, sizeof(res) / sizeof(res[0]), DCS_LED_CONTROLLER_NAMESPACE);

    s_is_initialized = DUER_TRUE;
    DUER_DCS_CRITICAL_EXIT();
}

void duer_dcs_led_controller_uninitialize_internal(void)
{
    s_is_initialized = DUER_FALSE;
}
