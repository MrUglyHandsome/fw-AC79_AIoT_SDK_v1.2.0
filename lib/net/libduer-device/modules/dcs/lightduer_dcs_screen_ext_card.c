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
 * File: lightduer_dcs_screen.c
 * Auth: Gang Chen (chengang12@baidu.com)
 * Desc: apply APIs to support DCS screen extended card interface.
 */

#include "lightduer_dcs.h"
#include "lightduer_dcs_router.h"
#include "lightduer_dcs_local.h"
#include "lightduer_connagent.h"
#include "lightduer_log.h"
#include "lightduer_ds_log_dcs.h"

static volatile duer_bool s_is_initialized;
static baidu_json *s_audio_list = NULL;
// TODO: execute it when ''play' executed
static baidu_json *s_player_info = NULL;

static duer_status_t duer_render_audio_list_cb(const baidu_json *directive)
{
    DUER_DCS_CRITICAL_ENTER();
    if (s_audio_list) {
        baidu_json_Delete(s_audio_list);
    }

    s_audio_list = baidu_json_Duplicate(directive, 1);
    DUER_DCS_CRITICAL_EXIT();

    return DUER_OK;
}

baidu_json *duer_dcs_audio_get_audio_list()
{
    baidu_json *ret = NULL;

    DUER_DCS_CRITICAL_ENTER();
    if (s_audio_list) {
        ret = baidu_json_Duplicate(s_audio_list, 1);
    }
    DUER_DCS_CRITICAL_EXIT();

    return ret;
}

static duer_status_t duer_render_player_info_cb(const baidu_json *directive)
{
    DUER_DCS_CRITICAL_ENTER();
    if (s_player_info) {
        baidu_json_Delete(s_player_info);
    }

    s_player_info = baidu_json_Duplicate(directive, 1);
    DUER_DCS_CRITICAL_EXIT();

    return DUER_OK;
}

baidu_json *duer_dcs_audio_get_player_info()
{
    baidu_json *ret = NULL;

    DUER_DCS_CRITICAL_ENTER();
    if (s_player_info) {
        ret = baidu_json_Duplicate(s_player_info, 1);
    }
    DUER_DCS_CRITICAL_EXIT();

    return ret;
}

void duer_dcs_screen_ext_card_init(void)
{
    DUER_DCS_CRITICAL_ENTER();

    if (s_is_initialized) {
        DUER_DCS_CRITICAL_EXIT();
        return;
    }

    duer_directive_list res[] = {
        {DCS_RENDER_PLAYER_INFO, duer_render_player_info_cb},
        {DCS_RENDER_AUDIO_LIST, duer_render_audio_list_cb},
    };

    duer_add_dcs_directive_internal(res, sizeof(res) / sizeof(res[0]),
                                    DCS_SCREEN_EXTENDED_CARD_NAMESPACE);

    s_is_initialized = DUER_TRUE;
    DUER_DCS_CRITICAL_EXIT();
}

void duer_dcs_screen_ext_card_uninit_internal(void)
{
    s_is_initialized = DUER_FALSE;
}

