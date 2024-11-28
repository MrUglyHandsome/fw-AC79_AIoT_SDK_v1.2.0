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
// Author: Chen Xihao (chenxihao@baidu.com)
//
// Description: Adapter for semaphore

#if defined(DUER_PLATFORM_A113L) || defined(DUER_PLATFORM_SV32WB0X)
#include <FreeRTOS.h>
#include <semphr.h>
#else
/* #include <freertos/FreeRTOS.h> */
/* #include <freertos/semphr.h> */
#include "os/os_api.h"
#endif
#include <limits.h>
#include "baidu_ca_adapter_internal.h"
#ifdef DUER_PLATFORM_ESP8266
#include "adaptation.h"
#endif

duer_semaphore_t semaphore_create(int count)
{
    SemaphoreHandle_t sem = xSemaphoreCreateCounting(count > 0 ? count : 1, count);

    return (duer_semaphore_t)sem;
}

duer_status_t semaphore_wait(duer_semaphore_t semaphore, duer_u32_t millisec)
{
    if (semaphore == NULL) {
        return DUER_ERR_FAILED;
    }

    SemaphoreHandle_t xSemaphore = (SemaphoreHandle_t)semaphore;
    uint32_t block = (millisec == UINT_MAX) ? portMAX_DELAY : (millisec / portTICK_PERIOD_MS);
    if (xSemaphoreTake(xSemaphore, block) == pdTRUE) {
        return DUER_OK;
    } else {
        return DUER_ERR_FAILED;
    }
}

duer_status_t semaphore_release(duer_semaphore_t semaphore)
{
    if (semaphore == NULL) {
        return DUER_ERR_FAILED;
    }

    SemaphoreHandle_t xSemaphore = (SemaphoreHandle_t)semaphore;

    if (xSemaphoreGive(xSemaphore) == pdTRUE) {
        return DUER_OK;
    } else {
        return DUER_ERR_FAILED;
    }
}

duer_status_t semaphore_destroy(duer_semaphore_t semaphore)
{
    if (semaphore == NULL) {
        return DUER_ERR_FAILED;
    }

    SemaphoreHandle_t xSemaphore = (SemaphoreHandle_t)semaphore;

    vSemaphoreDelete(xSemaphore);

    return DUER_OK;
}
