#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/*********************************************************************************************************************
*                                          用户：芯片和板子型号配置                                                  *
**********************************************************************************************************************
* 注意：芯片0M-sdram使用说明如下：                                                                                   *
* 使用到lwip库则需要更换lwip_xxx_sfc.a，wifi库需使用wl_xxx_sfc.a，加上CONFIG_NO_SDRAM_ENABLE，删除CONFIG_UI_ENABLE   *
* 方法如下：                                                                                                         *
* （1）windows使用codeblock的cbp工程文件，则在工程#define选项加上CONFIG_NO_SDRAM_ENABLE，同时删除CONFIG_UI_ENABLE，  *
*      另外在linker settings选项把wifi和lwip库换带lwip_xxx_sfc.a和wl_xxx_sfc.a库                                     *
* （2）linux使用makefile，则在app_cfg.mk打开CONFIG_NO_SDRAM_ENABLE = y，CONFIG_UI_ENABLE = n                         *
*********************************************************************************************************************/

//芯片型号
// #define CONFIG_BOARD_7911BA		// 2M-sdram，外置flash
// #define CONFIG_BOARD_7911BB		// 8M-sdram，外置flash
// #define CONFIG_BOARD_7911B0 		// 0M-sdram，外置flash
// #define CONFIG_BOARD_7911B8		// 0M-sdram，内置1M-flash
// #define CONFIG_BOARD_7912AB		// 8M-sdram，外置flash
// #define CONFIG_BOARD_7913A0		// 0M-sdram，外置flash
// #define CONFIG_BOARD_7913A6		// 2M-sdram，内置2M-flash
// #define CONFIG_BOARD_7915AA		// 2M-sdram，外置flash
// #define CONFIG_BOARD_7915B6A		// 8M-sdram，内置2M-flash
#define CONFIG_BOARD_7916AB		// 8M-sdram，外置flash

//板子型号
// #define CONFIG_BOARD_DEVELOP		// 开发板
#define CONFIG_BOARD_DEV_KIT		// 开源版



//芯片型号sdram和flash配置文件
#include "board_7911B_cfg.h"
#include "board_7912A_cfg.h"
#include "board_7913A_cfg.h"
#include "board_7915A_cfg.h"
#include "board_7915B_cfg.h"
#include "board_7916A_cfg.h"

//不同板子外设配置文件，如有新的板子，在这里同理添加
#include "board_dev_kit_cfg.h" //开源版
#include "board_develop_cfg.h" //开发板



#if (__SDRAM_SIZE__ == 0)
#ifndef CONFIG_NO_SDRAM_ENABLE
#define CONFIG_NO_SDRAM_ENABLE
#endif
#endif

#endif
