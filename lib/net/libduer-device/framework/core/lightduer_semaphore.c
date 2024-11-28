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
// Description: Wrapper for semaphore

#include "lightduer_semaphore.h"

typedef struct _duer_semaphore_cbs_s {
    duer_semaphore_create_f f_create;
    duer_semaphore_wait_f f_wait;
    duer_semaphore_release_f f_release;
    duer_semaphore_destroy_f f_destroy;
} duer_semaphore_cbs_t;

DUER_LOC_IMPL duer_semaphore_cbs_t s_duer_semaphore_cbs = { NULL };

DUER_EXT_IMPL void duer_semaphore_init(duer_semaphore_create_f f_create,
                                       duer_semaphore_wait_f f_wait,
                                       duer_semaphore_release_f f_release,
                                       duer_semaphore_destroy_f f_destroy)
{
    s_duer_semaphore_cbs.f_create = f_create;
    s_duer_semaphore_cbs.f_wait = f_wait;
    s_duer_semaphore_cbs.f_release = f_release;
    s_duer_semaphore_cbs.f_destroy = f_destroy;
}

DUER_INT_IMPL duer_semaphore_t duer_semaphore_create(int count)
{
    return s_duer_semaphore_cbs.f_create ? s_duer_semaphore_cbs.f_create(count) : NULL;
}

DUER_INT_IMPL duer_status_t duer_semaphore_wait(duer_semaphore_t semaphore, duer_u32_t millisec)
{
    return s_duer_semaphore_cbs.f_wait ? s_duer_semaphore_cbs.f_wait(
               semaphore, millisec) : DUER_ERR_FAILED;
}

DUER_INT_IMPL duer_status_t duer_semaphore_release(duer_semaphore_t semaphore)
{
    return s_duer_semaphore_cbs.f_release ? s_duer_semaphore_cbs.f_release(semaphore)
           : DUER_ERR_FAILED;
}

DUER_INT_IMPL duer_status_t duer_semaphore_destroy(duer_semaphore_t semaphore)
{
    return s_duer_semaphore_cbs.f_destroy ? s_duer_semaphore_cbs.f_destroy(semaphore)
           : DUER_ERR_FAILED;
}
