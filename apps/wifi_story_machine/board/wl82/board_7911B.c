#include "app_config.h"

#ifdef CONFIG_BOARD_7911B
#include "app_power_manage.h"
#include "system/includes.h"
#include "device/includes.h"
#include "device/iic.h"
#include "server/audio_dev.h"
#include "asm/includes.h"
#if TCFG_USB_SLAVE_ENABLE || TCFG_USB_HOST_ENABLE
#include "otg.h"
#include "usb_host.h"
#include "usb_storage.h"
#endif

// *INDENT-OFF*

UART0_PLATFORM_DATA_BEGIN(uart0_data)
    .baudrate = 1000000,
    /* .port = PORTA_5_6, */
    .port = PORTA_3_4,
    /* .tx_pin = IO_PORTA_05, */
    /* .rx_pin = IO_PORTA_06, */
    .tx_pin = IO_PORTA_03,
    .rx_pin = IO_PORTA_04,
    .max_continue_recv_cnt = 1024,
    .idle_sys_clk_cnt = 500000,
    .clk_src = PLL_48M,
    .flags = UART_DEBUG,
UART0_PLATFORM_DATA_END();

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
#if CONFIG_AUDIO_ENC_SAMPLE_SOURCE == AUDIO_ENC_SAMPLE_SOURCE_PLNK0
    .output_channel = OUTPUT_CHANNEL1,
#else
    .output_channel = OUTPUT_CHANNEL0,
#endif
    .tx_pin = TCFG_DEBUG_PORT,
    .rx_pin = -1,
    .max_continue_recv_cnt = 1024,
    .idle_sys_clk_cnt = 500000,
    .clk_src = PLL_48M,
    .flags = UART_DEBUG,
UART2_PLATFORM_DATA_END();


/****************SD模块**********************************************************
 * 暂只支持1个SD外设，如需要多个SD外设，自行去掉下面SD0/1_ENABLE控制，另外定义即可
********************************************************************************/
#if TCFG_SD0_ENABLE
//sd0
#define SD_PLATFORM_DATA_BEGIN() SD0_PLATFORM_DATA_BEGIN(sd0_data)
#define SD_PLATFORM_DATA_END() SD0_PLATFORM_DATA_END()
#define SD_CLK_DETECT_FUNC		sdmmc_0_clk_detect
#elif TCFG_SD1_ENABLE
//sd1
#define SD_PLATFORM_DATA_BEGIN() SD1_PLATFORM_DATA_BEGIN(sd1_data)
#define SD_PLATFORM_DATA_END() SD1_PLATFORM_DATA_END()
#define SD_CLK_DETECT_FUNC		sdmmc_1_clk_detect
#endif

#if TCFG_SD0_ENABLE || TCFG_SD1_ENABLE

SD_PLATFORM_DATA_BEGIN()
    .port 					= TCFG_SD_PORTS,
    .priority 				= 3,
    .data_width 			= TCFG_SD_DAT_WIDTH,
	.speed 					= TCFG_SD_CLK,
    .detect_mode 			= TCFG_SD_DET_MODE,
#if (TCFG_SD_DET_MODE == SD_CLK_DECT)
    .detect_func 			= SD_CLK_DETECT_FUNC,
#elif (TCFG_SD_DET_MODE == SD_IO_DECT)
    .detect_func 			= sdmmc_0_io_detect,
    .detect_io              = TCFG_SD_DET_IO,
    .detect_io_level        = TCFG_SD_DET_IO_LEVEL,
#else
    .detect_func 			= NULL,
#endif
#if TCFG_SD_POWER_ENABLE
    .power                  = sd_set_power,
#endif
SD_PLATFORM_DATA_END()

#endif
/****************SD模块*********************/


HW_IIC0_PLATFORM_DATA_BEGIN(hw_iic1_data)
    .clk_pin = IO_PORTC_01,
    .dat_pin = IO_PORTC_02,
    .baudrate = 0x01,
HW_IIC0_PLATFORM_DATA_END()

SW_IIC_PLATFORM_DATA_BEGIN(sw_iic0_data)
    .clk_pin = IO_PORTC_01,
    .dat_pin = IO_PORTC_02,
    .sw_iic_delay = 50,
SW_IIC_PLATFORM_DATA_END()


#if TCFG_IRKEY_ENABLE
const struct irkey_platform_data irkey_data = {
    .enable = 1,
    .port = IO_PORTC_07,
};
#endif

#if TCFG_RDEC_KEY_ENABLE
const struct rdec_device rdeckey_list[] = {
    {
        .index = RDEC0 ,
        .sin_port0 = IO_PORTC_09,
        .sin_port1 = IO_PORTC_10,
        .key_value0 = KEY_VOLUME_DEC | BIT(7),
        .key_value1 = KEY_VOLUME_INC | BIT(7),
    },
};
const struct rdec_platform_data rdec_key_data = {
    .enable = 1,
    .num = ARRAY_SIZE(rdeckey_list),            //RDEC按键的个数
    .rdec = rdeckey_list,                       //RDEC按键参数表
};
#endif

#if TCFG_IOKEY_ENABLE
static const struct iokey_port iokey_list[] = {
    {
        .connect_way = ONE_PORT_TO_LOW,         //IO按键的连接方式
        .key_type.one_io.port = IO_PORTA_07,    //IO按键对应的引脚
        .key_value = KEY_UP,                    //按键值
    },
    {
        .connect_way = ONE_PORT_TO_LOW,
        .key_type.one_io.port = IO_PORTA_08,
        .key_value = KEY_DOWN,                  //按键值
    },
    {
        .connect_way = ONE_PORT_TO_LOW,
        .key_type.one_io.port = IO_PORTB_01,
        .key_value = KEY_MODE,                  //按键值
    },
};
const struct iokey_platform_data iokey_data = {
    .enable = 1,                              //是否使能IO按键
    .num = ARRAY_SIZE(iokey_list),            //IO按键的个数
    .port = iokey_list,                       //IO按键参数表
};
#endif

#if TCFG_TOUCH_KEY_ENABLE
static const struct touch_key_port plcnt_port[] = {
    {
        .port = IO_PORTC_09,
        .key_value = KEY_VOLUME_DEC,
    },
    {
        .port = IO_PORTC_10,
        .key_value = KEY_VOLUME_INC,
    },
};
const struct touch_key_platform_data touch_key_data = {
    .num            = ARRAY_SIZE(plcnt_port),
    .clock          = TOUCH_KEY_PLL_240M_CLK,
    .change_gain 	= 4,		//变化放大倍数, 一般固定
    .press_cfg		= -100,		//触摸按下灵敏度, 类型:s16, 数值越大, 灵敏度越高
    .release_cfg0 	= -50,		//触摸释放灵敏度0, 类型:s16, 数值越大, 灵敏度越高
    .release_cfg1 	= -80,		//触摸释放灵敏度1, 类型:s16, 数值越大, 灵敏度越高
    .port_list      = plcnt_port,
};
#endif

#if TCFG_CTMU_TOUCH_KEY_ENABLE
static const struct touch_key_port ctmu_port[] = {
    {
        .port = IO_PORTC_09,
        .key_value = KEY_VOLUME_DEC,
    },
    {
        .port = IO_PORTC_10,
        .key_value = KEY_VOLUME_INC,
    },
};
const struct touch_key_platform_data ctmkey_data = {
    .num            = ARRAY_SIZE(ctmu_port),
    .press_cfg		= -10,
    .release_cfg0 	= -50,
    .release_cfg1 	= -80,
    .port_list      = ctmu_port,
};
#endif

#if TCFG_ADKEY_ENABLE
/*-------------ADKEY GROUP 1----------------*/

#define ADKEY_UPLOAD_R  22

#define ADC_VDDIO (0x3FF)
#define ADC_09   (0x3FF * 220 / (220 + ADKEY_UPLOAD_R))
#define ADC_08   (0x3FF * 100 / (100 + ADKEY_UPLOAD_R))
#define ADC_07   (0x3FF * 51  / (51  + ADKEY_UPLOAD_R))
#define ADC_06   (0x3FF * 33  / (33  + ADKEY_UPLOAD_R))
#define ADC_05   (0x3FF * 22  / (22  + ADKEY_UPLOAD_R))
#define ADC_04   (0x3FF * 15  / (15  + ADKEY_UPLOAD_R))
#define ADC_03   (0x3FF * 10  / (10  + ADKEY_UPLOAD_R))
#define ADC_02   (0x3FF * 51  / (51  + ADKEY_UPLOAD_R * 10))
#define ADC_01   (0x3FF * 22  / (22  + ADKEY_UPLOAD_R * 10))
#define ADC_00   (0)

#define ADKEY_V_9      	((ADC_09 + ADC_VDDIO)/2)
#define ADKEY_V_8 		((ADC_08 + ADC_09)/2)
#define ADKEY_V_7 		((ADC_07 + ADC_08)/2)
#define ADKEY_V_6 		((ADC_06 + ADC_07)/2)
#define ADKEY_V_5 		((ADC_05 + ADC_06)/2)
#define ADKEY_V_4 		((ADC_04 + ADC_05)/2)
#define ADKEY_V_3 		((ADC_03 + ADC_04)/2)
#define ADKEY_V_2 		((ADC_02 + ADC_03)/2)
#define ADKEY_V_1 		((ADC_01 + ADC_02)/2)
#define ADKEY_V_0 		((ADC_00 + ADC_01)/2)

const struct adkey_platform_data adkey_data = {
    .enable     = 1,
    .adkey_pin  = IO_PORTB_01,
    .extern_up_en = 1,
    .ad_channel = 3,
    .ad_value = {
        ADKEY_V_0,
        ADKEY_V_1,
        ADKEY_V_2,
        ADKEY_V_3,
        ADKEY_V_4,
        ADKEY_V_5,
        ADKEY_V_6,
        ADKEY_V_7,
        ADKEY_V_8,
        ADKEY_V_9,
    },
    .key_value = {
        KEY_POWER, /*0*/
        KEY_PHOTO, /*1*/
        KEY_ENC,   /*2*/
        KEY_F1,    /*3*/
        KEY_MODE,  /*4*/
        KEY_CANCLE,/*5*/
        KEY_OK,    /*6*/
        KEY_DOWN,  /*7*/
        KEY_UP,    /*8*/
        NO_KEY,
    },
};
#endif


/*
 * spi0接falsh
 */
SPI0_PLATFORM_DATA_BEGIN(spi0_data)
    .clk    = 40000000,
    .mode   = SPI_DUAL_MODE,
    .port   = 'A',
    .attr	= SPI_SCLK_L_UPL_SMPH | SPI_UPDATE_SAMPLE_SAME,
SPI0_PLATFORM_DATA_END()

SPI1_PLATFORM_DATA_BEGIN(spi1_data)
    .clk    = 20000000,
    .mode   = SPI_STD_MODE,
    .port   = 'B',
    .attr	= SPI_SCLK_L_UPH_SMPH | SPI_BIDIR_MODE,
SPI1_PLATFORM_DATA_END()

SPI2_PLATFORM_DATA_BEGIN(spi2_data)
    .clk    = 20000000,
    .mode   = SPI_STD_MODE,
    .port   = 'B',
    .attr	= SPI_SCLK_L_UPH_SMPH | SPI_BIDIR_MODE,
SPI2_PLATFORM_DATA_END()

#ifdef CONFIG_AUDIO_ENABLE
static const struct dac_platform_data dac_data = {
    .pa_auto_mute = 1,
    .pa_mute_port = TCFG_DAC_MUTE_PORT,
    .pa_mute_value = TCFG_DAC_MUTE_VALUE,
    .differ_output = 0,
    .hw_channel = 0x01,
    .ch_num = 1,	//差分只需开一个通道
    .vcm_init_delay_ms = 1000,
#ifdef CONFIG_DEC_ANALOG_VOLUME_ENABLE
    .fade_enable = 1,
    .fade_delay_ms = 10,
#endif
};
static const struct adc_platform_data adc_data = {
    .mic_channel = TCFG_MIC_CHANNEL_MAP,
    .linein_channel = TCFG_LINEIN_CHANNEL_MAP,
    .mic_ch_num = TCFG_MIC_CHANNEL_NUM,
    .linein_ch_num = TCFG_LINEIN_CHANNEL_NUM,
    .all_channel_open = 1,
    .isel = 2,
    .dump_num = 320,
};
static const struct iis_platform_data iis0_data = {
    .channel_in = 0,
    .channel_out = BIT(0),
    .port_sel = IIS_PORTC,
    .data_width = 0,
    .mclk_output = 0,
    .slave_mode = 0,
};
static void plnk0_port_remap_cb(void)
{
    extern int gpio_plnk_rx_input(u32 gpio, u8 index, u8 data_sel);
    gpio_plnk_rx_input(IO_PORTA_02, 0, 0);
    gpio_output_channle(IO_PORTA_01, CH0_PLNK0_SCLK_OUT);	//SCLK0使用outputchannel0
    JL_IOMAP->CON3 |= BIT(18);
}
static void plnk0_port_unremap_cb(void)
{
    JL_IOMAP->CON3 &= ~BIT(18);
    gpio_clear_output_channle(IO_PORTA_01, CH0_PLNK0_SCLK_OUT);
    gpio_set_die(IO_PORTA_02, 0);
}
//plnk的时钟和数据引脚都采用重映射的使用例子
static const struct plnk_platform_data plnk0_data = {
    .hw_channel = PLNK_CH_MIC_L,
    .clk_out = 1,
    .port_remap_cb = plnk0_port_remap_cb,
    .port_unremap_cb = plnk0_port_unremap_cb,
    .sample_edge = 0,   //在CLK的下降沿采样左MIC
    .share_data_io = 1, //两个数字MIC共用一个DAT脚
    .high_gain = 1,
    .dc_cancelling_filter = 14,
    .dump_points_num = 640,
};
static const struct audio_pf_data audio_pf_d = {
    .adc_pf_data = &adc_data,
    .dac_pf_data = &dac_data,
    .iis0_pf_data = &iis0_data,
    .plnk0_pf_data = &plnk0_data,
};
static const struct audio_platform_data audio_data = {
    .private_data = (void *) &audio_pf_d,
};
#endif


#ifdef CONFIG_VIDEO_ENABLE
#define CAMERA_GROUP_PORT	ISC_GROUPA
/* #define CAMERA_GROUP_PORT	ISC_GROUPC */
static const struct camera_platform_data camera0_data = {
    .xclk_gpio      = IO_PORTC_00,//注意： 如果硬件xclk接到芯片IO，则会占用OUTPUT_CHANNEL1
    .reset_gpio     = -1,
    .online_detect  = NULL,
    .pwdn_gpio      = -1,
    .power_value    = 0,
    .interface      = SEN_INTERFACE0,//SEN_INTERFACE_CSI2,
    .dvp = {
#if (CAMERA_GROUP_PORT == ISC_GROUPA)
        .pclk_gpio   = IO_PORTA_08,
        .hsync_gpio  = IO_PORTA_09,
        .vsync_gpio  = IO_PORTA_10,
#else
        .pclk_gpio   = IO_PORTC_08,
        .hsync_gpio  = IO_PORTC_09,
        .vsync_gpio  = IO_PORTC_10,
#endif
		.group_port  = CAMERA_GROUP_PORT,
        .data_gpio = {
#if (CAMERA_GROUP_PORT == ISC_GROUPA)
               IO_PORTA_07,
               IO_PORTA_06,
               IO_PORTA_05,
               IO_PORTA_04,
               IO_PORTA_03,
               IO_PORTA_02,
               IO_PORTA_01,
               IO_PORTA_00,
               -1,
               -1,
#else
               IO_PORTC_07,
               IO_PORTC_06,
               IO_PORTC_05,
               IO_PORTC_04,
               IO_PORTC_03,
               IO_PORTC_02,
               IO_PORTC_01,
               IO_PORTC_00,
               -1,
               -1,
#endif
        },
    }
};
static const struct video_subdevice_data video0_subdev_data[] = {
    { VIDEO_TAG_CAMERA, (void *)&camera0_data },
};
static const struct video_platform_data video0_data = {
    .data = video0_subdev_data,
    .num = ARRAY_SIZE(video0_subdev_data),
};
#endif

PWM_PLATFORM_DATA_BEGIN(pwm_data0)
    .port   = IO_PORTC_01,
    .pwm_ch = PWM_TIMER3_OPCH3 | PWMCH4_L,//初始化可选多通道,如:PWMCH0_H | PWMCH0_L | PWMCH1_H ... | PWMCH7_H | PWMCH7_L | PWM_TIMER2_OPCH2 | PWM_TIMER3_OPCH3 ,
    .freq   = 200,//频率
    .duty   = 90,//占空比
    .point_bit = 0,//根据point_bit值调节占空比小数点精度位: 0<freq<=4K,point_bit=2;4K<freq<=40K,point_bit=1; freq>40K,point_bit=0;
PWM_PLATFORM_DATA_END()

PWM_PLATFORM_DATA_BEGIN(pwm_data1)
    .port   = IO_PORTC_06,//选择定时器的TIMER PWM任意IO，pwm_ch加上PWM_TIMER3_OPCH3或PWM_TIMER2_OPCH2有效,只支持2个PWM,占用output_channel2/3，其他外设使用output_channel需留意
    .pwm_ch = PWM_TIMER2_OPCH2,//初始化可选多通道,如:PWMCH0_H | PWMCH0_L | PWMCH1_H ... | PWMCH7_H | PWMCH7_L | PWM_TIMER2_OPCH2 | PWM_TIMER3_OPCH3 ,
    .freq   = 32768,//频率
    .duty   = 50,//占空比
    .point_bit = 0,//根据point_bit值调节占空比小数点精度位: 0<freq<=4K,point_bit=2;4K<freq<=40K,point_bit=1; freq>40K,point_bit=0;
PWM_PLATFORM_DATA_END()


#if TCFG_USB_SLAVE_ENABLE || TCFG_USB_HOST_ENABLE
static const struct otg_dev_data otg_data = {
#ifdef CONFIG_ALL_USB_ENABLE
    .usb_dev_en = 0x03,
#else
    .usb_dev_en = 0x01,
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

#if TCFG_VIR_UDISK_ENABLE
extern const struct device_operations ram_disk_dev_ops;
#endif
#endif


#if defined CONFIG_BT_ENABLE || defined CONFIG_WIFI_ENABLE
#include "wifi/wifi_connect.h"
const struct wifi_calibration_param wifi_calibration_param = {
    .xosc_l     = 0xb,// 调节左晶振电容
    .xosc_r     = 0xb,// 调节右晶振电容
    .pa_trim_data = {1, 7, 4, 7, 11, 1, 7},// 根据MP测试生成PA TRIM值
	.mcs_dgain    = {
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

#ifdef CONFIG_RTC_ENABLE
const struct rtc_init_config rtc_init_data = {
    .rtc_clk_sel = RTC_CLK_SEL_INSIDE,
    .rtc_power_sel = RTCVDD_SUPPLY_OUTSIDE,
};
#endif

#ifdef TCFG_GPCNT_ENABLE
#include "gpcnt.h"
const struct gpcnt_platform_data gpcnt_data = {
#if (defined RF_FCC_TEST_ENABLE && CONFIG_RF_FCC_TRIGGER_MODE == GPCNT_TRIGGER_MODE)
    .gpcnt_gpio = CONFIG_RF_FCC_TRIGGER_GPCNT_PORT,
#else
    .gpcnt_gpio = IO_PORTC_01,
#endif
    .gss_clk_source = GPCNT_PLL_CLK,//480M
    .ch_source  = GPCNT_INPUT_CHANNEL1,
    .cycle      = CYCLE_15,
};
#endif


REGISTER_DEVICES(device_table) = {
    { "pwm0",   &pwm_dev_ops,  (void *)&pwm_data0},
    { "pwm1",   &pwm_dev_ops,  (void *)&pwm_data1},

    { "iic0",  &iic_dev_ops, (void *)&sw_iic0_data },
    { "iic1",  &iic_dev_ops, (void *)&hw_iic1_data },

#ifdef CONFIG_AUDIO_ENABLE
    { "audio", &audio_dev_ops, (void *)&audio_data },
#endif

#if TCFG_SD0_ENABLE
   	{ "sd0",  &sd_dev_ops, (void *)&sd0_data },
#endif

#if TCFG_SD1_ENABLE
    { "sd1",  &sd_dev_ops, (void *)&sd1_data },
#endif

#ifdef CONFIG_VIDEO_ENABLE
    { "video0.*",  &video_dev_ops, (void *)&video0_data },
#endif

    { "spi1", &spi_dev_ops, (void *)&spi1_data },
    { "spi2", &spi_dev_ops, (void *)&spi2_data },

    {"rtc", &rtc_dev_ops, NULL},

    {"uart0", &uart_dev_ops, (void *)&uart0_data },
    {"uart1", &uart_dev_ops, (void *)&uart1_data },
    {"uart2", &uart_dev_ops, (void *)&uart2_data },

#if TCFG_USB_SLAVE_ENABLE || TCFG_USB_HOST_ENABLE
    { "otg", &usb_dev_ops, (void *)&otg_data},
#endif
#if TCFG_UDISK_ENABLE
    { "udisk0", &mass_storage_ops, NULL },
#endif
#if TCFG_VIR_UDISK_ENABLE
    {"vir_udisk",  &ram_disk_dev_ops, NULL},
#endif

#ifdef TCFG_GPCNT_ENABLE
    {"gpcnt", &gpcnt_dev_ops, (void *) &gpcnt_data },
#endif
};


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

/************************** PWR config ****************************/
//#define PORT_VCC33_CTRL_IO		IO_PORTA_03					//VCC33 DCDC控制引脚,该引脚控制DCDC器件输出的3.3V连接芯片HPVDD、VDDIO、VDD33
#define PORT_WAKEUP_IO			IO_PORTB_01					//软关机和休眠唤醒引脚
#define PORT_WAKEUP_NUM			(PORT_WAKEUP_IO/IO_GROUP_NUM)//默认:0-7:GPIOA-GPIOH, 可以指定0-7组

static const struct port_wakeup port0 = {
    .edge       = FALLING_EDGE,                            //唤醒方式选择,可选：上升沿\下降沿
    .attribute  = BLUETOOTH_RESUME,                        //保留参数
    .iomap      = PORT_WAKEUP_IO,                          //唤醒口选择
    .low_power	= POWER_SLEEP_WAKEUP | POWER_OFF_WAKEUP,   //低功耗IO唤醒,不需要写0
};

static const struct long_press lpres_port = {
    .enable 	= FALSE,
    .use_sec4 	= TRUE,										//enable = TRUE , use_sec4: TRUE --> 4 sec , FALSE --> 8 sec
    .edge		= FALLING_EDGE,								//长按方式,可选：FALLING_EDGE /  RISING_EDGE --> 低电平/高电平
    .iomap 		= PORT_WAKEUP_IO,							//长按复位IO和IO唤醒共用一个IO
};

static const struct sub_wakeup sub_wkup = {
    .attribute  = BLUETOOTH_RESUME,
};

static const struct charge_wakeup charge_wkup = {
    .attribute  = BLUETOOTH_RESUME,
};

static const struct wakeup_param wk_param = {
    .port[PORT_WAKEUP_NUM] = &port0,
    .sub = &sub_wkup,
    .charge = &charge_wkup,
    .lpres = &lpres_port,
};

#ifdef CONFIG_PRESS_LONG_KEY_POWERON
//ad按键和开关机键复用
static unsigned int read_power_key(int dly)
{
    gpio_latch_en(adkey_data.adkey_pin, 0);
    gpio_direction_input(adkey_data.adkey_pin);
    gpio_set_die(adkey_data.adkey_pin, 0);
    gpio_set_pull_up(adkey_data.adkey_pin, 0);
    gpio_set_pull_down(adkey_data.adkey_pin, 0);

    if (dly) {
        delay_us(3000);
    }

    JL_ADC->CON = (0xf << 12) | (adkey_data.ad_channel << 8) | (1 << 6) | (1 << 4) | (1 << 3) | (6 << 0);
    while (!(JL_ADC->CON & BIT(7)));
    return (JL_ADC->RES < 50);
}

void sys_power_poweroff_wait_powerkey_up(void)
{
    JL_TIMER1->CON = 0;
    JL_TIMER1->CON = 0;
    JL_TIMER1->CON = 0;
    JL_ADC->CON = 0;
    JL_ADC->CON = 0;
    JL_ADC->CON = 0;
    while (read_power_key(1));
}
#endif

/*进软关机之前默认将IO口都设置成高阻状态，需要保留原来状态的请修改该函数*/
static void board_set_soft_poweroff(void)
{
    u32 IO_PORT;
    JL_PORT_FLASH_TypeDef *gpio[] = {JL_PORTA, JL_PORTB, JL_PORTC, JL_PORTD, JL_PORTE, JL_PORTF, JL_PORTG, JL_PORTH};

    for (u8 p = 0; p < 8; ++p) {
        //flash sdram PD PE PF PG口不能进行配置,由内部完成控制
        if (gpio[p] == JL_PORTD || gpio[p] == JL_PORTE || gpio[p] == JL_PORTF || gpio[p] == JL_PORTG) {
            continue;
        }
        for (u8 i = 0; i < 16; ++i) {
            IO_PORT = IO_PORTA_00 + p * 16 + i;
            gpio_set_pull_up(IO_PORT, 0);
            gpio_set_pull_down(IO_PORT, 0);
            gpio_set_direction(IO_PORT, 1);
            gpio_set_die(IO_PORT, 0);
            gpio_set_dieh(IO_PORT, 0);
            gpio_set_hd(IO_PORT, 0);
            gpio_set_hd1(IO_PORT, 0);
            gpio_latch_en(IO_PORT, 1);
        }
    }
#ifdef PORT_VCC33_CTRL_IO
    gpio_latch_en(PORT_VCC33_CTRL_IO, 0);
    gpio_direction_output(PORT_VCC33_CTRL_IO, 0);
    gpio_set_pull_up(PORT_VCC33_CTRL_IO, 0);
    gpio_set_pull_down(PORT_VCC33_CTRL_IO, 1);
    gpio_set_direction(PORT_VCC33_CTRL_IO, 1);
    gpio_set_die(PORT_VCC33_CTRL_IO, 0);
    gpio_set_dieh(PORT_VCC33_CTRL_IO, 0);
    gpio_latch_en(PORT_VCC33_CTRL_IO, 1);
#endif
}

static void sleep_exit_callback(u32 usec)
{
#ifdef PORT_VCC33_CTRL_IO
    gpio_direction_output(PORT_VCC33_CTRL_IO, 1);
    gpio_set_pull_up(PORT_VCC33_CTRL_IO, 1);
    gpio_set_pull_down(PORT_VCC33_CTRL_IO,0);
#endif
    /* putchar('-'); */
}

static void sleep_enter_callback(u8  step)
{
    /* 此函数禁止添加打印 */
    if (step == 1) {
        dac_power_off();
    } else {
#ifdef PORT_VCC33_CTRL_IO
        gpio_direction_output(PORT_VCC33_CTRL_IO, 0);
        gpio_set_pull_up(PORT_VCC33_CTRL_IO, 0);
        gpio_set_pull_down(PORT_VCC33_CTRL_IO, 1);
        gpio_set_direction(PORT_VCC33_CTRL_IO, 1);
        gpio_set_die(PORT_VCC33_CTRL_IO, 0);
#endif
	}
}

static void board_power_init(void)
{
#ifdef PORT_VCC33_CTRL_IO
    gpio_direction_output(PORT_VCC33_CTRL_IO, 1);
    gpio_set_pull_up(PORT_VCC33_CTRL_IO, 1);
    gpio_set_pull_down(PORT_VCC33_CTRL_IO,0);
#endif

    power_init(&power_param);

    power_set_callback(TCFG_LOWPOWER_LOWPOWER_SEL, sleep_enter_callback, sleep_exit_callback, board_set_soft_poweroff);

#ifdef CONFIG_AUDIO_ENABLE
    power_keep_state(POWER_KEEP_RESET | POWER_KEEP_DACVDD);//0, POWER_KEEP_DACVDD | POWER_KEEP_RTC | POWER_KEEP_RESET
#else
    power_keep_state(POWER_KEEP_RESET);//0, POWER_KEEP_DACVDD | POWER_KEEP_RTC | POWER_KEEP_RESET
#endif

#ifdef CONFIG_RTC_ENABLE
    power_keep_state(POWER_KEEP_RTC);//0, POWER_KEEP_DACVDD | POWER_KEEP_RTC | POWER_KEEP_RESET
#endif

    power_wakeup_init(&wk_param);

#ifdef CONFIG_PRESS_LONG_KEY_POWERON
    if (system_reset_reason_get() == SYS_RST_PORT_WKUP) {
        for (int i = 0; i < 500; i++) {
            if (0 == read_power_key(1)) {
                sys_power_poweroff();
                break;
            }
        }
    }
#endif
    extern void clean_wakeup_source_port(void);
    clean_wakeup_source_port();
}

#ifdef CONFIG_DEBUG_ENABLE
void debug_uart_init()
{
    uart_init(&uart2_data);
}
#endif

void board_early_init()
{
#ifdef CONFIG_AUDIO_ENABLE
    dac_early_init(0, dac_data.differ_output ? (dac_data.ch_num > 1 ? 0xf : 0x3): dac_data.hw_channel, 1000);
#endif
    devices_init();
}

void board_init()
{
    board_power_init();
#ifdef CONFIG_RTC_ENABLE
    rtc_early_init();
#endif
#if TCFG_ADKEY_ENABLE || (defined CONFIG_BT_ENABLE || defined CONFIG_WIFI_ENABLE)
    adc_init();
#endif
    key_driver_init();
#ifdef CONFIG_AUTO_SHUTDOWN_ENABLE
    sys_power_init();
#endif
#ifdef CONFIG_BT_ENABLE
    void cfg_file_parse(void);
    cfg_file_parse();
#endif
}

#endif
