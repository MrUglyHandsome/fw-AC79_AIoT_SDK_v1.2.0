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
// Author: Gang Chen (chengang12@baidu.com)
//
// Description: The AP information APIs.

#include "lightduer_ap_info.h"

DUER_LOC_IMPL duer_get_wifi_status_f s_wifi_status_cb = NULL;

DUER_EXT void baidu_ca_ap_info_init(duer_get_wifi_status_f f_wifi_status)
{
    s_wifi_status_cb = f_wifi_status;
}

DUER_INT_IMPL duer_status_t duer_wifi_status_get(void)
{
    duer_status_t rs = DUER_ERR_FAILED;

    if (s_wifi_status_cb) {
        rs = s_wifi_status_cb();
    }

    return rs;
}
