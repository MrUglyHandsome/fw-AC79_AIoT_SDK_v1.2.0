#ifndef __MATTER_OS_ADAPTER_H
#define __MATTER_OS_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "wifi/wifi_connect.h"

typedef enum {
    INTF_BLE_GATT_ACCESS,
    INTF_BLE_GAT_EVENT,
    INTF_WIFI_EVENT,
} intf_type;

enum {
    BLE_GATT_ACCESS_OP_READ_CHR = 0x01,
    BLE_GATT_ACCESS_OP_READ_DSC,
    BLE_GATT_ACCESS_OP_WRITE_CHR,
};

enum {
    BLE_GAP_EVENT_CONNECT = 0x1,
    BLE_GAP_EVENT_DISCONNECT,
    BLE_GAP_EVENT_ADV_COMPLETE,
    BLE_GAP_EVENT_SUBSCRIBE,
    BLE_GAP_EVENT_NOTIFY_TX,
    BLE_GAP_EVENT_MTU,
};

struct ble_gap_event {
    uint8_t type;
    uint16_t conn_handle;
    uint8_t cur_indicate;
};

struct ble_gatt_char_context {
    int op;
    uint8_t *value;
    size_t len;
    uint16_t conn_handle;
};

#define _SWAP32(x) ( \
   (((x) & 0x000000FFUL) << 24) | \
   (((x) & 0x0000FF00UL) << 8) | \
   (((x) & 0x00FF0000UL) >> 8) | \
   (((x) & 0xFF000000UL) >> 24))

typedef struct {
    int (*ble_event_callback)(struct ble_gap_event *event);
    int (*ble_gatt_access_op_callback)(uint16_t connId, uint16_t handle, struct ble_gatt_char_context *ctxt);
    void (*wifi_event_callback)(int32_t event);
} MATTER_OS_INTF;

int matter_os_adapter_register(intf_type type, void *intf);
void wifi_event_loop_set(void);
void wifi_start(void);
u32 timer_get_ms(void);

#ifdef __cplusplus
}
#endif

#endif // __MATTER_OS_ADAPTER_H


