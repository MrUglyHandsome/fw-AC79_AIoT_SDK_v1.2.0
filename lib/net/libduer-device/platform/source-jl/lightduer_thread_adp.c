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
// Author: Chang Li (changli@baidu.com)
//
// Description: Adapter for thread APIs

#if defined(DUER_PLATFORM_A113L) || defined(DUER_PLATFORM_SV32WB0X)
#include "FreeRTOS.h"
#include "task.h"
#else
/* #include <freertos/FreeRTOS.h> */
/* #include <freertos/task.h> */
#include "os/os_api.h"
#endif
#include "baidu_ca_adapter_internal.h"
#include "lightduer_memory.h"
#ifdef DUER_PLATFORM_ESP8266
#include "adaptation.h"
#endif

typedef struct {
    TaskHandle_t hdlr;
    char *name;
    duer_u8_t priority;
    duer_u32_t stack_size;
#if defined(DUER_PLATFORM_ESPRESSIF)
    StackType_t *stack_buf;
    duer_bool is_static;
    duer_s32_t core_id;
#endif
} thread_info_t;

duer_thread_t duer_adp_thread_create(char *name, duer_u8_t priority, duer_u32_t stack_size,
                                     duer_bool is_static, cpu_core_id_t core_id)
{
    thread_info_t *info = DUER_CALLOC(1, sizeof(thread_info_t));
    if (info == NULL) {
        return NULL;
    }

    info->hdlr = NULL;
    info->name = DUER_STRDUP(name);
    info->priority = priority;
    info->stack_size = stack_size;
#if defined(DUER_PLATFORM_ESPRESSIF)
    info->is_static = is_static;
    if (core_id == CORE_NONE) {
        info->core_id = tskNO_AFFINITY;
    } else {
        info->core_id = core_id;
    }
#endif

    DUER_LOGI("Create thread %s: priority=%d,stack_size=%u,is_static=%d, core_id=%x.", name, priority,
              stack_size, is_static, core_id);

    return info;
}

duer_status_t thread_start(duer_thread_t thread, duer_thread_entry_f_t entry_func,
                           void *args)
{
    if (thread == NULL) {
        return DUER_ERR_FAILED;
    }

    thread_info_t *info = (thread_info_t *)thread;

    duer_thread_entry_args_t *entry_args = DUER_CALLOC(1, sizeof(duer_thread_entry_args_t));
    if (entry_args == NULL) {
        DUER_LOGE("calloc entry_args failed!");
        return DUER_ERR_FAILED;
    }
    entry_args->p_thread = thread;
    entry_args->args = args;

#if defined(DUER_PLATFORM_ESPRESSIF)
    if (info->is_static) {
        /* As stack_buf is freed by freertos layer, we need to use memory API
         * depended on platform. */
        if ((info->stack_buf = heap_caps_calloc(1, info->stack_size, MALLOC_CAP_SPIRAM | \
                                                MALLOC_CAP_8BIT)) == NULL) {
            DUER_LOGE("%s: calloc stack_buf failed!", info->name);
            return DUER_ERR_FAILED;
        }
    }

    do {
        if (info->is_static) {
            TaskParameters_t task_params = {entry_func, info->name, info->stack_size / sizeof \
            (portSTACK_TYPE), entry_args, info->priority, info->stack_buf, {{NULL}}
                                           };
            if (pdPASS != xTaskCreateRestrictedPinnedToCore(&task_params, &(info->hdlr), info->core_id)) {
                DUER_LOGE("%s: xTaskCreateRestrictedPinnedToCore failed!", info->name);
                break;
            }
        } else {
            if (pdPASS != xTaskCreatePinnedToCore(entry_func, info->name, info->stack_size / sizeof \
                                                  (portSTACK_TYPE), entry_args, info->priority, &(info->hdlr), info->core_id)) {
                DUER_LOGE("%s: xTaskCreate failed!", info->name);
                break;
            }
        }

        return DUER_OK;
    } while (0);

    if (info->stack_buf) {
        heap_caps_free(info->stack_buf);
    }
#else
    do {
        xTaskCreate((TaskFunction_t)entry_func, info->name, info->stack_size / sizeof(portSTACK_TYPE),
                    entry_args, info->priority, &(info->hdlr));
        if (info->hdlr == NULL) {
            DUER_LOGE("[%s] xTaskCreate failed!", info->name);
            break;
        }
        return DUER_OK;
    } while (0);
#endif
    DUER_FREE(entry_args);

    return DUER_ERR_FAILED;
}

duer_status_t thread_exit(duer_thread_entry_args_t *entry_args, duer_bool is_released)
{
    if (entry_args == NULL) {
        return DUER_ERR_FAILED;
    }

    thread_info_t *info = NULL;
    TaskHandle_t hdlr = NULL;

    info = (thread_info_t *)(entry_args->p_thread);
    if (info == NULL || info->hdlr == NULL) {
        return DUER_ERR_FAILED;
    }
    DUER_LOGI("Exit thread %s, hdlr=%p.", info->name, info->hdlr);

    hdlr = info->hdlr;

    DUER_FREE(entry_args);

    /* free duer_thread_t here before thread exit */
    if (is_released) {
        duer_thread_destroy(info);
    }

    /* note: stack_buf can't be free here, leave vTaskDelete to handle. */
    vTaskDelete(hdlr);

    return DUER_OK;
}

duer_status_t thread_destroy(duer_thread_t thread)
{
    if (thread == NULL) {
        return DUER_ERR_FAILED;
    }

    thread_info_t *info = (thread_info_t *)thread;
    if (info->name) {
        DUER_FREE(info->name);
        info->name = NULL;
    }
    DUER_FREE(info);

    return DUER_OK;
}

duer_u32_t thread_get_id(duer_thread_t thread)
{
    if (thread == NULL) {
        return 0;
    }

    thread_info_t *info = (thread_info_t *)thread;

    return (duer_u32_t)(info->hdlr);
}

duer_status_t thread_get_stack_info(duer_thread_t thread)
{
    if (thread == NULL) {
        return DUER_ERR_FAILED;
    }

#if (INCLUDE_uxTaskGetStackHighWaterMark == 1)
    thread_info_t *info = (thread_info_t *)thread;

    //make sure you disabled the option CONFIG_FREERTOS_ASSERT_ON_UNTESTED_FUNCTION in sdkconfig
    unsigned int water_mark = uxTaskGetStackHighWaterMark(info->hdlr);
    DUER_LOGI("[Thread %s Stack] HighWaterMark: %d", info->name, water_mark * sizeof(StackType_t));
#endif

    return DUER_OK;
}
