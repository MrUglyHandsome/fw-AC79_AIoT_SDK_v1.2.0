#include "app_config.h"
#include "init.h"
#include "system/timer.h"
#include "lwip.h"
#include "lte_module/lte_module.h"
#include "lwip/sockets.h"

#ifdef CONFIG_LTE_PHY_ENABLE

static struct {
    void *dev;
    u16 check_timer;
    u8 wifi_on_flag;
    u8 hwaddr[6];
} lte_app_hdl;

#define __this	(&lte_app_hdl)

extern void ext_wifi_on(void);
extern void ext_wifi_off(void);

#if IP_NAPT_EXT
static void check_dhcps_client_ipaddr_timer(void *p)
{
    struct ip4_addr src_ipaddr, dest_ipaddr;
    char gw_addr[32];

    extern int dhcps_get_ipaddr(u8 hwaddr[6], struct ip4_addr * ipaddr);
    if (0 == dhcps_get_ipaddr(__this->hwaddr, &dest_ipaddr)) {
        sys_timer_del(__this->check_timer);
        __this->check_timer = 0;
        get_gateway(LTE_NETIF, gw_addr);
        inet_aton(gw_addr, &src_ipaddr);
        extern void ip_napt_ext_set_forward_addr(u32_t src_addr, u32_t dest_addr);
        ip_napt_ext_set_forward_addr(src_ipaddr.addr, dest_ipaddr.addr);
    }
}

void check_dhcps_client_ipaddr(u8 *hwaddr)
{
    memcpy(__this->hwaddr, hwaddr, 6);
    if (!__this->check_timer) {
        __this->check_timer = sys_timer_add_to_task("sys_timer", NULL, check_dhcps_client_ipaddr_timer, 500);
    }
}
#endif

int lte_lwip_event_cb(void *lwip_ctx, enum LWIP_EVENT event)
{
    char ip_addr[32];
    char gw_addr[32];

    switch (event) {
    case LWIP_LTE_DHCP_BOUND_TIMEOUT:
        break;

    case LWIP_LTE_DHCP_BOUND_SUCC:
        Get_IPAddress(LTE_NETIF, ip_addr);
        get_gateway(LTE_NETIF, gw_addr);
        printf("LTE DHCP SUCC, IP:[%s] , GW:[%s]", ip_addr, gw_addr);
        //此处4G网络已连通
        lwip_set_default_netif(LTE_NETIF);   //设置4G网卡为默认模块
#if IP_NAPT_EXT
        if (!__this->wifi_on_flag) {
            ext_wifi_on();
            __this->wifi_on_flag = 1;
        }
#endif
#if IP_NAPT
        struct ip4_addr ipaddr;
        struct lan_setting *lan_setting_info = net_get_lan_info(EXT_WIFI_NETIF);
        IP4_ADDR(&ipaddr, lan_setting_info->WIRELESS_IP_ADDR0, lan_setting_info->WIRELESS_IP_ADDR1, lan_setting_info->WIRELESS_IP_ADDR2, lan_setting_info->WIRELESS_IP_ADDR3);
        extern void ip_napt_enable(u32_t addr, int enable);
        ip_napt_enable(ipaddr.addr, 1);
#endif
        break;

    default:
        break;
    }

    return 0;
}

static int lte_state_cb(void *priv, int on)
{
    if (priv && !strncmp((const char *)priv, "wireless", strlen("wireless"))) {
        if (on) {
            printf("lte on\n");
            dev_ioctl(__this->dev, LTE_NETWORK_START, 0);
        } else {
            printf("lte off\n");
            dev_ioctl(__this->dev, LTE_NETWORK_STOP, 0);
#if 0//IP_NAPT_EXT	//TODO外挂wifi卸载会异常
            ext_wifi_off();
            __this->wifi_on_flag = 0;
#endif
        }
    }

    return 0;
}

static int lte_net_init(void)
{
    __this->dev = dev_open("lte", NULL);
    return dev_ioctl(__this->dev, LTE_DEV_SET_CB, (u32)lte_state_cb);
}
late_initcall(lte_net_init);

#endif

