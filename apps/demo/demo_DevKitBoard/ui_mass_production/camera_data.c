#include "app_config.h"
#ifdef UI_MASS_PRODUCTION

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
#include "wifi_raw_stream.h"
#include "system/includes.h"
#include "wifi/wifi_connect.h"
#include "event/net_event.h"
#include "net/assign_macaddr.h"
#include "uart.h"

/************视频部分*将摄像头数据显示到lcd上**********************************/
/* 开关打印提示 */
#if 1
#define log_info(x, ...)    printf("\n[production_msg]>>>>>>>>>>>>>>>>>>>###" x " \n", ## __VA_ARGS__)
#else
#define log_info(...)
#endif

#define SHOW_LCD_W  LCD_W/2
#define SHOW_LCD_H  LCD_H/3
#define CAMERA_DATA_LEN  LCD_RGB565_DATA_SIZE/6

static u8 recv_data[CAMERA_DATA_LEN];
static u8 rgb_data[LCD_RGB565_DATA_SIZE];

#define JPG_MAX_SIZE 100*1024
static u8 recBuf0[JPG_MAX_SIZE] = {0};
static u8 recBuf1[JPG_MAX_SIZE] = {0};
static u8 recBuf2[JPG_MAX_SIZE] = {0};
static u8 recBuf3[JPG_MAX_SIZE] = {0};
static u8 recBuf4[JPG_MAX_SIZE] = {0};
static u8 recBuf5[JPG_MAX_SIZE] = {0};
static struct __JPG_INFO jpg_info[6] = {
    {
        .buf_len = sizeof(recBuf0),
        .buf = recBuf0,
    },
    {
        .buf_len = sizeof(recBuf1),
        .buf = recBuf1,
    },
    {
        .buf_len = sizeof(recBuf2),
        .buf = recBuf2,
    },
    {
        .buf_len = sizeof(recBuf3),
        .buf = recBuf3,
    },
    {
        .buf_len = sizeof(recBuf4),
        .buf = recBuf4,
    },
    {
        .buf_len = sizeof(recBuf5),
        .buf = recBuf5,
    },
};
static u8 show_mub = 0;
static u8 cbuf_lcd_show[1024];
static u8 start_check = 0;
static u8 check_reset = 0;
static u8 raw_mac[6][6] = {0};
static u8 online[6] = {0};
static u8 show_mode = 0;
static u8 one_show_mub = 0;
static u8 recv_stop = 0;
static u8 uart_mode = 0;
static u8 change_mode_flog = 0;


static cbuffer_t data_cbuf;
static OS_MUTEX mutex1;

#define MAX_PAYLOAD (500-sizeof(struct frm_head))//最后4字节属于h264流的

extern void user_send_no_data_ready(int x, int y, int w, int h, char *img);
extern void user_send_no_data_ready1(int x, int y, int w, int h, char *img);
static void camera_show_lcd(u8 *buf, u32 size, int width, int height);
extern void cbuf_clear(cbuffer_t *cbuffer);


extern struct video_config_hd video_hd;


#define WIFI_TX_MAX_LEN 2510 //一次最大发送字节数

static int fps_cnt = 0;

#define JPEG_DATA_TYPE 2


static u8 camera_x_y[6][4] = {
    {SHOW_LCD_W, SHOW_LCD_H * 2, SHOW_LCD_W, SHOW_LCD_H},
    {         0, SHOW_LCD_H * 2, SHOW_LCD_W, SHOW_LCD_H},
    {SHOW_LCD_W,   SHOW_LCD_H, SHOW_LCD_W, SHOW_LCD_H},
    {         0,   SHOW_LCD_H, SHOW_LCD_W, SHOW_LCD_H},
    {SHOW_LCD_W,            0, SHOW_LCD_W, SHOW_LCD_H},
    {         0,            0, SHOW_LCD_W, SHOW_LCD_H}
};

static void Calculation_frame2(void)
{
    static u32 tstart = 0, tdiff = 0;
    static u8 fps_cnt = 0;

    fps_cnt++ ;

    if (!tstart) {
        tstart = timer_get_ms();
    } else {
        tdiff = timer_get_ms() - tstart;

        if (tdiff >= 1000) {
            printf("\n [MSG_out2]fps_count = %d\n", fps_cnt *  1000 / tdiff);
            tstart = 0;
            fps_cnt = 0;
        }
    }
}

static void Calculation_frame1(void)
{
    static u32 tstart = 0, tdiff = 0;
    static u8 fps_cnt = 0;

    fps_cnt++ ;

    if (!tstart) {
        tstart = timer_get_ms();
    } else {
        tdiff = timer_get_ms() - tstart;

        if (tdiff >= 1000) {
            printf("\n [MSG_out1]fps_count = %d\n", fps_cnt *  1000 / tdiff);
            tstart = 0;
            fps_cnt = 0;
        }
    }
}

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

static int wifi_raw_send(u8 *dev_mac, u8 *buf, int len)
{
    os_mutex_pend(&mutex1, 0);

    if (len > WIFI_TX_MAX_LEN) {
        return -1;
    }

    memcpy(wifi_get_payload_ptr() - 20, dev_mac, 6);

    u8 *pos = wifi_get_payload_ptr();

    memcpy(pos, buf, len);

    wifi_send_data(len, WIFI_TXRATE_54M);

    os_mutex_post(&mutex1);
    return len;
}

u8 control_change_video_show(u8 dev_munb)
{
    if (raw_mac[dev_munb] != 0) {
        struct frm_head ping_head = {0};
        ping_head.type |= CMD_CHANGE_VIDEO;
        check_reset = 0;
        wifi_raw_send(raw_mac[dev_munb], (u8 *)&ping_head, sizeof(struct frm_head));
    }
}

//接收视频数据
static int get_rgb_packet(char *buf, int len, struct __JPG_INFO *info)
{
    struct frm_head  *head_info = (struct frm_head *)(buf);

    switch (head_info->type & 0x7F) {
    case JPEG_VIDEO_TYPE:
        if (head_info->offset + head_info->payload_size > JPG_MAX_SIZE) {
            printf("\n%s %d->large data buf too small\n", __func__, __LINE__);
            goto ERROR;
        }

        if (head_info->seq != info->jpeg_old_frame_seq) { //new pkg start
            info->jpeg_old_frame_seq = head_info->seq;
            info->jpeg_total_payload_len = 0;
            memset(info->buf, CHECK_CODE, JPG_MAX_SIZE);
        }

        if (bytecmp((unsigned char *)info->buf + head_info->offset, CHECK_CODE, CHECK_CODE_NUM) != 0) {
            break;
        }

        memcpy(info->buf + head_info->offset, (buf) + sizeof(struct frm_head), head_info->payload_size);//对应数据放置到对应的位置
        info->jpeg_total_payload_len += head_info->payload_size;//累加总长度

        if (info->jpeg_total_payload_len == head_info->frm_sz) { //如果数据量相等,说明帧数据接收完成
            info->buf_len = info->jpeg_total_payload_len;
            return JPEG_DATA_TYPE;
        }
        break;
    }

    return 0;
ERROR:

    return -1;
}

extern void jpeg2yuv_lbuf_clear(void);

void reset_jpg2yuv(void)
{
    jpeg2yuv_close();
    os_time_dly(1);
    jpeg2yuv_yuv_callback_register(camera_show_lcd);
    os_time_dly(1);
    jpeg2yuv_open();
}

static void wifi_rx_cb(void *rxwi, struct ieee80211_frame *wh, void *data, u32 len, struct netif *netif)
{
    int ret;
    u8 find_same_mac_flog = 1;
    u8 no_enough_show_space = 0;
    static u8 clear_lbuf_flog = 0;

    static const u8 pkg_head_fill_magic[] = {
        /*dst*/ 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
    };

    if (len < 25 || memcmp(&((u8 *)data)[28], pkg_head_fill_magic, 6)) {
        return;
    }

    if (show_mode == 1) {
        if (clear_lbuf_flog) {
            clear_lbuf_flog = 0;
            jpeg2yuv_close();
            jpeg2yuv_open();
            jpeg2yuv_yuv_callback_register(camera_show_lcd);
            cbuf_clear(&data_cbuf);
        }

        if (memcmp(&((u8 *)data)[28 + 6], raw_mac[one_show_mub], 6)) {
            return;
        }
    } else {
        clear_lbuf_flog = 1;
    }

    if (uart_mode == 1) {
        for (u8 j = 0; j < 6; j++) { //查询是否有一样的mac地址
            if (memcmp(&((u8 *)data)[28 + 6], raw_mac[j], 6) == 0) {
                show_mub = j;
                find_same_mac_flog = 1;
                break;
            } else {
                find_same_mac_flog = 0;
            }
        }
        if (find_same_mac_flog == 0) {
            return;
        }
    } else {
        for (u8 j = 0; j < 6; j++) { //查询是否有一样的mac地址
            if (memcmp(&((u8 *)data)[28 + 6], raw_mac[j], 6) == 0) {
                find_same_mac_flog = 1;
                show_mub = j;
                break;
            } else {
                find_same_mac_flog = 0;
            }
        }

        if (find_same_mac_flog == 0) { //如果是新的mac地址缓存起来
            for (u8 i = 0; i < 6; i++) { //check mac
                if (raw_mac[i][0] == 0) {
                    show_mub = i;
                    no_enough_show_space = 1; //标志有位置
                    memcpy(raw_mac[i], &((u8 *)data)[28 + 6], 6);
                    break;
                }
            }
            if (no_enough_show_space  == 0) {
                return ;
            }
        }
    }

    online[show_mub] = 1;


    ret = get_rgb_packet(&((u8 *)data)[48], len - 24, &jpg_info[show_mub]);
    if (JPEG_DATA_TYPE == ret) {
        Calculation_frame();
        if (show_mode == 0) {
            cbuf_write(&data_cbuf, &show_mub, 1);
        }
        jpeg2yuv_jpeg_frame_write(jpg_info[show_mub].buf, jpg_info[show_mub].buf_len);
        fps_cnt++;
    }
}

static void camera_show_lcd(u8 *buf, u32 size, int width, int height)
{
    u8 mub;

    if (show_mode == 1) {
        if (width == 640) {
            set_turn_90_mode(2);
            lcd_show_frame(buf, size, width, height);
        } else {
            set_turn_90_mode(0);
            YUV420p_Soft_Scaling(buf, rgb_data, width, height, LCD_H, LCD_W);
            lcd_show_frame(buf, size, width, height);
        }
        Calculation_frame1();
    } else {
        YUV420p_Soft_Scaling(buf, NULL, width, height, SHOW_LCD_W, SHOW_LCD_H);
        yuv420p_to_rgb565(buf, recv_data, SHOW_LCD_W, SHOW_LCD_H, 1);
        cbuf_read(&data_cbuf, &mub, 1);
        Calculation_frame2();

        if (start_check == 1) {
            start_check = 0;
            if (change_mode_flog == 0) {
                memset(recv_data, 0x55, CAMERA_DATA_LEN);
                for (u8 i = 0; i < 6; i++) { //check mac
                    if (online[i] == 0) { //检查到设备不在线
                        memset(raw_mac[i], 0, 6);
                        user_send_no_data_ready(camera_x_y[i][0], camera_x_y[i][1], camera_x_y[i][2], camera_x_y[i][3], recv_data);
                        os_time_dly(2);
                    } else {
                        online[i] = 0;
                    }
                }
            }
        } else {
            user_send_data_ready(camera_x_y[mub][0], camera_x_y[mub][1], camera_x_y[mub][2], camera_x_y[mub][3], recv_data);
        }
    }

}

static void check_dev_online(void *priv)
{
    check_reset++;
    if (check_reset > 10) {
        check_reset = 0;
        if (start_check == 1) {
            memset(recv_data, 0x55, CAMERA_DATA_LEN);
            printf(">>>>clear all");
            for (u8 i = 0; i < 6; i++) { // no data came
                memset(raw_mac[i], 0, 6);
                user_send_no_data_ready1(camera_x_y[i][0], camera_x_y[i][1], camera_x_y[i][2], camera_x_y[i][3], recv_data);
                os_time_dly(2);
            }
        }
        start_check = 1;
    }
}

extern void clear_te_lbuf(void);
static void raw_mode_task(void *priv)
{
    int msg[32];
    static u8 hold_flog = 0;
    while (1) {
        os_taskq_pend("taskq", msg, ARRAY_SIZE(msg));
        check_reset = 0;
        switch (msg[1]) {
        case TOUCH_MORE_WINDOWS_UP:
            show_mode = 0;
            if (!hold_flog) {
                printf(">>>KEY_OK window_x = %d.\n", msg[2]);
                change_mode_flog = 1;
                control_change_video_show(msg[2]);
                os_time_dly(100);
                change_mode_flog = 0;
            } else {
                hold_flog = 0;
            }
            break;

        case TOUCH_MORE_WINDOWS_HOLD:
            recv_stop = 1;
            one_show_mub = msg[2];
            show_mode = 1;
            hold_flog = 1;
            set_lcd_show_data_mode(CAMERA);
            set_compose_mode(0, 0, 0, 0);
            os_time_dly(20);
            recv_stop = 0;
            break;

        case TOUCH_ONE_WINDOWS_UP:
            recv_stop = 1;
            show_mode = 0;
            os_time_dly(20);
            recv_stop = 0;
            set_lcd_show_data_mode(UI_RGB);
            break;

        case KEY_CMD_CHANGE_VIDEO:
            for (u8 i = 0; i < 6; i++) {
                control_change_video_show(i);
                os_time_dly(10);
            }
            break;
        }

    }
}

#define UART_MAX_RECV_SIZE   1024 //最大接收个数
static u8 uart_buf[UART_MAX_RECV_SIZE] __attribute__((aligned(32)));

static void raw_uart_task(void *priv)
{
    u8 same_mac_flog = 0;
    int len = sizeof(uart_buf);
    static u8 clear_mac_addr = 0;
    static u8 time = 0;

    void *hdl = dev_open("uart1", NULL);

    if (!hdl) {
        printf("open uart err !!!\n");
        return ;
    }

    u8 *recv_buf = zalloc(UART_MAX_RECV_SIZE);
    if (!recv_buf) {
        printf("malloc uart err !!!\n");
        return ;
    }
    dev_ioctl(hdl, UART_SET_CIRCULAR_BUFF_ADDR, (int)uart_buf);
    dev_ioctl(hdl, UART_SET_CIRCULAR_BUFF_LENTH, len);
    dev_ioctl(hdl, UART_SET_RECV_BLOCK, 1);
    u32 parm = 1000;//发送函数的超时时间要比接收函数超时时间大
    dev_ioctl(hdl, UART_SET_RECV_TIMEOUT, (u32)parm);
    dev_ioctl(hdl, UART_START, 0);

    printf("---> uart_send_test_task running \n");

    while (1) {
        len = dev_read(hdl, recv_buf, UART_MAX_RECV_SIZE);
        if (len <= 0) {
            dev_ioctl(hdl, UART_FLUSH, 0);
        } else {
            printf(">>>>>>>>>uart recv data");
            u16 crc = CRC16(recv_buf, 6);
            put_buf(recv_buf, 8);
            if ((u8)(crc >> 8) == recv_buf[6] && (u8)crc == recv_buf[7]) {
                printf(">>>>>>>>>uart recv ok_data");
                u16 crc = CRC16(recv_buf, 6);
                put_buf(recv_buf, 8);
                check_reset = 0;
                uart_mode = 1;
                if (clear_mac_addr == 0) {
                    clear_mac_addr = 1;
                    for (u8 j = 0; j < 6; j++) { //查询是否有一样的mac地址
                        memset(raw_mac[j], 0, 6);
                    }
                }
                for (u8 j = 0; j < 6; j++) { //查询是否有一样的mac地址
                    if (memcmp(raw_mac[j], recv_buf, 6) == 0) {
                        same_mac_flog = 1;
                        printf(">>>>send ch = %d", video_hd.ch);
                        recv_buf[8] = video_hd.ch;//通过串口告诉从机通信的通道号
                        time = 0;
                        online[j] = 1;
                        dev_write(hdl, &recv_buf[6], 3);
                        printf(">>>>>>uart find same_mac_addr");
                        break;
                    } else {
                        printf(">>>>>>uart find mac_addr");
                        same_mac_flog = 0;
                    }
                }
                if (same_mac_flog == 0) { //如果是新的mac地址缓存起来   //将收到的数据 尾部CRC转发出 加通道号
                    for (u8 i = 0; i < 6; i++) { //check mac
                        if (raw_mac[i][0] == 0) {
                            memcpy(raw_mac[i], recv_buf, 6);
                            printf(">>>>send ch = %d", video_hd.ch);
                            recv_buf[8] = video_hd.ch;//通过串口告诉从机通信的通道号
                            time = 0;
                            /*wifi_set_channel(video_hd.ch);//动态配置wifi信道*/
                            dev_write(hdl, &recv_buf[6], 3);
                            online[i] = 1;
                            put_buf(raw_mac[i], 6);
                            printf(">>>>>>>>>ok  i = %d", i);
                            break;
                        }
                    }
                }
            }
        }
        os_time_dly(10);
    }
}
int wifi_raw_init(void)
{
    int ret = 0;
    printf(">>>>>>>wifi_raw_init<<<<<<<<<<");
    wifi_raw_on(0);
    wifi_set_rts_threshold(0xffff);//配置即使长包也不发送RTS
    wifi_set_channel(CONFIG_DEFAULT_CH);//配置WIFI RF 通信信道
    wifi_set_long_retry(10);
    wifi_set_short_retry(30);
    wifi_set_frame_cb(wifi_rx_cb, NULL); //注册接收802.11数据帧回调
    os_mutex_create(&mutex1);

    cbuf_init(&data_cbuf, cbuf_lcd_show, sizeof(cbuf_lcd_show));

    sys_timer_add_to_task("test_check", NULL, check_dev_online, 300);

    ret = jpeg2yuv_open();

    if (ret) {
        printf("err in jpeg2yuv_open \n");
    }

    thread_fork("raw_mode_task", 5, 1024, 32, 0, raw_mode_task, NULL);
    thread_fork("raw_uart_task", 5, 1024, 32, 0, raw_uart_task, NULL);

    jpeg2yuv_yuv_callback_register(camera_show_lcd);
}

#endif

