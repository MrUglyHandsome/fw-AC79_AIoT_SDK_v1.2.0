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
 * File: lightduer_dcs_text_input.c
 * Auth: lingfeng meng (menglingfeng@baidu.com)
 * Desc: apply APIs to support DCS Text Input Interface
 */
#include "lightduer_lib.h"
#include "lightduer_dcs.h"
#include "lightduer_dcs_local.h"
#include "lightduer_connagent.h"
#include "lightduer_log.h"
#include "lightduer_ds_log_dcs.h"

static const char *DCS_TEXT_INPUT_NAMESPACE = "ai.dueros.device_interface.text_input";
static const char *DCS_EXECUTE_TEXT_INPUT_NAME = "ExecuteTextInput";

static volatile duer_bool s_is_initialized = DUER_FALSE;

int duer_dcs_text_input(const char *query)
{
    baidu_json *event = NULL;
    baidu_json *header = NULL;
    baidu_json *payload = NULL;
    baidu_json *data = NULL;
    baidu_json *client_context = NULL;
    const char *dialog_id = NULL;
    int rs = DUER_OK;

    do {
        if (!s_is_initialized) {
            DUER_LOGW("DCS text input module has not been intialized");
            rs = DUER_ERR_FAILED;
            break;
        }

        if (!query) {
            DUER_LOGE("Invalid param: text query cannot be NULL");
            rs = DUER_ERR_INVALID_PARAMETER;
            break;
        }

        DUER_LOGD("text query: %s", query);

        data = baidu_json_CreateObject();
        if (data == NULL) {
            DUER_DS_LOG_REPORT_DCS_MEMORY_ERROR();
            rs = DUER_ERR_FAILED;
            break;
        }

        client_context = duer_get_client_context_internal();
        if (client_context) {
            baidu_json_AddItemToObject(data, DCS_CLIENT_CONTEXT_KEY, client_context);
        }

        event = duer_create_dcs_event(DCS_TEXT_INPUT_NAMESPACE,
                                      DCS_TEXT_INPUT_NAME,
                                      DUER_TRUE);
        if (event == NULL) {
            rs = DUER_ERR_FAILED;
            break;
        }

        baidu_json_AddItemToObject(data, DCS_EVENT_KEY, event);

        header = baidu_json_GetObjectItem(event, DCS_HEADER_KEY);
        dialog_id = duer_create_request_id_internal(DCS_DIALOG_TEXT_INPUT);
        if (!dialog_id) {
            DUER_LOGE("Failed to get dialog id");
            rs = DUER_ERR_FAILED;
            break;
        }

        baidu_json_AddStringToObject(header, DCS_DIALOG_REQUEST_ID_KEY, dialog_id);

        payload = baidu_json_GetObjectItem(event, DCS_PAYLOAD_KEY);
        baidu_json_AddStringToObject(payload, DCS_QUERY_KEY, query);
        rs = duer_dcs_data_report_internal(data, DUER_TRUE);
    } while (0);

    if (rs != DUER_OK) {
        duer_ds_log_dcs_event_report_fail(DCS_TEXT_INPUT_NAME);
    }

    if (data) {
        baidu_json_Delete(data);
    }
    return rs;
}

static duer_status_t duer_dcs_execute_text_input_cb(const baidu_json *directive)
{
    baidu_json *json_payload = NULL;
    baidu_json *json_token = NULL;
    baidu_json *json_query = NULL;

    if (directive) {
        char *str = baidu_json_PrintBuffered(directive, JSON_PREBUFF_SIZE_1024, 0);
        if (str) {
            DUER_LOGI("%s: len=%d, str=%s.\n", __FUNCTION__, strlen(str), str);
            baidu_json_release(str);
        } else {
            DUER_LOGE("%s: directive parse failed!", __FUNCTION__);

        }
    } else {
        DUER_LOGE("Invliad param");
        return DUER_ERR_INVALID_PARAMETER;
    }

    json_payload = baidu_json_GetObjectItem(directive, DCS_PAYLOAD_KEY);
    if (!json_payload) {
        DUER_LOGE("Failed to get payload");
        return DUER_ERR_FAILED;
    }

    json_token = baidu_json_GetObjectItem(json_payload, DCS_TOKEN_KEY);
    if (!json_token) {
        return DUER_ERR_FAILED;
    }

    json_query = baidu_json_GetObjectItem(json_payload, "query");
    if (!json_query) {
        return DUER_ERR_FAILED;
    }

    int ret = duer_dcs_text_input(json_query->valuestring);

    return ret;
}

void duer_dcs_text_input_init(void)
{

    DUER_DCS_CRITICAL_ENTER();
    if (s_is_initialized) {
        DUER_DCS_CRITICAL_EXIT();
        return;
    }

    duer_directive_list res[] = {
        {DCS_EXECUTE_TEXT_INPUT_NAME, duer_dcs_execute_text_input_cb},
    };

    duer_add_dcs_directive_internal(res, sizeof(res) / sizeof(res[0]), DCS_TEXT_INPUT_NAMESPACE);

    s_is_initialized = DUER_TRUE;
    DUER_DCS_CRITICAL_EXIT();
}

void duer_dcs_text_input_uninitialize_internal(void)
{
    s_is_initialized = DUER_FALSE;
}
