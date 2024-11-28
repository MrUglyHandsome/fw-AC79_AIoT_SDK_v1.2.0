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
/*
 * Auth: Su Hao(suhao@baidu.com)
 * Desc: Record the end to end delay
 */

#include "lightduer_ds_log_e2e.h"
#include "lightduer_log.h"
#include "lightduer_timestamp.h"
#include "lightduer_lib.h"
#include "lightduer_ds_log.h"
#include "baidu_json.h"

#ifdef DUER_STATISTICS_E2E

enum {
    DUER_DS_LOG_END2END_DELAY = 0x101,
    DUER_DS_LOG_WAKEUP_END2END_DELAY = 0x102,
};

typedef struct _duer_ds_e2e_struct {
    duer_ds_e2e_event_t _event;
    duer_u32_t          _timestamp[DUER_E2E_EVENT_TOTAL];
} duer_ds_e2e_t;

static duer_ds_e2e_t g_ds_e2e;
static volatile duer_u32_t s_local_vad_silence_time;
static volatile duer_u32_t s_if_record_codec_timestamp = 0;
static duer_ds_e2e_get_dialog_id_cb s_dialog_id_cb;

static const char *const g_tags[DUER_E2E_EVENT_TOTAL - 1] = {
    "record",
    "sendDelay",
    "response",
    "play",
    "codec",
#ifdef DUER_SECOND_WAKEUP_ENABLE
    "wakeupEnqueue",
    "wakeupSend",
    "wakeupResponse",
#endif
};

#ifdef DUER_SECOND_WAKEUP_ENABLE
static void duer_ds_wakeup_e2e_result()
{
    baidu_json *msg = NULL;
    const char *dialog_id = NULL;
    duer_u32_t wakeup_enqueue_time = 0;
    duer_u32_t wakeup_send_time = 0;
    duer_u32_t wakeup_response_time = 0;
    duer_u32_t total_spend = 0;

    msg = baidu_json_CreateObject();
    if (msg == NULL) {
        DUER_LOGE("Memory overflow!");
        return;
    }

    wakeup_enqueue_time = g_ds_e2e._timestamp[DUER_E2E_WAKEUP_SEND_ENQUEUE]
                          - g_ds_e2e._timestamp[DUER_E2E_1ST_WAKEUP];
    wakeup_send_time = g_ds_e2e._timestamp[DUER_E2E_WAKEUP_SEND]
                       - g_ds_e2e._timestamp[DUER_E2E_WAKEUP_SEND_ENQUEUE];;
    wakeup_response_time = g_ds_e2e._timestamp[DUER_E2E_2ND_WAKEUP]
                           - g_ds_e2e._timestamp[DUER_E2E_WAKEUP_SEND];;
    total_spend = g_ds_e2e._timestamp[DUER_E2E_2ND_WAKEUP]
                  - g_ds_e2e._timestamp[DUER_E2E_1ST_WAKEUP];

    baidu_json_AddNumberToObject(msg, g_tags[DUER_E2E_WAKEUP_SEND_ENQUEUE - 1], wakeup_enqueue_time);
    baidu_json_AddNumberToObject(msg, g_tags[DUER_E2E_WAKEUP_SEND - 1], wakeup_send_time);
    baidu_json_AddNumberToObject(msg, g_tags[DUER_E2E_2ND_WAKEUP - 1], wakeup_response_time);
    baidu_json_AddNumberToObject(msg, "secondWakeup-total", total_spend);

    DUER_LOGI("[XiaoduXiaodu] sendEnqueueDelay:%d, sendDelay:%d, respSpend:%d, total:%d",
              wakeup_enqueue_time,
              wakeup_send_time,
              wakeup_response_time,
              total_spend);

    if (s_dialog_id_cb) {
        dialog_id = s_dialog_id_cb();
        baidu_json_AddStringToObject(msg, "dialogRequestId", dialog_id ? dialog_id : "");
    }

    duer_ds_log(DUER_DS_LOG_LEVEL_INFO, DUER_DS_LOG_MODULE_ANALYSIS,
                DUER_DS_LOG_WAKEUP_END2END_DELAY, msg);
}
#endif // DUER_SECOND_WAKEUP_ENABLE

static void duer_ds_e2e_result(int end_index)
{
    baidu_json *msg = NULL;
    int i = 0;
    const char *dialog_id = NULL;

    msg = baidu_json_CreateObject();
    if (msg == NULL) {
        DUER_LOGE("Memory overflow!");
        return;
    }

    duer_u32_t total_spend = 0;
    // use ugly method to print all info at one time instead of print (DUER_E2E_EVENT_TOTAL - 1) times in loop
    if (s_if_record_codec_timestamp) {
        for (i = 0; i < DUER_E2E_CODEC; i++) {
            baidu_json_AddNumberToObject(msg, g_tags[i],
                                         g_ds_e2e._timestamp[i + 1] - g_ds_e2e._timestamp[i]);
        }

        total_spend = g_ds_e2e._timestamp[DUER_E2E_SEND]
                      - g_ds_e2e._timestamp[DUER_E2E_RECORD_FINISH]
                      + g_ds_e2e._timestamp[DUER_E2E_RESPONSE] - g_ds_e2e._timestamp[DUER_E2E_SEND]
                      + g_ds_e2e._timestamp[DUER_E2E_PLAY] - g_ds_e2e._timestamp[DUER_E2E_RESPONSE]
                      + g_ds_e2e._timestamp[DUER_E2E_CODEC] - g_ds_e2e._timestamp[DUER_E2E_PLAY]
                      + s_local_vad_silence_time;

        DUER_LOGI("recSpend:%d,sendDelay:%d,respSpend:%d,handleDelay:%d,codecDelay:%d,total:%d",
                  g_ds_e2e._timestamp[DUER_E2E_RECORD_FINISH]
                  - g_ds_e2e._timestamp[DUER_E2E_1ST_WAKEUP],
                  g_ds_e2e._timestamp[DUER_E2E_SEND] - g_ds_e2e._timestamp[DUER_E2E_RECORD_FINISH],
                  g_ds_e2e._timestamp[DUER_E2E_RESPONSE] - g_ds_e2e._timestamp[DUER_E2E_SEND],
                  g_ds_e2e._timestamp[DUER_E2E_PLAY] - g_ds_e2e._timestamp[DUER_E2E_RESPONSE],
                  g_ds_e2e._timestamp[DUER_E2E_CODEC] - g_ds_e2e._timestamp[DUER_E2E_PLAY],
                  total_spend);

        baidu_json_AddNumberToObject(msg, "record-codec-spend", total_spend);
    } else {
        for (i = 0; i < DUER_E2E_PLAY; i++) {
            baidu_json_AddNumberToObject(msg, g_tags[i],
                                         g_ds_e2e._timestamp[i + 1] - g_ds_e2e._timestamp[i]);
        }

        total_spend = g_ds_e2e._timestamp[DUER_E2E_SEND]
                      - g_ds_e2e._timestamp[DUER_E2E_RECORD_FINISH]
                      + g_ds_e2e._timestamp[DUER_E2E_RESPONSE] - g_ds_e2e._timestamp[DUER_E2E_SEND]
                      + g_ds_e2e._timestamp[DUER_E2E_PLAY] - g_ds_e2e._timestamp[DUER_E2E_RESPONSE]
                      + s_local_vad_silence_time;

        DUER_LOGI("recSpend:%d,sendDelay:%d,respSpend:%d,handleDelay:%d,total:%d",
                  g_ds_e2e._timestamp[DUER_E2E_RECORD_FINISH]
                  - g_ds_e2e._timestamp[DUER_E2E_1ST_WAKEUP],
                  g_ds_e2e._timestamp[DUER_E2E_SEND] - g_ds_e2e._timestamp[DUER_E2E_RECORD_FINISH],
                  g_ds_e2e._timestamp[DUER_E2E_RESPONSE] - g_ds_e2e._timestamp[DUER_E2E_SEND],
                  g_ds_e2e._timestamp[DUER_E2E_PLAY] - g_ds_e2e._timestamp[DUER_E2E_RESPONSE],
                  total_spend);

        baidu_json_AddNumberToObject(msg, "record-playHandle-spend", total_spend);
    }

    if (s_local_vad_silence_time) {
        baidu_json_AddNumberToObject(msg, "vadSilenceTime", s_local_vad_silence_time);
    }

    if (s_dialog_id_cb) {
        dialog_id = s_dialog_id_cb();
        baidu_json_AddStringToObject(msg, "dialogRequestId", dialog_id ? dialog_id : "");
    }

    duer_ds_log(DUER_DS_LOG_LEVEL_INFO, DUER_DS_LOG_MODULE_ANALYSIS, DUER_DS_LOG_END2END_DELAY, msg);
}

void duer_ds_e2e_set_report_codec_timestamp()
{
    s_if_record_codec_timestamp = 1;
}

void duer_ds_e2e_set_not_report_codec_timestamp()
{
    s_if_record_codec_timestamp = 0;
}

void duer_ds_e2e_set_vad_silence_time(duer_u32_t silence_time)
{
    s_local_vad_silence_time = silence_time;
}

void duer_ds_e2e_set_dialog_id_cb(duer_ds_e2e_get_dialog_id_cb cb)
{
    s_dialog_id_cb = cb;
}

duer_bool duer_ds_e2e_wait_response(void)
{
    return (g_ds_e2e._event == DUER_E2E_SEND) ? DUER_TRUE : DUER_FALSE;
}

void duer_ds_e2e_event(duer_ds_e2e_event_t evt)
{
    duer_u32_t current_time = 0;

    if (evt >= DUER_E2E_EVENT_TOTAL || evt < 0) {
        DUER_LOGE("error event!!!");
        return;
    }

    DUER_LOGD("event %d -> %d", g_ds_e2e._event, evt);

    if (evt > DUER_E2E_1ST_WAKEUP && (evt != g_ds_e2e._event + 1)
        && (evt != DUER_E2E_SEND || g_ds_e2e._event != DUER_E2E_SEND)) {
#ifdef DUER_SECOND_WAKEUP_ENABLE
        if (evt < DUER_E2E_WAKEUP_SEND_ENQUEUE) {
#endif
            DUER_LOGD("event %d -> %d, invalid in e2e statistic!!!", g_ds_e2e._event, evt);
            return;
#ifdef DUER_SECOND_WAKEUP_ENABLE
        }
#endif
    }

    current_time = duer_timestamp();

    if (evt == DUER_E2E_1ST_WAKEUP) {
        DUER_MEMSET(&g_ds_e2e, 0, sizeof(g_ds_e2e));
    } else {
        if (current_time < g_ds_e2e._timestamp[g_ds_e2e._event]) {
            DUER_LOGE("Wrong timestamp: %u -> %u",
                      g_ds_e2e._timestamp[g_ds_e2e._event],
                      current_time);
            return;
        }
    }

#ifdef DUER_SECOND_WAKEUP_ENABLE
    if (evt < DUER_E2E_WAKEUP_SEND_ENQUEUE) {
#endif
        g_ds_e2e._event = evt;
#ifdef DUER_SECOND_WAKEUP_ENABLE
    }
#endif
    g_ds_e2e._timestamp[evt] = current_time;

    if (!s_if_record_codec_timestamp) {
        if (evt == DUER_E2E_PLAY) {
            duer_ds_e2e_result(evt);
        }
    } else {
        if (evt == DUER_E2E_CODEC) {
            duer_ds_e2e_result(evt);
        }
    }

#ifdef DUER_SECOND_WAKEUP_ENABLE
    if (evt == DUER_E2E_2ND_WAKEUP) {
        duer_ds_wakeup_e2e_result();
    }
#endif//DUER_SECOND_WAKEUP_ENABLE
}

#endif

