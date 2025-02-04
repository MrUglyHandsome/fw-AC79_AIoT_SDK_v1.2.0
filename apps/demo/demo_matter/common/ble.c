#include "app_config.h"
#include "btcontroller_config.h"
#include "btctrler/btctrler_task.h"
#include "btstack/btstack_task.h"
#include "btstack/le/att.h"
#include "btstack/le/le_user.h"
#include "btstack/avctp_user.h"
#include "bt_common.h"
#include "le_common.h"
#include "le_net_central.h"
#include "event/bt_event.h"
#include "syscfg/syscfg_id.h"
#include "../multi_demo/le_multi_common.h"

void (*lmp_ch_update_resume_hdl)(void *priv) = NULL;

static const u8 *ble_get_mac_addr(void)
{
    static u8 mac_addr[6];
    const u8 bc_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    if (syscfg_read(CFG_BT_MAC_ADDR, mac_addr, 6) == 6 && memcmp(mac_addr, bc_mac, 6)) {
        return mac_addr;
    }

    u8 flash_uid[16];
    memcpy(flash_uid, get_norflash_uuid(), 16);
    do {
        u32 crc32 = rand32()^CRC32(flash_uid, sizeof(flash_uid));
        u16 crc16 = rand32()^CRC16(flash_uid, sizeof(flash_uid));
        memcpy(mac_addr, &crc32, sizeof(crc32));
        memcpy(&mac_addr[4], &crc16, sizeof(crc16));
    } while (!bytecmp(mac_addr, 0, 6));
    //此处用户可自行修改为本地生成mac地址的算法
    mac_addr[0] &= ~((1 << 0) | (1 << 1));
    syscfg_write(CFG_BT_MAC_ADDR, mac_addr, 6);
    return mac_addr;
}

void bt_ble_module_init(void)
{
    void lmp_set_sniff_disable(void);
    lmp_set_sniff_disable();

#if TCFG_USER_BLE_ENABLE
    u8 tmp_ble_addr[6];
    extern void lib_make_ble_address(u8 * ble_address, u8 * edr_address);
    extern int le_controller_set_mac(void *addr);
    lib_make_ble_address(tmp_ble_addr, (u8 *)ble_get_mac_addr());
    le_controller_set_mac((void *)tmp_ble_addr);
    printf("\n-----edr + ble 's address-----");
    put_buf((void *)ble_get_mac_addr(), 6);
    put_buf((void *)tmp_ble_addr, 6);
#endif

    btstack_init();
}

static int bt_connction_status_event_handler(struct bt_event *bt)
{
    switch (bt->event) {
    case BT_STATUS_INIT_OK:
        puts("BT_STATUS_INIT_OK");
        bt_ble_init();
        break;
    }

    return 0;
}

int ble_demo_bt_event_handler(struct sys_event *event)
{
    if (event->from == BT_EVENT_FROM_CON) {
        return bt_connction_status_event_handler((struct bt_event *)event->payload);
    }

    return 0;
}

