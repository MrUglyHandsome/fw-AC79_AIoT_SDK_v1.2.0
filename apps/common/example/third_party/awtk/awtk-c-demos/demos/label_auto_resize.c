﻿/**
 * File:   label.c
 * Author: AWTK Develop Team
 * Brief:  label demo
 *
 * Copyright (c) 2018 - 2021  Guangzhou ZHIYUAN Electronics Co.,Ltd.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * License file for more details.
 *
 */

/**
 * History:
 * ================================================================
 * 2018-08-15 Li XianJing <xianjimli@hotmail.com> created
 *
 */

#include "awtk.h"

ret_t application_init()
{
    widget_t *win = window_create(NULL, 0, 0, 0, 0);
    widget_t *label1 = label_create(win, 10, 10, 200, 30);
    widget_t *label2 = label_create(win, 10, 60, 200, 30);

    widget_set_text(label1, L"Standing on the River Thames, London has been a major settlement for two millennia, its history going back to its founding by the Romans, who named it Londinium!");
    widget_set_auto_adjust_size(label1, TRUE);

    widget_set_text(label2, L"Standing on the River Thames, London has been a major settlement for two millennia, its history going back to its founding by the Romans, who named it Londinium!");

    label_set_line_wrap(label2, TRUE);
    label_set_word_wrap(label2, TRUE);
    widget_set_auto_adjust_size(label2, TRUE);

    widget_layout(win);

    return RET_OK;
}

ret_t application_exit()
{
    log_debug("application_exit\n");
    return RET_OK;
}

#include "awtk_main.inc"
