#include "app_config.h"
#ifdef CONFIG_UI_ENABLE //上电执行则打开app_config.h TCFG_DEMO_UI_RUN = 1

#include "device/device.h"//u8
#include "storage_device.h"//SD
#include "server/video_dec_server.h"//fopen
#include "system/includes.h"//GPIO
#include "lcd_drive.h"
#include "sys_common.h"
#include "yuv_soft_scalling.h"
#include "asm/jpeg_codec.h"
#include "get_yuv_data.h"
#include "lcd_config.h"
#include "asm/includes.h"
#include "ui_demo.h"
#include "video/camera_effect.h"
#include "asm/jpeg_encoder.h"
#include "lcd_data_driver.h"



/************视频部分*将摄像头数据显示到lcd上**********************************/
/* 开关打印提示 */
#if 1
#define log_info(x, ...)    printf("\n[camera_to_lcd_test]>>>>>>>>>>>>>>>>>>>###" x " \n", ## __VA_ARGS__)
#else
#define log_info(...)
#endif

struct scale_img {
    int yuv_pixformat;
    int in_w;
    int in_h;
    u8 *yuv;
    u8 *yuv1;
    u8 *y_in;
    u8 *u_in;
    u8 *v_in;
    int out_w;
    int out_h;
    u8 *y_out;
    u8 *u_out;
    u8 *v_out;
    u8 *line_buf;
    int line_buf_size;
    u8 line;
    u8 jpg_q;
    u32 jpg_buf_size;
    u8 *jpg_buf;
    struct jpeg_encoder_fd *jpg;
    struct YUV_frame_data *input_frame;
};

static struct scale_img *hdl;


#if PROCESS_EFFECT
struct camera_effect_config eft_cfg = {0};//配置变量
effect_mode_t kaleidoscope_mode = EM_END;//万花筒样式
#endif



u8 get_lcd_turn_mode(void);

static void Calculation_frame(void)
{
    static u32 tstart = 0, tdiff = 0;
    static u8 fps_cnt = 0;

    fps_cnt++ ;

    if (!tstart) {
        tstart = timer_get_ms();
    } else {
        tdiff = timer_get_ms() - tstart;

        if (tdiff >= 1000) {
            printf("\n [MSG]fps_count = %d\n", fps_cnt *  1000 / tdiff);
            tstart = 0;
            fps_cnt = 0;
        }
    }
}
/********保存JPG图片到SD卡中************/
static void save_jpg_ontime(u8 *buf, u32 len)
{
    if (storage_device_ready()) {//自动重命名文件数量限制配置搜索 FILE_AUTO_RENAME_NUM
        char file_name[64];//定义路径存储
        snprintf(file_name, 64, CONFIG_ROOT_PATH"JPG/JPG****.JPG");
        FILE *fd = fopen(file_name, "wb+");
        printf("[msg]>>>>>>>>>>>file_name=%s", file_name);
        fwrite(buf, 1, len, fd);
        fclose(fd);
        log_info("JPG save ok name=JPG\r\n");
    }
}
/********保存RGB数据到SD卡中************/
static void get_RGB_save_to_SD(char *RGB, u32 len)
{
    if (storage_device_ready()) {
        char file_name[64];//定义路径存储
        snprintf(file_name, 64, CONFIG_ROOT_PATH"rgb/RGB***.bin");
        FILE *fd = fopen(file_name, "wb+");
        fwrite(RGB, 1, len, fd);
        fclose(fd);
        log_info("RGB save ok name=RGB\r\n");
    }
}
/********保存YUV数据到SD卡中************/
static void save_YUV_date_ontime(u8 *buf, u32 len)
{
    if (storage_device_ready()) {
        char file_name[64];//定义路径存储
        snprintf(file_name, 64, CONFIG_ROOT_PATH"YUV/YUV***.bin");
        FILE *fd = fopen(file_name, "wb+");
        fwrite(buf, 1, len, fd);
        fclose(fd);
        log_info("YUV save ok name=YUV\r\n");
    }
}

/********读取JPG数据并保存在SD卡中该数据经过缩放保存大小1280*720********/
/* #define yuv_out_w   1280 */
/* #define yuv_out_h   720 */
/* #define in_w 		640 */
/* #define in_h 		480 */

#define IN_W 640                    //图像输入尺寸 宽
#define IN_H 480                    //图像输入尺寸 高
#define AIM_W 4800                  //图像输出尺寸 宽
#define AIM_H 2400                  //图像输出尺寸 宽
#define CALCULATE_H 16              //图像每次处理行数 //只能处理16行的整数倍
#define CNT                         (AIM_H/CALCULATE_H) //累计需要处理次数为输出高/每次行数
#define YUV_BUF_16_LINE_SIZE        (AIM_W*CALCULATE_H*3/2) //一次处理数据需要的内存长度
#define JPG_BUF_LEN                 (1*1024*1024) //jpg数据处理需要的内存长度 //需要评估最大内存
#define JPG_Q                       10  //输出jpg的质量 最大16

static void get_JPG_save_to_SD(char *yuv_buf, u8 mode)
{
    int err;
    u8 *ui_data;
    u8 *rgb_buf;
    u8 *yuv_420;
    //1.申请全局结构体内存
    hdl = malloc(sizeof(struct scale_img));
    if (hdl == NULL) {
        printf("[error]>>>>>>>>>>>>malloc fail");
    }
    //2.申请16行buf
    hdl->line_buf = malloc(YUV_BUF_16_LINE_SIZE);
    if (!hdl->line_buf) {
        goto exit;
    }
    //3.申请jpg输入帧结构体
    hdl->input_frame = malloc(sizeof(struct YUV_frame_data));
    if (hdl->input_frame == NULL) {
        goto exit;
    }
    //4.申请jpg_buf用于输出jpg照片
    hdl->jpg_buf = malloc(JPG_BUF_LEN);
    if (hdl->jpg_buf == NULL) {
        goto exit;
    }
    if (mode == PHOTO_MODE_TAKE_BOOTH) {
        //5.申请yuv_buf用于将ui数据放大到摄像头图像大小用于图像合成
        hdl->yuv = malloc(IN_W * IN_H * 2);
        if (hdl->yuv == NULL) {
            goto exit;
        }
    }

    rgb_buf = hdl->jpg_buf;
    yuv_420 = hdl->jpg_buf + lcd_w * lcd_h * 2;

    //获取ui保存的大头贴图片
    if (mode == PHOTO_MODE_TAKE_BOOTH) {
        ui_data = get_ui_buf();
        //将ui大头贴数据竖屏的RGB数据转为横屏的RGB数据
        RGB565_Soft_90(0, rgb_buf, ui_data, lcd_w, lcd_h);
        //将RGB转为YUV数据
        rgb565_to_yuv420p(rgb_buf, yuv_420, lcd_h, lcd_w, 1);
        //将屏幕那么大的YUV数据转为摄像头那么大的YUV数据
        YUV420p_Soft_Scaling(yuv_420, hdl->yuv, lcd_h, lcd_w, IN_W, IN_H);
        //进行yuv图像合成处理
        yuv_picture_compose(hdl->yuv, yuv_buf, IN_W, IN_H);
    }

    //5.配置全局参数配置
    hdl->in_w = IN_W;                                  //配置输入图像yuv420宽
    hdl->in_h = IN_H;                                  //配置输入图像yuv420高
    hdl->out_w = AIM_W;                                //配置输出图像yuv420宽
    hdl->out_h = AIM_H;                                //配置输出图像yuv420高
    hdl->line = CALCULATE_H;                           //配置每次放大行数必须为16的整数倍
    hdl->line_buf_size = YUV_BUF_16_LINE_SIZE;         //配置yuv420 16行buf长度
    hdl->jpg_buf_size = JPG_BUF_LEN;                   //配置jpg内存长度
    hdl->yuv_pixformat = JPG_SAMP_FMT_YUV420;          //配置jpg编码数据格式
    hdl->jpg_q = JPG_Q;                                //配置编码q值
    hdl->input_frame->width = hdl->out_w;              //配置jpg编码器输出jpg图像宽
    hdl->input_frame->height = hdl->out_h;             //配置jpg编码器输出jpg图像高
    hdl->input_frame->data_height = hdl->line;         //配置jpg编码器每次编码行数
    hdl->input_frame->pixformat = JPG_SAMP_FMT_YUV420; //配置jpg编码器编码数据格式
    hdl->jpg = mjpg_image_enc_open(NULL, JPEG_ENC_MANU_IMAGE);
    if (!hdl->jpg) {
        printf("mjpg_image_enc_open err\n");
        goto exit;
    }

    u32 time1 = timer_get_ms();
    //缩放处理
    for (u16 i = 0; i < CNT; i++) {
        hdl->y_in = yuv_buf;
        hdl->y_out = hdl->line_buf;
        //16行yuv有16行y
        jl_nearest_inter_line(hdl->line, i, hdl->in_w, hdl->in_h, hdl->out_w, hdl->out_h, hdl->y_in, hdl->y_out);
        hdl->u_in = yuv_buf + hdl->in_w * hdl->in_h;
        hdl->u_out = hdl->line_buf + hdl->out_w * hdl->line;
        //16行yuv有8行u
        jl_nearest_inter_line(hdl->line / 2, i, hdl->in_w / 2, hdl->in_h / 2, hdl->out_w / 2, hdl->out_h / 2, hdl->u_in, hdl->u_out);
        hdl->v_in = yuv_buf + hdl->in_w * hdl->in_h * 5 / 4;
        hdl->v_out = hdl->line_buf + hdl->out_w * hdl->line * 5 / 4;
        //16行yuv有8行v
        jl_nearest_inter_line(hdl->line / 2, i, hdl->in_w / 2, hdl->in_h / 2, hdl->out_w / 2, hdl->out_h / 2, hdl->v_in, hdl->v_out);
        //6.每1行MCU(16行像素点)编码一次
        hdl->input_frame->line_num = i * hdl->line;
        hdl->input_frame->y = hdl->y_out;
        hdl->input_frame->u = hdl->u_out;
        hdl->input_frame->v = hdl->v_out;
        err = mjpg_image_enc_start(hdl->jpg, hdl->input_frame, hdl->jpg_buf, hdl->jpg_buf_size, hdl->jpg_q);
        if (err < 0) {
            printf("\n [ERROR] %s -[yuyu] %d\n", __FUNCTION__, __LINE__);
            goto exit;
        }
    }

    save_jpg_ontime(hdl->jpg_buf, hdl->jpg->file_size);
    printf("[msg]>>>>>>>>>>>hdl->jpg->file_size=%dk", hdl->jpg->file_size / 1024);
    u32 time = timer_get_ms() - time1;
    printf("[msg]>>>>>>>>>>>take_photo time=%dms", time);
    mjpg_image_enc_close(hdl->jpg);

exit:
    printf("\n [ERROR] %s -[yuyu] %d\n", __FUNCTION__, __LINE__);
    if (mode == PHOTO_MODE_TAKE_BOOTH) {
        free(hdl->yuv);
    }
    free(hdl->jpg_buf);
    free(hdl->input_frame);
    free(hdl->line_buf);
    free(hdl);
}

#if PROCESS_EFFECT  /*万花筒功能*/
get_kaleidoscope_data(u8 *in, u8 *out, int yuv_in_w, int yuv_in_h)
{
    eft_cfg.eff_t.w = yuv_in_w;
    eft_cfg.eff_t.h = yuv_in_h;
    eft_cfg.eff_t.wh_mode = 0;  // 横屏-0
    eft_cfg.eff_t.mode_full = 0;//暂时没有用到
    eft_cfg.indicator = 1;//标志，不影响数据值，0-src=dst(把原来的数据也改了),1-dst(原来数据不变，dst是处理过的数据)
    eft_cfg.dst_buf = out; //dst
    eft_cfg.mode = kaleidoscope_mode;//样式选择

    if (!effect_init(&eft_cfg.eff_t, eft_cfg.eff_t.wh_mode, eft_cfg.eff_t.w, eft_cfg.eff_t.h, &eft_cfg.mode)) {
        process_effect(&eft_cfg.eff_t, in, eft_cfg.dst_buf, &eft_cfg.mode, &eft_cfg.indicator);
        effect_free(&eft_cfg.eff_t);
    } else {
        log_e("camera effect init err!\n");
    }
}


void set_kaleidoscope_mode(u8 demo)
{
    kaleidoscope_mode = demo;
}
#endif  //PROCESS_EFFECT

/*********摄像头应用部分***********/
static void test_main(u8 *buf, u32 len, int yuv_in_w, int yuv_in_h)
{
#if CONFIG_LCD_QR_CODE_ENABLE
    char *ssid = NULL;
    char *pwd = NULL;
    qr_code_get_one_frame_YUV_420(buf, yuv_in_w, yuv_in_h);
    qr_get_ssid_pwd(&ssid, &pwd);
    printf(">>>>>>>>>>>>>>>sssid = %s, pwd = %s", ssid, pwd);
#endif //CONFIG_LCD_QR_CODE_ENABLE

#if PROCESS_EFFECT  /*万花筒功能*/
    u8 *baddr = NULL;
    if (kaleidoscope_mode != EM_END) {
        baddr = (u8 *)malloc(len);
        get_kaleidoscope_data(buf, baddr, yuv_in_w, yuv_in_h);
        buf = baddr;
        if (get_take_photo_status() == PHOTO_MODE_TAKE_KALEIDOSCOPE) {
            get_JPG_save_to_SD(buf, PHOTO_MODE_TAKE_KALEIDOSCOPE);
            set_take_photo_ok();
        }
    }
#endif //PROCESS_EFFECT


    u8 mode = get_take_photo_status();
    if (mode != 0 && mode != PHOTO_MODE_TAKE_KALEIDOSCOPE) {
        get_JPG_save_to_SD(buf, PHOTO_MODE_TAKE_BOOTH); //拍照大头贴 | 多图像合成,保存成JPG的方法一样
        set_take_photo_ok();
    }
    switch (mode) {
    case PHOTO_MODE_TAKE_BOOTH://拍照大头贴 | 多图像合成,保存成JPG的方法一样
        printf(">>>>>>>PHOTO_MODE_TAKE_BOOTH<<<<<<<<\n");
        break;
    case PHOTO_MODE_TAKE_MULTIPLE_1://高二等分 图像合成模式
        printf(">>>>>>>PHOTO_MODE_TAKE_MULTIPLE_1<<<<<<<<\n");
        break;
    case PHOTO_MODE_TAKE_MULTIPLE_2://高三等分 图像合成模式
        printf(">>>>>>>PHOTO_MODE_TAKE_MULTIPLE_2<<<<<<<<\n");
        break;
    case PHOTO_MODE_TAKE_MULTIPLE_3://对角等分 图像合成模式
        printf(">>>>>>>PHOTO_MODE_TAKE_BOOTH<<<<<<<<\n");
        break;
    case PHOTO_MODE_TAKE_MULTIPLE_4://不规则 图像合成模式
        printf(">>>>>>>PHOTO_MODE_TAKE_BOOTH<<<<<<<<\n");
        break;
    }

    lcd_show_frame(buf, len, yuv_in_w, yuv_in_h); //240*320*2=153600

#if PROCESS_EFFECT  /*万花筒功能*/
    if (kaleidoscope_mode != EM_END) {
        if (baddr != NULL) {
            free(baddr);
        }
    }
#endif //PROCESS_EFFECT

    Calculation_frame();
}

void set_video_rt_cb(u32(*cb)(void *, u8 *, u32), void *priv);
int user_net_video_rec_open(char forward);
int user_net_video_rec_close(char forward);
int jpeg2yuv_open(void);
void jpeg2yuv_yuv_callback_register(void (*cb)(u8 *data, u32 len, int width, int height));
int jpeg2yuv_jpeg_frame_write(u8 *buf, u32 len);
void jpeg2yuv_close(void);
static void uvc_video_test(void);

void get_yuv_init(void (*cb)(u8 *data, u32 len, int width, int height));

void camera_to_lcd_init(char camera_ID)//摄像头选择 并初始化
{
#ifdef CONFIG_UVC_VIDEO2_ENABLE
    uvc_video_test();
#else
    get_yuv_init(test_main);
#endif
}

void camera_to_lcd_uninit(void)
{
    void get_yuv_uninit(void);
    get_yuv_uninit();
}
int isc_log_en()//如果定义该函数丢帧信息屏蔽
{
    return 0;
}

static void uvc_jpeg_cb(void *hdr, void *data, u32 len)
{
#define JPEG_HEAD 0xE0FFD8FF
#define JPEG_HEAD1 0xC0FFD8FF
    u32 *head = (u32 *)data;
    if (*head == JPEG_HEAD || *head == JPEG_HEAD1) {
        //video
        jpeg2yuv_jpeg_frame_write((u8 *)data, len);
    } else {
        //audio
    }
}

static void camera_show_lcd(u8 *buf, u32 size, int width, int height)
{
    char *ssid = NULL;
    char *pwd = NULL;
#if CONFIG_LCD_QR_CODE_ENABLE
    qr_code_get_one_frame_YUV_420(buf, width, height);
    qr_get_ssid_pwd(&ssid, &pwd);
    printf(">>>>>>>>>>>>>>>sssid = %s, pwd = %s", ssid, pwd);
#endif
    lcd_show_frame(buf, size, width, height);
    Calculation_frame();
}

static void uvc_video_task(void *priv)
{
    int ret;
start:
    //0.等待UVC上线
    while (!dev_online("uvc")) {
        os_time_dly(10);
    }
    //1.打开jpeg解码YUV
    ret = jpeg2yuv_open();
    if (ret) {
        return;
    }
    //2.注册YUV数据回调函数
    jpeg2yuv_yuv_callback_register(camera_show_lcd);

#if (APP_VIDEO_REC_RUN)
    /* extern OS_SEM video_sem; */
    /* os_sem_set(&video_sem, 0); */
    /* os_sem_pend(&video_sem, 0); */
#endif

    //3.打开UVC实时流
    ret = user_net_video_rec_open(1);
    if (ret) {
        jpeg2yuv_close();
        return;
    }
    //3.注册jpeg数据回调函数
    set_video_rt_cb(uvc_jpeg_cb, NULL);

#if	TCFG_LCD_USB_SHOW_COLLEAGUE
    void set_tcp_uvc_rt_cb(u32(*cb)(void *priv, void *data, u32 len));
    void set_udp_uvc_rt_cb(u32(*cb)(void *priv, void *data, u32 len));
    set_udp_uvc_rt_cb(uvc_jpeg_cb); //wifi udp
    set_tcp_uvc_rt_cb(uvc_jpeg_cb); //wifi tcp
#endif

    //4.检查是否掉线
    struct lbuf_test_head *rbuf = NULL;

    while (dev_online("uvc")) {
        os_time_dly(5);
    }
#if (!APP_VIDEO_REC_RUN)
    //5.关闭UVC实时流
    user_net_video_rec_close(1);
    //6.关闭jpeg解码YUV
    jpeg2yuv_close();
#endif
    goto start;

}
static void uvc_video_test(void)
{
    thread_fork("uvc_video_task", 5, 1024, 0, 0, uvc_video_task, NULL);
}
#endif

