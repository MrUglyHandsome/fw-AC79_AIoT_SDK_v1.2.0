#ifndef CONFIG_BOARD_7913A_CFG_H
#define CONFIG_BOARD_7913A_CFG_H

//7913A0
#ifdef CONFIG_BOARD_7913A0
#define __FLASH_SIZE__    (2 * 1024 * 1024)
#define __SDRAM_SIZE__    (0)
#endif

//7913A6
#ifdef CONFIG_BOARD_7913A6
#define __FLASH_SIZE__    (2 * 1024 * 1024)//内置flash
#define __SDRAM_SIZE__    (0)
#endif

#if (defined CONFIG_BOARD_7913A0 || defined CONFIG_BOARD_7913A6)
#undef CONFIG_VIDEO_ENABLE
#undef CONFIG_VIDEO1_ENABLE

//7913AXX 有FUSB 无HUSB
#define TCFG_FUSB_EN		1				//1:FUSB使能,0:不使能
#define TCFG_HUSB_EN		0				//1:HUSB使能,0:不使能
#endif

#endif
