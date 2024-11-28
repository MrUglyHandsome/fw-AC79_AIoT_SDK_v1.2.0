#ifndef CONFIG_BOARD_7915B_CFG_H
#define CONFIG_BOARD_7915B_CFG_H


//7915B6A
#ifdef CONFIG_BOARD_7915B6A
#define __FLASH_SIZE__    (2 * 1024 * 1024)//内置flash
#define __SDRAM_SIZE__    (2 * 1024 * 1024)
#endif

#if (defined CONFIG_BOARD_7915B6A)
//7915BXX 有FUSB 无HUSB
#define TCFG_FUSB_EN		1				//1:FUSB使能,0:不使能
#define TCFG_HUSB_EN		0				//1:HUSB使能,0:不使能
#endif

#endif
