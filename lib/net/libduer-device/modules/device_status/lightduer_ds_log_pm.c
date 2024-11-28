/**
 * Copyright (2018) Baidu Inc. All rights reserved.
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
 * Auth: Chen Xihao(chenxihao@baidu.com)
 * Desc: power manager module related report log
 */

#include "lightduer_ds_log_pm.h"
#include "lightduer_ds_log.h"
#include "lightduer_log.h"

duer_status_t duer_ds_log_pm(duer_ds_log_pm_code_t log_code,
                             const baidu_json *message)
{
    duer_ds_log_level_enum_t log_level = DUER_DS_LOG_LEVEL_INFO;

    switch (log_code) {
    case DUER_DS_LOG_PM_SLEEP:
    case DUER_DS_LOG_PM_WAKEUP:
        log_level = DUER_DS_LOG_LEVEL_INFO;
        break;
    default:
        DUER_LOGW("unkown log code 0x%0x", log_code);
        return DUER_ERR_FAILED;
    }

    return duer_ds_log_important(log_level, DUER_DS_LOG_MODULE_PM, log_code, message);
}
