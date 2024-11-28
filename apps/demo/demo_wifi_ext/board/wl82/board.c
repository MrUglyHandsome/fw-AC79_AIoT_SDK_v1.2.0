#include "app_config.h"
#include "system/includes.h"
#include "device/includes.h"
#include "asm/includes.h"
#if TCFG_USB_SLAVE_ENABLE || TCFG_USB_HOST_ENABLE
#include "otg.h"
#include "usb_host.h"
#include "usb_storage.h"
#endif
#ifdef CONFIG_EXT_WIFI_ENABLE
#include "net/wireless_ext/wifi_dev.h"
#endif
#ifdef CONFIG_LTE_PHY_ENABLE
#include "lte_module/lte_module.h"
#endif

// *INDENT-OFF*

UART1_PLATFORM_DATA_BEGIN(uart1_data)
    .baudrate = 115200,
    .port = PORTUSB_A,
    .tx_pin = IO_PORT_USB_DPA,
    .rx_pin = IO_PORT_USB_DMA,
    .max_continue_recv_cnt = 1024,
    .idle_sys_clk_cnt = 500000,
    .clk_src = PLL_48M,
    .flags = UART_DEBUG,
UART1_PLATFORM_DATA_END();


UART2_PLATFORM_DATA_BEGIN(uart2_data)
	.baudrate = 1000000,
	.port = PORT_REMAP,
	.output_channel = OUTPUT_CHANNEL0,
	.tx_pin = IO_PORTC_00,
	.rx_pin = -1,
	.max_continue_recv_cnt = 1024,
	.idle_sys_clk_cnt = 500000,
	.clk_src = PLL_48M,
	.flags = UART_DEBUG,
UART2_PLATFORM_DATA_END();


/************************** LOW POWER config ****************************/
static const struct low_power_param power_param = {
    .config         = TCFG_LOWPOWER_LOWPOWER_SEL,
    .btosc_disable  = TCFG_LOWPOWER_BTOSC_DISABLE,         //进入低功耗时BTOSC是否保持
    .vddiom_lev     = TCFG_LOWPOWER_VDDIOM_LEVEL,          //强VDDIO等级,可选：2.2V  2.4V  2.6V  2.8V  3.0V  3.2V  3.4V  3.6V
    .vddiow_lev     = TCFG_LOWPOWER_VDDIOW_LEVEL,          //弱VDDIO等级,可选：2.1V  2.4V  2.8V  3.2V
	.vdc14_dcdc 	= TRUE,	   							   //打开内部1.4VDCDC，关闭则用外部
    .vdc14_lev		= VDC14_VOL_SEL_LEVEL, 				   //VDD1.4V配置
	.sysvdd_lev		= SYSVDD_VOL_SEL_LEVEL,				   //内核、sdram电压配置
	.vlvd_enable	= TRUE,                                //TRUE电压复位使能
	.vlvd_value		= VLVD_SEL_25V,                        //低电压复位电压值
};


#if defined CONFIG_BT_ENABLE || defined CONFIG_WIFI_ENABLE
#include "wifi/wifi_connect.h"
const struct wifi_calibration_param wifi_calibration_param = {
    .xosc_l     = 0xb,// 调节左晶振电容
    .xosc_r     = 0xb,// 调节右晶振电容
    .pa_trim_data  ={1, 7, 4, 7, 11, 1, 7},// 根据MP测试生成PA TRIM值
	.mcs_dgain     ={
		50,//11B_1M
	  	50,//11B_2.2M
	  	50,//11B_5.5M
		50,//11B_11M

		72,//11G_6M
		72,//11G_9M
		85,//11G_12M
		80,//11G_18M
		64,//11G_24M
		64,//11G_36M
		62,//11G_48M
		52,//11G_54M

		72,//11N_MCS0
		90,//11N_MCS1
		80,//11N_MCS2
		64,//11N_MCS3
		64,//11N_MCS4
		64,//11N_MCS5
		50,//11N_MCS6
		43,//11N_MCS7
	}
};
#endif


#ifdef CONFIG_EXT_WIFI_ENABLE
WIFI_PLATFORM_DATA_BEGIN(wifi_data)
#ifdef CONFIG_RTL8189E_ENABLE
	.module = RTL8189E,
#elif CONFIG_RTL8822ES_10M_ENABLE
	.module = RTL8822E,
#elif CONFIG_RTL8822CS_ENABLE
	.module = RTL8822C,
#endif
	.sdio_parm = SDIO_GRP_0 | SDIO_PORT_3 | SDIO_4_BIT_DATA | SDIO_DATA1_IRQ | SDIO_CLOCK_2M,
	.wakeup_port = IO_PORTB_11,
	.cs_port = IO_PORTB_12,
	.power_port = IO_PORTB_13,
WIFI_PLATFORM_DATA_END()
#endif


#if defined CONFIG_LTE_PHY_ENABLE
extern const struct device_operations lte_module_dev_ops;
LTE_MODULE_DATA_BEGIN(lte_module_data)
    .name = (u8 *)"usb_rndis",
LTE_MODULE_DATA_END()
#endif


#if TCFG_USB_SLAVE_ENABLE || TCFG_USB_HOST_ENABLE
static const struct otg_dev_data otg_data = {
#if defined CONFIG_MP_TEST_ENABLE
    .usb_dev_en = 0x02,
#else
    .usb_dev_en = 0x03,
#endif
#if TCFG_USB_SLAVE_ENABLE
    .slave_online_cnt = 10,
    .slave_offline_cnt = 10,
#endif
#if TCFG_USB_HOST_ENABLE
    .host_online_cnt = 10,
    .host_offline_cnt = 10,
#endif
    .detect_mode = OTG_HOST_MODE | OTG_SLAVE_MODE | OTG_CHARGE_MODE,
    .detect_time_interval = 50,
};
#endif


REGISTER_DEVICES(device_table) = {
    {"uart1", &uart_dev_ops, (void *)&uart1_data },
    {"uart2", &uart_dev_ops, (void *)&uart2_data },
    {"rtc", &rtc_dev_ops, NULL},
#if TCFG_USB_SLAVE_ENABLE || TCFG_USB_HOST_ENABLE
    { "otg", &usb_dev_ops, (void *)&otg_data},
#endif
#ifdef CONFIG_EXT_WIFI_ENABLE
    { "wifi",  &wifi_dev_ops, (void *) &wifi_data},
#endif
#ifdef CONFIG_LTE_PHY_ENABLE
    { "lte",  &lte_module_dev_ops, (void *)&lte_module_data},
#endif
};

#ifdef CONFIG_DEBUG_ENABLE
void debug_uart_init()
{
    uart_init(&uart2_data);
}
#endif

void board_early_init()
{
    devices_init();
}

static void board_power_init(void)
{
    power_init(&power_param);
}

void board_init()
{
    board_power_init();
	adc_init();
}

