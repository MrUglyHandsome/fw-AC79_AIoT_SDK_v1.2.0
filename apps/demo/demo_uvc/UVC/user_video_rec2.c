#include "system/includes.h"
#include "server/video_server.h"
#include "app_config.h"
#include "asm/debug.h"
#include "asm/osd.h"
#include "asm/isc.h"
#include "app_database.h"
#include "storage_device.h"
#include "server/ctp_server.h"
#include "os/os_api.h"
#include "camera.h"
#include "net_server.h"
#include "server/net_server.h"
#include "stream_protocol.h"

#if 0 //def CONFIG_USR_VIDEO_ENABLE

struct user_video_hdl {
    u8 state;
    u8 page_turning;
    u8 req_send;
    u8 channel;
    u8 *user_isc_buf;
    u8 *user_video_buf;
    u8 *user_audio_buf;
    struct server *user_video_rec;
};

#ifdef CONFIG_VIDEO_720P
#define USER_ISC_SBUF_SIZE     		(CONFIG_VIDEO_IMAGE_W*64*3/2)
#define USER_VIDEO_SBUF_SIZE   		(200 * 1024)
#define USER_AUDIO_SBUF_SIZE   		(32 * 1024)
#define CAP_IMG_SIZE 				(150*1024)
#define CONFIG_USER_VIDEO_WIDTH		1280
#define CONFIG_USER_VIDEO_HEIGHT	720
#else
#define USER_ISC_SBUF_SIZE     		(CONFIG_VIDEO_IMAGE_W*32*3/2)
#define USER_VIDEO_SBUF_SIZE   		(80 * 1024)
#define USER_AUDIO_SBUF_SIZE   		(32 * 1024)
#define CAP_IMG_SIZE 				(40*1024)
#define CONFIG_USER_VIDEO_WIDTH		640
#define CONFIG_USER_VIDEO_HEIGHT	480
#endif

#define USER_ISC_STATIC_BUFF_ENABLE	0	//1使用静态内部ram
#define USER_AUDIO_ENABLE			0	//1开启音频需要添加video_rt_usr.c，音视频回调也会回调到video_rt_usr.c

#if USER_ISC_STATIC_BUFF_ENABLE
extern u8 net_isc_sbuf[USER_ISC_SBUF_SIZE];
static u8 *user_isc_sbuf = net_isc_sbuf;
/*static u8 user_isc_sbuf[USER_ISC_SBUF_SIZE] SEC(.sram) ALIGNE(32);//必须32对齐*/
#endif

static const unsigned char user_osd_format_buf[] = "yyyy-nn-dd hh:mm:ss";
extern int uvc_host_online(void);

int user_video_audio_packet_channel0_callback(u8 type, u8 *buf, u32 size)
{
    if (type == VIDEO_REC_JPEG_TYPE_VIDEO) {
        putchar('V');
    } else {
        putchar('A');
    }
    return 0;
}
int user_video_audio_packet_channel1_callback(u8 type, u8 *buf, u32 size)
{
    if (type == VIDEO_REC_JPEG_TYPE_VIDEO) {
        putchar('V');
    } else {
        putchar('A');
    }
    return 0;
}
/*码率控制，根据具体分辨率设置*/
int user_video_rec_get_abr(u32 width)
{
    if (width <= 640) {
        return 2800;
    } else {
        return 4000;
    }
}
void *user_video_rec0_open(char channel)//channel:0/1，如果开启录像则channel是1
{
    int err = 0;
    union video_req req = {0};
    char path[48];

    printf("\n [ERROR] %s -[yuyu] %d\n", __FUNCTION__, __LINE__);
    struct user_video_hdl *user = zalloc(sizeof(struct user_video_hdl));
    if (!user) {
        printf("user open err \n");
        return 0;
    }
    if (!user->user_video_buf) {
        user->user_video_buf = malloc(USER_VIDEO_SBUF_SIZE);
        if (!user->user_video_buf) {
            puts("no mem \n");
            free(user);
            return NULL;
        }
    }
#if USER_AUDIO_ENABLE
    if (!user->user_audio_buf) {
        user->user_audio_buf = malloc(USER_AUDIO_SBUF_SIZE);
        if (!user->user_audio_buf) {
            puts("no mem \n");
            free(user);
            return NULL;
        }
    }
#endif
#if USER_ISC_STATIC_BUFF_ENABLE
    user->user_isc_buf = user_isc_sbuf;
#else
    if (!user->user_isc_buf) {
        user->user_isc_buf = malloc(USER_ISC_SBUF_SIZE);
        if (!user->user_isc_buf) {
            puts("no mem \n");
            goto __exit;
        }
    }
#endif

    if (!user->user_video_rec) {
        char dev_name[32];
        user->channel = channel;
#ifdef CONFIG_UVC_VIDEO2_ENABLE
        sprintf(dev_name, "video2.%d", user->channel);
        user->user_video_rec = server_open("video_server", dev_name);
#else
        sprintf(dev_name, "video0.%d", user->channel);
        user->user_video_rec = server_open("video_server", dev_name);
#endif
        if (!user->user_video_rec) {
            goto __exit;
        }
    }
#ifdef CONFIG_UVC_VIDEO2_ENABLE
    req.rec.uvc_id = uvc_host_online();
#endif
#ifdef CONFIG_VIDEO_REC_PPBUF_MODE
    req.rec.bfmode = VIDEO_PPBUF_MODE;
#endif
#ifdef  CONFIG_VIDEO_SPEC_DOUBLE_REC_MODE
    req.rec.wl80_spec_mode = VIDEO_WL80_SPEC_DOUBLE_REC_MODE;
#endif
    req.rec.picture_mode = 0;//非绘本模式
    req.rec.isc_sbuf = user->user_isc_buf;
    req.rec.sbuf_size = USER_ISC_SBUF_SIZE;
    req.rec.camera_type = VIDEO_CAMERA_NORMAL;
    req.rec.channel = user->channel;
    req.rec.width 	= CONFIG_USER_VIDEO_WIDTH;
    req.rec.height 	= CONFIG_USER_VIDEO_HEIGHT;
    req.rec.state 	= VIDEO_STATE_START;
    req.rec.fpath 	= CONFIG_REC_PATH_0;
#if USER_AUDIO_ENABLE
    req.rec.format 	= NET_VIDEO_FMT_AVI;//支持音视频
#else
    req.rec.format 	= USER_VIDEO_FMT_AVI;//USER_VIDEO_FMT_AVI模式仅支持视频
#endif
    req.rec.quality = VIDEO_LOW_Q;//VIDEO_MID_Q;
    req.rec.fps = 0;
    req.rec.real_fps = 25;//帧率

    //需要音频：请写audio.sample_rate和audio.buf、audio.buf_len
#if USER_AUDIO_ENABLE
    req.rec.audio.sample_rate = 8000;// 音频采样率
#else
    req.rec.audio.sample_rate = 0;//8000 音频采样率
#endif
    req.rec.audio.channel = 1;
    req.rec.audio.channel_bit_map = 0;
    req.rec.audio.volume    = 64;//音频增益0-100
    req.rec.audio.buf = user->user_audio_buf;//音频BUFF
    req.rec.audio.buf_len = USER_AUDIO_SBUF_SIZE;//音频BUFF长度

    req.rec.abr_kbps = user_video_rec_get_abr(req.rec.width);//JPEG图片码率
    req.rec.buf = user->user_video_buf;
    req.rec.buf_len = USER_VIDEO_SBUF_SIZE;
    req.rec.block_done_cb = 0;//user_yuv420_block_scan;//设置YUV420中断回调函数

#ifdef CONFIG_OSD_ENABLE
    struct video_text_osd text_osd = {0};
    text_osd.font_w = OSD_DEFAULT_WIDTH;//必须16对齐
    text_osd.font_h = OSD_DEFAULT_HEIGHT;//必须16对齐
    text_osd.text_format = user_osd_format_buf;
    text_osd.x = (req.rec.width - text_osd.font_w * strlen(text_osd.text_format) + 15) / 16 * 16;
    text_osd.y = (req.rec.height - text_osd.font_h + 15) / 16 * 16;
    text_osd.osd_yuv = 0xe20095;
    req.rec.text_osd = &text_osd;
#endif

    sprintf(path, "usr://%s", "user_path");
    strcpy(req.rec.net_par.netpath, path);

    err = server_request(user->user_video_rec, VIDEO_REQ_REC, &req);
    if (err != 0) {
        puts("user start rec err\n");
        goto __exit;
    }
    req.rec.packet_cb = user->channel ? user_video_audio_packet_channel1_callback : user_video_audio_packet_channel0_callback;//注册数据包回调函数进行协议转发
    err = server_request(user->user_video_rec, VIDEO_REQ_SET_PACKET_CALLBACK, &req);
    if (err != 0) {
        puts("stream_packet_cb set err\n");
        goto __exit;
    }
    user->state = true;
    printf("user video rec open ok\n");

    return user;

__exit:
    if (user->user_video_rec) {
        memset(&req, 0, sizeof(req));
        req.rec.channel = user->channel;
        req.rec.state = VIDEO_STATE_STOP;
        server_request(user->user_video_rec, VIDEO_REQ_REC, &req);
        server_close(user->user_video_rec);
        user->user_video_rec = NULL;
    }
    if (user->user_video_buf) {
        free(user->user_video_buf);
        user->user_video_buf = NULL;
    }
#if !USER_ISC_STATIC_BUFF_ENABLE
    if (user->user_isc_buf) {
        free(user->user_isc_buf);
        user->user_isc_buf = NULL;
    }
#endif
    free(user);
    return NULL;
}

int user_video_rec0_close(void *p_user)
{
    int err = 0;
    struct user_video_hdl *user = p_user;
    union video_req req = {0};
    if (!user) {
        return -EINVAL;
    }
    if (user->user_video_rec) {
        req.rec.channel = user->channel;
        req.rec.state = VIDEO_STATE_STOP;
        err = server_request(user->user_video_rec, VIDEO_REQ_REC, &req);
        if (err != 0) {
            printf("\nstop rec err 0x%x\n", err);
            return -EINVAL;
        }
        server_close(user->user_video_rec);
        user->user_video_rec = NULL;
        user->state = false;
        printf("user video rec close ok\n");
    }
    if (user->user_video_buf) {
        free(user->user_video_buf);
        user->user_video_buf = NULL;
    }
    if (user->user_audio_buf) {
        free(user->user_audio_buf);
        user->user_audio_buf = NULL;
    }
#if !USER_ISC_STATIC_BUFF_ENABLE
    if (user->user_isc_buf) {
        free(user->user_isc_buf);
        user->user_isc_buf = NULL;
    }
#endif
    free(user);
    return 0;
}
#if 0
static void user_video_test1(void)
{
    void *user_video0, *user_video1;
    int cnt = 0;
#ifdef CONFIG_UVC_VIDEO2_ENABLE
    while (uvc_host_online() < 0) {
        os_time_dly(100);
    }
#endif
    printf("~~~~~~~~~~~~~~~~~~~~~~0~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    while (1) {
        user_video0 = user_video_rec0_open(0);
        user_video1 = user_video_rec0_open(1);
        cnt = 10;
        while (--cnt) {
            os_time_dly(100);
            puts("close 0");
            user_video_rec0_close(user_video1);
            puts("open 0");
            user_video1 = user_video_rec0_open(1);
        }
        puts("colse 0");
        user_video_rec0_close(user_video0);
        puts("colse 1");
        user_video_rec0_close(user_video1);
    }
}
static void user_video_init(void)
{
    thread_fork("user_video", 10, 1500, 64, 0, user_video_test1, NULL);
}
late_initcall(user_video_init);
#endif
#endif

