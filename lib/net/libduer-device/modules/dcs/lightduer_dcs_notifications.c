/**
 * Copyright (2021) Baidu Inc. All rights reserved.
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
 * File: lightduer_dcs_notifications.c
 * Auth: Chen Xihao (chenxihao@baidu.com)
 * Desc: apply APIs to support DCS notifications interface
 */

#include "lightduer_dcs.h"
#include "lightduer_dcs_router.h"
#include "lightduer_dcs_local.h"
#include "lightduer_connagent.h"
#include "lightduer_lib.h"
#include "lightduer_log.h"
#include "lightduer_memory.h"
#include "lightduer_ds_log_dcs.h"

static const char *DCS_NOTIFICATIONS_NAMESPACE = "ai.dueros.device_interface.notifications";
static const char *DCS_SET_INDICATOR_NAME = "SetIndicator";
static const char *DCS_CLEAR_INDICATOR_NAME = "ClearIndicator";
static const char *DCS_PLAY_AUDIO_INDICATOR_KEY = "playAudioIndicator";
static const char *DCS_ASSET_KEY = "asset";
static const char *DCS_TARGET_CAPABILITY_KEY = "targetCapability";

static volatile duer_bool s_is_initialized;
static char *s_token = NULL;

static duer_status_t duer_set_indicator_cb(const baidu_json *directive)
{
    baidu_json *payload = NULL;
    baidu_json *token = NULL;
    baidu_json *play_audio = NULL;
    baidu_json *asset = NULL;
    baidu_json *target = NULL;

    payload = baidu_json_GetObjectItem(directive, DCS_PAYLOAD_KEY);
    if (!payload) {
        return DUER_MSG_RSP_BAD_REQUEST;
    }

    // token = baidu_json_GetObjectItem(payload, DCS_TOKEN_KEY);
    // if (!token || !token->valuestring) {
    //     return DUER_MSG_RSP_BAD_REQUEST;
    // }

    play_audio = baidu_json_GetObjectItem(payload, DCS_PLAY_AUDIO_INDICATOR_KEY);
    if (!play_audio) {
        return DUER_MSG_RSP_BAD_REQUEST;
    }

    DUER_LOGI("playAudioIndicator %d", (play_audio->type & baidu_json_True) != 0);

    // asset = baidu_json_GetObjectItem(payload, DCS_ASSET_KEY);
    // if (asset) {
    //     DUER_LOGI("asset is not NULL");
    // }

    // target = baidu_json_GetObjectItem(payload, DCS_TARGET_CAPABILITY_KEY);
    // if (!target || !target->valuestring) {
    //     return DUER_MSG_RSP_BAD_REQUEST;
    // }

    // DUER_LOGI("targetCapability %s", target->valuestring);

    duer_dcs_set_indicator_handler();

    return DUER_OK;
}

static duer_status_t duer_clear_indicator_cb(const baidu_json *directive)
{
    baidu_json *payload = NULL;
    baidu_json *token = NULL;
    baidu_json *target = NULL;

    payload = baidu_json_GetObjectItem(directive, DCS_PAYLOAD_KEY);
    if (!payload) {
        return DUER_MSG_RSP_BAD_REQUEST;
    }

    token = baidu_json_GetObjectItem(payload, DCS_TOKEN_KEY);
    if (!token || !token->valuestring) {
        return DUER_MSG_RSP_BAD_REQUEST;
    }

    target = baidu_json_GetObjectItem(payload, DCS_TARGET_CAPABILITY_KEY);
    if (!target || !target->valuestring) {
        return DUER_MSG_RSP_BAD_REQUEST;
    }

    DUER_LOGI("targetCapability %s", target->valuestring);

    duer_dcs_clear_indicator_handler();

    return DUER_OK;
}

void duer_dcs_notifications_init(void)
{
    DUER_DCS_CRITICAL_ENTER();

    if (s_is_initialized) {
        DUER_DCS_CRITICAL_EXIT();
        return;
    }

    duer_directive_list res[] = {
        {DCS_SET_INDICATOR_NAME, duer_set_indicator_cb},
        {DCS_CLEAR_INDICATOR_NAME, duer_clear_indicator_cb},
    };

    duer_add_dcs_directive_internal(res, sizeof(res) / sizeof(res[0]), DCS_NOTIFICATIONS_NAMESPACE);
    s_is_initialized = DUER_TRUE;
    DUER_DCS_CRITICAL_EXIT();
}

void duer_dcs_notifications_uninitialize_internal(void)
{
    s_is_initialized = DUER_FALSE;
}
