#include "device/iic.h"
#include "asm/isp_dev.h"
#include "gc032a.h"
#include "gpio.h"
#include "generic/jiffies.h"
#include "app_config.h"

#ifdef CONFIG_VIDEO_ENABLE

#define GC032A_8BIT 1
#define GC032A_2BIT 0


static void *iic = NULL;
static u8 GC032A_reset_io[2] = {-1, -1};
static u8 GC032A_power_io[2] = {-1, -1};


#if (CONFIG_VIDEO_IMAGE_W > 640)
#define GC032A_DEVP_INPUT_W 640
#define GC032A_DEVP_INPUT_H 480
#else
#define GC032A_DEVP_INPUT_W 	CONFIG_VIDEO_IMAGE_W
#define GC032A_DEVP_INPUT_H		CONFIG_VIDEO_IMAGE_H
#endif
#define GC032A_WRCMD 0x42
#define GC032A_RDCMD 0x43

#define CONFIG_INPUT_FPS	15
#define GC032A_8BIT_PLCK    48


struct reginfo {
    u8 reg;
    u8 val;
};
#define DELAY_TIME	10

#if GC032A_8BIT
static const struct reginfo sensor_init_data[] = {
    {0xf3, 0xff},
    {0xf5, 0x06},
    {0xf7, 0x01},
    {0xf8, 0x03},
    {0xf9, 0xce},
    {0xfa, 0x00},
    {0xfc, 0x02},
    {0xfe, 0x02},
    {0x81, 0x03},
    {0xfe, 0x00},
    {0x03, 0x01},
    {0x04, 0xce},
    {0x05, 0x01},
    {0x06, 0xad},
    {0x07, 0x00},
    {0x08, 0x10},
    {0x0a, 0x00}, //04
    {0x0c, 0x00}, //04
    {0x0d, 0x01},
    {0x0e, 0xe8},
    {0x0f, 0x02},
    {0x10, 0x88},
    {0x17, 0x54},
    {0x19, 0x08}, //04
    {0x1a, 0x0a},
    {0x1f, 0x40},
    {0x20, 0x30},
    {0x2e, 0x80},
    {0x2f, 0x2b},
    {0x30, 0x1a},
    {0xfe, 0x02},
    {0x03, 0x02},
    {0x05, 0xd7},
    {0x06, 0x60},
    {0x08, 0x80},
    {0x12, 0x89},
    {0xfe, 0x00},
    {0x18, 0x02},
    {0xfe, 0x02},
    {0x40, 0x22},
    {0x45, 0x00},
    {0x46, 0x00},
    {0x49, 0x20},
    {0x4b, 0x3c},
    {0x50, 0x20},
    {0x42, 0x10},
    {0xfe, 0x01},
    {0x0a, 0xc5},
    {0x45, 0x00},
    {0xfe, 0x00},
    {0x40, 0xff},
    {0x41, 0x25},
    {0x42, 0xcf},
    {0x43, 0x10},
    {0x44, 0x83},
    {0x46, 0x22},
    {0x49, 0x03},
    {0xfe, 0x02},
    {0x22, 0xf6},
    {0xfe, 0x01},
    {0x12, 0xa0},
    {0x13, 0x3a},
    {0xc1, 0x38}, //3c
    {0xc2, 0x4c}, //50
    {0xc3, 0x00},
    {0xc4, 0x32},
    {0xc5, 0x24},
    {0xc6, 0x16},
    {0xc7, 0x08},
    {0xc8, 0x08},
    {0xc9, 0x00},
    {0xca, 0x20},
    {0xdc, 0x8a},
    {0xdd, 0xa0},
    {0xde, 0xa6},
    {0xdf, 0x75},
    {0xfe, 0x01},
    {0x7c, 0x09},
    {0x65, 0x06},
    {0x7c, 0x08},
    {0x56, 0xf4},
    {0x66, 0x0f},
    {0x67, 0x84},
    {0x6b, 0x80},
    {0x6d, 0x12},
    {0x6e, 0xb0},
    {0x86, 0x00},
    {0x87, 0x00},
    {0x88, 0x00},
    {0x89, 0x00},
    {0x8a, 0x00},
    {0x8b, 0x00},
    {0x8c, 0x00},
    {0x8d, 0x00},
    {0x8e, 0x00},
    {0x8f, 0x00},
    {0x90, 0xef},
    {0x91, 0xe1},
    {0x92, 0x0c},
    {0x93, 0xef},
    {0x94, 0x65},
    {0x95, 0x1f},
    {0x96, 0x0c},
    {0x97, 0x2d},
    {0x98, 0x20},
    {0x99, 0xaa},
    {0x9a, 0x3f},
    {0x9b, 0x2c},
    {0x9c, 0x5f},
    {0x9d, 0x3e},
    {0x9e, 0xaa},
    {0x9f, 0x67},
    {0xa0, 0x60},
    {0xa1, 0x00},
    {0xa2, 0x00},
    {0xa3, 0x0a},
    {0xa4, 0xb6},
    {0xa5, 0xac},
    {0xa6, 0xc1},
    {0xa7, 0xac},
    {0xa8, 0x55},
    {0xa9, 0xc3},
    {0xaa, 0xa4},
    {0xab, 0xba},
    {0xac, 0xa8},
    {0xad, 0x55},
    {0xae, 0xc8},
    {0xaf, 0xb9},
    {0xb0, 0xd4},
    {0xb1, 0xc3},
    {0xb2, 0x55},
    {0xb3, 0xd8},
    {0xb4, 0xce},
    {0xb5, 0x00},
    {0xb6, 0x00},
    {0xb7, 0x05},
    {0xb8, 0xd6},
    {0xb9, 0x8c},
    {0xfe, 0x01},
    {0xd0, 0x40},
    {0xd1, 0xf8},
    {0xd2, 0x00},
    {0xd3, 0xfa},
    {0xd4, 0x45},
    {0xd5, 0x02},
    {0xd6, 0x30},
    {0xd7, 0xfa},
    {0xd8, 0x08},
    {0xd9, 0x08},
    {0xda, 0x58},
    {0xdb, 0x02},
    {0xfe, 0x00},
    {0xfe, 0x00},
    {0xba, 0x00},
    {0xbb, 0x04},
    {0xbc, 0x0a},
    {0xbd, 0x0e},
    {0xbe, 0x22},
    {0xbf, 0x30},
    {0xc0, 0x3d},
    {0xc1, 0x4a},
    {0xc2, 0x5d},
    {0xc3, 0x6b},
    {0xc4, 0x7a},
    {0xc5, 0x85},
    {0xc6, 0x90},
    {0xc7, 0xa5},
    {0xc8, 0xb5},
    {0xc9, 0xc2},
    {0xca, 0xcc},
    {0xcb, 0xd5},
    {0xcc, 0xde},
    {0xcd, 0xea},
    {0xce, 0xf5},
    {0xcf, 0xff},
    {0xfe, 0x00},
    {0x5a, 0x08},
    {0x5b, 0x0f},
    {0x5c, 0x15},
    {0x5d, 0x1c},
    {0x5e, 0x28},
    {0x5f, 0x36},
    {0x60, 0x45},
    {0x61, 0x51},
    {0x62, 0x6a},
    {0x63, 0x7d},
    {0x64, 0x8d},
    {0x65, 0x98},
    {0x66, 0xa2},
    {0x67, 0xb5},
    {0x68, 0xc3},
    {0x69, 0xcd},
    {0x6a, 0xd4},
    {0x6b, 0xdc},
    {0x6c, 0xe3},
    {0x6d, 0xf0},
    {0x6e, 0xf9},
    {0x6f, 0xff},
    {0xfe, 0x00},
    {0x70, 0x50},
    {0xfe, 0x00},
    {0x4f, 0x01},
    {0xfe, 0x01},
    {0x44, 0x04},
    {0x1f, 0x30},
    {0x20, 0x40},
    {0x26, 0x9a},
    {0x27, 0x01},
    {0x28, 0xce},
    {0x29, 0x03},
    {0x2a, 0x02},
    {0x2b, 0x04},
    {0x2c, 0x36},
    {0x2d, 0x07},
    {0x2e, 0xd2},
    {0x2f, 0x0b},
    {0x30, 0x6e},
    {0x31, 0x0e},
    {0x32, 0x70},
    {0x33, 0x12},
    {0x34, 0x0c},
    {0x3c, 0x30},
    {0x3e, 0x20},
    {0x3f, 0x2d},
    {0x40, 0x40},
    {0x41, 0x5b},
    {0x42, 0x82},
    {0x43, 0xb7},
    {0x04, 0x0a},
    {0x02, 0x79},
    {0x03, 0xc0},
    {0xcc, 0x08},
    {0xcd, 0x08},
    {0xce, 0xa4},
    {0xcf, 0xec},
    {0xfe, 0x00},
    {0x81, 0xb8},
    {0x82, 0x12},
    {0x83, 0x0a},
    {0x84, 0x01},
    {0x86, 0x50},
    {0x87, 0x18},
    {0x88, 0x10},
    {0x89, 0x70},
    {0x8a, 0x20},
    {0x8b, 0x10},
    {0x8c, 0x08},
    {0x8d, 0x0a},
    {0xfe, 0x00},
    {0x8f, 0xaa},
    {0x90, 0x9c},
    {0x91, 0x52},
    {0x92, 0x03},
    {0x93, 0x03},
    {0x94, 0x08},
    {0x95, 0x44},
    {0x97, 0x00},
    {0x98, 0x00},
    {0xfe, 0x00},
    {0xa1, 0x30},
    {0xa2, 0x41},
    {0xa4, 0x30},
    {0xa5, 0x20},
    {0xaa, 0x30},
    {0xac, 0x32},
    {0xfe, 0x00},
    {0xd1, 0x3c},
    {0xd2, 0x3c},
    {0xd3, 0x38},
    {0xd6, 0xf4},
    {0xd7, 0x1d},
    {0xdd, 0x73},
    {0xde, 0x84},
#if 1
    {0xfe, 0x00},
    {0xd3, 0x40}, //目标亮度
    {0xd1, 0x00},
    {0xd2, 0x00},
    {0X40, 0xed}, //关闭噪声使能
    {0X40, 0xfd}, //

#endif

};
#endif

#if GC032A_2BIT
static const struct reginfo sensor_init_data[] = {
    /*System*/
    {0xf3, 0x83},
    {0xf5, 0x0c},
    {0xf7, 0x01},

#if(PCLK == 24)
    {0xf8, 0x81}, //PLL 01//24Mhz-7fps
#elif(PCLK == 48)
    {0xf8, 0x83}, //PLL 03//48mhz-14fps
#else
    {0xf8, 0x81}, //PLL 01//default-24mhz
#endif

    {0xf9, 0x4e},
    {0xfa, 0x10},
    {0xfc, 0x02},
    {0xfe, 0x02},
    {0x81, 0x03},

    {0xfe, 0x00},
    {0x77, 0x64},
    {0x78, 0x40},
    {0x79, 0x60},

    /*Analog&Cisctl*/
    {0xfe, 0x00},
    {0x03, 0x01},
    {0x04, 0x20},
    {0x05, 0x01},
    {0x06, 0xaf},
    {0x07, 0x00},
    {0x08, 0x08},
    {0x0a, 0x00},
    {0x0c, 0x00},
    {0x0d, 0x01},
    {0x0e, 0xe8},
    {0x0f, 0x02},
    {0x10, 0x88},
    {0x17, 0x54},
    {0x19, 0x08},
    {0x1a, 0x0a},
    {0x1f, 0x40},
    {0x20, 0x30},
    {0x2e, 0x80},
    {0x2f, 0x2b},
    {0x30, 0x1a},
    {0xfe, 0x02},
    {0x03, 0x02},
    {0x06, 0x60},
    {0x05, 0xd7},
    {0x12, 0x89},

    /*SPI*/
    {0xfe, 0x03},
    {0x51, 0x01},
    {0x52, 0xd8},
    {0x53, 0x25}, //[7]crc [3:2]line_num
    {0x54, 0x20},
    {0x55, 0x00}, //20
    {0x59, 0x10},
    {0x5a, 0x00},

    //640*480
    {0x5b, 0x80},
    {0x5c, 0x02},
    {0x5d, 0xe0},
    {0x5e, 0x01},


    {0x64, 0x01}, //[1]sck always 06 04
    {0x65, 0xff}, //head sync code
    {0x66, 0x00},
    {0x67, 0x00},

    //SYNC code
    {0x60, 0xab}, //frame_start
    {0x61, 0x80}, //line sync start
    {0x62, 0x9d}, //line sync end
    {0x63, 0xb6}, //frame end */

    /*blk*/
    {0xfe, 0x00},
    {0x18, 0x02},

    {0xfe, 0x02},
    {0x40, 0x22},
    {0x45, 0x00},
    {0x46, 0x00},
    {0x49, 0x20},
    {0x4b, 0x3c},
    {0x50, 0x20},
    {0x42, 0x10},

    /*isp*/
    {0xfe, 0x01},
    {0x0a, 0xc5},
    {0x45, 0x00},
    {0xfe, 0x00},
    {0x40, 0xff},
    {0x41, 0x25},
    {0x42, 0xcf},
    {0x43, 0x10},
    {0x44, 0x83},
    {0x46, 0xff},
    {0x49, 0x03},
    {0x52, 0x02},
    {0x54, 0x00},
    {0xfe, 0x02},
    {0x22, 0xf6},

    /*Shading*/
    {0xfe, 0x01},
    {0xc1, 0x38},
    {0xc2, 0x4c},
    {0xc3, 0x00},
    {0xc4, 0x32},
    {0xc5, 0x24},
    {0xc6, 0x16},
    {0xc7, 0x08},
    {0xc8, 0x08},
    {0xc9, 0x00},
    {0xca, 0x20},
    {0xdc, 0x8a},
    {0xdd, 0xa0},
    {0xde, 0xa6},
    {0xdf, 0x75},

    /*AWB*/ /*20170110*/
    {0xfe, 0x01},
    {0x7c, 0x09},
    {0x65, 0x06},
    {0x7c, 0x08},
    {0x56, 0xf4},
    {0x66, 0x0f},
    {0x67, 0x84},
    {0x6b, 0x80},
    {0x6d, 0x12},
    {0x6e, 0xb0},
    {0xfe, 0x01},
    {0x90, 0x00},
    {0x91, 0x00},
    {0x92, 0xf4},
    {0x93, 0xd5},
    {0x95, 0x0f},
    {0x96, 0xf4},
    {0x97, 0x2d},
    {0x98, 0x0f},
    {0x9a, 0x2d},
    {0x9b, 0x0f},
    {0x9c, 0x59},
    {0x9d, 0x2d},
    {0x9f, 0x67},
    {0xa0, 0x59},
    {0xa1, 0x00},
    {0xa2, 0x00},
    {0x86, 0x00},
    {0x87, 0x00},
    {0x88, 0x00},
    {0x89, 0x00},
    {0xa4, 0x00},
    {0xa5, 0x00},
    {0xa6, 0xd4},
    {0xa7, 0x9f},
    {0xa9, 0xd4},
    {0xaa, 0x9f},
    {0xab, 0xac},
    {0xac, 0x9f},
    {0xae, 0xd4},
    {0xaf, 0xac},
    {0xb0, 0xd4},
    {0xb1, 0xa3},
    {0xb3, 0xd4},
    {0xb4, 0xac},
    {0xb5, 0x00},
    {0xb6, 0x00},
    {0x8b, 0x00},
    {0x8c, 0x00},
    {0x8d, 0x00},
    {0x8e, 0x00},
    {0x94, 0x50},
    {0x99, 0xa6},
    {0x9e, 0xaa},
    {0xa3, 0x0a},
    {0x8a, 0x00},
    {0xa8, 0x50},
    {0xad, 0x55},
    {0xb2, 0x55},
    {0xb7, 0x05},
    {0x8f, 0x00},
    {0xb8, 0xb3},
    {0xb9, 0xb6},

    /*CC*/
    {0xfe, 0x01},
    {0xd0, 0x40},
    {0xd1, 0xf8},
    {0xd2, 0x00},
    {0xd3, 0xfa},
    {0xd4, 0x45},
    {0xd5, 0x02},
    {0xd6, 0x30},
    {0xd7, 0xfa},
    {0xd8, 0x08},
    {0xd9, 0x08},
    {0xda, 0x58},
    {0xdb, 0x02},
    {0xfe, 0x00},

    /*Gamma*/
    {0xfe, 0x00},
    {0xba, 0x00},
    {0xbb, 0x04},
    {0xbc, 0x0a},
    {0xbd, 0x0e},
    {0xbe, 0x22},
    {0xbf, 0x30},
    {0xc0, 0x3d},
    {0xc1, 0x4a},
    {0xc2, 0x5d},
    {0xc3, 0x6b},
    {0xc4, 0x7a},
    {0xc5, 0x85},
    {0xc6, 0x90},
    {0xc7, 0xa5},
    {0xc8, 0xb5},
    {0xc9, 0xc2},
    {0xca, 0xcc},
    {0xcb, 0xd5},
    {0xcc, 0xde},
    {0xcd, 0xea},
    {0xce, 0xf5},
    {0xcf, 0xff},

    /*Auto Gamma*/
    {0xfe, 0x00},
    {0x5a, 0x08},
    {0x5b, 0x0f},
    {0x5c, 0x15},
    {0x5d, 0x1c},
    {0x5e, 0x28},
    {0x5f, 0x36},
    {0x60, 0x45},
    {0x61, 0x51},
    {0x62, 0x6a},
    {0x63, 0x7d},
    {0x64, 0x8d},
    {0x65, 0x98},
    {0x66, 0xa2},
    {0x67, 0xb5},
    {0x68, 0xc3},
    {0x69, 0xcd},
    {0x6a, 0xd4},
    {0x6b, 0xdc},
    {0x6c, 0xe3},
    {0x6d, 0xf0},
    {0x6e, 0xf9},
    {0x6f, 0xff},

    /*Gain*/
    {0xfe, 0x00},
    {0x70, 0x50},

    /*AEC*/
    {0xfe, 0x00},
    {0x4f, 0x01},
    {0xfe, 0x01},
    {0x0d, 0x00}, //08 add 20170110
    {0x12, 0xa0},
    {0x13, 0x3a},
    {0x44, 0x04},
    {0x1f, 0x30},
    {0x20, 0x40},

    {0x3e, 0x20},
    {0x3f, 0x2d},
    {0x40, 0x40},
    {0x41, 0x5b},
    {0x42, 0x82},
    {0x43, 0xb7},
    {0x04, 0x0a},
    {0x02, 0x79},
    {0x03, 0xc0},

    /*measure window*/
    {0xfe, 0x01},
    {0xcc, 0x08},
    {0xcd, 0x08},
    {0xce, 0xa4},
    {0xcf, 0xec},

    /*DNDD*/
    {0xfe, 0x00},
    {0x81, 0xb8},
    {0x82, 0x12},
    {0x83, 0x0a},
    {0x84, 0x01},
    {0x86, 0x50},
    {0x87, 0x18},
    {0x88, 0x10},
    {0x89, 0x70},
    {0x8a, 0x20},
    {0x8b, 0x10},
    {0x8c, 0x08},
    {0x8d, 0x0a},

    /*Intpee*/
    {0xfe, 0x00},
    {0x8f, 0xaa},
    {0x90, 0x9c},
    {0x91, 0x52},
    {0x92, 0x03},
    {0x93, 0x03},
    {0x94, 0x08},
    {0x95, 0x44},
    {0x97, 0x00},
    {0x98, 0x00},

    /*ASDE*/
    {0xfe, 0x00},
    {0xa1, 0x30},
    {0xa2, 0x41},
    {0xa4, 0x30},
    {0xa5, 0x20},
    {0xaa, 0x30},
    {0xac, 0x32},

    /*YCP*/
    {0xfe, 0x00},
    {0xd1, 0x3c},
    {0xd2, 0x3c},
    {0xd3, 0x38},
    {0xd6, 0xf4},
    {0xd7, 0x1d},
    {0xdd, 0x73},
    {0xde, 0x84},

    /*Banding*/

    {0xfe, 0x00},
    {0x05, 0x01}, //hb
    {0x06, 0xe4},
    {0x07, 0x00}, //vb
    {0x08, 0x08},

    {0xfe, 0x01},
    {0x25, 0x00}, //step
    {0x26, 0x24},

    {0x27, 0x01}, //7.1fps
    {0x28, 0xb0},
    {0x29, 0x01}, //7.1fps
    {0x2a, 0xb0},
    {0x2b, 0x01}, //7.1fps
    {0x2c, 0xb0},
    {0x2d, 0x01}, //7.1fps
    {0x2e, 0xb0},
    {0x2f, 0x02}, //6.24fps
    {0x30, 0x40},
    {0x31, 0x02}, //4.99fps
    {0x32, 0xd0},
    {0x33, 0x03}, //4.16fps
    {0x34, 0x60},
    {0x3c, 0x30},
    {0xfe, 0x00},
};
#endif

static s32 GC032A_set_output_size(u16 *width, u16 *height, u8 *freq);
static unsigned char wrGC032AReg(unsigned char regID, unsigned char regDat)
{
    u8 ret = 1;
    dev_ioctl(iic, IIC_IOCTL_START, 0);
    if (dev_ioctl(iic, IIC_IOCTL_TX_WITH_START_BIT, GC032A_WRCMD)) {
        ret = 0;
        printf("iic write err!!! line : %d \n", __LINE__);
        goto exit;
    }
    delay(DELAY_TIME);
    if (dev_ioctl(iic, IIC_IOCTL_TX, regID)) {
        ret = 0;
        printf("iic write err!!! line : %d \n", __LINE__);
        goto exit;
    }
    delay(DELAY_TIME);
    if (dev_ioctl(iic, IIC_IOCTL_TX_WITH_STOP_BIT, regDat)) {
        ret = 0;
        printf("iic write err!!! line : %d \n", __LINE__);
        goto exit;
    }
    delay(DELAY_TIME);

exit:
    dev_ioctl(iic, IIC_IOCTL_STOP, 0);
    return ret;
}

static unsigned char rdGC032AReg(unsigned char regID, unsigned char *regDat)
{
    u8 ret = 1;
    dev_ioctl(iic, IIC_IOCTL_START, 0);
    if (dev_ioctl(iic, IIC_IOCTL_TX_WITH_START_BIT, GC032A_WRCMD)) {
        ret = 0;
        printf("iic write err!!! line : %d \n", __LINE__);
        goto exit;
    }
    delay(DELAY_TIME);
    if (dev_ioctl(iic, IIC_IOCTL_TX_WITH_STOP_BIT, regID)) {
        ret = 0;
        printf("iic write err!!! line : %d \n", __LINE__);
        goto exit;
    }
    delay(DELAY_TIME);
    if (dev_ioctl(iic, IIC_IOCTL_TX_WITH_START_BIT, GC032A_RDCMD)) {
        ret = 0;
        printf("iic write err!!! line : %d \n", __LINE__);
        goto exit;
    }
    delay(DELAY_TIME);
    dev_ioctl(iic, IIC_IOCTL_RX_WITH_STOP_BIT, (u32)regDat);

exit:
    dev_ioctl(iic, IIC_IOCTL_STOP, 0);
    return ret;
}

static void GC032A_config_SENSOR(u16 *width, u16 *height, u8 *format, u8 *frame_freq)
{
    int i;
    for (i = 0; i < sizeof(sensor_init_data) / sizeof(sensor_init_data[0]); i++) {
        wrGC032AReg(sensor_init_data[i].reg, sensor_init_data[i].val);
    }
}
static s32 GC032A_set_output_size(u16 *width, u16 *height, u8 *freq)
{
    u16 liv_width = *width;
    u16 liv_height = *height;

    return 0;
}
static s32 GC032A_power_ctl(u8 isp_dev, u8 is_work)
{
    return 0;
}

static s32 GC032A_ID_check(void)
{
    int ret;
    u16 pid = 0x00;
    u8 id0, id1;
    ret = rdGC032AReg(0xf0, &id0);
    printf("GC032A Sensor ID : 0x%x\n", pid);
    rdGC032AReg(0xf1, &id1);
    pid = (u16)((id0 << 8) | id1);
    printf("GC032A DVP Sensor ID : 0x%x\n", pid);
    if (pid != 0x232a) {
        return -1;
    }

    return 0;
}

static void GC032A_powerio_ctl(u32 _power_gpio, u32 on_off)
{
    gpio_direction_output(_power_gpio, on_off);
}
static void GC032A_reset(u8 isp_dev)
{
    u8 res_io;
    u8 powd_io;
    u8 id = 0;
    puts("GC032A reset\n");

    if (isp_dev == ISP_DEV_0) {
        res_io = (u8)GC032A_reset_io[0];
        powd_io = (u8)GC032A_power_io[0];
    } else {
        res_io = (u8)GC032A_reset_io[1];
        powd_io = (u8)GC032A_power_io[1];
    }

    if (powd_io != (u8) - 1) {
        GC032A_powerio_ctl((u32)powd_io, 0);
    }
    if (res_io != (u8) - 1) {
        gpio_direction_output(res_io, 1);
        gpio_direction_output(res_io, 0);
        os_time_dly(1);
        gpio_direction_output(res_io, 1);
    }
}


static s32 GC032A_check(u8 isp_dev, u32 _reset_gpio, u32 _power_gpio)
{
    if (!iic) {
        if (isp_dev == ISP_DEV_0) {
            iic = dev_open("iic0", 0);
        } else {
            iic = dev_open("iic1", 0);
        }
        GC032A_reset_io[isp_dev] = (u8)_reset_gpio;
        GC032A_power_io[isp_dev] = (u8)_power_gpio;
    }
    if (iic == NULL) {
        printf("GC032A iic open err!!!\n\n");
        return -1;
    }
    GC032A_reset(isp_dev);

    if (0 != GC032A_ID_check()) {
        dev_close(iic);
        iic = NULL;
        printf("-------not GC032A------\n\n");
        return -1;
    }
    printf("-------hello GC032A------\n\n");
    return 0;
}


static s32 GC032A_init(u8 isp_dev, u16 *width, u16 *height, u8 *format, u8 *frame_freq)
{
    puts("\n\n GC032A \n\n");
    if (0 != GC032A_check(isp_dev, 0, 0)) {
        return -1;
    }
    GC032A_config_SENSOR(width, height, format, frame_freq);
    return 0;
}

void set_rev_sensor_GC032A(u16 rev_flag)
{
    /* if (!rev_flag) { */
    /* wrGC032AReg(0x1e, 0x30); */
    /* } else { */
    /* wrGC032AReg(0x1e, 0x00); */
    /* } */
}

u16 GC032A_dvp_rd_reg(u16 addr)
{
    u8 val;
    rdGC032AReg((u8)addr, &val);
    return val;
}

void GC032A_dvp_wr_reg(u16 addr, u16 val)
{
    wrGC032AReg((u8)addr, (u8)val);
}
// *INDENT-OFF*
REGISTER_CAMERA(GC032A) = {
    .logo 				= 	"GC032A",
    .isp_dev 			= 	ISP_DEV_NONE,
    .in_format 			= 	SEN_IN_FORMAT_YUYV,

#if GC032A_8BIT
    .mbus_type          =   SEN_MBUS_PARALLEL,
    .mbus_config        =   SEN_MBUS_DATA_WIDTH_8B  | SEN_MBUS_HSYNC_ACTIVE_HIGH | \
    						SEN_MBUS_PCLK_SAMPLE_FALLING | SEN_MBUS_VSYNC_ACTIVE_HIGH,
#if CONFIG_CAMERA_H_V_EXCHANGE
    .sync_config		=   SEN_MBUS_SYNC0_VSYNC_SYNC1_HSYNC,//WL82/AC791才可以H-V SYNC互换，请留意
#endif
#endif

#if GC032A_2BIT
    .mbus_type          =   SEN_MBUS_BT656,
    .mbus_config        =   SEN_MBUS_DATA_WIDTH_2B | SEN_MBUS_PCLK_SAMPLE_FALLING,
    .sync_config		=   0,
#endif

    .fps         		= 	CONFIG_INPUT_FPS,
    .out_fps			=   CONFIG_INPUT_FPS,
    .sen_size 			= 	{GC032A_DEVP_INPUT_W, GC032A_DEVP_INPUT_H},
    .cap_fps         	= 	CONFIG_INPUT_FPS,
    .sen_cap_size 		= 	{GC032A_DEVP_INPUT_W, GC032A_DEVP_INPUT_H},


    .ops                =   {
        .avin_fps           =   NULL,
        .avin_valid_signal  =   NULL,
        .avin_mode_det      =   NULL,
        .sensor_check 		= 	GC032A_check,
        .init 		        = 	GC032A_init,
        .set_size_fps 		=	GC032A_set_output_size,
        .power_ctrl         =   GC032A_power_ctl,


        .sleep 		        =	NULL,
        .wakeup 		    =	NULL,
        .write_reg 		    =	GC032A_dvp_wr_reg,
        .read_reg 		    =	GC032A_dvp_rd_reg,
        .set_sensor_reverse =   set_rev_sensor_GC032A,
    }
};
#endif
