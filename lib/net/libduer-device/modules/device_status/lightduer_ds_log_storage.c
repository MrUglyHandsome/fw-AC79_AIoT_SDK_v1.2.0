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
 * Auth: Gang Chen(chengang12@baidu.com)
 * Desc: Save the ds log to ROM.
 */
#ifdef DUER_DS_LOG_STORAGE_ENABLED

#include "lightduer_ds_log_storage.h"
#include "lightduer_mutex.h"
#include "lightduer_log.h"
#include "baidu_json.h"
#include "lightduer_memory.h"
#include "lightduer_connagent.h"

static duer_flash_ring_buf_context_t s_flash_ctx;
static duer_mutex_t s_flash_lock;
static duer_bool s_is_initialized;

duer_status_t duer_ds_log_storage_push(const char *log)
{
    duer_status_t rs = DUER_OK;

    if (s_is_initialized) {
        duer_mutex_lock(s_flash_lock);
        rs = duer_flash_ring_buf_append(&s_flash_ctx, log);
        duer_mutex_unlock(s_flash_lock);
    } else {
        DUER_LOGE("Please call duer_ds_log_storage_init first");
        rs = DUER_ERR_FAILED;
    }

    return rs;
}

duer_status_t duer_ds_log_storage_send(void)
{
    char *log = NULL;
    baidu_json *data = NULL;
    duer_status_t rs = DUER_OK;

    if (!s_is_initialized) {
        DUER_LOGE("Please call duer_ds_log_storage_init first");
        return DUER_ERR_FAILED;
    }

    duer_mutex_lock(s_flash_lock);
    do {
        log = duer_flash_ring_buf_top(&s_flash_ctx);
        if (log) {
            DUER_LOGD("log: %s", log);
            data = baidu_json_Parse(log);
            DUER_FREE(log);
            if (!data) {
                DUER_LOGE("Failed to parse log");
                rs = DUER_ERR_FAILED;
                break;
            }
            rs = duer_data_report(data);
            baidu_json_Delete(data);
            if (rs != DUER_OK) {
                rs = DUER_ERR_FAILED;
                break;
            }
            duer_flash_ring_buf_header_remove(&s_flash_ctx);
        }
    } while (log);
    duer_mutex_unlock(s_flash_lock);

    return rs;
}

duer_status_t duer_ds_log_storage_init(duer_flash_context_t *ctx,
                                       duer_flash_config_t *config)
{
    if (!ctx || !config) {
        DUER_LOGE("Invalid parameter");
        return DUER_ERR_INVALID_PARAMETER;
    }

    if (!s_flash_lock) {
        s_flash_lock = duer_mutex_create();
        if (!s_flash_lock) {
            DUER_LOGE("Failed to create flash mutex");
            return DUER_ERR_FAILED;
        }
    }

    memcpy(&s_flash_ctx.ctx, ctx, sizeof(duer_flash_context_t));
    duer_flash_ring_buf_set_config(config);
    duer_flash_ring_buf_load(&s_flash_ctx);
    s_is_initialized = DUER_TRUE;

    return DUER_OK;
}
#endif
