#ifndef CONFIG_BOARD_7915A_CFG_H
#define CONFIG_BOARD_7915A_CFG_H

//7915AA
#ifdef CONFIG_BOARD_7915AA
#define __FLASH_SIZE__    (2 * 1024 * 1024)
#define __SDRAM_SIZE__    (2 * 1024 * 1024)
#endif

#if (defined CONFIG_BOARD_7915AA)
//7915AXX 有FUSB 无HUSB
#define TCFG_FUSB_EN		1				//1:FUSB使能,0:不使能
#define TCFG_HUSB_EN		0				//1:HUSB使能,0:不使能
#endif

#endif
