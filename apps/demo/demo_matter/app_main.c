#include "app_config.h"
#include "system/includes.h"
#include "os/os_api.h"
#include "event/net_event.h"
#include "wifi/wifi_connect.h"
#include "net/config_network.h"
#include "event/key_event.h"

/*中断列表 */
const struct irq_info irq_info_table[] = {
    //中断号   //优先级0-7   //注册的cpu(0或1)
#ifdef CONFIG_IPMASK_ENABLE
    //不可屏蔽中断方法：支持写flash，但中断函数和调用函数和const要全部放在内部ram
    { IRQ_SOFT5_IDX,      6,   0    }, //此中断强制注册到cpu0
    { IRQ_SOFT4_IDX,      6,   1    }, //此中断强制注册到cpu1
#if 0 //如下，SPI1使用不可屏蔽中断设置,优先级固定7
    { IRQ_SPI1_IDX,      7,   1    },//中断强制注册到cpu0/1
#endif
#endif
#if CPU_CORE_NUM == 1
    { IRQ_SOFT5_IDX,      7,   0    }, //此中断强制注册到cpu0
    { IRQ_SOFT4_IDX,      7,   1    }, //此中断强制注册到cpu1
    { -2,     			-2,   -2   },//如果加入了该行, 那么只有该行之前的中断注册到对应核, 其他所有中断强制注册到CPU0
#endif
    { -1,     -1,   -1    },
};

/*任务列表 */
const struct task_info task_info_table[] = {
    {"app_core",            15,     2048,	  1024 },
    {"sys_event",           29,     512,	   0 },
    {"systimer",            14,     256, 	   0 },
    {"sys_timer",            9,     512,	  128 },

#ifdef CONFIG_WIFI_ENABLE
    {"tasklet",             10,     1400,    0},//通过调节任务优先级平衡WIFI收发占据总CPU的比重
    {"RtmpMlmeTask",        17,     700,  	 0},
    {"RtmpCmdQTask",        17,     300,   	 0},
    {"wl_rx_irq_thread",     5,     256,  	 0},
#endif

//添加蓝牙
#ifdef CONFIG_BT_ENABLE
#if CPU_CORE_NUM > 1
    {"#C0btctrler",         19,      512,   384   },
    {"#C0btstack",          18,      1024,  384   },
#else
    {"btctrler",            19,      512,   384   },

    {"btstack",             18,      768,   384   },
#endif
#endif

    {0, 0, 0, 0, 0},
};


static int app_demo_state_machine(struct application *app, enum app_state state, struct intent *it)
{
    return 0;
}
static int app_demo_event_handler(struct application *app, struct sys_event *sys_event)
{
    switch (sys_event->type) {
    case SYS_NET_EVENT:
        struct net_event *net_event = (struct net_event *)sys_event->payload;
        if (!ASCII_StrCmp(net_event->arg, "net", 4)) {
            switch (net_event->event) {
            case NET_CONNECT_TIMEOUT_NOT_FOUND_SSID:
                puts("app_demo_event_handler recv NET_CONNECT_TIMEOUT_NOT_FOUND_SSID \r\n");
                break;
            case NET_CONNECT_ASSOCIAT_FAIL:
                puts("app_demo_event_handler recv NET_CONNECT_ASSOCIAT_FAIL \r\n");
                break;
            case NET_EVENT_SMP_CFG_FIRST:
                puts("app_demo_event_handler recv NET_EVENT_SMP_CFG_FIRST \r\n");
                break;
            case NET_EVENT_SMP_CFG_FINISH:
                puts("app_demo_event_handler recv NET_EVENT_SMP_CFG_FINISH \r\n");
                if (is_in_config_network_state()) {
                    config_network_stop();
                }
                config_network_connect();
                break;
            case NET_EVENT_CONNECTED:
                puts("app_demo_event_handler recv NET_EVENT_CONNECTED \r\n");
                extern void config_network_broadcast(void);
                config_network_broadcast();
                break;
            case NET_EVENT_DISCONNECTED:
                puts("app_demo_event_handler recv NET_EVENT_DISCONNECTED \r\n");
                break;
            case NET_EVENT_SMP_CFG_TIMEOUT:
                puts("app_demo_event_handler recv NET_EVENT_SMP_CFG_TIMEOUT \r\n");
                break;
            case NET_SMP_CFG_COMPLETED:
                puts("app_demo_event_handler recv NET_SMP_CFG_COMPLETED \r\n");
#ifdef CONFIG_AIRKISS_NET_CFG
                wifi_smp_set_ssid_pwd();
#endif
                break;
            case NET_EVENT_DISCONNECTED_AND_REQ_CONNECT:
                puts("app_demo_event_handler recv NET_EVENT_DISCONNECTED_AND_REQ_CONNECT \r\n");
                extern void wifi_return_sta_mode(void);
                wifi_return_sta_mode();
                break;
            case NET_NTP_GET_TIME_SUCC:	//NTP获取成功事件返回
                break;
            default:
                break;
            }
        }
    default:
        return false;
    }
}

__attribute__((weak)) void button_isr_handler(void)
{
}

static int key_event_handler(struct key_event *key)
{
    switch (key->action) {
    case KEY_EVENT_CLICK:
        switch (key->value) {
        case 6: //k5
            break;
        case 7: //k8
            puts("factory reset \n");
            void factory_reset(void);
            factory_reset();
            break;
        case 10: //k7
            break;
        case 11: //k6
            break;
        case 12: //k3
            break;
        case 13: //k4
            break;
        case 17: //k2
            puts("button_isr_handler event\n");
            button_isr_handler();
            break;
        default:
            return false;
        }
        break;
    case KEY_EVENT_LONG:
        break;
    default:
        return false;
    }

    return true;
}

/*
 * 默认的系统事件处理函数
 * 当所有活动的app的事件处理函数都返回false时此函数会被调用
 */
void app_default_event_handler(struct sys_event *event)
{
    switch (event->type) {
    case SYS_KEY_EVENT:
        key_event_handler((struct key_event *)event->payload);
        break;
    case SYS_DEVICE_EVENT:
        break;
    case SYS_BT_EVENT:
        extern int ble_demo_bt_event_handler(struct sys_event * event);
        ble_demo_bt_event_handler(event);
        break;
    case SYS_NET_EVENT:
        break;
    default:
        ASSERT(0, "unknow event type: %s\n", __func__);
        break;
    }
}

static const struct application_operation app_demo_ops = {
    .state_machine  = app_demo_state_machine,
    .event_handler 	= app_demo_event_handler,
};


REGISTER_APPLICATION(app_demo) = {
    .name 	= "app_demo",
    .ops 	= &app_demo_ops,
    .state  = APP_STA_DESTROY,
};

/*
 * 应用程序主函数
 */
void app_main()
{
    printf("\r\n\r\n\r\n\r\n ------------------- demo_matter app_main run %s ---------------\r\n\r\n\r\n\r\n\r\n", __TIME__);
    matter_main();
}


