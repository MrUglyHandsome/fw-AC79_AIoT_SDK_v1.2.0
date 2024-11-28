#include "app_config.h"
#ifdef CONFIG_UI_ENABLE //上电执行则打开app_config.h TCFG_DEMO_UI_RUN = 1

#include "device/device.h"//u8
#include "event/key_event.h"

#include "asm/gpio.h"
#include "system/includes.h"

/****************按键部分**********************/
enum data_mode {
    camera,
    ui,
    ui_camera,
};

extern u8 data_mode;

extern int ui_key_control(u8 value, u8 event);
extern int ui_camera_key_control(u8 value, u8 event);

static int camera_ui_key_click(struct key_event *key)
{
    static u8 mode = 0; //0为摄像头输出模式 1为UI输出模式

    int err;

    printf("[msg]>>>>>>>>>>>key->value=%d", key->value);
    switch (key->action) {
    case KEY_EVENT_CLICK:
        switch (key->value) {
        case KEY_MODE: //IO_key//键值
            printf(">>>>>>>>>>>>>>>>key power_off");
            break;
        case KEY_1:
            printf(">>>>>>>>>>>>>>>key1");
            break;
        case KEY_2:
            printf(">>>>>>>>>>>>>>>>key2");
            break;
        case KEY_3:
            printf(">>>>>>>>>>>>>>>>key3");
            break;
        case KEY_4:
            mode = !mode;//按键切显示模式
            printf(">>>>>>>>>>>>>>>key4");
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
    printf("[msg]>>>>>>>>>>>mode=%d", mode);
    if (!mode) { //按键分支 一个模式对应一个按键分支 便于编写
        printf(">>>>>>>>ui_camera_key");
        ui_camera_key_control(key->value, key->action);
    } else {
        printf(">>>>>>>>ui_key");
        ui_key_control(key->value, key->action);
    }
    return false;
}

int camera_ui_key_event_handler(struct key_event *key)
{
    char ret;
    switch (key->action) {
    case KEY_EVENT_CLICK:
        ret = camera_ui_key_click(key);
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
#endif
