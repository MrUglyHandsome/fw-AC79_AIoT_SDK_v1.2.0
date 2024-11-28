
#ifndef _EXT_WIFI_MODULE_EVENT_H_
#define _EXT_WIFI_MODULE_EVENT_H_

#include "wifi/wifi_connect.h"

enum WIFI_MODULE {
    RTL8189E,
    RTL8189F,
    RTL8188E,
    RTL8188F,
    RTL8822B,
    RTL8822C,
    RTL8822E,
    RTL8852B,
    MARVEL_88W8782,
    MARVEL_88W8801,
    MEDIATEK_7601U,
    RALINK_3070,
    WL_30,
    S9082,
    RTL8192E,
    RTL8192F,
    RTL8821CS,
    HI3881,
    NONE_MODULE = -1,
};

//enum WIFI_MODE {
//    STA_MODE,
//    AP_MODE,
//    SMP_CFG_MODE,
//    MP_TEST_MODE,
//    P2P_MODE,
//    NONE_MODE,
//};


//enum P2P_ROLE {
//    P2P_GC_MODE,
//    P2P_GO_MODE,
//};


//enum SMP_CFG_TYPE {
//    PRIV_SMP_CFG1 = 1,
//    PRIV_SMP_CFG2,
//    AIRKISS_SMP_CFG,
//};

struct WIFI_INIT_INFO {
    enum WIFI_MODULE module;
    enum WIFI_MODE mode;
    u8 pwd[3][64];
    u8 ssid[3][33];
    u8 p2p_role;
} __attribute__((packed));

enum WIFI_MODULE_EVENT {
    SMP_CFG_START = 0x0,
    SMP_CFG_STOP = 0x1,
    SMP_CFG_TIMEOUT = 0x2,
    SMP_CFG_COMPLETED = 0x3,
    AP_ON_DISCONNECTED = 0x4,
    AP_ON_ASSOC = 0x5,
    PM_SUSPEND = 0x6,
    PM_RESUME = 0x7,
};

struct netdev_ssid_info {
    char ssid[32];
    unsigned int ssid_len;
    unsigned char mac_addr[6];
    long rssi;
    unsigned int channel_number;
    unsigned char	SignalStrength;//(in percentage)
    unsigned char	SignalQuality;//(in percentage)
    unsigned char   SupportedRates[16];
};

//struct smp_cfg_result {
//    enum SMP_CFG_TYPE type;
//    unsigned int ssid_crc;
//    unsigned int random_val;
//    char ssid[32];
//    char passphrase[64];
//};

#endif  //_WIFI_MODULE_EVENT_H_

