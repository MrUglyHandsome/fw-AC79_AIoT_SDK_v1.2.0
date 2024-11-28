#ifndef CONFIG_BOARD_7912A_CFG_H
#define CONFIG_BOARD_7912A_CFG_H


//7912AB
#ifdef CONFIG_BOARD_7912AB
#define __FLASH_SIZE__    (8 * 1024 * 1024)
#define __SDRAM_SIZE__    (8 * 1024 * 1024)
#endif

#if (defined CONFIG_BOARD_7912AB)
//7912AXX 有FUSB、HUSB
#define TCFG_FUSB_EN		1				//1:FUSB使能,0:不使能
#define TCFG_HUSB_EN		1				//1:HUSB使能,0:不使能
#endif

#endif
