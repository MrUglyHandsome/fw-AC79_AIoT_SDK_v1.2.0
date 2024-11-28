#include "app_config.h"
#include "system/includes.h"
#include "os/os_api.h"

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
#ifdef CONFIG_UI_ENABLE
    {"ui",                  21,      768,   256   },
    {"lcd_task_0",			8,	  1024,	  32   },
    {"lcd_task_1",			8,	  1024,	  32   },
    {"te_task",		    	9,	  1024,	  32   },
#endif

    {0, 0, 0, 0, 0},
};



void app_main()
{
#ifdef CONFIG_CXX_SUPPORT
    //注意：使用时也可以不放置在这里调用，但必须在使用c++前进行调用初始化c++环境
    void cpp_run_init(void);
    cpp_run_init();
#endif
    printf("\r\n\r\n\r\n\r\n\r\n -----------ui demo run %s-------------\r\n\r\n\r\n\r\n\r\n", __TIME__);
}

