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
// Description: The thread adapter APIs.

#include "lightduer_thread.h"
#include "lightduer_log.h"

typedef struct {
    duer_thread_create_f_t  f_create;
    duer_thread_start_f_t   f_start;
    duer_thread_exit_f_t    f_exit;
    duer_thread_destroy_f_t f_destroy;
    duer_thread_get_id_f_t  f_get_id;
    duer_thread_get_stack_info_f f_get_stack_info;
} duer_thread_cbs_t;

static duer_thread_cbs_t s_thread_cbs = {NULL};

void duer_thread_init(
    duer_thread_create_f_t  _f_create,
    duer_thread_start_f_t   _f_start,
    duer_thread_exit_f_t    _f_exit,
    duer_thread_destroy_f_t _f_destroy,
    duer_thread_get_id_f_t  _f_get_id,
    duer_thread_get_stack_info_f f_get_stack_info)
{
    s_thread_cbs.f_create   = _f_create;
    s_thread_cbs.f_start    = _f_start;
    s_thread_cbs.f_exit     = _f_exit;
    s_thread_cbs.f_destroy  = _f_destroy;
    s_thread_cbs.f_get_id   = _f_get_id;
    s_thread_cbs.f_get_stack_info = f_get_stack_info;
}

duer_thread_t duer_thread_create(char *name, duer_u8_t priority, duer_u32_t stack_size, duer_bool is_static, cpu_core_id_t core_id)
{
    return s_thread_cbs.f_create ? s_thread_cbs.f_create(name, priority, stack_size, is_static, core_id) : NULL;
}

duer_status_t duer_thread_start(duer_thread_t thread, duer_thread_entry_f_t entry_func, void *args)
{
    return s_thread_cbs.f_start ? s_thread_cbs.f_start(thread, entry_func, args) : DUER_ERR_FAILED;
}

duer_status_t duer_thread_exit(duer_thread_entry_args_t *entry_args, duer_bool is_released)
{
    return s_thread_cbs.f_exit ? s_thread_cbs.f_exit(entry_args, is_released) : DUER_ERR_FAILED;
}

duer_status_t duer_thread_destroy(duer_thread_t thread)
{
    return s_thread_cbs.f_destroy ? s_thread_cbs.f_destroy(thread) : DUER_ERR_FAILED;
}

duer_u32_t duer_thread_get_id(duer_thread_t thread)
{
    return s_thread_cbs.f_get_id ? s_thread_cbs.f_get_id(thread) : 0;
}

duer_status_t duer_thread_get_stack_info(duer_thread_t thread)
{
    return s_thread_cbs.f_get_stack_info ? s_thread_cbs.f_get_stack_info(thread) : DUER_ERR_FAILED;
}
