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
 * File: lightduer_dcs_local_player.c
 * Auth: Gang Chen (chengang12@baidu.com)
 * Desc: apply APIs to control the pause/resume of loacl player.
 */

#include "lightduer_dcs.h"
#include "lightduer_dcs_router.h"
#include "lightduer_dcs_local.h"

enum _play_state {
    IDLE_STATE,
    PLAYING_STATE,
    STOPPED_STATE,
    PAUSED_STATE,
    MAX_PLAY_STATE,
};

static int s_play_state = IDLE_STATE;

duer_bool duer_local_player_is_paused_internal()
{
    return (s_play_state == PAUSED_STATE);
}

duer_bool duer_local_player_is_playing_internal()
{
    return (s_play_state == PLAYING_STATE);
}

void duer_pause_local_player_internal()
{
    duer_dcs_local_player_pause_handler();
}

void duer_resume_local_player_internal()
{
    duer_dcs_local_player_resume_handler();
}

void duer_stop_local_player_internal()
{
    duer_dcs_local_player_stop_handler();
}

void duer_dcs_local_player_on_paused(void)
{
    s_play_state = PAUSED_STATE;
}

void duer_dcs_local_player_on_played(void)
{
    s_play_state = PLAYING_STATE;
    duer_dcs_audio_on_stopped_internal();
}

void duer_dcs_local_player_on_stopped(void)
{
    s_play_state = STOPPED_STATE;
}

