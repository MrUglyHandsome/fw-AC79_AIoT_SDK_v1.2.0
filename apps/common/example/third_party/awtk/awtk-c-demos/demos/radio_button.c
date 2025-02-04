﻿/**
 * File:   check_button.c
 * Author: AWTK Develop Team
 * Brief:  check_button demo
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
 * 2018-08-16 Li XianJing <xianjimli@hotmail.com> created
 *
 */

#include "awtk.h"

static ret_t on_changed(void *ctx, event_t *e)
{
    check_button_t *btn = CHECK_BUTTON(e->target);
    widget_t *widget = WIDGET(e->target);

    log_debug("%s on_changed: %s\n", widget->name, btn->value ? "true" : "false");

    return RET_OK;
}

static ret_t on_test_click(void *ctx, event_t *e)
{
    widget_t *win = WIDGET(ctx);
    widget_t *z = widget_lookup(win, "Zzzzzz", TRUE);
    widget_t *checked = check_button_get_checked_button(z);

    log_debug("on_test_click: %s is checked %d\n", checked->name, widget_get_value_int(checked));

    return RET_OK;
}

static ret_t on_quit_click(void *ctx, event_t *e)
{
    tk_quit();
    return RET_OK;
}

widget_t *create_check_button(widget_t *win, const char *text, int x, int y, int w,
                              int h)
{
    widget_t *btn = check_button_create_radio(win, x, y, w, h);
    widget_on(btn, EVT_VALUE_CHANGED, on_changed, NULL);

    widget_set_name(btn, text);
    widget_set_text_utf8(btn, text);
    widget_set_value(btn, TRUE);

    return btn;
}

ret_t application_init()
{
    widget_t *win = window_create(NULL, 0, 0, 0, 0);
    widget_t *test = button_create(win, 0, 0, 0, 0);
    widget_t *quit = button_create(win, 0, 0, 0, 0);

    create_check_button(win, "Zzzzzz", 10, 10, 128, 30);
    create_check_button(win, "Llllll", 10, 50, 128, 30);
    create_check_button(win, "Gggggg", 10, 90, 128, 30);

    widget_set_tr_text(test, "test get checked");
    widget_set_self_layout_params(test, "center", "middle:-40", "128", "30");
    widget_on(test, EVT_CLICK, on_test_click, win);

    widget_set_tr_text(quit, "quit");
    widget_set_self_layout_params(quit, "center", "middle:40", "128", "30");
    widget_on(quit, EVT_CLICK, on_quit_click, win);

    widget_layout(win);

    return RET_OK;
}

ret_t application_exit()
{
    log_debug("application_exit\n");
    return RET_OK;
}

#include "awtk_main.inc"
