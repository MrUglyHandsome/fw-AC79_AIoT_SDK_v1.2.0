#include "app_config.h"
#ifdef CONFIG_UI_ENABLE //上电执行则打开app_config.h TCFG_DEMO_UI_RUN = 1
#ifdef UI_MASS_PRODUCTION

#include "ui/ui.h"
#include "ui_api.h"
#include "system/timer.h"
#include "server/server_core.h"
#include "os/os_api.h"
#include "asm/gpio.h"
#include "system/includes.h"
#include "server/audio_server.h"
#include "storage_device.h"
#include "font/font_textout.h"
#include "ui/includes.h"
#include "ui_action_video.h"
#include "font/font_all.h"
#include "font/language_list.h"
#include "ename.h"
#include "asm/rtc.h"
#include "lcd_drive.h"
#include "wifi_raw_stream.h"
#include "syscfg/syscfg_id.h"
/*#include "video_rec.h"*/
#include "yuv_soft_scalling.h"
#include "event/key_event.h"
/*#include "net_video_rec.h"*/
/*#include "asm/jpeg_codec.h"*/

/* 开关打印提示 */
#if 1
#define log_info(x, ...)    printf("\n[ui_production_msg]>>>>>>>>>>>>>>>>>>>###" x " \n", ## __VA_ARGS__)
#else
#define log_info(...)
#endif

#define CAMERA_WINDOW    BASEFORM_2
#define CAMERA1_WINDOW   BASEFORM_43
#define MODE_LIST        BASEFORM_11
#define VIDEO_LIST       BASEFORM_22

#define VIDEO_MUB1       BASEFORM_28
#define VIDEO_MUB2       BASEFORM_29
#define VIDEO_MUB3       BASEFORM_30
#define VIDEO_MUB4       BASEFORM_31

static u8 msg_key = 0;

struct video_config_hd video_hd;

extern u8 control_change_video_show(u8 dev_munb);

const static u8 fps_list[4] = {5, 15, 20, 25};
const static u8 win_list[4] = {1, 4, 6, 8};
static u8 list_x;
static u8 page_x = 0;

#define POST_TASK_NAME "raw_send_task"
#define    CFG_RAW_VIDEO_ID             245 //245-250

static int camera_window_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    char window_x = 0;

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        window_x = ui_grid_cur_item(grid);
        os_taskq_post("ui_main_task", 2, TOUCH_MORE_WINDOWS_UP, window_x);
        os_time_dly(20);
        break;
    case ELM_EVENT_TOUCH_HOLD:
        window_x = ui_grid_cur_item(grid);
        os_taskq_post("ui_main_task", 2, TOUCH_MORE_WINDOWS_HOLD, window_x);
        os_time_dly(20);
        break;
    }
    return false;
}
REGISTER_UI_EVENT_HANDLER(CAMERA_WINDOW)//多窗口显示界面
.ontouch = camera_window_ontouch,
};

static int camera1_window1_ontouch1(void *ctr, struct element_touch_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_DOWN:
        printf(">>>>>ELM_EVENT_TOUCH_DOWN");
        os_taskq_post("ui_main_task", 2, TOUCH_ONE_WINDOWS_UP, 0);
        os_time_dly(20);
        break;
    }
    return false;
}

REGISTER_UI_EVENT_HANDLER(CAMERA1_WINDOW)//显示界面
.ontouch = camera1_window1_ontouch1,
};

/*==============视频出图配置=====================*/
static void ui_mub_show(int id, int mub)
{
    struct unumber timer;
    timer.type = TYPE_NUM;
    timer.numbs = 2;
    timer.number[0] =  mub;
    ui_number_update_by_id(id, &timer);
}

static int mode_list_onkey(void *grid, struct element_key_event *e)
{
    list_x = ui_grid_get_hindex(grid);
    printf(">>>>>>list_x = %d", list_x);
    switch (e->value) {
    case KEY_OK:
        switch (list_x) {
        case 0://视频图传测试
            page_x = 1;//标记已经进入了第一页
            ui_hide_main(PAGE_1);
            ui_show_main(PAGE_3);
            ui_key_msg_post(KEY_OK);
            break;
        case 1://摄像头测试
            break;
        case 2://升级
            break;
        }

        break;
    }
    return false;
}

REGISTER_UI_EVENT_HANDLER(MODE_LIST)//模式选择界面
.onkey = mode_list_onkey,
};


static int video_list_onchange(void *_time, enum element_change_event event, void *arg)
{
    printf(">>>>>>>>event = %d", event);
    switch (event) {
    case ON_CHANGE_INIT:
        syscfg_read(CFG_RAW_VIDEO_ID, &video_hd, sizeof(video_hd));
        if (video_hd.default_flog == 0) { //如果是第一次上电 给一个配置初始值
            video_hd.ch = 1;
            video_hd.fps = 2;
            video_hd.win = 2;
            video_hd.chinge_time = 10;
            video_hd.default_flog = 1;//标志已经给过初始值
            syscfg_write(CFG_RAW_VIDEO_ID, &video_hd, sizeof(video_hd));
        }
        put_buf(&video_hd, sizeof(video_hd));
    }

    return true;
}


static int video_list_onkey(void *grid, struct element_key_event *e)
{
    struct element *elm;
    list_x = ui_grid_get_hindex(grid);
    switch (e->value) {
    case KEY_ADD:
        switch (list_x) {
        case 0://图传信道配置
            video_hd.ch++;
            if (video_hd.ch > 12) {
                video_hd.ch	= 1;
            }
            ui_mub_show(VIDEO_MUB1, video_hd.ch);
            break;
        case 1://图传帧率配置
            video_hd.fps++;
            if (video_hd.fps > 3) {
                video_hd.fps = 0;
            }
            ui_mub_show(VIDEO_MUB2, fps_list[video_hd.fps]);
            break;
        case 2://显示窗口数量配置
            video_hd.win++;
            if (video_hd.win > 3) {
                video_hd.win = 0;
            }
            ui_mub_show(VIDEO_MUB3, win_list[video_hd.win]);
            break;
        case 3://前后镜头自动切换时间
            video_hd.chinge_time++;
            if (video_hd.chinge_time > 99) {
                video_hd.chinge_time = 0;
            }
            ui_mub_show(VIDEO_MUB4, video_hd.chinge_time);
            break;
        }

        break;
    case KEY_MINUS:
        switch (list_x) {
        case 0://图传信道配置
            video_hd.ch--;
            if (video_hd.ch == 0) {
                video_hd.ch	= 12;
            }
            ui_mub_show(VIDEO_MUB1, video_hd.ch);
            break;
        case 1://图传帧率配置
            video_hd.fps--;
            if (video_hd.fps > 3) {
                video_hd.fps = 3;
            }
            ui_mub_show(VIDEO_MUB2, fps_list[video_hd.fps]);
            break;
        case 2://显示窗口数量配置
            video_hd.win--;
            if (video_hd.win > 3) {
                video_hd.win = 3;
            }
            ui_mub_show(VIDEO_MUB3, win_list[video_hd.win]);
            break;
        case 3://前后镜头自动切换时间
            video_hd.chinge_time--;
            if (video_hd.chinge_time > 99) {
                video_hd.chinge_time = 99;
            }
            ui_mub_show(VIDEO_MUB4, video_hd.chinge_time);
            break;
        }
        break;
    case KEY_UP:
        switch (list_x) {
        case 0://图传信道配置
            elm = ui_core_get_element_by_id(VIDEO_MUB1);
            ui_core_highlight_element(elm, msg_key);
            ui_core_redraw(elm->parent);
            break;
        case 1://图传帧率配置
            elm = ui_core_get_element_by_id(VIDEO_MUB2);
            ui_core_highlight_element(elm, msg_key);
            ui_core_redraw(elm->parent);
            break;
        case 2://显示窗口数量配置
            elm = ui_core_get_element_by_id(VIDEO_MUB3);
            ui_core_highlight_element(elm, msg_key);
            ui_core_redraw(elm->parent);
            break;
        case 3://前后镜头自动切换时间
            elm = ui_core_get_element_by_id(VIDEO_MUB4);
            ui_core_highlight_element(elm, msg_key);
            ui_core_redraw(elm->parent);
            break;
        }
        break;
    case KEY_DOWN:
        switch (list_x) {
        case 0://图传信道配置
            elm = ui_core_get_element_by_id(VIDEO_MUB1);
            ui_core_highlight_element(elm, msg_key);
            ui_core_redraw(elm->parent);
            break;
        case 1://图传帧率配置
            elm = ui_core_get_element_by_id(VIDEO_MUB2);
            ui_core_highlight_element(elm, msg_key);
            ui_core_redraw(elm->parent);
            break;
        case 2://显示窗口数量配置
            elm = ui_core_get_element_by_id(VIDEO_MUB3);
            ui_core_highlight_element(elm, msg_key);
            ui_core_redraw(elm->parent);
            break;
        case 3://前后镜头自动切换时间
            elm = ui_core_get_element_by_id(VIDEO_MUB4);
            ui_core_highlight_element(elm, msg_key);
            ui_core_redraw(elm->parent);
            break;
        }
        break;
    case KEY_OK:
        ui_mub_show(VIDEO_MUB1, video_hd.ch);
        ui_mub_show(VIDEO_MUB2, fps_list[video_hd.fps]);
        ui_mub_show(VIDEO_MUB3, win_list[video_hd.win]);
        ui_mub_show(VIDEO_MUB4, video_hd.chinge_time);
        printf(">>>>>>>list_x = %d", list_x);
        if (list_x != 4) {
            msg_key = ~msg_key;
        } else {
            os_taskq_post("ui_main_task", 0, KEY_CMD_CHANGE_VIDEO);
            page_x = 2;
        }
        switch (list_x) {
        case 0://图传信道配置
            elm = ui_core_get_element_by_id(VIDEO_MUB1);
            ui_core_highlight_element(elm, msg_key);
            ui_core_redraw(elm->parent);
            break;
        case 1://图传帧率配置
            elm = ui_core_get_element_by_id(VIDEO_MUB2);
            ui_core_highlight_element(elm, msg_key);
            ui_core_redraw(elm->parent);
            break;
        case 2://显示窗口数量配置
            elm = ui_core_get_element_by_id(VIDEO_MUB3);
            ui_core_highlight_element(elm, msg_key);
            ui_core_redraw(elm->parent);
            break;
        case 3://前后镜头自动切换时间
            elm = ui_core_get_element_by_id(VIDEO_MUB4);
            ui_core_highlight_element(elm, msg_key);
            ui_core_redraw(elm->parent);
            break;
        case 4://开始测试
            syscfg_write(CFG_RAW_VIDEO_ID, &video_hd, sizeof(video_hd));
            extern int wifi_raw_init(void);
            wifi_raw_init();
            ui_hide_main(PAGE_3);
            ui_show_main(PAGE_0);
            set_lcd_show_data_mode(UI_RGB);
            /*wifi_set_channel(video_hd.ch);//动态配置wifi信道*/
            printf(">>>>>>>>set_run wifi_ch = %d", video_hd.ch);
            page_x = 2;
            break;
        }
        break;
    }
    return false;
}


REGISTER_UI_EVENT_HANDLER(VIDEO_LIST)//视频配置选项
.onchange = video_list_onchange,
 .onkey = video_list_onkey,
};
/****************END************************/

static int raw_key_click(struct key_event *key)
{
    static u8 mode = 0; //0为摄像头输出模式 1为UI输出模式
    int err;
    printf(">>>>>>>list_x = %d , page_x = %d", list_x, page_x);
    switch (key->action) { //发送按键消息给UI处理
    case KEY_EVENT_CLICK:
        switch (key->value) {
        case KEY_K1: //IO_key//键值
            printf(">>>>>>>>>>>>>>>key1");
            if (page_x == 0) { //开始选择界面
                ui_key_msg_post(KEY_DOWN);
            } else if (page_x == 1) { //视频设置界面
                if (msg_key) {
                    ui_key_msg_post(KEY_ADD);
                } else {
                    ui_key_msg_post(KEY_UP);
                }
            } else if (page_x == 2) { //视频出图界面
                for (u8 i = 0; i < 6; i++) {
                    control_change_video_show(i);
                }
            }

            break;
        case KEY_K2:
            printf(">>>>>>>>>>>>>>>key2");
            if (page_x == 0) {
                ui_key_msg_post(KEY_UP);
            } else if (page_x == 1) {
                if (msg_key) {
                    ui_key_msg_post(KEY_MINUS);
                } else {
                    ui_key_msg_post(KEY_DOWN);
                }
            }
            break;
        case KEY_K3:
            printf(">>>>>>>>>>>>>>>key3");
            if (page_x == 0) {
                ui_key_msg_post(KEY_OK);
            } else if (page_x == 1) {
                ui_key_msg_post(KEY_OK);
            }
            break;
        case KEY_K4:
            printf(">>>>>>>>>>>>>>>key4");
            break;
        case KEY_K5:
            printf(">>>>>>>>>>>>>>>key5");
            break;
        case KEY_K6:
            printf(">>>>>>>>>>>>>>>key6");
            break;
        case KEY_K7:
            printf(">>>>>>>>>>>>>>>key7");
            break;
        case KEY_K8:
            printf(">>>>>>>>>>>>>>>key8");
            break;
        }
    }
    return false;
}

int raw_key_event_handler(struct key_event *key)
{
    char ret;
    switch (key->action) {
    case KEY_EVENT_CLICK:
        ret = raw_key_click(key);
        break;
    case KEY_EVENT_LONG:
        break;
    case KEY_EVENT_HOLD:
        break;
    case KEY_EVENT_UP:
        break;
    default:
        break;
    }
    return 0;
}

void ui_main_task(void *priv)
{
    int msg[32];
    user_ui_lcd_init();
    set_lcd_show_data_mode(UI);
    ui_show_main(PAGE_2);
    os_time_dly(200);
    ui_hide_main(PAGE_2);
    ui_show_main(PAGE_1);
    while (1) {
        os_taskq_pend("taskq", msg, ARRAY_SIZE(msg));
        switch (msg[1]) {
        case TOUCH_MORE_WINDOWS_UP:
            printf("[msg]>>>TOUCH_MORE_WINDOWS_UP");
            os_taskq_post("raw_mode_task", 2, TOUCH_MORE_WINDOWS_UP, msg[2]);
            break;
        case TOUCH_MORE_WINDOWS_HOLD:
            printf("[msg]>>>TOUCH_MORE_WINDOWS_HOLD");
            ui_hide_main(PAGE_1);
            ui_show_main(PAGE_2);
            os_taskq_post("raw_mode_task", 2, TOUCH_MORE_WINDOWS_HOLD, msg[2]);
            break;
        case TOUCH_ONE_WINDOWS_UP:
            printf("[msg]>>>TOUCH_MORE_WINDOWS_HOLD");
            ui_hide_main(PAGE_2);
            ui_show_main(PAGE_1);
            os_taskq_post("raw_mode_task", 2, TOUCH_ONE_WINDOWS_UP, msg[2]);
            break;
        case KEY_CMD_CHANGE_VIDEO:
            printf("[msg]>>>KEY_CMD_CHANGE_VIDEO");
            os_taskq_post("raw_mode_task", 2, KEY_CMD_CHANGE_VIDEO, msg[2]);
            break;
        }
    }
}

static void ui_init(void *priv)
{
    thread_fork("ui_main_task", 5, 1024, 32, 0, ui_main_task, NULL);
}
late_initcall(ui_init);


#endif
#endif

