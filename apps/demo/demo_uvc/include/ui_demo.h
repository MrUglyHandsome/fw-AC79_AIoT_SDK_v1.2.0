#ifndef __UI_DEMO_H__
#define __UI_DEMO_H__

#include "ui/ui.h"
#include "ui_api.h"
#include "video_rec.h"
#include "yuv_soft_scalling.h"
#include "event/key_event.h"
#include "net_video_rec.h"
#include "asm/jpeg_codec.h"
#include "ui_demo.h"
#include "action.h"

struct ui_demo_date_hdl {
    FILE *fs;	            //文件指针
    u8 page_x;			    //记录光标位置
    u8 page_y;			    //记录页信息
    u8 page2_x;			    //记录二级界面光标信息
    u8 page2_y;			    //记录二级界面光标信息
    u8 page2_k;			    //记录二级菜单确认信息
    u8 language_indx;       //语言序号
    u8 init_flog;           //刷新标志
    u8 get_photo;   	    //拍照控制
    u8 play_mp3_sound_size; //播放音频声音
    u32 red_time_id;        //录像定时器ID
    u32 time_time_id;       //刷新时间定时器ID
    u32 ui_check_id;        //刷新时间定时器ID
    u8 status_sd;   	    //SD卡状态

    char path[128];			// 文件夹路径
    u8 if_in_bro;			// 是否在预览窗口
    u16 page;				// 文件预览页码
    u16 item;				// 页内位置
    FILE *cur_file;			// 当前文件

    u8 if_in_rep;           /* 是否正在播放 */
    s8 ff_fr_flag;          /* 是否正在快进快退, 0 否，1 快进， -1快退 */
    u8 no_file;
    u8 file_type;           /* 文件是JPG还是MOV,0 jpg, 1 mov */
    u8 file_type_index;     /* 文件分辨率的索引号，根据UI */
    u8 ppt;
    u8 is_lock;
    u8 err_file;
    const char *fname;      /* 文件名 */
    struct utime sum_time;  /* 视频的总时间 */
    struct utime cur_time;  /* 视频的当前时间 */

    u8 menu_status;
    u8 init;
    u8 battery_val;
    u8 battery_char;


};

//初始化时间结构体
static struct sys_time test_rtc_time = {
    .sec = 44,
    .min = 23,
    .hour = 14,
    .day = 21,
    .month = 4,
    .year = 2021,
};


//重定义Ui ID
#define __TIME1 BASEFORM_51 //年
#define __TIME2 BASEFORM_52 //月
#define __TIME3 BASEFORM_53 //日
#define __TIME4 BASEFORM_54 //时
#define __TIME5 BASEFORM_55 //分
#define __TIME6 BASEFORM_56 //秒

#define __SOUND BASEFORM_69

//ui_demo_使用的音频文件定义
#define SD_IN_MP3   		   "car_sd_in.mp3"
#define SD_OUT_MP3             "car_sd_out.mp3"
#define POWER_ON_MP3           "poweron.mp3"
#define TAKE_PHOTO_MP3         "car_photo.mp3"
#define VIDEIO_STOP_MP3        "car_stop.mp3"
#define VIDEIO_START_MP3       "car_start.mp3"
#define PLEASE_INSERT_SD_MP3   "car_need_sd.mp3"

//ui_demo_涉及到的UI状态事件
enum {
    UI_EVENT_VIDEO_START_STOP = 1,//开始录像
    UI_EVENT_COMPLETE_RECORD,     //完成录像事件
    UI_EVENT_TAKE_PHOTO,          //拍照
    UI_EVENT_SD_IN_OUT,           //sd卡插入sd卡弹出
    UI_EVENT_IN_SET_MODE,         //进入设置模式
    UI_EVENT_IN_VIEW,             //进入视图浏览图片视频
    UI_EVENT_IN_TIME,             //进入时间屏保
    UI_EVENT_POWER_CHECK,         //电池电量检查
};

#if CONFIG_MP3_DEC_ENABLE
extern void post_msg_play_flash_mp3(char *file_name, u8 dec_volume);
#endif
//摄像头选择 并初始化
extern void camera_to_lcd_init(char camera_ID);
//关闭摄像头
extern void camera_to_lcd_uninit(void);
extern void set_take_photo_ok(void);
extern u8 get_take_photo_status(void);
extern void *get_ui_demo_hd(void);
//开录像后去获取照片用这个接口
extern int video_rec_take_photo(void);
//获取当前录像状态
extern int get_video_rec_status(void);

#endif
