#include "system/includes.h"
#include "action.h"
#include "app_core.h"
#include "event/key_event.h"
#include "event/device_event.h"
#include "app_config.h"
#include "storage_device.h"
#include "generic/log.h"
#include "os/os_api.h"
#include "usb/host/usb_host.h"
#include "usb_ctrl_transfer.h"
#include "usb_config.h"

u16 timer_id_park_det;
u8  usb_id;
u16 usb_vid;
u16 usb_pid;
u8  vendor_name[64];
u8  product_name[64];
u8 park_sta;
u8 park_sta_prev;

u8 get_parking_status(void)
{
    return park_sta;
}

static void parking_detect_timer(void *arg)
{
    struct usb_host_device *host_dev = (struct usb_host_device *)host_id2device(usb_id);
    u8 buf[8];
    u8 requesttype = 0, request = 0;
    u16 wvalue = 0, windex = 0, wlength = 0;
    u16 pk_state = 0;
    int ret = 0;

    if ((usb_vid == 0x4c4a) && (usb_pid & 0x00ff) == 0x55) {  //DV20 USB-CAMERA
        requesttype = 0xa1;
        request = 0x81;
        wvalue = 0x0100;
        windex = 0x0200;
        wlength = 2;
        pk_state = 0x0079;
    }
    if (request) {
        ret = usb_control_msg(host_dev, request, requesttype, wvalue, windex, buf, wlength);
        if (!ret) {
            if (!memcmp(buf, &pk_state, wlength)) {
                park_sta = 1;
            } else {
                park_sta = 0;
            }
        } else {
            park_sta = 0;
        }
    } else {
        park_sta = 0;
    }

    if (park_sta_prev != park_sta) {
        park_sta_prev = park_sta;
        printf(">>>>>>> parking state change to %d\n", park_sta);
    }
}

static void usb_host_event_for_parking_detect(struct sys_event *e)
{
    struct device_event *event = (struct device_event *)e->payload;
    struct usb_host_device *host_dev;
    struct usb_device_descriptor device_desc = {0};
    int ret = 0;
    u8 buf[64];

    if (!strncmp((char *)event->value, "uvc", 3)) {
        usb_id = ((char *)event->arg)[8] - '0';
        if (event->event == DEVICE_EVENT_IN) {
            host_dev = (struct usb_host_device *) host_id2device(usb_id);
            ret = usb_get_device_descriptor(host_dev, &device_desc);
            if (!ret) {
                usb_vid = device_desc.idVendor;
                usb_pid = device_desc.idProduct;
            }
            if (device_desc.iManufacturer) {
                ret = usb_control_msg(host_dev, 0x06, 0x80, 0x0301, 0x0409, buf, sizeof(buf));
                if (!ret) {
                    for (int i = 0; i < (buf[0] - 2) / 2; i++) {
                        vendor_name[i] = buf[2 + i * 2];
                    }
                }
            }
            if (device_desc.iProduct) {
                ret = usb_control_msg(host_dev, 0x06, 0x80, 0x0302, 0x0409, buf, sizeof(buf));
                if (!ret) {
                    for (int i = 0; i < (buf[0] - 2) / 2; i++) {
                        product_name[i] = buf[2 + i * 2];
                    }
                }
            }
            printf("vid %04x, pid %04x, %s, %s\n", usb_vid, usb_pid, vendor_name, product_name);
            timer_id_park_det = sys_timer_add(NULL, parking_detect_timer, 200);
        } else if (event->event == DEVICE_EVENT_OUT) {
            if (timer_id_park_det) {
                sys_timer_del(timer_id_park_det);
                timer_id_park_det = 0;
            }
            usb_vid = 0;
            usb_pid = 0;
            memset(vendor_name, 0, sizeof(vendor_name));
            memset(product_name, 0, sizeof(product_name));
            park_sta = 0;
            park_sta_prev = 0;
        }
    }
}

static int parking_detect_init(void)
{
    register_sys_event_handler(SYS_DEVICE_EVENT, DEVICE_EVENT_FROM_USB_HOST, 0, usb_host_event_for_parking_detect);
    return 0;
}
late_initcall(parking_detect_init);

