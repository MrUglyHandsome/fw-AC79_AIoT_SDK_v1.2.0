﻿#include "awtk.h"
#include "title_value_register.h"
#include "number_label_register.h"

static ret_t on_close(void *ctx, event_t *e)
{
    tk_quit();

    return RET_OK;
}

/**
 * 初始化
 */
ret_t application_init(void)
{
    title_value_register();
    number_label_register();

    widget_t *win = window_open("main");
    widget_child_on(win, "close", EVT_CLICK, on_close, NULL);

    return RET_OK;
}

/**
 * 退出
 */
ret_t application_exit(void)
{
    log_debug("application_exit\n");
    return RET_OK;
}
