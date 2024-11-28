#include "app_config.h"
#include "system/includes.h"
#include "device/device.h"
#include "lcd_drive.h"
#include "os/os_api.h"
#include "asm/includes.h"
#include "system/timer.h"
#include "sys_common.h"
#include "system/includes.h"
#include "simple_avi_unpkg.h"
#include "server/server_core.h"
#include "server/audio_server.h"
#include "server/video_dec_server.h"
#include "lcd_config.h"
#include "yuv_soft_scalling.h"
#include "storage_device.h"


#if 1
#define     log_info(x, ...)     printf("[AVI_PLAY][INFO] " x " ", ## __VA_ARGS__)
#define     log_err(x, ...)      printf("[AVI_PLAY][ERR] " x " ", ## __VA_ARGS__)
#else
#define     log_info(...)
#define     log_err(...)
#endif

#define	TIMER_BASE	(2)
#define AVI_OFFSET_THR	(50)

enum {
    MSG_VIDS_UPDATE,
    MSG_VIDS_EXIT,
};


enum replay_speed_type {
    REPLAY_SPEED_1X = 1,
    REPLAY_SPEED_2X,
    REPLAY_SPEED_3X,
    REPLAY_SPEED_4X,
    REPLAY_SPEED_0_5X,
    REPLAY_SPEED_0_25X,
};


enum replay_status_type {
    RE_ST_NULL = 0,
    RE_ST_START,
    RE_ST_EXIT,
    RE_ST_PAUSE,
};

struct avi_hdl {
    FILE *fd;
    int play_time;
    char *jpg_buf;
    char *yuv_buf;
    char *scale_buf;
    char *rgb_buf;
    u8 vids_drop;
    u8 vids_exit;
    u8 vids_busy;
    u16 vids_inr;
    int vids_idx;
    int vids_total;
    u8 auds_vol;
    u8 auds_drop;
    int auds_idx;
    u8 auds_exit;
    u16 auds_inr;
    int auds_rate;
    int auds_total;
    char *auds_data;
    char *auds_frame;
    cbuffer_t auds_cbuf;
    u8 dec_status;
    struct server *dec_server;
    int sync_timer;
    OS_MUTEX mutex;
    OS_SEM auds_rsem;
    enum replay_speed_type speed;
    enum replay_status_type status;
};


static struct avi_hdl hdl = {0};
#define	__this	(&hdl)

#define	AUDS_CBUF_LEN	(10 * 1024)
#define	AUDS_FRAME_LEN  (1 * 1024)

#define DEC_JPG_LEN     100*1024
#define IDX_00DC   	ntohl(0x30306463)
#define IDX_01WB   	ntohl(0x30317762)
#define IDX_00WB   	ntohl(0x30307762)


static void video_decode(char *buf, int buf_len)
{
    int pix;
    int ytype;
    int yuv_len;
    char *cy, *cb, *cr;
    struct jpeg_decode_req req = {0};
    struct jpeg_image_info info = {0};

    info.input.data.buf = buf;
    info.input.data.len = buf_len;

    if (jpeg_decode_image_info(&info)) {
        log_err("jpeg_decode_image_info err\n");
        return;
    }

    switch (info.sample_fmt) {
    case JPG_SAMP_FMT_YUV444:
        ytype = 1;
        break;//444

    case JPG_SAMP_FMT_YUV420:
        ytype = 4;
        break;//420

    default:
        ytype = 2;
        break;
    }

    pix = info.width * info.height;
    yuv_len = pix + pix / ytype * 2;
    //log_info("ytype = %d, width = %d , height = %d\n", ytype, info.width, info.height);

    if (!__this->yuv_buf) {
        if (!(__this->yuv_buf = malloc(yuv_len))) {
            log_err("yuv_buf malloc error, size = %d\n", yuv_len);
            return;
        }
    }

    cy = __this->yuv_buf;
    cb = cy + pix;
    cr = cb + pix / ytype;

    req.input_type     = JPEG_INPUT_TYPE_DATA;
    req.input.data.buf = info.input.data.buf;
    req.input.data.len = info.input.data.len;
    req.buf_y 		   = cy;
    req.buf_u 		   = cb;
    req.buf_v 		   = cr;
    req.buf_width      = info.width;
    req.buf_height     = info.height;
    req.out_width      = info.width;
    req.out_height     = info.height;
    req.output_type    = JPEG_DECODE_TYPE_DEFAULT;
    req.bits_mode      = BITS_MODE_UNCACHE;
    req.dec_query_mode = TRUE;

    if (jpeg_decode_one_image(&req)) {
        log_err("jpeg decode error\n");
        return;
    }

#if 0
    if (!__this->scale_buf) {
        if (!(__this->scale_buf = malloc(LCD_H * LCD_W * (1 + 1 / ytype * 2)))) {
            log_err("scale_buf malloc error, size = %d\n", LCD_H * LCD_W * (1 + 1 / ytype * 2));
            return;
        }
    }

    YUV420p_Soft_Scaling(__this->yuv_buf, __this->scale_buf, info.width, info.height, LCD_W, LCD_H);
    lcd_show_frame(__this->scale_buf, info.width * info.height * 3 / 2, info.width, info.height);
#else
    lcd_show_frame(__this->yuv_buf, info.width * info.height * 3 / 2, info.width, info.height);
#endif
}


static int audio_vfs_fread(void *priv, void *data, u32 len)
{
    int rlen = 0, ret;
    int read_len, recv_len, remain_len;

    if (__this->status == RE_ST_EXIT) {
        return 0;
    }

    os_sem_pend(&__this->auds_rsem, 0);
    //putchar(')');
    if (cbuf_get_data_size(&__this->auds_cbuf)) {
        rlen = cbuf_read(&__this->auds_cbuf, data, len);
    }
    return len;
}


static const struct audio_vfs_ops audio_vfs_ops = {
    .fread  = audio_vfs_fread,
};


static int open_audio_dec(void)
{
    union audio_req req = {0};

    req.dec.output_buf      = NULL;
    req.dec.volume          = __this->auds_vol;
    req.dec.output_buf_len  = AUDS_CBUF_LEN;
    req.dec.channel         = 1;
    req.dec.sample_rate     = __this->auds_rate;
    req.dec.priority        = 1;
    req.dec.vfs_ops         = &audio_vfs_ops;
    req.dec.file            = (FILE *)__this;
    req.dec.dec_type 		= "pcm";
    req.dec.sample_source   = "dac";
    if (server_request(__this->dec_server, AUDIO_REQ_DEC, &req) != 0) {
        return -1;
    }

    req.dec.cmd = AUDIO_DEC_START;
    if (server_request(__this->dec_server, AUDIO_REQ_DEC, &req) != 0) {
        return -1;
    }

    return 0;
}


static int close_audio_dec(void)
{
    union audio_req req = {0};

    if (__this->dec_server) {
        req.dec.cmd = AUDIO_DEC_STOP;
        server_request(__this->dec_server, AUDIO_REQ_DEC, &req);
        server_close(__this->dec_server);
    }
    return 0;
}


static void audio_replay_task(void *priv)
{
    u8 empty = 0;
    int frame_len, msg[2];

    for (;;) {
        if (os_taskq_pend(NULL, msg, ARRAY_SIZE(msg)) != OS_TASKQ) {
            continue;
        }

        switch (msg[0]) {
        case MSG_VIDS_UPDATE:
            //putchar('}');
            break;

        case MSG_VIDS_EXIT:
            __this->auds_exit = 1;
            return;

        default:
            continue;
        }

        os_mutex_pend(&__this->mutex, 0);
        if ((frame_len = avi_audio_get_frame(__this->fd, msg[1], (u8 *)__this->auds_frame, AUDS_FRAME_LEN, 1)) <= 0) {
            log_err("get audio frame error\n");
            __this->vids_exit = 1;
            os_mutex_post(&__this->mutex);
            return;
        }
        os_mutex_post(&__this->mutex);

_audio_retry_:
        if (cbuf_write(&__this->auds_cbuf, __this->auds_frame, frame_len) > 0) {
            os_sem_set(&__this->auds_rsem, 0);
            os_sem_post(&__this->auds_rsem);
            //putchar('(');

            if (!__this->dec_status) {
                __this->dec_status = 1;
                open_audio_dec();
            }
        } else {
            os_time_dly(2);
            //putchar('R');
            goto _audio_retry_;
        }

        if (msg[1] == __this->auds_total) {
            __this->auds_exit = 1;
            return;
        }
    }
}


static void video_replay_task(void *priv)
{
    int frame_len, msg[2];

    for (;;) {
        if (os_taskq_pend(NULL, msg, ARRAY_SIZE(msg)) != OS_TASKQ) {
            continue;
        }

        switch (msg[0]) {
        case MSG_VIDS_UPDATE:
            //putchar('>');
            break;

        case MSG_VIDS_EXIT:
            __this->vids_exit = 1;
            return;

        default:
            continue;
        }

        os_mutex_pend(&__this->mutex, 0);
        if ((frame_len = avi_video_get_frame(__this->fd, msg[1], (u8 *)__this->jpg_buf, DEC_JPG_LEN, 1)) <= 0) {
            log_err("get video frame error\n");
            __this->vids_exit = 1;
            os_mutex_post(&__this->mutex);
            return;
        }
        os_mutex_post(&__this->mutex);

        if (*(u32 *)__this->jpg_buf == IDX_00DC || *(u32 *)__this->jpg_buf == IDX_01WB || *(u32 *)__this->jpg_buf == IDX_00WB) {
            frame_len -= 8;
            __this->jpg_buf += 8;
        }

        video_decode(__this->jpg_buf, frame_len);

        __this->vids_busy = 0;

        if (msg[1] == __this->vids_total) {
            __this->vids_exit = 1;
            return;
        }
    }
}


static void sync_timer_hdl(void *priv)
{
    int offset;
    static u32 time = 0, vids_time = 0, auds_time = 0, time_last = 0;

    if (__this->auds_exit && __this->vids_exit) {
        __this->status = RE_ST_EXIT;
        os_sem_set(&__this->auds_rsem, 0);
        os_sem_post(&__this->auds_rsem);

        close_audio_dec();
        free(__this->auds_data);
        free(__this->auds_frame);
        os_sem_del(&__this->auds_rsem, 0);

        free(__this->jpg_buf);
        free(__this->yuv_buf);
        if (__this->scale_buf) {
            free(__this->scale_buf);
        }

        avi_unpkg_exit(__this->fd, 1);
        fclose(__this->fd);

        sys_timer_del(__this->sync_timer);

        memset(__this, 0, sizeof(struct avi_hdl));
        log_info("STEP -> VIDEO COMPLETE\n");
        return;
    }

    switch (__this->status) {
    case RE_ST_NULL:
        return;
    case RE_ST_START:
        break;
    case RE_ST_EXIT:
        break;
    case RE_ST_PAUSE:
        return;
    default:
        return;
    }

    offset = (__this->auds_idx * __this->auds_inr) - (__this->vids_idx * __this->vids_inr);
    //if (offset >= AVI_OFFSET_THR) {
    //	log_info("vids_drop = %d, offset = %d\n", offset / __this->vids_inr, offset);
    //}

    if ((time - vids_time >= __this->vids_inr && !__this->vids_busy) || !time) {
        vids_time = time;
        if (__this->speed == REPLAY_SPEED_0_5X || __this->speed == REPLAY_SPEED_0_25X) {
            __this->vids_idx += __this->vids_drop;
            __this->vids_idx = (__this->vids_idx > __this->vids_total) ? __this->vids_total : __this->vids_idx;
            __this->vids_busy = 1;
            //putchar('<');
            if (os_taskq_post_type("video_replay_task", MSG_VIDS_UPDATE, 1, &__this->vids_idx) != OS_NO_ERR) {
                log_err("update vids error\n");
            }
        } else {
            if (offset) {
                __this->vids_idx += offset >= AVI_OFFSET_THR ? (offset / __this->vids_inr) : __this->vids_drop;
                __this->vids_idx = (__this->vids_idx > __this->vids_total) ? __this->vids_total : __this->vids_idx;
                __this->vids_busy = 1;
                //putchar('<');
                if (os_taskq_post_type("video_replay_task", MSG_VIDS_UPDATE, 1, &__this->vids_idx) != OS_NO_ERR) {
                    //log_err("update vids error\n");
                }
            }
        }
    }

    if ((time - auds_time >= __this->auds_inr) || !time) {
        auds_time = time;
        if (__this->speed == REPLAY_SPEED_0_5X || __this->speed == REPLAY_SPEED_0_25X) {
            if (offset < 0) {
                __this->auds_idx += fabs(offset) >= AVI_OFFSET_THR ? (fabs(offset) / __this->auds_inr) : __this->auds_drop;
                __this->auds_idx = (__this->auds_idx > __this->auds_total) ? __this->auds_total : __this->auds_idx;
                //putchar('{');
                if (os_taskq_post_type("audio_replay_task", MSG_VIDS_UPDATE, 1, &__this->auds_idx) != OS_NO_ERR) {
                    //log_err("update auds error\n");
                    __this->auds_idx -= __this->auds_drop;
                }
            } else {

            }
        } else {
            __this->auds_idx += __this->auds_drop;
            __this->auds_idx = (__this->auds_idx > __this->auds_total) ? __this->auds_total : __this->auds_idx;
            //putchar('{');
            if (os_taskq_post_type("audio_replay_task", MSG_VIDS_UPDATE, 1, &__this->auds_idx) != OS_NO_ERR) {
                //log_err("update auds error\n");
                __this->auds_idx -= __this->auds_drop;
            }
        }
    }

    time += TIMER_BASE;

    if (time - time_last >= 1000) {
        time_last = time;
        log_info("time = %d, vids = %d / %d, auds = %d / %d\n", time, __this->vids_idx, __this->vids_idx * __this->vids_inr, __this->auds_idx, __this->auds_idx * __this->auds_inr);
    }
}


char avi_replay_start(char *path)
{
    if (!path) {
        log_err("%s path error\n");
        return -1;
    }

    if (__this->status != RE_ST_NULL) {
        return -1;
    }

    if (!(__this->fd = fopen(path, "r"))) {
        log_err("%s open error\n", path);
        return -1;
    }

    if (avi_playback_unpkg_init(__this->fd, 1)) {
        log_err("avi_playback_unpkg_init error\n");
        return -1;
    }

    __this->jpg_buf = (char *)calloc(1, DEC_JPG_LEN);
    __this->auds_data = (char *)calloc(1, AUDS_CBUF_LEN);
    __this->auds_frame = (char *)calloc(1, AUDS_FRAME_LEN);
    ASSERT(__this->auds_data || __this->auds_frame || __this->jpg_buf);

    cbuf_init(&__this->auds_cbuf, __this->auds_data, AUDS_CBUF_LEN);

    __this->auds_drop = 1;
    __this->vids_drop = 1;
    __this->auds_vol = 50;
    __this->play_time = avi_get_file_time(__this->fd, 1);
    __this->auds_total = avi_get_audio_chunk_num(__this->fd, 1);
    __this->vids_total = avi_get_video_chunk_num(__this->fd, 1);
    __this->auds_rate = avi_get_audio_sample_rate(__this->fd, 1);
    __this->vids_inr = (__this->play_time * 1000) / __this->vids_total;
    __this->auds_inr = (__this->play_time * 1000) / __this->auds_total;

    __this->dec_server = server_open("audio_server", "dec");
    if (!__this->dec_server) {
        return -1;
    }

    log_info("total_time = %d, vids_num = %d, auds_num = %d, vids_inr = %d, auds_inr = %d, auds_rate = %d\n", __this->play_time, __this->vids_total, __this->auds_total, __this->vids_inr, __this->auds_inr, __this->auds_rate);

    os_mutex_create(&__this->mutex);
    os_sem_create(&__this->auds_rsem, 0);
    thread_fork("audio_replay_task", 11, 1024, 1024, 0, audio_replay_task, NULL);
    thread_fork("video_replay_task", 11, 1024, 256, 0, video_replay_task, NULL);

    __this->status = RE_ST_START;
    __this->sync_timer = sys_timer_add(NULL, sync_timer_hdl, TIMER_BASE);

    log_info("STEP -> START\n");
    return 0;
}


char avi_replay_exit(void)
{
    if (__this->status == RE_ST_NULL || __this->status == RE_ST_EXIT) {
        return -1;
    }
    if (os_taskq_post_type("audio_replay_task", MSG_VIDS_EXIT, 0, NULL) != OS_NO_ERR) {
        log_err("exit auds error\n");
        return -1;
    }

    if (os_taskq_post_type("video_replay_task", MSG_VIDS_EXIT, 0, NULL) != OS_NO_ERR) {
        log_err("exit vids error\n");
        return -1;
    }
    return 0;
}


char avi_replay_pause(u8 pause)
{
    if (__this->status == RE_ST_START || __this->status == RE_ST_PAUSE) {
        __this->status = pause ? RE_ST_PAUSE : RE_ST_START;
        return 0;
    }
    return -1;
}


char avi_replay_change_speed(enum replay_speed_type speed)
{
    char *speed_str[] = {
        "",
        "REPLAY_SPEED_1X",
        "REPLAY_SPEED_2X",
        "REPLAY_SPEED_3X",
        "REPLAY_SPEED_4X",
        "REPLAY_SPEED_0_5X",
        "REPLAY_SPEED_0_25X",
    };

    if (!(__this->status == RE_ST_START || __this->status == RE_ST_PAUSE)) {
        return -1;
    }

    switch (speed) {
    case REPLAY_SPEED_1X:
    case REPLAY_SPEED_2X:
    case REPLAY_SPEED_3X:
    case REPLAY_SPEED_4X:
        __this->auds_drop = speed;
        __this->auds_inr = (__this->play_time * 1000 * 1) / __this->auds_total;
        __this->vids_drop = 1;
        __this->vids_inr = (__this->play_time * 1000 * 1) / __this->vids_total;
        break;

    case REPLAY_SPEED_0_5X:
        __this->auds_drop = 1;
        __this->auds_inr = (__this->play_time * 1000 * 2) / __this->auds_total;
        __this->vids_drop = 1;
        __this->vids_inr = (__this->play_time * 1000 * 2) / __this->vids_total;
        break;

    case REPLAY_SPEED_0_25X:
        __this->auds_drop = 1;
        __this->auds_inr = (__this->play_time * 1000 * 4) / __this->auds_total;
        __this->vids_drop = 1;
        __this->vids_inr = (__this->play_time * 1000 * 4) / __this->vids_total;
        break;

    default:
        return -1;
    }
    __this->speed = speed;

    log_info("CHANTE_SPEED : %s, auds_inr = %d, auds_drop = %d\n", speed_str[speed], __this->auds_inr, __this->auds_drop);
    return 0;
}


char avi_get_litimg(char *path, int idx, u8 *data, int *len, int width, int height)
{
    FILE *fd;
    int pix;
    int ytype;
    int jpg_len, yuv_len;
    char *cy, *cb, *cr;
    struct jpeg_decode_req req = {0};
    struct jpeg_image_info info = {0};
    u8  *jpg_data, *yuv_data;

    if (!path || !data) {
        log_err("params error\n");
        return -1;
    }

    if (!(fd = fopen(path, "r"))) {
        log_err("%s open error\n", path);
        return -1;
    }

    if (avi_playback_unpkg_init(fd, 1)) {
        log_err("avi_playback_unpkg_init error\n");
        return -1;
    }

    if (!(jpg_data = malloc(DEC_JPG_LEN))) {
        log_err("jpg_data malloc error, size = %d\n", DEC_JPG_LEN);
        return -1;
    }

    jpg_len = avi_video_get_frame(fd, idx, jpg_data, DEC_JPG_LEN, 1);
    avi_unpkg_exit(fd, 1);
    fclose(fd);

    info.input.data.buf = jpg_data;
    info.input.data.len = jpg_len;
    if (jpeg_decode_image_info(&info)) {
        log_err("jpeg_decode_image_info err\n");
        return -1;
    }

    switch (info.sample_fmt) {
    case JPG_SAMP_FMT_YUV444:
        ytype = 1;
        break;//444

    case JPG_SAMP_FMT_YUV420:
        ytype = 4;
        break;//420

    default:
        ytype = 2;
        break;
    }

    pix = info.width * info.height;
    yuv_len = pix + pix / ytype * 2;

    if (!(yuv_data = malloc(yuv_len))) {
        log_err("yuv_data malloc error, size = %d\n", yuv_len);
        return -1;
    }

    cy = yuv_data;
    cb = cy + pix;
    cr = cb + pix / ytype;

    req.input_type     = JPEG_INPUT_TYPE_DATA;
    req.input.data.buf = info.input.data.buf;
    req.input.data.len = info.input.data.len;
    req.buf_y 		   = cy;
    req.buf_u 		   = cb;
    req.buf_v 		   = cr;
    req.buf_width      = info.width;
    req.buf_height     = info.height;
    req.out_width      = info.width;
    req.out_height     = info.height;
    req.output_type    = JPEG_DECODE_TYPE_DEFAULT;
    req.bits_mode      = BITS_MODE_UNCACHE;
    req.dec_query_mode = TRUE;

    if (jpeg_decode_one_image(&req)) {
        log_err("jpeg decode error\n");
        return -1;
    }

    YUV420p_Soft_Scaling(yuv_data, data, info.width, info.height, width, height);

    pix = width * height;
    *len = pix + pix / ytype * 2;
    printf("len = %d\n", *len);

    free(jpg_data);
    free(yuv_data);
    return 0;
}








#include "demo_config.h"
#ifdef AVI_REPLAY_DEMO

#include "event/device_event.h"
#include "event/key_event.h"

static void avi_litimg_show_task(void *priv)
{
    int len;
    FILE *fd;
    u8 *data = malloc(LCD_W * LCD_H * 2);

    avi_get_litimg("storage/sd0/C/SJ.avi", 1, data, &len, LCD_W, LCD_H);
    if (fd = fopen("storage/sd0/C/litimg.yuv", "w+")) {
        fwrite(data, len, 1, fd);
    }
    fclose(fd);
    lcd_show_frame(data, len, LCD_W, LCD_H);
    free(data);
}


static int key_event_handler(struct sys_event *event)
{
    enum replay_speed_type speed;
    static u32 k2_cnt = 0, k4_cnt = 0, k5_cnt = 0;
    struct key_event *key = (struct key_event *)event->payload;

    if (event->type == SYS_KEY_EVENT) {
        printf("key : value = %d, action = %d", key->value, key->action);
        if (key->action == KEY_EVENT_CLICK) {
            switch (key->value) {
            case 13://K1
                avi_replay_start("storage/sd0/C/SJ.avi");
                break;

            case 6://K2
                k2_cnt++;
                avi_replay_pause(k2_cnt % 2);
                break;

            case 4://K3
                avi_replay_exit();
                break;

            case 5://K4
                k4_cnt++;
                log_info("k4_cnt = %d, k4_cnt/4 = %d\n", k4_cnt, k4_cnt % 4);
                switch (k4_cnt % 4) {
                case 0:
                    speed = REPLAY_SPEED_1X;
                    break;
                case 1:
                    speed = REPLAY_SPEED_2X;
                    break;
                case 2:
                    speed = REPLAY_SPEED_3X;
                    break;
                case 3:
                    speed = REPLAY_SPEED_4X;
                    break;
                default:
                    speed = REPLAY_SPEED_1X;
                    break;
                }
                avi_replay_change_speed(speed);
                break;

            case 12://K5
                k5_cnt++;
                log_info("k5_cnt = %d, k5_cnt/3 = %d\n", k5_cnt, k5_cnt % 3);
                switch (k5_cnt % 3) {
                case 0:
                    speed = REPLAY_SPEED_1X;
                    break;
                case 1:
                    speed = REPLAY_SPEED_0_5X;
                    break;
                case 2:
                    speed = REPLAY_SPEED_0_25X;
                    break;
                default:
                    speed = REPLAY_SPEED_1X;
                    break;
                }
                avi_replay_change_speed(speed);
                break;

            case 10://K6
                thread_fork("avi_litimg_show_task", 6, 1024, 0, NULL, avi_litimg_show_task, NULL);
                break;

            default:
                break;
            }
        }
    }
}


static void avi_replay_test(void)
{
    user_ui_lcd_init();
    set_lcd_show_data_mode(CAMERA);
    register_sys_event_handler(SYS_KEY_EVENT, 0, 1, key_event_handler);

    while (!storage_device_ready()) {
        os_time_dly(10);
        putchar('S');
    }
}
late_initcall(avi_replay_test);
#endif


