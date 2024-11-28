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
 * File: lightduer_dcs_infrared_control.c
 * Auth: Chang Li (changli@baidu.com)
 * Desc: Apply APIs to support DCS infrared control interface.
 *       See infrared-ray-code-private.md for details.
 */

#include "lightduer_dcs_infrared_control.h"
#include "lightduer_dcs.h"
#include "lightduer_dcs_router.h"
#include "lightduer_dcs_local.h"
#include "lightduer_connagent.h"
#include "lightduer_memory.h"
#include "lightduer_log.h"
#include "lightduer_ds_log_dcs.h"

const char *INFRARED_CONTROL_NAMESPACE =
    "ai.dueros.device_interface.infrared_ray_code";

static const char *const s_event_name_tab[] = {
    "InfraredRayCodesSendCompleted",
    "InfraredRayCodesSendFailed",
    "InfraredRayCodeReceived",
    "InfraredRayCodeReceiveTimeout"
};
static volatile duer_bool s_is_initialized = DUER_FALSE;
static char *s_token = NULL;

static int duer_get_pattern_cnt(char *pattern)
{
    int cnt = 0;
    int i = 0;

    if (!pattern) {
        return 0;
    }

    while (pattern[i]) {
        if (pattern[i++] == ',') {
            ++cnt;
        }
    }
    ++cnt;

    return cnt;
}

static int duer_construct_pattern_array(char *pattern, int pattern_cnt, duer_ray_item_t *item)
{
    char *saveptr = NULL;
    char *str = NULL;
    char *p_token = NULL;
    int  i = 0;

    if (!pattern || !item || pattern_cnt == 0) {
        return DUER_ERR_INVALID_PARAMETER;
    }

    if (pattern_cnt == 1) {
        item->pattern[0] = atoi(pattern);
        return DUER_OK;
    }

    for (str = pattern; ; str = NULL) {
        p_token = strtok_r(str, ",", &saveptr);
        if (!p_token) {
            break;
        }
        item->pattern[i++] = atoi(p_token);
    }

    /*
    printf("item info: freq=%d, in_ms=%d, cnt=%d.\n", item->carrier_freq, item->interval_ms, item->pattern_cnt);
    for (i = 0; i < pattern_cnt; i++) {
        printf("%d ", item->pattern[i]);
    }
    printf("\n");
    */

    return DUER_OK;
}

static duer_errcode_t duer_construct_ray_items(duer_ray_item_list_t *list, baidu_json *json)
{
    duer_errcode_t ret = DUER_OK;
    baidu_json *freq_json = NULL;
    baidu_json *pattern_json = NULL;
    baidu_json *in_ms_json = NULL;
    char *pattern = NULL;
    int pattern_cnt = 0;
    duer_ray_item_t *item = NULL;

    do {
        if (!list || !json) {
            ret = DUER_ERR_INVALID_PARAMETER;
            break;
        }

        freq_json = baidu_json_GetObjectItem(json, "carrierFrequency");
        in_ms_json = baidu_json_GetObjectItem(json, "sleepAfterSendInMilliseconds");
        pattern_json = baidu_json_GetObjectItem(json, "pattern");
        if (!freq_json || !pattern_json) {
            ret =  DUER_MSG_RSP_BAD_REQUEST;
            break;
        }

        pattern = pattern_json->valuestring;
        pattern_cnt = duer_get_pattern_cnt(pattern);
        if (pattern_cnt == 0) {
            DUER_LOGW("pattern_cnt is 0!");
            break;
        }

        if ((item = DUER_CALLOC(1, sizeof(*item) + sizeof(uint16_t) * pattern_cnt)) == NULL) {
            DUER_LOGE("Failed to calloc duer_ray_item_t!");
            ret = DUER_ERR_MEMORY_OVERFLOW;
            break;
        }

        item->pattern_cnt = pattern_cnt;
        item->carrier_freq = freq_json->valueint;
        if (in_ms_json) {
            item->interval_ms = in_ms_json->valueint;
        } else {
            item->interval_ms = 0;
        }
        DUER_LOGD("pattern_cnt=%d, freq=%d, in_ms=%d.\n", item->pattern_cnt, item->carrier_freq, item->interval_ms);

        ret = duer_construct_pattern_array(pattern, pattern_cnt, item);
        if (ret != DUER_OK) {
            break;
        }

        if (list->head == NULL) {
            list->head = item;
        } else {
            duer_ray_item_t *node = NULL;
            duer_ray_item_t *prev_node = NULL;
            node = list->head;
            while (node) {
                prev_node = node;
                node = node->next;
            }
            prev_node->next = item;
        }
    } while (0);

    if (ret != DUER_OK && item) {
        DUER_FREE(item);
    }

    return ret;
}

void duer_destroy_ray_items(duer_ray_item_list_t *list)
{
    duer_ray_item_t *item = NULL;
    duer_ray_item_t *p = NULL;

    if (!list) {
        return;
    }

    p = list->head;
    while (p) {
        item = p;
        p = p->next;
        DUER_FREE(item);
    }

    DUER_FREE(list);
}

static duer_status_t duer_infrared_send_cb(const baidu_json *directive)
{
    baidu_json *payload = NULL;
    baidu_json *token = NULL;
    baidu_json *ray_codes = NULL;
    baidu_json *ray_code = NULL;
    duer_status_t ret = DUER_OK;
    int ray_cnt = 0;
    int i = 0;
    duer_ray_item_list_t *list = NULL;

    char *str = baidu_json_PrintUnformatted(directive);
    if (str) {
        printf("%s: len=%d, str=%s.\n", __FUNCTION__, strlen(str), str);
        baidu_json_release(str);
    } else {
        DUER_LOGE("%s: directive parse failed!", __FUNCTION__);
    }

    payload = baidu_json_GetObjectItem(directive, DCS_PAYLOAD_KEY);
    if (!payload) {
        return DUER_MSG_RSP_BAD_REQUEST;
    }

    ray_codes = baidu_json_GetObjectItem(payload, "infraredRayCodes");
    token = baidu_json_GetObjectItem(payload, "token");
    if (!ray_codes || !token) {
        return DUER_MSG_RSP_BAD_REQUEST;
    }

    if (s_token) {
        DUER_FREE(s_token);
        s_token = NULL;
    }
    s_token = duer_strdup_internal(token->valuestring);
    DUER_LOGI("s_token=%s.", s_token);

    if ((list = DUER_CALLOC(1, sizeof(*list))) == NULL) {
        DUER_LOGE("Failed to calloc duer_ray_item_list_t!");
        return DUER_ERR_MEMORY_OVERFLOW;
    }

    ray_cnt = baidu_json_GetArraySize(ray_codes);
    DUER_LOGI("%s: ray_cnt=%d.", __FUNCTION__, ray_cnt);
    for (i = 0; i < ray_cnt; i++) {
        ray_code = baidu_json_GetArrayItem(ray_codes, i);
        if (!ray_code) {
            DUER_LOGW("ray_codes[%d] null!", i);
            continue;
        }

        ret = duer_construct_ray_items(list, ray_code);
        if (ret == DUER_MSG_RSP_BAD_REQUEST) {
            duer_destroy_ray_items(list);
            return ret;
        }
    }

    duer_dcs_infrared_send_handler(list);

    return DUER_OK;
}

static duer_status_t duer_infrared_recv_cb(const baidu_json *directive)
{
    baidu_json *payload = NULL;
    baidu_json *token = NULL;
    baidu_json *timeout = NULL;
    long timeout_ms = 0;

    char *str = baidu_json_PrintUnformatted(directive);
    if (str) {
        printf("%s: len=%d, str=%s.\n", __FUNCTION__, strlen(str), str);
        baidu_json_release(str);
    } else {
        DUER_LOGE("%s: directive parse failed!", __FUNCTION__);
    }

    payload = baidu_json_GetObjectItem(directive, DCS_PAYLOAD_KEY);
    if (!payload) {
        return DUER_MSG_RSP_BAD_REQUEST;
    }

    token = baidu_json_GetObjectItem(payload, DCS_TOKEN_KEY);
    timeout = baidu_json_GetObjectItem(payload, "timeoutInMilliseconds");
    if (!token || !timeout) {
        return DUER_MSG_RSP_BAD_REQUEST;
    }

    if (s_token) {
        DUER_FREE(s_token);
        s_token = NULL;
    }
    s_token = duer_strdup_internal(token->valuestring);
    DUER_LOGI("s_token=%s.", s_token);
    DUER_LOGI("timeout=%s.", timeout->string);

    //timeout_ms = atol(timeout->valuestring);
    timeout_ms = timeout->valueint;
    DUER_LOGI("timeout_ms=%ld", timeout_ms);

    duer_dcs_infrared_receive_handler(timeout_ms);

    return DUER_OK;
}

static char *duer_construct_pattern_string(uint16_t *array, int cnt)
{
    char *str = NULL;
    int str_len = 5 * cnt;
    int i = 0;
    int len = 0;
    int offset = 0;

    if (!array || cnt == 0) {
        return NULL;
    }

    if ((str = (char *)DUER_CALLOC(str_len, sizeof(char))) == NULL) {
        DUER_LOGE("str calloc failed!");
        return NULL;
    }

    while (cnt--) {
        if (cnt == 0) {
            len = snprintf(str + offset, str_len - offset, "%d", array[i]);
        } else {
            len = snprintf(str + offset, str_len - offset, "%d,", array[i]);
        }
        offset += len;
        i++;
    }

    return str;
}

int duer_dcs_infrared_control_report(duer_infrared_event_t event, uint16_t *array, int cnt)
{
    baidu_json *json_data = NULL;
    baidu_json *json_event = NULL;
    baidu_json *json_payload = NULL;
    char *str_pattern = NULL;
    int ret = DUER_OK;

    if (event < DCS_INFRARED_SEND_COMPLETED || event > DCS_INFRARED_RECEIVE_TIMEOUT) {
        ret = DUER_ERR_INVALID_PARAMETER;
        DUER_LOGE("Invalid paramenters!");
        return ret;
    }

    do {
        if ((json_data = baidu_json_CreateObject()) == NULL) {
            DUER_LOGE("json_date create failed!");
            ret = DUER_ERR_MEMORY_OVERFLOW;
            break;
        }

        json_event = duer_create_dcs_event(INFRARED_CONTROL_NAMESPACE, s_event_name_tab[event],
                                           DUER_TRUE);
        if (!json_event) {
            ret = DUER_ERR_FAILED;
            break;
        }

        json_payload = baidu_json_GetObjectItem(json_event, DCS_PAYLOAD_KEY);
        if (!json_payload) {
            ret = DUER_ERR_FAILED;
            break;
        }

        baidu_json_AddStringToObject(json_payload, DCS_TOKEN_KEY, s_token ? s_token : "");

        if (event == DCS_INFRARED_SEND_FAILED) {
            baidu_json_AddStringToObject(json_payload, "errorMessage", "send infrared failed!");
        }

        if (event == DCS_INFRARED_RECEIVED) {
            baidu_json_AddNumberToObject(json_payload, "carrierFrequency", 38000);

            str_pattern = duer_construct_pattern_string(array, cnt);
            if (str_pattern) {
                baidu_json_AddStringToObject(json_payload, "pattern", str_pattern);
                DUER_FREE(str_pattern);
            } else {
                baidu_json_AddStringToObject(json_payload, "pattern", "");
            }
        }

        baidu_json_AddItemToObjectCS(json_data, DCS_EVENT_KEY, json_event);
    } while (0);

    if (ret == DUER_OK) {
        duer_dcs_data_report_internal(json_data, DUER_TRUE);
    }

    if (json_data) {
        char *str = baidu_json_PrintUnformatted(json_data);
        if (str) {
            printf("%s: len=%d, str=%s.\n", __FUNCTION__, strlen(str), str);
            baidu_json_release(str);
        } else {
            DUER_LOGE("%s: directive parse failed!", __FUNCTION__);
        }

        baidu_json_Delete(json_data);
    }

    return ret;
}

void duer_dcs_infrared_control_init(void)
{
    DUER_DCS_CRITICAL_ENTER();
    if (s_is_initialized) {
        DUER_DCS_CRITICAL_EXIT();
        return;
    }

    duer_directive_list res[] = {
        {"SendInfraredRayCodes", duer_infrared_send_cb},
        {"ReceiveInfraredRayCode", duer_infrared_recv_cb}
    };

    duer_add_dcs_directive_internal(res, sizeof(res) / sizeof(res[0]), INFRARED_CONTROL_NAMESPACE);
    s_is_initialized = DUER_TRUE;
    DUER_DCS_CRITICAL_EXIT();
}

void duer_dcs_infrared_control_uninitialize_internal(void)
{
    s_is_initialized = DUER_FALSE;

    if (s_token) {
        DUER_FREE(s_token);
        s_token = NULL;
    }
}

