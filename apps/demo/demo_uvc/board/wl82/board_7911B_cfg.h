#ifndef CONFIG_BOARD_7911B_CFG_H
#define CONFIG_BOARD_7911B_CFG_H


//7911B0
#ifdef CONFIG_BOARD_7911B0
#define __FLASH_SIZE__    (2 * 1024 * 1024)
#define __SDRAM_SIZE__    (0)
#endif

//7911B8
#ifdef CONFIG_BOARD_7911B8
#define __FLASH_SIZE__    (1 * 1024 * 1024)//内置flash
#define __SDRAM_SIZE__    (0)
#endif

//7911BA
#ifdef CONFIG_BOARD_7911BA
#define __FLASH_SIZE__    (8 * 1024 * 1024)
#define __SDRAM_SIZE__    (2 * 1024 * 1024)
#endif

//7911BB
#ifdef CONFIG_BOARD_7911BB
#define __FLASH_SIZE__    (8 * 1024 * 1024)
#define __SDRAM_SIZE__    (8 * 1024 * 1024)
#endif

#if (defined CONFIG_BOARD_7911B0 || defined CONFIG_BOARD_7911B8 ||\
		defined CONFIG_BOARD_7911B8 || defined CONFIG_BOARD_7911BB)
//7911BXX 有FUSB 无HUSB
#define TCFG_FUSB_EN		1				//1:FUSB使能,0:不使能
#define TCFG_HUSB_EN		0				//1:HUSB使能,0:不使能
#endif

#endif
