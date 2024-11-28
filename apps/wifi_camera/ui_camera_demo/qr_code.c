#include "app_config.h"
#include "device/device.h"//u8
#include "server/video_dec_server.h"//fopen
#include "system/includes.h"//GPIO
#include "lcd_drive.h"
#include "sys_common.h"
#include "yuv_soft_scalling.h"
#include "asm/jpeg_codec.h"
#include "get_yuv_data.h"
#include "lcd_config.h"

#include "asm/jpeg_codec.h"
#include "qrcode/qrcode.h"
#include "json_c/json_tokener.h"
#include "mbedtls/base64.h"

#if CONFIG_LCD_QR_CODE_ENABLE
/************视频部分*将摄像头数据显示到lcd上**********************************/
/* 开关打印提示 */
#if 1
#define log_info(x, ...)    printf("\n[camera_to_lcd_test]>>>>>>>>>>>>>>>>>>>###" x " \n", ## __VA_ARGS__)
#else
#define log_info(...)
#endif


static struct qr_handle {
    void *decoder;
    u8 *buf;
    char frame_use_flog: 1;
    char ssid[33];
    char pwd[65];
} qr_hdl;

void qr_code_get_one_frame_YUV_420(u8 *buf, u16 buf_w, u16 buf_h)
{
    if (qr_hdl.frame_use_flog == 0) {
        if (qr_hdl.buf == NULL) {
            qr_hdl.buf = malloc(QR_FRAME_W * QR_FRAME_H * 3 / 2);
        }
        YUV420p_Soft_Scaling(buf, qr_hdl.buf, buf_w, buf_h, QR_FRAME_W, QR_FRAME_H);
        qr_hdl.frame_use_flog = 1;
    }
}

void qr_get_ssid_pwd(u32 ssid, u32 pwd)
{
    *((u32 *)ssid) = (u32)qr_hdl.ssid;
    *((u32 *)pwd) = (u32)qr_hdl.pwd ;
}

static void qr_code_start(void)
{
    memset(&qr_hdl, 0, sizeof(qr_hdl));
#if (CONFIG_LCD_QR_CODE_ENABLE == 1)
    qr_hdl.decoder = qrcode_init(QR_FRAME_W, QR_FRAME_H, QR_FRAME_W, QRCODE_MODE_NORMAL, SYM_QRCODE, 1, 60, 0, 0, 150);
#else
    qr_hdl.decoder = qrcode_init(QR_FRAME_W, QR_FRAME_H, QR_FRAME_W, QRCODE_MODE_NORMAL, SYM_BARCODE, 1, 60, 0, 0, 150);
#endif
    if (!qr_hdl.decoder) {
        printf("[error] qrcode_init fail");
    }
}

static void qr_code_stop(void)
{
    if (qr_hdl.decoder) {
        qrcode_deinit(qr_hdl.decoder);
        qr_hdl.decoder = NULL;
        free(qr_hdl.buf);
    }
}
static int qrcode_other_type_find(u8 *str, u8 **ssid, u8 **pwd)//配网二维码其他类型查找
{
    if (!str || !ssid || !pwd) {
        return 0;
    }
    u8 *p, *e, *s, *d;
    u8 *fs = 0, *fp = 0;
    const u8 *keyword[] = {
        //WIFI   ssid  pwd   end;
        "WIFI:", "S:", "P:", ";",//huawei,xiaomi: WIFI:T:WPA;S:GJ1;P:8888888899;;
        NULL, NULL, NULL, NULL,
    };
    *pwd = *ssid = NULL;
    for (int i = 0; keyword[i] != NULL; i += 4) {
        p = strstr(str, keyword[i]);
        if (p) {
            p = strstr(p + strlen(keyword[i]), keyword[i + 3]);//查找WIFI的结束符
            if (p) {
                for (int j = 1; j <= 2; j++) {//j=1,ssid; j=2,pwd
                    s = strstr(p, keyword[i + j]);//查找ssid/pwd起始符
                    if (s) {
                        s += strlen(keyword[i + j]);
                        e = strstr(s, keyword[i + 3]);//查找ssid/pwd结束符
                        if (e && (e - s) > 0) {
                            if (j == 1) {
                                fs = &s[e - s];
                                *ssid = s;
                            } else {
                                fp = &s[e - s];
                                *pwd = s;
                            }
                        }
                    }
                }
            }
        }
        if (fs) {
            *fs = 0;
            if (fp) {
                *fp = 0;
            }
            return 1;
        }
    }
    return 0;
}
static void qr_code_check_run(void)
{
    if (qr_hdl.frame_use_flog) {
        int ret = -1, type = 0;
        char *buf = NULL;
        int buf_size = 0;
        int enc_type = 0;
        int md_detected = 1; //是否检测到运动物体
        json_object *new_obj = NULL;
        json_object *key = NULL;

        qrcode_detectAndDecode(qr_hdl.decoder, qr_hdl.buf, &md_detected);
        ret = qrcode_get_result(qr_hdl.decoder, &buf, &buf_size);
        type = qrcode_get_symbol_type(qr_hdl.decoder);
        qr_hdl.frame_use_flog = 0;
        if (buf_size > 0 && ret == 0) {
            buf[buf_size] = 0;
            printf("qr code type = %d decode: %s\n", type, buf);
            qrcode_get_qr_decode_success(qr_hdl.decoder);
            const char *str_ssid = NULL, *str_pwd = NULL;
            new_obj = json_tokener_parse(buf);
            if (!new_obj) {
                goto qr_other;
            }

            str_ssid = json_object_get_string(json_object_object_get(new_obj, "ssid"));
            if (str_ssid) {
                sprintf(qr_hdl.ssid, "%s", str_ssid);
                printf("qr code ssid : %s\n", qr_hdl.ssid);
            } else {
                goto qr_other;
            }

            str_pwd = json_object_get_string(json_object_object_get(new_obj, "pass"));
qr_other:
            if (!str_ssid) {
                if (!qrcode_other_type_find(buf, &str_ssid, &str_pwd)) {
                    goto __exit;
                }
            }
            if (str_pwd) {
                sprintf(qr_hdl.pwd, "%s", str_pwd);
                printf("qr code pwd : %s\n", qr_hdl.pwd);
            } else {
                str_pwd = "";
                sprintf(qr_hdl.pwd, "%s", str_pwd);
            }
        }
__exit:
        if (new_obj) {
            json_object_put(new_obj);
        }
    }
}
static void qr_code_task(void *priv)
{
    qr_code_start();
    while (1) {
        qr_code_check_run();
        os_time_dly(15);
    }
    qr_code_stop();
}
int qr_code_test(void)
{
    if (qr_hdl.buf == NULL) {
        printf("<<<<<<<<<<<<<<<<<qr_hdl.buf[malloc fail]");
    }
    return thread_fork("qr_code_task", 5, 1024 * 6, 0, 0, qr_code_task, NULL);
}
late_initcall(qr_code_test);

#endif

