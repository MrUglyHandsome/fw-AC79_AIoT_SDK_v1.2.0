
#include "matter_os_adapter.h"

static MATTER_OS_INTF matter_intf = {0};

int matter_os_adapter_register(intf_type type, void *intf)
{
    switch (type) {
    case INTF_BLE_GATT_ACCESS:
        matter_intf.ble_gatt_access_op_callback = intf;
        break;
    case INTF_BLE_GAT_EVENT:
        matter_intf.ble_event_callback = intf;
        break;
    case INTF_WIFI_EVENT:
        matter_intf.wifi_event_callback = intf;
        break;
    default:
        printf("type not supporte : 0x%x\n", type);
        return -1;
    }

    return 0;
}

void *get_mattr_os_intf(void)
{
    return &matter_intf;
}

u32 _swap32(u32 value)
{
    return _SWAP32(value);
}

void wifi_start(void)
{
    if (wifi_is_on()) {
        return ;
    }

    wifi_on();
}
