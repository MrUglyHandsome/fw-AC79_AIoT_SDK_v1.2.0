#include "app_config.h"
#ifdef CONFIG_EXT_WIFI_ENABLE
#include "system/includes.h"
#include "device/device.h"
#include "wireless_ext/wifi_dev.h"
#include "wireless_ext/wifi_connect.h"
#include "lwip.h"
#include "lwip/sockets.h"

#define AP_TEST_MODE	(0)
#define STA_TEST_MODE	(1)
#define EXT_WIFI_TEST_MODE	AP_TEST_MODE

#ifdef CONFIG_RTL8822ES_10M_ENABLE
#define AP_SSID "HF_GPS5G-123456"        //配置 AP模式的SSID前缀
#define AP_PWD  "12345678"               //配置 AP模式的密码
#else
#define AP_SSID "AC79_EXT_WIFI"  		 //配置 AP模式的SSID前缀
#define AP_PWD  "12345678"    			 //配置 AP模式的密码
#endif

#define STA_SSID  "Aaaaaaaaaaaaaaaaa"    //配置 STA模式的SSID
#define STA_PWD  "123456789"             //配置 STA模式的密码

#define WIFI_APP_TASK_NAME "ext_wifi_app_task"

enum WIFI_APP_MSG_CODE {
    WIFI_MSG_TICK_1_SEC,
    WIFI_MSG_SMP_CFG_COMPLETED,
    WIFI_MSG_STA_SCAN_COMPLETED,
    WIFI_MSG_STA_NETWORK_STACK_DHCP_SUCC,
};


static void *wifi_dev = NULL;

extern unsigned int time_lapse(unsigned int *handle, unsigned int time_out);
extern u32 ext_wifi_get_upload_rate(void);
extern u32 ext_wifi_get_download_rate(void);
extern int wifi_module_is_init(void);


static void wifi_app_timer_func(void *p)
{
    os_taskq_post(WIFI_APP_TASK_NAME, 1, WIFI_MSG_TICK_1_SEC);
}


static int network_user_callback(void *network_ctx, enum WIFI_EVENT state, void *priv)
{
    struct cfg_info info = {0};

    switch (state) {

    case WIFI_EVENT_MODULE_INIT:
        puts("|ext_network_user_callback->WIFI_EVENT_MODULE_INIT\n");

//wifi module port seting
        info.port_status = 0;
        dev_ioctl(wifi_dev, DEV_SET_WIFI_POWER, (u32)&info);
        msleep(10);
        info.port_status = 1;
        dev_ioctl(wifi_dev, DEV_SET_WIFI_POWER, (u32)&info);

        info.port_status = 1;
        dev_ioctl(wifi_dev, DEV_SET_WIFI_CS, (u32)&info);

        info.port_status = 1;
        dev_ioctl(wifi_dev, DEV_SET_WIFI_WKUP, (u32)&info);

        msleep(100);

        info.mode = AP_MODE;
        info.ssid = AP_SSID;
        info.pwd  = AP_PWD;
        info.force_default_mode = 1;
        dev_ioctl(wifi_dev, DEV_SET_DEFAULT_MODE, (u32)&info);
        break;

    case WIFI_EVENT_MODULE_START:
        puts("|ext_network_user_callback->WIFI_EVENT_MODULE_START\n");
        info.mode = AP_MODE;
        info.ssid = AP_SSID;
        info.pwd  = AP_PWD;
        info.force_default_mode = 1;
        dev_ioctl(wifi_dev, DEV_SAVE_DEFAULT_MODE, (u32)&info);

        wpa_supplicant_set_connect_timeout(20);
        break;

    case WIFI_EVENT_MODULE_STOP:
        puts("|ext_network_user_callback->WIFI_EVENT_MODULE_STOP\n");
        break;

    case WIFI_EVENT_AP_START:
        puts("|ext_network_user_callback->WIFI_EVENT_AP_START\n");
        break;

    case WIFI_EVENT_AP_STOP:
        puts("|ext_network_user_callback->WIFI_EVENT_AP_STOP\n");
        break;

    case WIFI_EVENT_STA_START:
        puts("|ext_network_user_callback->WIFI_EVENT_STA_START\n");
        break;

    case WIFI_EVENT_MODULE_START_ERR:
        puts("|ext_network_user_callback->WIFI_EVENT_MODULE_START_ERR\n");
        break;

    case WIFI_EVENT_STA_STOP:
        puts("|ext_network_user_callback->WIFI_EVENT_STA_STOP\n");
        break;

    case WIFI_EVENT_STA_DISCONNECT:
        puts("|ext_network_user_callback->WIFI_STA_DISCONNECT\n");
        break;

    case WIFI_EVENT_STA_SCAN_COMPLETED:
        puts("|ext_network_user_callback->WIFI_STA_SCAN_COMPLETED\n");
        os_taskq_post(WIFI_APP_TASK_NAME, 1, WIFI_MSG_STA_SCAN_COMPLETED);
        break;

    case WIFI_EVENT_STA_CONNECT_SUCC:
        dev_ioctl(wifi_dev, DEV_GET_WIFI_CHANNEL, (u32)&info);
        printf("|ext_network_user_callback->WIFI_STA_CONNECT_SUCC,CH=%d\r\n", info.sta_channel);
        break;

    case WIFI_EVENT_MP_TEST_START:
        puts("|ext_network_user_callback->WIFI_EVENT_MP_TEST_START\n");
        break;

    case WIFI_EVENT_MP_TEST_STOP:
        puts("|ext_network_user_callback->WIFI_EVENT_MP_TEST_STOP\n");
        break;

    case WIFI_EVENT_STA_CONNECT_TIMEOUT_NOT_FOUND_SSID:
        puts("|ext_network_user_callback->WIFI_STA_CONNECT_TIMEOUT_NOT_FOUND_SSID\n");
        break;

    case WIFI_EVENT_STA_CONNECT_ASSOCIAT_TIMEOUT:
        puts("|ext_network_user_callback->WIFI_EVENT_STA_CONNECT_ASSOCIAT_TIMEOUT\n");
        break;

    case WIFI_EVENT_STA_NETWORK_STACK_DHCP_SUCC:
        puts("|ext_network_user_callback->WIFI_EVENT_STA_NETWPRK_STACK_DHCP_SUCC\n");
        os_taskq_post(WIFI_APP_TASK_NAME, 1, WIFI_MSG_STA_NETWORK_STACK_DHCP_SUCC);
        break;

    case WIFI_EVENT_STA_NETWORK_STACK_DHCP_TIMEOUT:
        puts("|ext_network_user_callback->WIFI_EVENT_STA_NETWPRK_STACK_DHCP_TIMEOUT\n");
        break;

    case WIFI_EVENT_P2P_START:
        puts("|ext_network_user_callback->WIFI_EVENT_P2P_START\n");
        break;

    case WIFI_EVENT_P2P_STOP:
        puts("|ext_network_user_callback->WIFI_EVENT_P2P_STOP\n");
        break;

    case WIFI_EVENT_P2P_GC_DISCONNECTED:
        puts("|ext_network_user_callback->WIFI_EVENT_P2P_GC_DISCONNECTED\n");
        break;

    case WIFI_EVENT_P2P_GC_NETWORK_STACK_DHCP_SUCC:
        puts("|ext_network_user_callback->WIFI_EVENT_P2P_GC_NETWORK_STACK_DHCP_SUCC\n");
        break;

    case WIFI_EVENT_P2P_GC_NETWORK_STACK_DHCP_TIMEOUT:
        puts("|ext_network_user_callback->WIFI_EVENT_P2P_GC_NETWORK_STACK_DHCP_TIMEOUT\n");
        break;

    case WIFI_EVENT_SMP_CFG_TIMEOUT:
        puts("|ext_network_user_callback->WIFI_EVENT_SMP_CFG_TIMEOUT\n");
        break;

    case WIFI_EVENT_SMP_CFG_COMPLETED:
        puts("|ext_network_user_callback->WIFI_EVENT_SMP_CFG_COMPLETED\n");
        os_taskq_post(WIFI_APP_TASK_NAME, 1, WIFI_MSG_SMP_CFG_COMPLETED);
        break;

    case WIFI_EVENT_PM_SUSPEND:
        puts("|ext_network_user_callback->WIFI_EVENT_PM_SUSPEND\n");
        break;

    case WIFI_EVENT_PM_RESUME:
        puts("|ext_network_user_callback->WIFI_EVENT_PM_RESUME\n");
        break;

    case WIFI_EVENT_AP_ON_ASSOC:
        struct eth_addr *hwaddr = (struct eth_addr *)network_ctx;
        printf("|ext_network_user_callback->WIFI_EVENT_AP_ON_ASSOC hwaddr = %02x:%02x:%02x:%02x:%02x:%02x \r\n\r\n",
               hwaddr->addr[0], hwaddr->addr[1], hwaddr->addr[2], hwaddr->addr[3], hwaddr->addr[4], hwaddr->addr[5]);
#if IP_NAPT_EXT && defined CONFIG_LTE_PHY_ENABLE
        extern void check_dhcps_client_ipaddr(u8 * hwaddr);
        check_dhcps_client_ipaddr(hwaddr->addr);
#endif
        break;

    case WIFI_EVENT_AP_ON_DISCONNECTED:
        hwaddr = (struct eth_addr *)network_ctx;
        printf("|ext_network_user_callback->WIFI_EVENT_AP_ON_DISCONNECTED hwaddr = %02x:%02x:%02x:%02x:%02x:%02x \r\n\r\n",
               hwaddr->addr[0], hwaddr->addr[1], hwaddr->addr[2], hwaddr->addr[3], hwaddr->addr[4], hwaddr->addr[5]);
        break;

    default:
        break;
    }

    return 0;
}


static void wifi_set_lan_setting_info(void)
{
    struct lan_setting lan_setting_info = {
        .WIRELESS_IP_ADDR0  = 192,
        .WIRELESS_IP_ADDR1  = 168,
        .WIRELESS_IP_ADDR2  = 1,
        .WIRELESS_IP_ADDR3  = 1,

        .WIRELESS_NETMASK0  = 255,
        .WIRELESS_NETMASK1  = 255,
        .WIRELESS_NETMASK2  = 255,
        .WIRELESS_NETMASK3  = 0,

        .WIRELESS_GATEWAY0  = 192,
        .WIRELESS_GATEWAY1  = 168,
        .WIRELESS_GATEWAY2  = 1,
        .WIRELESS_GATEWAY3  = 1,

        .SERVER_IPADDR1  = 192,
        .SERVER_IPADDR2  = 168,
        .SERVER_IPADDR3  = 1,
        .SERVER_IPADDR4  = 1,

        .CLIENT_IPADDR1  = 192,
        .CLIENT_IPADDR2  = 168,
        .CLIENT_IPADDR3  = 1,
        .CLIENT_IPADDR4  = 2,

        .SUB_NET_MASK1   = 255,
        .SUB_NET_MASK2   = 255,
        .SUB_NET_MASK3   = 255,
        .SUB_NET_MASK4   = 0,
    };

    struct cfg_info info;
    info.__lan_setting_info = &lan_setting_info;
    dev_ioctl(wifi_dev, DEV_SET_LAN_SETTING, (u32)&info);
}


void ext_wifi_on(void)
{
    dev_ioctl(wifi_dev, DEV_NETWORK_START, 0);
}


void ext_wifi_off(void)
{
    dev_ioctl(wifi_dev, DEV_NETWORK_STOP, 0);
}


static void ext_wifi_app_task(void *priv)
{
    int msg[32];
    int res;
    unsigned int timehdl = 0;
    struct cfg_info info = {0};

//wifi app start
    wifi_dev = dev_open("wifi", NULL);

    info.cb = network_user_callback;
    info.net_priv = NULL;
    dev_ioctl(wifi_dev, DEV_SET_CB_FUNC, (u32)&info);

#if 0
    printf("\n >>>> DEV_SET_WIFI_POWER_SAVE<<<<   \n");
    dev_ioctl(wifi_dev, DEV_SET_WIFI_POWER_SAVE, 0);//打开就启用低功耗模式, 只有STA模式才有用
#endif

#if 1
    printf("\n >>>> DEV_SET_WIFI_TX_PWR_BY_RATE<<<   \n");

    info.tx_pwr_lmt_enable = 0;//  解除WIFI发送功率限制
    dev_ioctl(wifi_dev, DEV_SET_WIFI_TX_PWR_LMT_ENABLE, (u32)&info);
    info.tx_pwr_by_rate = 1;// 设置WIFI根据不同datarate打不同power
    dev_ioctl(wifi_dev, DEV_SET_WIFI_TX_PWR_BY_RATE, (u32)&info);
#endif

    wifi_set_lan_setting_info();

#if !IP_NAPT_EXT || !defined CONFIG_LTE_PHY_ENABLE
    ext_wifi_on();
#endif

    sys_timer_add(NULL, wifi_app_timer_func, 1000);

#if (EXT_WIFI_TEST_MODE == AP_TEST_MODE)
    info.mode = AP_MODE;
    info.ssid = AP_SSID;
    info.pwd  = AP_PWD;
    info.force_default_mode = 1;
    dev_ioctl(wifi_dev, DEV_AP_MODE, (u32)&info);
#else
    info.mode = STA_MODE;
    info.ssid = STA_SSID;
    info.pwd  = STA_PWD;
    info.force_default_mode = 1;
    dev_ioctl(wifi_dev, DEV_STA_MODE, (u32)&info);
#endif

    while (1) {
        res = os_task_pend("taskq", msg, ARRAY_SIZE(msg));

        switch (msg[1]) {
        case WIFI_MSG_TICK_1_SEC:
            if (time_lapse(&timehdl, 3 * 1000)) {
                if (wifi_module_is_init()) {
                    printf("WIFI U= %d KB/s, D= %d KB/s\r\n", ext_wifi_get_upload_rate() / 1024, ext_wifi_get_download_rate() / 1024);
#if 0
                    if (ext_get_cur_wifi_info()->mode == STA_MODE) {
                        get_rx_signal();
                    }
#endif
                }
            }

            break;

        default:
            break;
        }
    }
}


static int ext_wireless_net_init(void)//主要是create wifi 线程的
{
    puts("wifi early init \n\n\n\n\n\n");
    malloc_stats();
    thread_fork(WIFI_APP_TASK_NAME, 10, 0x1000, 64, 0, ext_wifi_app_task, NULL);
    return 0;
}
late_initcall(ext_wireless_net_init);

#endif

