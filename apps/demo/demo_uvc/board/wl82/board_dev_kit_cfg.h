#ifdef CONFIG_BOARD_DEV_KIT

#define TCFG_SD0_ENABLE		1				//SD卡选择
#define TCFG_ADKEY_ENABLE   1           	//AD按键
#define TCFG_DEBUG_PORT		IO_PORTB_03 	//调试打印IO
//#define CONFIG_PRESS_LONG_KEY_POWERON     	//长按开关机功能

//*********************************************************************************//
//                         video摄像头配置                                         //
//*********************************************************************************//
//#define CONFIG_VIDEO_ENABLE
//#define CONFIG_VIDEO1_ENABLE 	//spi video1

/*video0的IO组和硬件IO */
#define CONFIG_CAMERA_H_V_EXCHANGE		1	//vsync和hsync互换（如芯片IO的v和h-sync和镜头的v和h-sync反接，则为1）
#define CAMERA_GROUP_PORT				ISC_GROUPA	//ISC_GROUPC //摄像头数据在PA还是PC口
#define TCFG_VIDEO0_CAMERA_XCLOCK		IO_PORTH_02
#define TCFG_VIDEO0_CAMERA_RESET		IO_PORTH_03
#define TCFG_VIDEO0_CAMERA_PWDN			-1
#define TCFG_IIC0_CLK					IO_PORTH_00
#define TCFG_IIC0_DAT					IO_PORTH_01

/*video1的IO组和硬件IO */
#ifdef CONFIG_VIDEO1_ENABLE
#define CONFIG_SPI_VIDEO_ENABLE			//SPI协议下的video(可以使用spi接收，也使用ISC1接收(BT656))
#define CONFIG_SPI_VIDEO_USE_ISC1_RECV	0 //1:使用ISC1-BT656接收(收到YUV420P),0:使用SPI接收(需要软件处理YUYV数据)
#define CONFIG_SPI_ONE_LINE_ENABLE		0 //1:单线spi镜头使能，0则使用双线

/*
 * 使用get_yuv_data.c函数获取YUV的配置:CONFIG_SPI_USE_GET_YUV_C_ENABLE
 * 注意：使用bt656_camera_get_yuv.c或spi_video.c获取YUV，则硬件IO在对应的文件前100行配置
 * 注意：get_yuv_data.c获取video1的YUV，则video0摄像头无法使用(如APP不能出图video0)
 * 注意：使用get_yuv_data.c函数获取YUV，则摄像头的的IO配置在video0组，即TCFG_VIDEO0_CAMERA_XXX
 */
//#define CONFIG_SPI_USE_GET_YUV_C_ENABLE	//video1使用get_yuv_data.c函数获取YUV，注释则使用(bt656_camera_get_yuv.c或spi_video.c获取YUV)

#define CAMERA1_GROUP_PORT				0
#define TCFG_VIDEO1_CAMERA_XCLOCK		IO_PORTH_02
#define TCFG_VIDEO1_CAMERA_RESET		IO_PORTH_03
#define TCFG_VIDEO1_CAMERA_PWDN			-1
#define TCFG_IIC1_CLK					IO_PORTH_00
#define TCFG_IIC1_DAT					IO_PORTH_01

#if (CONFIG_SPI_VIDEO_USE_ISC1_RECV == 0) /*SPI接收则需要配置SPI的极性，不同镜头不一样!!!!*/
//不同的SPI镜头，SPI采样边缘不一样，需要改SPI_SCLK_X_UPX_SMPX/采样边缘（主要包括：从机，CLK低, 更新数据边缘，单向模式）
#define TCFG_GC0310_SPI_VIDEO_ATTR		(SPI_MODE_SLAVE | SPI_SCLK_L_UPL_SMPL | SPI_UNIDIR_MODE)
#define TCFG_BYD20A6_SPI_VIDEO_ATTR		(SPI_MODE_SLAVE | SPI_SCLK_L_UPL_SMPH | SPI_UNIDIR_MODE)
#define TCFG_BYD3901_SPI_VIDEO_ATTR		(SPI_MODE_SLAVE | SPI_SCLK_H_UPL_SMPH | SPI_UNIDIR_MODE)
#define TCFG_SPI_ATTR					TCFG_GC0310_SPI_VIDEO_ATTR
/*#define TCFG_SPI_ATTR					TCFG_BYD20A6_SPI_VIDEO_ATTR*/
/*#define TCFG_SPI_ATTR					TCFG_GC0310_SPI_VIDEO_ATTR*/
#endif
#endif

//*********************************************************************************//
//                        SD 配置（暂只支持打开一个SD外设）                        //
//*********************************************************************************//
//SD0 	cmd,  clk,  data0, data1, data2, data3
//A     PB6   PB7   PB5    PB5    PB3    PB2
//B     PA7   PA8   PA9    PA10   PA5    PA6
//C     PH1   PH2   PH0    PH3    PH4    PH5
//D     PC9   PC10  PC8    PC7    PC6    PC5

//SD1 	cmd,  clk,  data0, data1, data2, data3
//A     PH6   PH7   PH5    PH4    PH3    PH2
//B     PC0   PC1   PC2    PC3    PC4    PC5
#define TCFG_SD_PORTS                       'A'			//SD0/SD1的ABCD组(默认为开发板SD0-D),注意:IO占用问题
#define TCFG_SD_DAT_WIDTH                   1			//1:单线模式, 4:四线模式
#define TCFG_SD_DET_MODE                    SD_CMD_DECT	//检测模式:命令检测，时钟检测，IO检测
#define TCFG_SD_DET_IO                      -1	//SD_DET_MODE为SD_IO_DECT时有效
#define TCFG_SD_DET_IO_LEVEL                0			//IO检卡上线的电平(0/1),SD_DET_MODE为SD_IO_DECT时有效
#define TCFG_SD_CLK                         24000000		//SD时钟


//*********************************************************************************//
//                            AUDIO通道配置                                        //
//*********************************************************************************//
#define CONFIG_AUDIO_ENC_SAMPLE_SOURCE      AUDIO_ENC_SAMPLE_SOURCE_MIC
// #define CONFIG_AUDIO_ENC_SAMPLE_SOURCE      AUDIO_ENC_SAMPLE_SOURCE_PLNK0

#define TCFG_DAC_MUTE_PORT                  IO_PORTB_02
#define TCFG_DAC_MUTE_VALUE                 0 //0/1

#if CONFIG_AUDIO_ENC_SAMPLE_SOURCE == AUDIO_ENC_SAMPLE_SOURCE_PLNK0
#define TCFG_MIC_CHANNEL_MAP                LADC_CH_MIC3_P_N
#define TCFG_MIC_CHANNEL_NUM                1
#else
#define TCFG_MIC_CHANNEL_MAP                (LADC_CH_MIC1_P_N | LADC_CH_MIC3_P_N)
#define TCFG_MIC_CHANNEL_NUM                2
#endif
#define TCFG_LINEIN_CHANNEL_MAP             (LADC_CH_AUX1 | LADC_CH_AUX3)
#define TCFG_LINEIN_CHANNEL_NUM             2


#ifndef TCFG_FUSB_EN
#define TCFG_FUSB_EN		1 //没有使能则默认开启FUSB
#endif
#ifndef TCFG_HUSB_EN
#define TCFG_HUSB_EN		0 //没有使能则默认关闭HUSB
#endif

#endif
