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
 * File: lightduer_voice.c
 * Auth: Su Hao (suhao@baidu.com)
 * Desc: Light duer voice procedure.
 */

#include "fs/fs.h"
#include "printf.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "duerapp_config.h"

#include "lightduer_log.h"
#include "lightduer_memory.h"
#include "lightduer_events.h"
#include "lightduer_connagent.h"
#include "lightduer_voice.h"
#include "lightduer_random.h"
#if !USE_OWN_SPEEX_LIB
#include "lightduer_speex.h"
#endif
#include "lightduer_lib.h"
#include "lightduer_session.h"

#include "mbedtls/base64.h"

#include "lightduer_ds_log_recorder.h"
#include "lightduer_timestamp.h"
#include "lightduer_ds_log_e2e.h"
#include "lightduer_queue_cache.h"
#include "lightduer_mutex.h"
#include "lightduer_sleep.h"
#include "lightduer_dcs.h"

#ifdef ENABLE_DUER_STORE_VOICE
#include "lightduer_store_voice.h"
#endif // ENABLE_DUER_STORE_VOICE

//unused static void *_encoder_buffer;

#define O_BUFFER_LEN        (800 + 16)
#define I_BUFFER_LEN        ((O_BUFFER_LEN - 1) / 4 * 3)
#define MAX_CACHED_VOICE_SEGMENTS ((5000) / ((O_BUFFER_LEN) / 4))

typedef struct _duer_topic_s {
    duer_u32_t  _id;
    duer_u32_t  _samplerate;
    duer_u32_t  _segment;
    duer_bool   _eof;
    duer_qcache_handler cache;
    duer_bool _need_cache;
} duer_topic_t;

typedef struct _duer_voice_statistics {
    duer_u32_t  _topic_id;
    duer_u32_t  _segment;
    duer_bool   _eof;
    duer_u32_t  _request;
    duer_u32_t  _start;
    duer_u32_t  _finish;
} duer_vstat_t;

typedef struct _duer_cache_item_s {
    baidu_json    *value;
    duer_context_t  context;
} duer_cache_item_t;

static duer_topic_t     g_topic;
static duer_mutex_t     s_mutex;

static char     _data[I_BUFFER_LEN];
static size_t   _size;

#ifdef DUER_SECOND_WAKEUP_ENABLE
static duer_wakeup_bit _wakeup_bits = 0;
static duer_voice_send_state s_voice_send_state = DUER_VOICE_SEND_STATE_DEFAULT;
static duer_voice_cache_t *s_wakeup_voice_cache = NULL;
static int s_wakeup_eof_num = 0;
#endif//DUER_SECOND_WAKEUP_ENABLE

#if !USE_OWN_SPEEX_LIB
static duer_speex_handler     _speex = NULL;
#endif

#ifdef DUER_VOICE_SEND_ASYNC
static duer_events_handler g_events_handler = NULL;
#endif

static duer_voice_mode s_voice_mode = DUER_VOICE_MODE_DEFAULT;
static duer_mutex_t s_func_mutex = NULL;

static duer_u32_t s_voice_delay_threshold = (duer_u32_t) - 1;
static duer_voice_delay_func s_voice_delay_callback = NULL;

// duer_voice cache:
#define DUER_VOICE_CACHE_SIZE (20*5) // (20 * 5) * (20 ms) = 2000 ms
#define DUER_SPEEX_ENCODE_SIZE (46) // speex enocde size

static duer_voice_cache_t *s_voice_cache = NULL;

static duer_get_dialog_id_func s_dialog_id_cb;

#ifndef DUER_COMPRESS_QUALITY
#define DUER_COMPRESS_QUALITY           (5)
#endif
static int s_speex_quality = DUER_COMPRESS_QUALITY;

static void local_mutex_lock(duer_bool status)
{
    duer_status_t rs;

    if (status) {
        rs = duer_mutex_lock(s_mutex);
    } else {
        rs = duer_mutex_unlock(s_mutex);
    }

    if (rs != DUER_OK) {
        DUER_LOGE("lock failed: rs = %d, status = %d", rs, status);
    }
}

static void duer_cache_item_release(void *param)
{
    duer_cache_item_t *item = (duer_cache_item_t *)param;

    if (item) {
        if (item->value) {
            baidu_json_Delete(item->value);
            item->value = NULL;
        }

        if (item->context._param) {
            DUER_FREE(item->context._param);
            item->context._param = NULL;
        }

        DUER_FREE(item);
    }
}

static int duer_request_send_start(duer_context_t *context)
{
    duer_vstat_t *stat = context ? context->_param : NULL;

    if (stat) {
        local_mutex_lock(DUER_TRUE);
        duer_cache_item_t *item = duer_qcache_top(g_topic.cache);
        local_mutex_lock(DUER_FALSE);

        if (item == NULL || item->context._param != stat) {
            DUER_LOGE("The qcache is not matched, may be destroyed, stat:%p, item:%p", stat, item);
        } else {
            stat->_start = duer_timestamp();
        }
    }

    return DUER_OK;
}

static int duer_request_send_finish(duer_context_t *context, duer_ds_e2e_event_t evt)
{
    duer_vstat_t *stat = context ? context->_param : NULL;
    duer_u32_t delay = 0;

    if (stat) {
        int context_status = context->_status;
        local_mutex_lock(DUER_TRUE);
        duer_cache_item_t *item = duer_qcache_pop(g_topic.cache);
        local_mutex_lock(DUER_FALSE);
        if (context_status != DUER_CANCEL) {
            duer_ds_e2e_event(evt);
        }

        if (item == NULL || item->context._param != stat) {
            DUER_LOGE("The qcache is not matched, may be destroyed, item:%p, stat:%p", item, stat);
        } else {
            stat->_finish = duer_timestamp();
            DUER_LOGD("finish send segment: %d\n", stat->_segment);
            delay = stat->_finish - stat->_request;
            if (context_status != DUER_CANCEL) {
                DUER_LOGD("id: %d, segment: %d, eof: %d, request: %u, start: %u, finish: %u, "
                          "send delay: %u, sent usage: %u",
                          stat->_topic_id, stat->_segment, stat->_eof, stat->_request,
                          stat->_start, stat->_finish,
                          stat->_start - stat->_request, stat->_finish - stat->_start);
            } else {
                DUER_LOGD("id: %d, segment: %d, eof: %d, request: %u, start: %u, stop: %u",
                          stat->_topic_id, stat->_segment, stat->_eof, stat->_request,
                          stat->_start, stat->_finish);
            }
            if (context_status != DUER_CANCEL) {
                duer_ds_rec_delay_info_update(stat->_request, stat->_start, stat->_finish);
                if (delay > s_voice_delay_threshold && s_voice_delay_callback) {
                    s_voice_delay_callback(stat->_finish - stat->_request);
                }
            }

            duer_cache_item_release(item);

            local_mutex_lock(DUER_TRUE);
            item = duer_qcache_top(g_topic.cache);

            if (item) {
                duer_data_report_async(&(item->context), item->value);

                if (item->value) {
                    baidu_json_Delete(item->value);
                    item->value = NULL;
                }
            }

            local_mutex_lock(DUER_FALSE);
        }
    } else {
        DUER_LOGE("No stat in the request!!!");
    }

    return DUER_OK;
}

static int duer_request_send_finish_callback(duer_context_t *context)
{
    return duer_request_send_finish(context, DUER_E2E_SEND);
}

#ifdef DUER_SECOND_WAKEUP_ENABLE
static int duer_request_send_wakeup_finish_callback(duer_context_t *context)
{
    return duer_request_send_finish(context, DUER_E2E_WAKEUP_SEND);
}
#endif//DUER_SECOND_WAKEUP_ENABLE

void duer_add_translate_flag(baidu_json *data)
{
    if (!data) {
        DUER_LOGE("Invalid param: data cannot be null");
        return;
    }

    switch (s_voice_mode) {
    case DUER_VOICE_MODE_CHINESE_TO_ENGLISH:
        baidu_json_AddStringToObject(data, "translate", "c2e");
        break;

    case DUER_VOICE_MODE_ENGLISH_TO_CHINESE:
        baidu_json_AddStringToObject(data, "translate", "e2c");
        break;

    case DUER_VOICE_MODE_WCHAT:
        baidu_json_AddNumberToObject(data, "wchat", 1);
        break;

    case DUER_VOICE_MODE_C2E_BOT:
        baidu_json_AddStringToObject(data, "translate", "duer_c2e");
        break;

    case DUER_VOICE_MODE_INTERACTIVE_CLASS:
        baidu_json_AddStringToObject(data, "interactive", "class");
        break;

    default:
        break;
    }
}

void duer_reg_dialog_id_cb(duer_get_dialog_id_func cb)
{
    s_dialog_id_cb = cb;
}

static int duer_send_content(const void *data, size_t size, int eof, int wakeup_bits)
{
#ifdef DUER_SECOND_WAKEUP_ENABLE
    duer_u32_t current_is_wakeup = 0;
#endif//DUER_SECOND_WAKEUP_ENABLE
    baidu_json *voice = NULL;
    baidu_json *value = NULL;
    duer_vstat_t *stat = NULL;
    duer_u32_t timestamp;
    duer_cache_item_t *item = NULL;
    duer_cache_item_t *top = NULL;
    int voice_segments_num = 0;

    int rs = DUER_ERR_FAILED;
    size_t olen;
    char buff[O_BUFFER_LEN];
    const char *dialog_id = NULL;

    DUER_LOGV("duer_send_content ==>");
    local_mutex_lock(DUER_TRUE);
    voice_segments_num = duer_qcache_length(g_topic.cache);
    printf("voice_segments_num  ===%d\n", voice_segments_num);
    local_mutex_lock(DUER_FALSE);

    if (voice_segments_num > MAX_CACHED_VOICE_SEGMENTS) {
        // ignore the voice data
        DUER_LOGW("too many voice segmetns cached, max:%d!!!", MAX_CACHED_VOICE_SEGMENTS);
        return rs;
    }

    do {
        g_topic._eof = eof || data == NULL || size == 0;

        if (data != NULL && size > 0) {
#ifdef ENABLE_DUER_STORE_VOICE
            duer_store_speex_write(data, size);
#endif // ENABLE_DUER_STORE_VOICE
            rs = mbedtls_base64_encode(
                     (unsigned char *)buff,
                     O_BUFFER_LEN,
                     &olen,
                     (const unsigned char *)data, size);

            if (rs < 0) {
                DUER_LOGE("Encode the speex data failed: rs = %d", rs);
                break;
            }
        }

        timestamp = duer_timestamp();

        item = DUER_MALLOC(sizeof(duer_cache_item_t));

        if (item == NULL) {
            DUER_LOGE("Memory overflow!!!");
            break;
        }

        DUER_MEMSET(item, 0, sizeof(duer_cache_item_t));

        voice = baidu_json_CreateObject();

        if (voice == NULL) {
            DUER_LOGE("Memory overflow!!!");
            break;
        }

        value = baidu_json_CreateObject();

        if (value == NULL) {
            DUER_LOGE("Memory overflow!!!");
            break;
        }

        stat = DUER_MALLOC(sizeof(duer_vstat_t));

        if (stat) {
            DUER_MEMSET(stat, 0, sizeof(duer_vstat_t));
            stat->_request = timestamp;
            stat->_topic_id = g_topic._id;
            stat->_segment = g_topic._segment;
            stat->_eof = g_topic._eof;
        } else {
            DUER_LOGE("Memory overflow!!!");
        }

        baidu_json_AddNumberToObjectCS(voice, "id", g_topic._id);
        baidu_json_AddNumberToObjectCS(voice, "segment", g_topic._segment++);
        baidu_json_AddNumberToObjectCS(voice, "rate", g_topic._samplerate);
        baidu_json_AddNumberToObjectCS(voice, "channel", 1);
#ifdef DUER_SECOND_WAKEUP_ENABLE
        current_is_wakeup = (wakeup_bits & DUER_WAKEUP_BIT) > 0;
        if (current_is_wakeup) {
            baidu_json_AddNumberToObjectCS(voice, "wakeup",
                                           (wakeup_bits & DUER_WAKEUP_EOF_BIT) ? s_wakeup_eof_num : 0);

            DUER_LOGD("segment: %d, wakeup: %d, wakeup_eof: %d",
                      stat->_segment,
                      (wakeup_bits & DUER_WAKEUP_BIT) ? 1 : 0,
                      (wakeup_bits & DUER_WAKEUP_EOF_BIT) ? s_wakeup_eof_num : 0);
        } else {
            DUER_LOGD("segment: %d, is not wakeup voice.", stat->_segment);
        }
#endif//DUER_SECOND_WAKEUP_ENABLE
        baidu_json_AddNumberToObjectCS(voice, "eof", g_topic._eof ? 1 : 0);
        duer_add_translate_flag(voice);
        if (s_dialog_id_cb) {
            dialog_id = s_dialog_id_cb();
            if (dialog_id) {
                baidu_json_AddStringToObject(voice, "dialogRequestId", dialog_id);
            }
        }

        baidu_json_AddNumberToObjectCS(voice, "ts", timestamp);

#ifdef DISABLE_SPX_ON_WCHAT

        if (duer_voice_get_mode() == DUER_VOICE_MODE_WCHAT) {
            baidu_json_AddStringToObjectCS(voice, "format", "pcm");
        }

#endif

        if (data != NULL && size > 0) {
            baidu_json_AddStringToObjectCS(voice, "voice_data", buff);
        }

        baidu_json_AddItemToObjectCS(value, "duer_voice", voice);
        voice = NULL;

        item->context._param = stat;
        item->context._on_report_start = duer_request_send_start;
#ifdef DUER_SECOND_WAKEUP_ENABLE
        if (current_is_wakeup) {
            item->context._on_report_finish = duer_request_send_wakeup_finish_callback;
        } else {
            item->context._on_report_finish = duer_request_send_finish_callback;
        }
#else // if second wakeup disabled.
        item->context._on_report_finish = duer_request_send_finish_callback;
#endif//DUER_SECOND_WAKEUP_ENABLE
        item->value = value;
        value = NULL;

        local_mutex_lock(DUER_TRUE);
        top = duer_qcache_top(g_topic.cache);
        rs = duer_qcache_push(g_topic.cache, item);
        local_mutex_lock(DUER_FALSE);

        if (rs < DUER_OK) {
            DUER_LOGE("send segment %d fail for topic_id:%d!", g_topic._segment, g_topic._id);
            --g_topic._segment;
            break;
        }

        if (top == NULL) {
            rs = duer_data_report_async(&(item->context), item->value);
            if (rs < DUER_OK) {
                DUER_LOGE("send segment %d fail for topic_id:%d", g_topic._segment, g_topic._id);
                local_mutex_lock(DUER_TRUE);
                duer_cache_item_t *tmp_item = duer_qcache_pop(g_topic.cache);
                local_mutex_lock(DUER_FALSE);
                --g_topic._segment;
                if (tmp_item == NULL) {
                    DUER_LOGW("qcache has been finalized!");
                    return rs;
                }
                break;
            }
        }

        item = NULL;
    } while (0);

    DUER_LOGV("duer_send_content <== rs = %d", rs);

    if (item != NULL) {
        if (item->value) {
            baidu_json_Delete(item->value);
        }

        if (item->context._param) {
            DUER_FREE(item->context._param);
        }

        DUER_FREE(item);
    }

#ifdef DUER_SECOND_WAKEUP_ENABLE
    if (current_is_wakeup) {
        duer_ds_e2e_event(DUER_E2E_WAKEUP_SEND_ENQUEUE);
    }
#endif//DUER_SECOND_WAKEUP_ENABLE

    return rs;
}

static void duer_voice_clean_cache(duer_voice_cache_t *cache)
{
    int i = 0;

    if (cache == NULL) {
        DUER_LOGE("voice cache haven't initialized");
        return;
    }

    for (i = 0; i < (cache->size); ++i) {
        if (cache->items[i]) {
            DUER_FREE(cache->items[i]);
            cache->items[i] = NULL;
        }
    }

    cache->tail = 0;
    cache->head = 0;
}

static int duer_voice_save_cache(duer_voice_cache_t *cache, const char *buff_ps)
{
    if (cache == NULL) {
        DUER_LOGE("voice cache haven't initialized");
        return -1;
    }

    // check  if  the neweset covered the oldest
    if (cache->tail == cache->head && cache->items[cache->head] != NULL) {
        cache->head = (cache->head + 1) % cache->size;
    }

    // check if memory malloc
    if (NULL == cache->items[cache->tail]) {
        char *tmp = DUER_MALLOC(DUER_SPEEX_ENCODE_SIZE);

        if (NULL == tmp) {
            DUER_LOGE("[duer_voice_save_cache] malloc fail\n");
            return DUER_ERR_FAILED;
        }

        cache->items[cache->tail] = tmp;
    }

    // store voice
    DUER_MEMCPY(cache->items[cache->tail], buff_ps,  DUER_SPEEX_ENCODE_SIZE);

    // new_index  ++
    cache->tail = (cache->tail + 1) % cache->size;

    //DUER_LOGE("(%d,%d)",cache->head,cache->tail);
    return 0;
}

static char *duer_voice_get_cache(duer_voice_cache_t *cache)
{
    char *old_voice = NULL;

    if (cache == NULL) {
        DUER_LOGE("voice cache haven't initialized");
        return NULL;
    }

    old_voice = cache->items[cache->head];
    cache->items[cache->head] = NULL;

    // check  if  cache empty
    if (old_voice == NULL) {
        ;
    } else {
        // old_index ++
        cache->head = (cache->head + 1) % cache->size;
    }

    //DUER_LOGE("[%d,%d]",cache->head,cache->tail);
    return old_voice;
}

static duer_bool duer_voice_not_empty(duer_voice_cache_t *cache)
{
    if (cache == NULL) {
        DUER_LOGE("voice cache haven't initialized");
        return DUER_FALSE;
    }

    //DUER_LOGE("[duer_voice_not_empty] %d %d %p %p",cache->head,cache->tail,cache->items[cache->head],cache->items[cache->tail]);
    if (NULL == cache->items[cache->head]) {
        //DUER_LOGE(" empty\n");
        return DUER_FALSE;
    } else {
        //DUER_LOGE("not empty\n");
        return DUER_TRUE;
    }
}

static void duer_voice_init_cache(duer_voice_cache_t **p_cache, int size)
{
    if (*p_cache) {
        if (size == (*p_cache)->size) {
            return;
        } else {
            DUER_FREE((*p_cache));
            (*p_cache) = NULL;
        }
    }

    size_t mem_size = (sizeof(duer_voice_cache_t) + sizeof(char *) * size);
    (*p_cache) = (duer_voice_cache_t *)DUER_MALLOC(mem_size);
    if ((*p_cache) == NULL) {
        DUER_LOGE("[duer_voice_init_cache] malloc fail\n");
        return;
    }

    memset((*p_cache), 0, mem_size);
    (*p_cache)->size = size;
    (*p_cache)->tail = 0;
    (*p_cache)->head = 0;
}

static void duer_voice_uninit_cache(duer_voice_cache_t **p_cache)
{
    DUER_FREE(*p_cache);
    (*p_cache) = NULL;
}

static int duer_voice_cache_size(duer_voice_cache_t *cache)
{
    if (cache == NULL) {
        DUER_LOGE("voice cache haven't initialized");
        return -1;
    }

    if (cache->tail > cache->head) {
        return cache->tail - cache->head;
    } else {
        return cache->tail + (cache->size - cache->head);
    }
}

#if !USE_OWN_SPEEX_LIB
static void duer_speex_encoded_callback_send(const void *data, size_t size,
        int wakeup_bits)
{
    //DUER_LOGD("_size + size:%d, I_BUFFER_LEN:%d", _size + size, I_BUFFER_LEN);
    if (_size + size > I_BUFFER_LEN) {
#ifdef DUER_SECOND_WAKEUP_ENABLE
        duer_send_content(_data, _size, 0, _wakeup_bits);
        _wakeup_bits = 0;
#else// if second wakeup disabled
        duer_send_content(_data, _size, 0, 0);
#endif//DUER_SECOND_WAKEUP_ENABLE
        _size = 0;
    }

    DUER_MEMCPY(_data + _size, data, size);
    _size += size;
#ifdef DUER_SECOND_WAKEUP_ENABLE
    _wakeup_bits = wakeup_bits;
#endif//DUER_SECOND_WAKEUP_ENABLE
}

static void duer_speex_encoded_callback(const void *data, size_t size)
{
#ifdef DUER_SECOND_WAKEUP_ENABLE
    static duer_u32_t old_topic_id = 0;
    //int count = 0;

    if (s_voice_send_state != DUER_VOICE_SEND_STATE_DEFAULT) {
        if (s_wakeup_voice_cache == NULL) {
            DUER_LOGE("call duer_voice_second_wakeup_init at first!");
        }

        // Manipulate cached data.
        switch (s_voice_send_state) {
        case DUER_VOICE_SEND_STATE_WAKEUP:
            //count = 0;
            if (old_topic_id != g_topic._id &&
                duer_session_is_matched(g_topic._id) == DUER_TRUE) {
                old_topic_id = g_topic._id;
                //DUER_LOGI("before send_wakeup_count = %d, _size: %d", count, _size);
                while (1) {
                    char *voice_data = NULL;
                    duer_bool wakeup_eof = DUER_FALSE;
                    if (duer_voice_not_empty(s_wakeup_voice_cache)) {
                        voice_data = duer_voice_get_cache(s_wakeup_voice_cache);
                        wakeup_eof = !duer_voice_not_empty(s_wakeup_voice_cache);

                        if (voice_data) {
                            //++count;
                            duer_speex_encoded_callback_send(voice_data, DUER_SPEEX_ENCODE_SIZE,
                                                             DUER_WAKEUP_BIT);
                            DUER_FREE(voice_data);
                        }
                    } else {
                        break;
                    }

                }
                //DUER_LOGI("end send_wakeup_count = %d, _size: %d", count, _size);
            }
            break;
        default:
            break;
        }

        // Process new data.
        if (size > 0) {
            switch (s_voice_send_state) {
            case DUER_VOICE_SEND_STATE_WAKEUP:
                // set second wakeup bit.
                duer_speex_encoded_callback_send(data,  size, DUER_WAKEUP_BIT);
                break;
            case DUER_VOICE_SEND_STATE_QUERY:
                // clear wakeup bit.
                duer_speex_encoded_callback_send(data,  size, 0);
                break;
            default:
                break;
            }

            // Process caching.
            duer_voice_save_cache(s_wakeup_voice_cache, data);
        }
    } else {
#endif//DUER_SECOND_WAKEUP_ENABLE
        if (g_topic._need_cache) {
            duer_voice_save_cache(s_voice_cache, data);
        } else {
            while (duer_voice_not_empty(s_voice_cache)) {
                char *voice_data = duer_voice_get_cache(s_voice_cache);

                if (voice_data) {
                    duer_speex_encoded_callback_send(voice_data, DUER_SPEEX_ENCODE_SIZE, 0);
                    DUER_FREE(voice_data);
                }
            }

            duer_speex_encoded_callback_send(data,  size, 0);
        }
#ifdef DUER_SECOND_WAKEUP_ENABLE
    }
#endif//DUER_SECOND_WAKEUP_ENABLE
}
#endif

static void duer_send_original_voice(const void *data, size_t size)
{
    if (g_topic._need_cache) {
        DUER_LOGE("TODO: vad cache does not support original voice");
    }

    while (_size + size > I_BUFFER_LEN) {
        if (_size < I_BUFFER_LEN) {
            size_t left = I_BUFFER_LEN - _size;
            DUER_MEMCPY(_data + _size, data, left);
            _size += left;
            data = (const char *)data + left;
            size -= left;
        }

        duer_send_content(_data, _size, 0, 0);
        _size = 0;
    }

    DUER_MEMCPY(_data + _size, data, size);
    _size += size;
}

size_t duer_increase_topic_id(void)
{
    return duer_session_generate();
}

static void duer_voice_terminate_internal(int what, void *object)
{
    //duer_cache_item_t *top = NULL; //unused
    duer_session_consume(g_topic._id);

    local_mutex_lock(DUER_TRUE);

    if (g_topic.cache) {
        duer_qcache_destroy_traverse(g_topic.cache, duer_cache_item_release);
        g_topic.cache = NULL;
    }

    local_mutex_lock(DUER_FALSE);
}

static void duer_voice_start_internal(int what, void *object)
{
#ifdef DUER_SECOND_WAKEUP_ENABLE
    if (s_wakeup_voice_cache && duer_voice_not_empty(s_wakeup_voice_cache)) {
        duer_voice_clean_cache(s_wakeup_voice_cache);
    }
#endif

    if (s_voice_cache && duer_voice_not_empty(s_voice_cache)) {
        duer_voice_clean_cache(s_voice_cache);
    }

    duer_voice_terminate_internal(what, object);

#if !USE_OWN_SPEEX_LIB
    if (_speex) {
        duer_speex_destroy(_speex);
        _speex = NULL;
    }
#endif

    // Initialize the topic parameters
    g_topic._samplerate = what;
    g_topic._id = duer_session_generate();
    g_topic._segment = 0;
    g_topic._eof = DUER_FALSE;
    local_mutex_lock(DUER_TRUE);
    g_topic.cache = duer_qcache_create();
    local_mutex_lock(DUER_FALSE);

#ifdef DISABLE_SPX_ON_WCHAT

    if (duer_voice_get_mode() != DUER_VOICE_MODE_WCHAT)
#endif
    {
#if !USE_OWN_SPEEX_LIB
        _speex = duer_speex_create(g_topic._samplerate, s_speex_quality);
#endif
    }

    _size = 0;

    duer_ds_log_rec_start(g_topic._id);
#ifdef ENABLE_DUER_STORE_VOICE
    duer_store_voice_start(g_topic._id);
#endif // ENABLE_DUER_STORE_VOICE
}

static void duer_voice_send_internal(int what, void *object)
{
    if (object) {
        if (duer_session_is_matched(g_topic._id) == DUER_TRUE) {
            DUER_LOGD("duer_voice_send_internal");
#ifdef DISABLE_SPX_ON_WCHAT

            if (duer_voice_get_mode() == DUER_VOICE_MODE_WCHAT) {
                duer_send_original_voice(object, (size_t)what);
                goto out;
            }

#endif

#if !USE_OWN_SPEEX_LIB
            if (_speex) {
#ifdef ENABLE_DUER_PRINT_SPEEX_COST
                duer_u32_t t1 = duer_timestamp();
#endif // ENABLE_DUER_PRINT_SPEEX_COST
                duer_speex_encode(_speex, object, (size_t)what, duer_speex_encoded_callback);
#ifdef ENABLE_DUER_PRINT_SPEEX_COST
                duer_u32_t t2 = duer_timestamp();
                DUER_LOGI("size:%d, speex cost:%d", what, t2 - t1);
#endif // ENABLE_DUER_PRINT_SPEEX_COST
            }

#else
            if (what) {
                int len = 0;

                do {
                    if (len + I_BUFFER_LEN > what) {
                        duer_send_content((unsigned char *)object + len, what - len, 0, 0);
                        break;
                    } else {
                        duer_send_content((unsigned char *)object + len, I_BUFFER_LEN, 0, 0);
                        len += I_BUFFER_LEN;
                    }
                } while (len < what);
            }
#endif
        }
out:
        ;
#ifdef DUER_VOICE_SEND_ASYNC
        DUER_FREE(object);
#endif
    }
}

static void duer_voice_cache_internal(int what, void *object)
{
    g_topic._need_cache = what;
}

static void duer_voice_stop_internal(int what, void *object)
{
#ifdef DUER_SECOND_WAKEUP_ENABLE
    if (s_wakeup_voice_cache && duer_voice_not_empty(s_wakeup_voice_cache)) {
        duer_voice_clean_cache(s_wakeup_voice_cache);
    }
#endif

    if (s_voice_cache && duer_voice_not_empty(s_voice_cache)) {
        duer_voice_clean_cache(s_voice_cache);
    }

    if (duer_session_is_matched(g_topic._id) == DUER_TRUE) {
#if !USE_OWN_SPEEX_LIB
        if (_speex) {
            duer_speex_encode(_speex, NULL, 0, duer_speex_encoded_callback);
        }
#endif
        if (_size > 0 || g_topic._segment > 0) {
#if !USE_OWN_SPEEX_LIB
            duer_send_content(_data, _size, 1, 0, 0);
#else
            if (what >= I_BUFFER_LEN) {
                duer_send_content((unsigned char *)object, I_BUFFER_LEN, 1, 0);
            } else {
                duer_send_content((unsigned char *)object, what, 1, 0);
            }
#endif
        }
    }

    duer_ds_log_rec_stop(g_topic._id);
    _size = 0;

#if !USE_OWN_SPEEX_LIB
    if (_speex) {
        duer_speex_destroy(_speex);
        _speex = NULL;
    }
#endif

#ifdef ENABLE_DUER_STORE_VOICE
    duer_store_voice_end();
#endif // ENABLE_DUER_STORE_VOICE

    // consume the topic id, so that we can check if voice stopped.
    duer_session_consume(g_topic._id);
}

static int duer_events_call_internal(duer_events_func func, int what, void *object)
{
#ifdef ENABLE_DUER_STORE_VOICE
    duer_store_voice_write(object, what);
#endif // ENABLE_DUER_STORE_VOICE
#ifdef DUER_VOICE_SEND_ASYNC

    if (g_events_handler) {
        return duer_events_call(g_events_handler, func, what, object);
    } else {
        DUER_LOGE("duer_events_call_internal, handler not initialied...");
    }

    return DUER_ERR_FAILED;
#else

    if (func) {
        if (!s_func_mutex || !s_mutex) {
            DUER_LOGW("voice has finalized, s_func_mutex:%p, s_mutex:%p",
                      s_func_mutex, s_mutex);
            return DUER_ERR_FAILED;
        }

        duer_mutex_lock(s_func_mutex);
        func(what, object);
        duer_mutex_unlock(s_func_mutex);
    }

    return DUER_OK;
#endif
}

int duer_voice_initialize(void)
{
#ifdef DUER_VOICE_SEND_ASYNC

    if (g_events_handler == NULL) {
        g_events_handler = duer_events_create("lightduer_voice", 1024 * 6, 64);
    }

#endif

    if (!s_mutex) {
        s_mutex = duer_mutex_create();
    }

    if (!s_func_mutex) {
        DUER_LOGI("Mutex initializing");
        s_func_mutex = duer_mutex_create();
    }

    if (!s_func_mutex) {
        duer_voice_init_cache(&s_voice_cache, DUER_VOICE_CACHE_SIZE);
    }

    s_voice_mode = DUER_VOICE_MODE_DEFAULT;

    return DUER_OK;
}

void duer_voice_finalize(void)
{
#ifdef DUER_VOICE_SEND_ASYNC

    if (g_events_handler) {
        duer_events_destroy(g_events_handler);
        g_events_handler = NULL;
    }

#endif

    duer_voice_terminate_internal(0, NULL);

    if (s_mutex) {
        duer_mutex_destroy(s_mutex);
        s_mutex = NULL;
    }

    if (s_func_mutex) {
        duer_mutex_destroy(s_func_mutex);
        s_func_mutex = NULL;
    }

    if (s_func_mutex) {
        duer_voice_uninit_cache(&s_voice_cache);
    }
}

void duer_voice_set_delay_threshold(duer_u32_t delay, duer_voice_delay_func func)
{
    s_voice_delay_threshold = delay;
    s_voice_delay_callback = func;
}

void duer_voice_set_mode(duer_voice_mode mode)
{
    s_voice_mode = mode;
}

duer_voice_mode duer_voice_get_mode(void)
{
    return s_voice_mode;
}

int duer_voice_start(int sample_rate)
{
    duer_dcs_on_listen_started(sample_rate);
    return duer_events_call_internal(duer_voice_start_internal, sample_rate, NULL);
}

int duer_voice_send(const void *data, size_t size)
{
    int rs = DUER_ERR_FAILED;

#ifdef DUER_VOICE_SEND_ASYNC
    void *buff = NULL;

    do {
        buff = DUER_MALLOC(size);

        if (!buff) {
            DUER_LOGE("Alloc temp memory failed!");
            break;
        }

        DUER_MEMCPY(buff, data, size);

        rs = duer_events_call_internal(duer_voice_send_internal, (int)size, buff);

        if (rs != DUER_OK) {
            DUER_FREE(buff);
        }
    } while (0);

#else
    DUER_LOGD("duer_voice_send");

    rs = duer_events_call_internal(duer_voice_send_internal, (int)size, (void *)data);

#endif

    return rs;
}

int duer_voice_cache(duer_bool cached)
{
    return duer_events_call_internal(duer_voice_cache_internal, cached, NULL);
}

int duer_voice_stop(const void *data, int size)
{
    return duer_events_call_internal(duer_voice_stop_internal, size, (void *)data);
}

int duer_voice_terminate(void)
{
    return duer_events_call_internal(duer_voice_terminate_internal, 0, NULL);
}

int duer_voice_set_speex_quality(int quality)
{
    // the valid range of speex quality is [0,10]
    if (quality < 0 || quality > 10) {
        DUER_LOGE("Invalid speex quality");
        return DUER_ERR_FAILED;
    }

    s_speex_quality = quality;
    return DUER_OK;
}

#ifdef DUER_SECOND_WAKEUP_ENABLE
void duer_voice_truncate_wakeup_cache(int count)
{
    int cur_count = 0;
    char *voice_data = NULL;

    if (s_wakeup_voice_cache == NULL) {
        DUER_LOGE("s_wakeup_voice_cache haven't initialized!");
        return;
    }

    if (!duer_voice_not_empty(s_wakeup_voice_cache)) {
        return;
    }

    cur_count = duer_voice_cache_size(s_wakeup_voice_cache);
    while (cur_count > count) {
        voice_data = duer_voice_get_cache(s_wakeup_voice_cache);
        if (voice_data) {
            DUER_FREE(voice_data);
        }
        --cur_count;
    }
}

static void duer_voice_restart_internal(int what, void *object)
{
    // discard old id.
    duer_session_consume(g_topic._id);

    // Initialize the topic parameters
    g_topic._id = duer_session_generate();
    g_topic._segment = 0;
    g_topic._eof = DUER_FALSE;

    _size = 0;
}

static void duer_voice_send_wakeup_internal(int what, void *object)
{
    if (duer_session_is_matched(g_topic._id) == DUER_TRUE) {
        s_wakeup_eof_num = what;
        if (_size > 0) {
            duer_send_content(_data, _size, 0, DUER_WAKEUP_BIT | DUER_WAKEUP_EOF_BIT);
            _size = 0;
        }
    }
}

void duer_voice_second_wakeup_init(int cache_size)
{
    duer_voice_init_cache(&s_wakeup_voice_cache, cache_size);
    DUER_LOGD("Init wakeup cache with size: %d", cache_size);
}

void duer_voice_second_wakeup_uninit()
{
    duer_voice_uninit_cache(&s_wakeup_voice_cache);
}

duer_bool duer_voice_is_started()
{
    return duer_session_is_matched(g_topic._id);
}

void duer_voice_set_send_state(duer_voice_send_state send_state)
{
    s_voice_send_state = send_state;
}

int duer_voice_restart(void)
{
    return duer_events_call_internal(duer_voice_restart_internal, 0, NULL);
}

int duer_voice_send_wakeup(int wakeup)
{
    return duer_events_call_internal(duer_voice_send_wakeup_internal, wakeup, NULL);
}
#endif//DUER_SECOND_WAKEUP_ENABLE

