#include "app_config.h"
#ifdef CONFIG_UI_ENABLE //上电执行则打开app_config.h TCFG_DEMO_UI_RUN = 1

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
#include "video_rec.h"
#include "yuv_soft_scalling.h"
#include "event/key_event.h"
#include "net_video_rec.h"
#include "asm/jpeg_codec.h"
#include "ui_demo.h"
#include "action.h"
#include "lcd_data_driver.h"

#if TCFG_DEMO_UI_RUN

#if (defined CONFIG_VIDEO_ENABLE && (__SDRAM_SIZE__ >= (2 * 1024 * 1024)))

#if 1
#define log_info(x, ...)    printf("\r\n>>>>>>[UI]" x " ", ## __VA_ARGS__)
#else
#define log_info(...)
#endif


struct ui_demo_date_hdl *ui_data;
#define __this ui_data

#define POST_TASK_NAME "ui_demo"




extern void get_jpg_show_to_lcd(u8 *img_bug, u32 img_len);
extern void get_jpg_show_lcd(u8 *img_buf, u32 img_len);
extern void lcd_show_frame_1(u8 *buf, u32 len);
extern int video_rec_start_notify(void);
extern int avi_net_playback_unpkg_init(FILE *fd, u8 state);//state : 0 preview , 1 playback
extern int avi_video_get_frame(FILE *fd, int offset_num, u8 *buf, u32 buf_len, u8 state);



static void ui_play_mp3(char *name)
{
#if CONFIG_MP3_DEC_ENABLE
    post_msg_play_flash_mp3(name, __this->play_mp3_sound_size); //开机提示音
#endif
}

static void get_sys_time(struct sys_time *time)
{
    void *fd = dev_open("rtc", NULL);

    if (!fd) {
        memset(time, 0, sizeof(*time));
        return;
    }

    dev_ioctl(fd, IOCTL_GET_SYS_TIME, (u32)time);
    /* printf("get_sys_time : %d-%d-%d,%d:%d:%d\n", time->year, time->month, time->day, time->hour, time->min, time->sec); */
    dev_close(fd);
}


/*==================开机播提示音已经开机动画==================*/
static void open_animation(char speed)//开机图片以及开机音乐播放
{
    set_lcd_show_data_mode(UI);

    key_event_disable();//如果需要提示音播放完要关按键开关

    ui_play_mp3(POWER_ON_MP3);

    ui_show_main(PAGE_2);

    /* ui_show_main(PAGE_5); */

    /* while(1){ */
    /* os_time_dly(10); */
    /* } */

    for (u8 i = 0; i < 8; i++) {
        log_info("i=%d.\n", i);
        ui_pic_show_image_by_id(BASEFORM_19, i);
        os_time_dly(speed);
    }

    ui_hide_main(PAGE_2);

    key_event_enable();
}

void *get_ui_demo_hd(void)
{
    return ui_data;
}
u8 get_take_photo_status(void)
{
    return __this->get_photo;
}

void set_take_photo_ok(void)
{
    __this->get_photo = 0;
}
/*****************end**********************/

/*********************************************************
*                    PAGE_0 显示时钟表盘功能
*********************************************************/
static u32 sidebar_root = 0;

/*
 * 表盘以及子控件参数更新
 * */
int watch_child_cb(void *_ctrl, int id, int type)
{
    switch (type) {
    case CTRL_TYPE_WATCH:
        /* watch_refresh(id, (struct ui_watch *)_ctrl); */
        break;

    case CTRL_TYPE_PROGRESS:
        /* progress_refresh(id, (struct ui_progress *)_ctrl); */
        break;

    case CTRL_TYPE_MULTIPROGRESS:
        /* multiprogress_refresh(id, (struct ui_multiprogress *)_ctrl); */
        break;

    case CTRL_TYPE_TEXT:
        /* text_refresh(id, (struct ui_text *)_ctrl); */
        break;

    case CTRL_TYPE_NUMBER:
        /* number_refresh(id, (struct ui_number *)_ctrl); */
        break;

    case CTRL_TYPE_TIME:
        /* time_refresh(id, (struct ui_time *)_ctrl); */
        break;

    case CTRL_TYPE_LAYOUT:
        /* struct element *p; */
        /* list_for_each_child_element(p, (struct element *)_ctrl) { */
        /* if (watch_child_cb(p, p->id, ui_id2type(p->id))) { */
        /* break; */
        /* } */
        /* } */
        break;
    }

    return 0;
}

static void WATCH_timer(void *priv)
{
    ui_get_child_by_id(WATCH, watch_child_cb);

    struct element *elm = NULL;

    /* if (sidebar_root) { */
    /* elm = ui_core_get_element_by_id(sidebar_root); */
    /* } */


    if (!elm) {
        elm = ui_core_get_element_by_id(WATCH);
    }

    if (elm) {
        ui_core_redraw(elm);
    }
}

/*
 * 表盘以及子控件onkey响应
 * */
int watch_child_onkey(void *_ctrl, struct element_key_event *e)
{
    /* struct element *elm = (struct element *)_ctrl; */
    /* int type = ui_id2type(elm->id); */

#if 0
    switch (type) {
    case CTRL_TYPE_PROGRESS:
        /* progress_onkey(_ctrl, e); */
        break;

    case CTRL_TYPE_MULTIPROGRESS:
        /* multiprogress_onkey(_ctrl, e); */
        break;

    case CTRL_TYPE_TEXT:
        /* text_onkey(_ctrl, e); */
        break;

    case CTRL_TYPE_NUMBER:
        /* number_onkey(_ctrl, e); */
        break;

    case CTRL_TYPE_TIME:
        /* time_onkey(_ctrl, e); */
        break;
    }

#endif

    return 0;
}

static int watch_child_ontouch(void *_ctrl, struct element_touch_event *e)
{
    struct element *elm = (struct element *)_ctrl;

    /* int type = ui_id2type(elm->id); */


    /* switch (type) { */
    /* case CTRL_TYPE_PROGRESS: */
    /* progress_ontouch(_ctrl, e); */
    /* break; */
    /* case CTRL_TYPE_MULTIPROGRESS: */
    /* multiprogress_ontouch(_ctrl, e); */
    /* break; */
    /* case CTRL_TYPE_TEXT: */
    /* text_ontouch(_ctrl, e); */
    /* break; */
    /* case CTRL_TYPE_NUMBER: */
    /* number_ontouch(_ctrl, e); */
    /* break; */
    /* case CTRL_TYPE_TIME: */
    /* time_ontouch(_ctrl, e); */
    /* break; */
    /* default: */
    /* break; */
    /* } */

    return 0;
}
struct watch_param {
    int curr_watch_style;
    FILE *view_file;
    int find_animation;
    int picture_num;
    int curr_picture;
};
/*
 * 表盘以及子控件onchange响应
 * */
int watch_child_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    /* struct element *elm = (struct element *)_ctrl; */
    /* int type = ui_id2type(elm->id); */

    /* switch (type) { */
    /* case CTRL_TYPE_PROGRESS: */
    /* progress_onchange(_ctrl, event, arg); */
    /* break; */
    /* case CTRL_TYPE_MULTIPROGRESS: */
    /* multiprogress_onchange(_ctrl, event, arg); */
    /* break; */
    /* case CTRL_TYPE_TEXT: */
    /* text_onchange(_ctrl, event, arg); */
    /* break; */
    /* case CTRL_TYPE_NUMBER: */
    /* number_onchange(_ctrl, event, arg); */
    /* break; */
    /* case CTRL_TYPE_TIME: */
    /* time_onchange(_ctrl, event, arg); */
    /* break; */
    /* default: */
    /* break; */
    /* } */

    return 0;
}



/* extern void ui_watch_tick(void *_watch); */

static int WATCH_onchange(void *ctr, enum element_change_event e, void *arg)
{
    struct ui_watch *watch = (struct ui_watch *)ctr;
    struct draw_context *dc = (struct draw_context *)arg;
    struct element *elm = (struct element *)ctr;
    static int timer = 0;
    struct sys_time time;
    char animpath[46];
    static struct watch_param param = {0};
    FILE *view_file;
    u32 flag;
    char *bg_path;
    int timer_interval = 0;
    union ui_control_info info;
    void *hdl;
    struct element_css *css;

    switch (e) {
    case ON_CHANGE_INIT:
        printf("\n [ERROR] %s -[yuyu] %d\n", __FUNCTION__, __LINE__);

        /* ui_watch_set_time(watch, time.hour % 12, time.min, time.sec); */
        watch->hour = time.hour % 12;
        watch->min = time.min;
        watch->sec = time.sec;
        /* ui_set_default_handler(watch_child_ontouch, watch_child_onkey, watch_child_onchange); */
        timer_interval = 500;

        /* ui_watch_tick(watch); */
        /* param.curr_watch_style = -1; */
        /* param.find_animation = false; */
        /* param.curr_picture = 0; */
        /* param.view_file = NULL; */

        if (!timer) {
            timer = sys_timer_add(NULL, WATCH_timer, timer_interval);
        }

        break;

    case ON_CHANGE_SHOW:

        break;

    case ON_CHANGE_RELEASE:

        break;
    }
}

REGISTER_UI_EVENT_HANDLER(WATCH)
.onchange = WATCH_onchange,
 /* .onkey = NULL, */
 /* .ontouch = WATCH_ontouch, */
};



/*===========声音数字加减操作==============*/
static void sound_mub(int mub, char dir)//0加时间 1减时间
{
    struct unumber timer;

    timer.type = TYPE_NUM;
    timer.numbs = 2;

    if (dir) { //+
        __this->play_mp3_sound_size++;
    } else { //-
        __this->play_mp3_sound_size--;
    }

    timer.number[0] = mub;
    ui_number_update_by_id(__SOUND, &timer);
}
static void sound_mub_show_init(int mub)
{
    struct unumber timer;

    timer.type = TYPE_NUM;
    timer.numbs = 2;
    timer.number[0] =  mub;
    ui_number_update_by_id(__SOUND, &timer);
}
static void form_highlight(u32 id, int status)
{
    struct element *elm = ui_core_get_element_by_id(id);
    /* log_info("elm = %x.\n", elm); */
    int ret1 = ui_core_highlight_element(elm, status);
    /* log_info("ret1 = %d.\n", ret1); */
    int ret2 = ui_core_redraw(elm->parent);
    /* log_info("ret2 = %d.\n", ret2); */
}
/*****************end**********************/

/*===========日期时间=刷新时间设置界面==============*/
static void time_set_page(char day, char dir) //0加时间 1减时间 day时间选择
{
    struct unumber timer;

    timer.type = TYPE_NUM;
    timer.numbs = 2;

    if (dir) { //加时间
        switch (day) {
        case 0://年
            test_rtc_time.year++;
            timer.number[0] = test_rtc_time.year;
            ui_number_update_by_id(__TIME1, &timer);
            break;

        case 1://月
            test_rtc_time.month++;

            if (test_rtc_time.month == 13) {
                test_rtc_time.month = 1;
            }

            timer.number[0] = test_rtc_time.month;
            ui_number_update_by_id(__TIME2, &timer);
            break;

        case 2://日
            test_rtc_time.day++;

            if (test_rtc_time.day == 31) {
                test_rtc_time.day = 1;
            }

            timer.number[0] = test_rtc_time.day;
            ui_number_update_by_id(__TIME3, &timer);
            break;

        case 3://时
            test_rtc_time.hour++;

            if (test_rtc_time.hour == 25) {
                test_rtc_time.hour = 1;
            }

            timer.number[0] = test_rtc_time.hour;
            ui_number_update_by_id(__TIME4, &timer);
            break;

        case 4://分
            test_rtc_time.min++;

            if (test_rtc_time.min == 61) {
                test_rtc_time.min = 1;
            }

            timer.number[0] = test_rtc_time.min;
            ui_number_update_by_id(__TIME5, &timer);
            break;

        case 5://退出

            break;
        }
    } else { //减时间
        switch (day) {
        case 0://年
            test_rtc_time.year--;

            if (test_rtc_time.year == 0) {
                test_rtc_time.year = 2021;
            }

            timer.number[0] = test_rtc_time.year;
            ui_number_update_by_id(__TIME1, &timer);
            break;

        case 1://月
            test_rtc_time.month--;

            if (test_rtc_time.month == 0) {
                test_rtc_time.month = 12;
            }

            timer.number[0] = test_rtc_time.month;
            ui_number_update_by_id(__TIME2, &timer);
            break;

        case 2://日
            test_rtc_time.day--;

            if (test_rtc_time.day == 0) {
                test_rtc_time.day = 31;
            }

            timer.number[0] = test_rtc_time.day;
            ui_number_update_by_id(__TIME3, &timer);
            break;

        case 3://时
            test_rtc_time.hour--;

            if (test_rtc_time.hour == 0) {
                test_rtc_time.hour = 24;
            }

            timer.number[0] = test_rtc_time.hour;
            ui_number_update_by_id(__TIME4, &timer);
            break;

        case 4://分
            test_rtc_time.min--;

            if (test_rtc_time.min == 0) {
                test_rtc_time.min = 60;
            }

            timer.number[0] = test_rtc_time.min;
            ui_number_update_by_id(__TIME5, &timer);
            break;

        case 5://退出

            break;
        }
    }
}
/*****************end**********************/

/*===========刷新系统时间================*/
void undata_sys_time(void)
{
    void *dev = NULL;
    dev = dev_open("rtc", NULL);
    dev_ioctl(dev, IOCTL_SET_SYS_TIME, (u32)&test_rtc_time);
    dev_close(dev);
    /* log_info("@@@@@@@@@@@@@@@@get_sys_time: %d-%d-%d %d:%d:%d\n", test_rtc_time.year, test_rtc_time.month, test_rtc_time.day, test_rtc_time.hour, test_rtc_time.min, test_rtc_time.sec); */
}

/*===========日期时间=刷新时间设置界面==============*/
void time_updata(void)
{
    struct unumber timer;

    timer.type = TYPE_NUM;
    timer.numbs = 2;
    timer.number[0] = test_rtc_time.year;
    ui_number_update_by_id(__TIME1, &timer);

    timer.number[0] = test_rtc_time.month;
    ui_number_update_by_id(__TIME2, &timer);

    timer.number[0] = test_rtc_time.day;
    ui_number_update_by_id(__TIME3, &timer);

    timer.number[0] = test_rtc_time.hour;
    ui_number_update_by_id(__TIME4, &timer);

    timer.number[0] = test_rtc_time.min;
    ui_number_update_by_id(__TIME5, &timer);

    /*timer.number[0] = test_rtc_time.sec;*/
    timer.number[0] = 00;
    ui_number_update_by_id(__TIME6, &timer);
}
/*****************end**********************/

/*==================UI_camera时间显示==================*/
static void set_rtc_init_time(void)
{
    void *dev = NULL;
    /* 打开RTC设备 */
    dev = dev_open("rtc", NULL);
    /* 赋值时间信息 */
    test_rtc_time.year = 2021;
    test_rtc_time.month = 5;
    test_rtc_time.day = 19;
    test_rtc_time.hour = 11;
    test_rtc_time.min = 43;
    test_rtc_time.sec = 0;
    /* 设置时间信息  */
    dev_ioctl(dev, IOCTL_SET_SYS_TIME, (u32)&test_rtc_time);
    dev_close(dev);
}
static void get_rtc_time(void)
{
    void *dev = NULL;
    /* 打开RTC设备 */
    dev = dev_open("rtc", NULL);
    /* 获取时间信息 */
    dev_ioctl(dev, IOCTL_GET_SYS_TIME, (u32)&test_rtc_time);
    /* 打印时间信息 */
    /*log_info("get_sys_time: %d-%d-%d %d:%d:%d\n", test_rtc_time.year, test_rtc_time.month, test_rtc_time.day, test_rtc_time.hour, test_rtc_time.min, test_rtc_time.sec);*/
    dev_close(dev);
}

static void timer_change_handler(void *priv)
{
    void *dev = NULL;
    static u32 sec = 0;
    struct utime time_r;

    if (!__this->init_flog) {
        dev = dev_open("rtc", NULL);
        /* 获取时间信息 */
        dev_ioctl(dev, IOCTL_GET_SYS_TIME, (u32)&test_rtc_time);

        /* log_info("get_sys_time: %d-%d-%d %d:%d:%d\n", test_rtc_time.year, test_rtc_time.month, test_rtc_time.day, test_rtc_time.hour, test_rtc_time.min, test_rtc_time.sec); */
        time_r.year = test_rtc_time.year;
        time_r.month = test_rtc_time.month;
        time_r.day = test_rtc_time.day;
        time_r.hour = test_rtc_time.hour;
        time_r.min = test_rtc_time.min;
        time_r.sec = test_rtc_time.sec;

        ui_time_update_by_id(BASEFORM_2, &time_r);
        /* 关闭RTC设备 */
        dev_close(dev);
    }
}

static int timer_onchange(void *ctr, enum element_change_event e, void *arg)
{
    struct ui_time *time = (struct ui_time *)ctr;
    static u8 init_flog = 0;

    switch (e) {
    case ON_CHANGE_INIT:
        if (!init_flog) {
            init_flog = 1;
            log_info("!!!!!!!!!!!!!!!!!init");
            set_rtc_init_time();
            __this->time_time_id = sys_timer_add(NULL, timer_change_handler, 1000);
        }

        break;

    case ON_CHANGE_HIDE:
        break;

    case ON_CHANGE_SHOW_PROBE:
        break;

    case ON_CHANGE_SHOW_POST:
        break;

    default:
        return false;
    }

    return false;
}

REGISTER_UI_EVENT_HANDLER(BASEFORM_2)
.onchange = timer_onchange,
};
/*****************end**********************/

/*==================battery电池图标显示==================*/
static void battery_change_handler(void *priv)
{
    static u8 percent = 0;
    ui_battery_level_change(percent, 0);
    percent += 10;

    if (percent >= 120) {
        percent = 0;
    }
}

static int battery_onchange(void *ctr, enum element_change_event e, void *arg)
{
    struct ui_battery *battery = (struct ui_battery *)ctr;
    static u32 timer = 0;

    switch (e) {
    case ON_CHANGE_INIT:
        if (!timer) {
            timer = sys_timer_add(NULL, battery_change_handler, 1000); //开启进入动态电池电量显示
        }

        break;

    case ON_CHANGE_RELEASE:
        if (timer) {
            sys_timer_del(timer);
            timer = 0;
        }

        break;

    default:
        return false;
    }

    return false;
}

REGISTER_UI_EVENT_HANDLER(BASEFORM_4)
.onchange = battery_onchange,
};
/*****************end**********************/


/*=============SD卡图标===============*/
static void SD_car_image_change(char sd_in)
{
    if (sd_in) {
        ui_pic_show_image_by_id(BASEFORM_103, 0);
    } else {
        ui_pic_show_image_by_id(BASEFORM_103, 1);
    }
}
/*****************end**********************/

/*============录像红点闪烁以及录像控制===============*/
static void display_red_dot(void *priv)
{
    static u8 time = 0;
    time++;

    if (time == 2) {
        time = 0;
    }

    log_info(">>>>>>>>>red_dot=%d.\n", time);
    ui_pic_show_image_by_id(BASEFORM_6, time);
}

static void reset_ui_video_time(void)
{
    struct utime time_r;
    time_r.hour = 0;
    time_r.min = 0;
    time_r.sec = 0;
    ui_time_update_by_id(BASEFORM_5, &time_r);
}
static void ui_video_rec_display(void)
{
    struct utime time_r;
    static u8 run_flog = 0;
    log_info(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>ui_video_star");

    if (!run_flog) {
        run_flog = 1;
        reset_ui_video_time();
        ui_show_main(BASEFORM_104);
        ui_show_main(BASEFORM_5);
        log_info(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>add_time");
        ui_play_mp3(VIDEIO_START_MP3);
        __this->red_time_id = sys_timer_add(NULL, display_red_dot, 1000);
    } else {
        log_info(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>ui_video_stop");
        ui_hide_main(BASEFORM_104);
        ui_hide_main(BASEFORM_5);
        ui_play_mp3(VIDEIO_STOP_MP3);
        run_flog = 0;
        sys_timer_del(__this->red_time_id);
        ui_pic_show_image_by_id(BASEFORM_6, 0);
        __this->red_time_id = 0;
    }
}
//UI_camera录像控制
static void ui_video_rec_contrl_doing()
{
    video_rec_control_doing();
}

/*****************end**********************/

/*===========文件预览===============*/

/*********************************************************************************
 *  		     				预览框回调
 *********************************************************************************/
#if 0
static int browser_open_file(int p)
{
    struct intent it;
    FILE *fp = (FILE *)p;
    if (fp) {
        init_intent(&it);
        it.name = "video_dec";
        it.action = ACTION_VIDEO_DEC_OPEN_FILE;
        it.data = (const char *)fp;
        it.exdata = (u32)__this->path;
    }
    return 0;
}

static int browser_onchange(void *ctr, enum element_change_event e, void *arg)
{
    struct ui_browser *bro = ctr;

    switch (e) {
    case ON_CHANGE_INIT:
        bro->order = 1;
        bro->path = __this->path;
        /* __this->browser->path = cPATH[__this->dir]; */
        bro->ftype = "-tMOVJPGAVI -st -d";
        bro->show_mode = 1;

        if (__this->cur_file) {
            fclose(__this->cur_file);
            __this->cur_file = 0;
        }
        if (__this->page) {
            ui_file_browser_set_page(bro, __this->page);
        }
        if (__this->item) {
            ui_file_browser_highlight_item(bro, 0, false);
            ui_file_browser_highlight_item(bro, __this->item, true);
        }
        break;
    case ON_CHANGE_FIRST_SHOW:
        __this->if_in_bro = 1;
        sys_key_event_takeover(true, false);
        break;
    case ON_CHANGE_SHOW_COMPLETED:
        if (!ui_file_browser_page_num(bro)) {
            __this->no_file = 1;
            /* __dec_msg_show(BOX_MSG_NO_FILE, 0); */
        } else {
            __this->no_file = 0;
            /* __dec_msg_hide(BOX_MSG_NO_FILE); */
        }
        break;
    default:
        break;
    }
    return false;
}
static int browser_onkey(void *ctr, struct element_key_event *e)
{
    struct ui_browser *_browser = ctr;
    struct ui_file_attrs attrs;
    int _file_num = 0;
    __this->page = ui_file_browser_cur_page(_browser, &_file_num);
    __this->item = ui_file_browser_cur_item(_browser);

    switch (e->event) {
    case KEY_EVENT_CLICK:
        switch (e->value) {
        case KEY_MODE:
            //返回上级目录
            if (strcmp(__this->path, CONFIG_DEC_ROOT_PATH)) {
                __this->if_in_bro = 1;
                int len = strlen(__this->path);
                len--;
                while (--len) {
                    if (__this->path[len] != '/') {
                        __this->path[len] = 0;
                    } else {
                        break;
                    }
                }
                printf("path = %s\n", __this->path);
                /* ui_text_set_str_by_id(TEXT_PATH_BROWSER, "ascii", __this->path + sizeof(CONFIG_ROOT_PATH) - 1); */
                ui_file_browser_set_dir_by_id(BROWSER, __this->path, "-tMOVJPGAVI -st -d");
                return true;
            } else {
                log_d("root ptah");
                sys_key_event_takeover(false, true);
                sys_key_event_takeover(false, false);
                __this->if_in_bro = 0;
            }
            break;
        case KEY_OK:
            if (_file_num == 0) {
                log_d("no file\n");
                return true;
            }
            printf("item=%d \n", __this->item);
            ui_file_browser_get_file_attrs(_browser, __this->item, &attrs); //获取文件属性
            if (attrs.ftype == UI_FTYPE_DIR) {
                //进入文件夹
                if (__this->path == 0) {
                    log_e("DIR ERR\n");
                    return false;
                }
                strcat(__this->path, attrs.fname);
                int len = strlen(__this->path);
                __this->path[len] = '/';
                __this->path[len + 1] = '\0';

                printf("path=%s\n", __this->path);
#ifdef CONFIG_EMR_DIR_ENABLE
                if (strcmp(__this->path, CONFIG_DEC_PATH_1) == 0
                    || strcmp(__this->path, CONFIG_DEC_PATH_2) == 0
                    || strcmp(__this->path, CONFIG_DEC_PATH_3) == 0) {
                    log_d("dec_path");
                    ui_file_browser_set_dir_by_id(BROWSER, __this->path, "-tMOVJPGAVI -st -r");
                } else {
                    ui_file_browser_set_dir_by_id(BROWSER, __this->path, "-tMOVJPGAVI -st -d");
                }
#else
                ui_file_browser_set_dir_by_id(BROWSER, __this->path, "-tMOVJPGAVI -st -d");
#endif

                /* ui_text_set_str_by_id(TEXT_PATH_BROWSER, "ascii", __this->path + sizeof(CONFIG_ROOT_PATH) - 1); */
                __this->if_in_bro = 1;
                return true;
            } else if (attrs.ftype == UI_FTYPE_IMAGE || attrs.ftype == UI_FTYPE_VIDEO) {
                //播放文件
                printf("path=%s, num=%d\n", __this->path, attrs.file_num);
                if (attrs.attr.attr & F_ATTR_RO) {
                    __this->is_lock = 1;
                } else {
                    __this->is_lock = 0;
                }
                FILE *fp = ui_file_browser_open_file(_browser, __this->item);
                __this->sum_time.sec = 0;
                __this->sum_time.min = 0;
                __this->sum_time.hour = 0;
                /* ui_hide(LAYER_BROWSER); */
                /* ui_show(LAYER_UP_REP); */
                ui_set_call(browser_open_file, (int)fp);
                __this->if_in_bro = 0;
                return true;
            }
            break;
        case KEY_MENU:
            if (_file_num == 0) {
                log_d("no file\n");
                return false;
            }
            ui_file_browser_get_file_attrs(_browser, __this->item, &attrs); //获取文件属性
            if (attrs.ftype == UI_FTYPE_IMAGE || attrs.ftype == UI_FTYPE_VIDEO) {
                __dec_msg_hide(0);
                __this->cur_file = ui_file_browser_open_file(_browser, __this->item);
                /* ui_hide(LAYER_UP_REP); */
                /* ui_hide(LAYER_BROWSER); */
                /* ui_show(LAYER_MENU_REP); */
                /* ui_show(LAYOUT_MN_REP_REP); */
                __this->menu_status = 1;
                return true;
            }
            break;
        }
    default:
        if (e->event == KEY_EVENT_LONG && e->value == KEY_POWER) {
            sys_key_event_takeover(false, true);
            /* puts("\n take off msg power rep\n"); */
            return false;
        }
        break;
    }
    return false;
}


REGISTER_UI_EVENT_HANDLER(BROWSER)
.onchange = browser_onchange,
 .onkey = browser_onkey,
};
#endif

/*****************end**********************/

/*===========视频回放ui================*/
static const char ascii_str[] = "ABCDEFGHIJK";

static void ui_video_playback_show_text(char *file_name)
{
    /* ui_text_set_text_by_id(BASEFORM_64, file_name, strlen(file_name), FONT_DEFAULT); */
}
/*****************end**********************/

/*===========照片回放ui================*/
static void ui_photo_playback_show_text(char *file_name)
{
    ui_text_set_text_by_id(BASEFORM_68, file_name, strlen(file_name), FONT_DEFAULT);
}
void photo_find_to_show_lcd(char dir, char run_flog)//选择文件的方向dir = 1 nextfile 0you know,last ,run_flog = 1start, run_flog = 0end
{
    static FILE *fd = NULL;
    static struct vfscan *fs = NULL;
    static u8 run_time = 0;
    u32 f_size;
    char path[64];
    char *buf_img = NULL;

    if (run_time == 0) { // run one time start
        run_time = 1;
        strcpy(path, CONFIG_REC_PATH_0);
        fs = fscan(path, "-tJPG -st", 0);//筛选出JPG图片 每次调用这个都会重新指向第一个

        if (fs == NULL) {
            log_info("[error]>>>>>fs = NULL");
            return;
        }
    }

    if (run_flog == 0) { //run end
        run_time = 0;
        free(buf_img);
        fscan_release(fs);
        buf_img = NULL;
        fs = NULL;
        fd = NULL;
        run_time = 0;
        return;
    }

    if (fd == NULL) {
        fd = fselect(fs, FSEL_FIRST_FILE, 0);//文件选择第一个
    } else {
        if (dir) { //next
            fd = fselect(fs, FSEL_NEXT_FILE, 0);//文件选择下一个
        } else {
            fd = fselect(fs, FSEL_PREV_FILE, 0);//
            if (fd == NULL) {
                fd = fselect(fs, FSEL_LAST_FILE, 0);//
            }
        }
    }

    if (fd == NULL) {
        log_info("[error]>>>>>>>>>>>>>>>>fd == NULL");
        return;
    }

    f_size = flen(fd);
    buf_img = malloc(f_size);
    fread(buf_img, f_size, 1, fd);
    fclose(fd);
    log_info("\n [ERROR] %s -[yuyu] %d\n", __FUNCTION__, __LINE__);
    /* get_jpg_show_lcd(buf_img, f_size); */
    free(buf_img);
}


/*****************end**********************/
extern int avi_net_playback_unpkg_init(FILE *fd, u8 state);//state : 0 preview , 1 playbackh
void yuv420p_quto_rgb565(unsigned char *yuvBuffer_in, unsigned char *rgbBuffer_out, int width, int height);
int avi_video_get_frame(FILE *fd, int offset_num, u8 *buf, u32 buf_len, u8 state);
#define IDX_00DC   ntohl(0x30306463)
#define IDX_01WB   ntohl(0x30317762)
#define IDX_00WB   ntohl(0x30307762)
/*int start_play_video(const char *path)*/
/*==========视频回放ui=================*/
static void video_find_to_show_lcd(char dir, run_flog)//选择文件的方向
{
    int ret;
    char *fbuf = NULL;
    char *yuv = NULL;
    char *cy, *cb, *cr;
    int fbuflen = 50 * 1024;
    int num = 0;
    int pix;
    char ytype;
    int yuv_len;

    FILE *fd = NULL;
    char path[64];
    struct vfscan *fs = NULL;
    static char *buf_img = NULL;
    static u8 run_time = 0;

    if (buf_img == NULL) {
        buf_img = malloc(320 * 240 * 2);
        fbuf = malloc(fbuflen);
    }

    if (run_time == 0) {
        run_time = 1;
        strcpy(path, CONFIG_REC_PATH_0);
        fs = fscan(path, "-tAVI -st", 0);//筛选出AVI图片 每次调用这个都会重新指向第一个

        if (fs == NULL) {
            log_info("[error]>>>>>fs = NULL");
            return;
        }

    }

    if (run_flog == 0) { //run end
        run_time = 0;
        free(buf_img);
        fscan_release(fs);
        buf_img = NULL;
        fs = NULL;
        fd = NULL;
        run_time = 0;
        return;
    }

    if (fd == NULL) {
        fd = fselect(fs, FSEL_FIRST_FILE, 0);//文件选择第一个
    } else {
        if (dir) { //next
            fd = fselect(fs, FSEL_NEXT_FILE, 0);//文件选择下一个
        } else {
            fd = fselect(fs, FSEL_PREV_FILE, 0);//

            if (fd == NULL) {
                fd = fselect(fs, FSEL_LAST_FILE, 0);//
            }
        }
    }

    if (fd == NULL) {
        log_info("[error]>>>>>>>>>>>>>>>>fd == NULL");
        return;
    }

    ret = avi_net_playback_unpkg_init(fd, 1); //解码初始化,最多10分钟视频

    if (ret) {
        log_info("avi_net_playback_unpkg_init err!!!\n");
    }

    log_info("@@@>>>>>>>>>>video_start");

    while (1) {
        ret = avi_video_get_frame(fd, ++num, fbuf, fbuflen, 1); //全回放功能获取帧

        if (ret > 0) {
            struct jpeg_image_info info = {0};
            struct jpeg_decode_req req = {0};
            u32 *head = (u32 *)fbuf;
            u8 *dec_buf = fbuf;
            u32 fblen = ret;

            if (*head == IDX_00DC || *head == IDX_01WB || *head == IDX_00WB) {
                fblen -= 8;
                dec_buf += 8;
            }

            info.input.data.buf = dec_buf;
            info.input.data.len = fblen;

            if (jpeg_decode_image_info(&info)) { //获取JPEG图片信息
                log_info("jpeg_decode_image_info err\n");
                break;
            } else {
                switch (info.sample_fmt) {
                case JPG_SAMP_FMT_YUV444:
                    ytype = 1;
                    break;//444

                case JPG_SAMP_FMT_YUV420:
                    ytype = 4;
                    break;//420

                default:
                    ytype = 2;
                }

                pix = info.width * info.height;
                yuv_len = pix + pix / ytype * 2;

                if (!yuv) {
                    yuv = malloc(yuv_len);

                    if (!yuv) {
                        log_info("yuv malloc err len : %d , width : %d , height : %d \n", yuv_len, info.width, info.height);
                        break;
                    }
                }

                /*log_info("width : %d , height : %d \n", info.width, info.height);*/

                cy = yuv;
                cb = cy + pix;
                cr = cb + pix / ytype;

                req.input_type = JPEG_INPUT_TYPE_DATA;
                req.input.data.buf = info.input.data.buf;
                req.input.data.len = info.input.data.len;
                req.buf_y = cy;
                req.buf_u = cb;
                req.buf_v = cr;
                req.buf_width = info.width;
                req.buf_height = info.height;
                req.out_width = info.width;
                req.out_height = info.height;
                req.output_type = JPEG_DECODE_TYPE_DEFAULT;
                req.bits_mode = BITS_MODE_UNCACHE;
                /*req.bits_mode = BITS_MODE_CACHE;*/

                ret = jpeg_decode_one_image(&req);//JPEG转YUV解码

                if (ret) {
                    log_info("jpeg decode err !!\n");
                    break;
                }

                YUV420p_Soft_Scaling(yuv, NULL, 640, 480, 320, 240);
                yuv420p_quto_rgb565(yuv, buf_img, 320, 240);
                /* lcd_show_frame_1(buf_img, (320 * 240 * 2));//240*320*2=153600 */
                log_info("\n [ERROR] %s -[yuyu] %d\n", __FUNCTION__, __LINE__);
            }
        } else {
            log_info("@@@>>>>>>>>>>video_end");
            break ;
        }
    }

    run_time = 0; //这里没有做切视频播放 请参考照片切换
    free(buf_img);
    fscan_release(__this->fs);
    buf_img = NULL;
    __this->fs = NULL;
    fd = NULL;
    run_time = 0;
}
/*****************end**********************/

/*==========ui界面：主界面=================*/
static int ui_page1_main(void *_grid, enum element_change_event e, void *arg)
{
    struct ui_grid *grid = (struct ui_grid *)_grid;
    int err;

    switch (e) {
    case ON_CHANGE_INIT:
        (grid)->hi_index = __this->page_x;
        (grid)->page_mode = 1;
        break;

    case ON_CHANGE_RELEASE:
        break;

    default:
        return FALSE;
    }

    return FALSE;
}

REGISTER_UI_EVENT_HANDLER(BASEFORM_32)
.onchange = ui_page1_main,
};
/*****************end**********************/

/*==========显示某个界面===============*/
static void show_page(int id)
{
    ui_hide_curr_main(); //关闭上一个画面
    ui_show_main(id);
}
/*****************end**********************/
static void ui_demo_check_system_status(void)
{
    os_taskq_post(POST_TASK_NAME, 1, UI_EVENT_SD_IN_OUT);
}

static void ui_demo(void *priv)
{
    int msg[32];
    //初始化demo使用的变量
    ui_data = (struct ui_demo_date_hdl *)calloc(1, sizeof(struct ui_demo_date_hdl));
    //设置全局提示音音量
    __this->play_mp3_sound_size = 50;
    //设置定时器定时检查系统状态
    __this->ui_check_id = sys_timer_add(NULL, ui_demo_check_system_status, 200);
    //初始化摄像头
    camera_to_lcd_init(0);
    //设置ui方向 0为竖屏 1为横屏
    set_turn_mode(0);
    //初始化ui服务和lcd
    user_ui_lcd_init();
    //开机动画 20为延时 200ms每帧
    open_animation(1);
    //设置图像合成模式
    set_lcd_show_data_mode(UI_CAMERA);
    //ui显示界面
    ui_show_main(PAGE_0);

    while (1) {
        os_taskq_pend("taskq", msg, ARRAY_SIZE(msg));

        switch (msg[1]) {
        case UI_EVENT_VIDEO_START_STOP://录像启停
            if (storage_device_ready()) {
                ui_video_rec_display();
                ui_video_rec_contrl_doing();
            } else {
                ui_play_mp3(PLEASE_INSERT_SD_MP3);
            }

            break;

        case UI_EVENT_COMPLETE_RECORD://完成录像
            reset_ui_video_time();

            break;

        case UI_EVENT_TAKE_PHOTO://拍照控制
            if (storage_device_ready()) {
                ui_show_main(BASEFORM_108);
                printf("\n [ERROR] %s -[yuyu] %d\n", __FUNCTION__, __LINE__);

#ifdef CONFIG_UVC_VIDEO2_ENABLE

                __this->get_photo = 1;
#else
                if (get_video_rec_status()) { //如果开了录像就走VIDEO的视频录取拿照片
                    /* video_rec_take_photo(); */
                    user_video_rec_take_photo();
                } else {//如果没有开录像就走GET_YUV_DATA去获取照片
                    __this->get_photo = 1;
                }
#endif
                ui_play_mp3(TAKE_PHOTO_MP3);
                os_time_dly(10);
                ui_hide_main(BASEFORM_108);
            } else {
                ui_play_mp3(PLEASE_INSERT_SD_MP3);
            }

            break;

        case UI_EVENT_SD_IN_OUT://SD卡事件
            if (storage_device_ready()) {
                if (__this->status_sd) {
                    __this->status_sd = 0;
                    SD_car_image_change(1);
                    ui_play_mp3(SD_IN_MP3);
                }
            } else {
                if (!__this->status_sd) {
                    __this->status_sd = 1;
                    SD_car_image_change(0);
                    ui_play_mp3(SD_OUT_MP3);
                }
            }

            break;

        case UI_EVENT_POWER_CHECK://供电检查

            break;

        case UI_EVENT_IN_SET_MODE://进入设置模式

            break;

        case UI_EVENT_IN_VIEW://进入文件预览

            break;

        case UI_EVENT_IN_TIME://进入待机屏保

            break;
        }

#if 0
        if (get_lcd_show_deta_mode() == ui_user) {
            if (choice_photo_flog == 1) { //start init
                choice_photo_flog = 0;
                photo_find_to_show_lcd(1, 1);
            }

            if (choice_photo_flog == 2) { //photo next
                choice_photo_flog = 0;
                photo_find_to_show_lcd(1, 1);
            }

            if (choice_photo_flog == 3) { //photo last
                choice_photo_flog = 0;
                photo_find_to_show_lcd(0, 1);
            }

            if (choice_photo_flog == 4) { //out end
                choice_photo_flog = 0;
                photo_find_to_show_lcd(0, 0);
            }

            if (choice_video_flog == 1) {
                choice_video_flog = 0;
                video_find_to_show_lcd(1, 1);
            }
        }
#endif
    }
}

static int ui_demo_task_init(void)
{
    puts("ui_demo_task_init \n\n");
    return thread_fork("ui_demo", 11, 1024, 32, 0, ui_demo, NULL);
}
platform_initcall(ui_demo_task_init);

/*****************end**********************/

/*==========ui按键控制============*/
int ui_key_control(u8 value, u8 event)//纯ui按键控制
{
    int msg;

    switch (event) {
    case KEY_EVENT_CLICK:
        switch (value) {
        case KEY_K1:
            log_info(">>>>>>>>>>>>>>>>ui_key1");//向下

            if (__this->page_y == 0) {
                __this->page_x++;

                if (__this->page_x == 6) {
                    __this->page_x = 0;
                }

                log_info(">>>>>>>>>>page_x=%d", __this->page_x);
                ui_key_msg_post(KEY_DOWN); //这里的按键下是发命令给底层UI控制控件向下 不是按键的方向
            } else { //表示已经进入二级界面
                switch (__this->page_x) {
                case 0:	//图片质量
                    __this->page2_x++;

                    if (__this->page2_x == 4) {
                        __this->page2_x = 0;
                    }

                    ui_key_msg_post(KEY_DOWN);//这里的按键下是发命令给底层UI控制控件向下 不是按键的方向
                    break;

                case 1:	//语言
                    __this->language_indx ^= 1;
                    ui_text_show_index_by_id(BASEFORM_11, __this->language_indx);//切换语言文字
                    break;

                case 2:	//日期时间
                    if (__this->page2_y) { //二级菜单确认
                        time_set_page(__this->page2_x, 0);//加时间操作
                    } else { //二级菜单退出确认
                        __this->page2_x++;

                        if (__this->page2_x == 6) {
                            __this->page2_x = 0;
                        }

                        ui_key_msg_post(KEY_DOWN); //这里的按键下是发命令给底层UI控制控件向下 不是按键的方向
                    }

                    break;

                case 3:	//声音
                    sound_mub(__this->play_mp3_sound_size, 0);

                    break;

                case 4:	//录像回放
                    /* choice_video_flog = 1; */

                    break;

                case 5:	//相册
                    /* choice_photo_flog = 2; */
                    break;
                }
            }

            break;

        case KEY_K2:
            log_info(">>>>>>>>>>>>>>>ui_key1");//向上

            if (__this->page_y == 0) {
                __this->page_x--;

                if (__this->page_x == 255) {
                    __this->page_x = 5;
                }

                log_info(">>>>>>>>>>page_x=%d", __this->page_x);
                ui_key_msg_post(KEY_UP);
            } else { //表示已经进入二级界面
                switch (__this->page_x) {
                case 0:	//图片质量
                    __this->page2_x--;

                    if (__this->page2_x == 255) {
                        __this->page2_x = 3;
                    }

                    ui_key_msg_post(KEY_UP);
                    break;

                case 1:	//语言
                    __this->language_indx ^= 1;
                    ui_text_show_index_by_id(BASEFORM_11, __this->language_indx);//切换语言文字
                    break;

                case 2:	//日期时间
                    if (__this->page2_y) { //二级菜单确认
                        time_set_page(__this->page2_x, 1);//减时间操作
                    } else { //二级菜单退出确认
                        __this->page2_x--;

                        if (__this->page2_x == 255) {
                            __this->page2_x = 5;
                        }

                        ui_key_msg_post(KEY_UP);
                    }

                    break;

                case 3:	//声音
                    sound_mub(__this->play_mp3_sound_size, 1);
                    break;

                case 4:	//录像回放

                    break;

                case 5:	//相册
                    /* choice_photo_flog = 3; */
                    break;
                }
            }

            break;

        case KEY_K3:
            log_info(">>>>>>>>>>>>>>>>ui_key3");//确定

            switch (__this->page_x) { //目前在一级菜单 二级菜单控制 note.page_y = 0; 表示退出二级菜单
            case 0:	//图片质量
                __this->page_y++;

                if (__this->page_y == 2) {
                    __this->page_y = 0;
                }

                break;

            case 1:	//语言 //后面可以进行高亮语言文字
                __this->page_y++;

                if (__this->page_y == 2) {
                    __this->page_y = 0;
                }

                break;

            case 2:	//日期时间
                __this->page_y = 1;

                if (__this->init_flog == 1) { //初始化完后在点确认//才能++；
                    __this->page2_y++;
                }

                if (__this->page2_y == 2) { //二级菜单中确认控制
                    __this->page2_y = 0;
                }

                if (__this->page2_x == 5) { //完成秒设置后退出
                    __this->page_y++;

                    if (__this->page_y == 2) {
                        __this->page_y = 0;
                    }
                }

                break;

            case 3:	//声音
                __this->page_y++;

                if (__this->page_y == 2) {
                    __this->page_y = 0;
                }

                break;

            case 4:	//录像回放
                __this->page_y++;

                if (__this->page_y == 2) {
                    __this->page_y = 0;
                }

                break;

            case 5:	//相册
                __this->page_y++;

                if (__this->page_y == 2) {
                    __this->page_y = 0;
                }

                break;
            }

            if (__this->page_y == 1) { //进入二级菜单
                switch (__this->page_x) {
                case 0:	//图片质量
                    show_page(PAGE_4);
                    break;

                case 1:	//语言 //后面可以进行高亮语言文字
                    break;

                case 2:	//日期时间
                    if (__this->init_flog == 0) {
                        __this->init_flog = 1;
                        show_page(PAGE_3);
                        get_rtc_time();
                        time_updata();
                    }

                    break;

                case 3:	//声音
                    if (__this->init_flog == 0) {
                        __this->init_flog = 1;
                        form_highlight(BASEFORM_69, __this->init_flog); //自定义数字不支持高亮
                    }
                    break;

                case 4:	//录像回放
                    set_lcd_show_data_mode(UI_USER);
                    show_page(PAGE_5);
                    /* ui_video_playback_show_text("video"); */
                    /* choice_video_flog = 1; */
                    break;

                case 5:	//相册
                    set_lcd_show_data_mode(UI_USER);
                    show_page(PAGE_6);
                    /* choice_photo_flog = 1; */
                    log_info(">>>>>lcd_mode == ui_camera");
                    break;
                }
            } else { //退出二级菜单
                switch (__this->page_x) {
                case 1:	//语言
                    if (__this->language_indx == 1) {
                        ui_language_set(Chinese_Traditional);
                        log_info(">>>Chinese_Traditional");
                    } else {
                        ui_language_set(Chinese_Simplified);
                        log_info(">>>Chinese_Simplified");
                    }

                    break;

                case 2:	//日期时间
                    __this->init_flog = 0;
                    __this->page2_x = 0;
                    __this->page2_y = 0;
                    undata_sys_time();
                    break;

                case 3:	//声音
                    __this->init_flog = 0;
                    form_highlight(BASEFORM_69, __this->init_flog); //自定义数字不支持高亮
                    break;

                case 4:	//录像回放
                    set_lcd_show_data_mode(UI);
                    break;

                case 5:	//相册
                    set_lcd_show_data_mode(UI);
                    log_info(">>>>>lcd_mode == ui");
                    /* choice_photo_flog = 4; */
                    break;
                }
                show_page(PAGE_1);
                sound_mub_show_init(__this->play_mp3_sound_size);
            }

            break;

        case KEY_K4:
            log_info(">>>>>>>>>>>>>>>key4");//按键切显示模式
            set_lcd_show_data_mode(UI);
            log_info(">>>>>lcd_mode == ui");
            show_page(PAGE_1);
            sound_mub_show_init(__this->play_mp3_sound_size);
            break;
        }

        break;

    default:
        break;
    }

    return 0;
}
/*****************end**********************/

/*==========ui_camera按键控制============*/
int ui_camera_key_control(u8 value, u8 event)//图像合成界面按键控制
{
    switch (event) {
    case KEY_EVENT_CLICK:
        switch (value) {
        case KEY_K1:
            log_info(">>>>>>>>>>>>>>>>ui_camera_key1"); //拍照
            os_taskq_post(POST_TASK_NAME, 1, UI_EVENT_TAKE_PHOTO);
            break;

        case KEY_K2:
            log_info(">>>>>>>>>>>>>>>>ui_camera_key2"); //录像
            os_taskq_post(POST_TASK_NAME, 1, UI_EVENT_VIDEO_START_STOP);
            break;

        case KEY_K3:
            log_info(">>>>>>>>>>>>>>>>ui_camera_key3"); //切换摄像头
            break;

        case KEY_K4:
            log_info(">>>>>>>>>>>>>>>key4");//按键切显示模式
            show_page(PAGE_0);
            sound_mub_show_init(__this->play_mp3_sound_size);
            set_lcd_show_data_mode(UI_CAMERA);
            log_info(">>>>>lcd_mode == ui_camera");
            break;
        }

    case KEY_K5:
        log_info(">>>>>>>>>>>>>>>>ui_camera_key5");
        break;

    case KEY_K6:
        log_info(">>>>>>>>>>>>>>>>ui_camera_key6");
        break;

    case KEY_K7:
        log_info(">>>>>>>>>>>>>>>>ui_camera_key7");
        break;
        break;

    default:
        break;
    }

    return 0;
}
/*****************end**********************/
#endif
#endif
#endif
