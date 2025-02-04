// *INDENT-OFF*

EXTERN(
_start
#include "sdk_used_list.c"
);

#include "app_config.h"

BOOT_INFO_SIZE = 52;

#if defined CONFIG_MMU_ENABLE
TLB_SIZE =(0X2000 * 2);
#else
TLB_SIZE =0;
#endif

SDRAM_SIZE = __SDRAM_SIZE__;
RAM0_SIZE = 0x1c7fe00 - 0x1c00000 - TLB_SIZE - BOOT_INFO_SIZE - 128;
UPDATA_BEG = 0x1c7fe00 - 128;

FREE_DACHE_WAY = 0; //max is 7
FREE_IACHE_WAY = 0; //max is 7
CACHE_RAM_SIZE = FREE_DACHE_WAY*4K+FREE_IACHE_WAY*4K;

MEMORY
{
    rom(rx)             : ORIGIN =  0x2000120, LENGTH = __FLASH_SIZE__
    sdram(rwx)          : ORIGIN =  0x4000120, LENGTH = SDRAM_SIZE
    ram0(rwx)           : ORIGIN =  0x1c00000 + TLB_SIZE, LENGTH = RAM0_SIZE
    boot_info(rwx)      : ORIGIN =  0x1c00000 + TLB_SIZE + RAM0_SIZE, LENGTH = BOOT_INFO_SIZE
    cache_ram(rw)       : ORIGIN =  0x1f20000+((8-FREE_IACHE_WAY)*4K), LENGTH = CACHE_RAM_SIZE
}
SECTIONS
{
/********************************************/
#ifdef CONFIG_SFC_ENABLE
    . = ORIGIN(rom);
#else
    . = ORIGIN(sdram);
#endif
    .text ALIGN(4):
    {
        _text_rodata_begin = .;
        PROVIDE(text_rodata_begin = .);

        *startup.S.o(.text)
        *(.boot_code)
        *(.text*)
        *(.rodata*)

        #include "system/system_lib_text.ld"
        #include "common/movable/movable_text.ld"
        #include "driver/cpu/wl82/media_text.ld"
#if defined CONFIG_BT_ENABLE
        . = ALIGN(4);
        #include "btctrler/btctler_lib_text.ld"
        . = ALIGN(4);
        #include "btstack/btstack_lib_text.ld"
#endif
        . = ALIGN(4);
        #include "ui/ui/ui.ld"
#if !defined CONFIG_RF_TRIM_CODE_AT_RAM && !defined CONFIG_RF_TRIM_CODE_MOVABLE
        *(*.rf.text.cache.L2.rf_trim)
#endif
        . = ALIGN(32);
        _text_rodata_end = .;
        PROVIDE(text_rodata_end = .);
    }

#ifdef CONFIG_SFC_ENABLE
	>rom
    . = ORIGIN(sdram);
#else
	> sdram
#endif

    .data ALIGN(32):
    {
#if defined CONFIG_ASR_ALGORITHM && defined CONFIG_VIDEO_ENABLE
        *(.libaisp.text)
        *(.libaisp.rodata)
#endif
#if defined CONFIG_DNS_ENC_ENABLE
        . = ALIGN(4);
        *(.jlsp_data)
#endif
#if defined CONFIG_RTOS_AND_MM_LIB_CODE_SECTION_IN_SDRAM
        *(.dlmalloc_code)
        *(.dlmalloc_const)
        *(.mem_heap_code)
        *(.mem_heap_const)
        _os_begin = .;
        PROVIDE(os_begin = .);
        *(.os_code)
        *(.os_const)
        *(.os_str)
        *(.os_data)
        *(.os_critical_code)
        *(.os_port_code)
        *(.os_port_data)
        *(.os_port_const)
        _os_end = .;
        PROVIDE(os_end = .);
#endif
#if defined CONFIG_UI_ENABLE
        . = ALIGN(4);
        *(.ui_ram)
#endif
        . = ALIGN(4);
        #include "driver/cpu/wl82/system.ld"
        . = ALIGN(4);
        #include "driver/cpu/wl82/system_data.ld"
        . = ALIGN(4);
        #include "server/media.ld"
        . = ALIGN(4);
        #include "net/server/net_server_data.ld"
#if defined CONFIG_BT_ENABLE
        . = ALIGN(4);
        #include "btstack/btstack_lib_data.ld"
#endif

        . = ALIGN(4);
        *(.data)

        . = ALIGN(4);
    } > sdram

    .bss ALIGN(32) (NOLOAD):
    {
        *(.bss)
#if defined CONFIG_CXX_SUPPORT
		*(.bss.*)
#endif
        *(COMMON)
        *(.mem_heap)
        *(.memp_memory_x)

        . = ALIGN(4);
        *(.os_port_bss)
        *(.os_bss)
        *(.update_bss)

#if defined CONFIG_BT_ENABLE
        . = ALIGN(4);
        #include "btstack/btstack_lib_bss.ld"
#endif

        . = ALIGN(4);
    } > sdram

    _HEAP_BEGIN = ( . + 31 ) / 32 * 32 ;
    PROVIDE(HEAP_BEGIN = (. + 31) / 32 * 32);
    _HEAP_END = 0x4000120 + SDRAM_SIZE - 32;
    PROVIDE(HEAP_END = 0x4000120 + SDRAM_SIZE - 32);
    _MALLOC_SIZE = _HEAP_END - _HEAP_BEGIN;
    PROVIDE(MALLOC_SIZE = _HEAP_END - _HEAP_BEGIN);


/********************************************/
    . =ORIGIN(ram0);
    .ram0_data ALIGN(4):
    {
        _ram_text_rodata_begin = .;
        PROVIDE(ram_text_rodata_begin = .);
        _VM_CODE_START = . ;
        *(.vm)
        _VM_CODE_END = . ;
        . = ALIGN(4);
        _SPI_CODE_START = . ;
        *(.spi_code)
        . = ALIGN(4);
        _SPI_CODE_END = . ;

        *(.flushinv_icache)
        *(.volatile_ram_code)
#if !defined CONFIG_VIDEO_ENABLE
        *(.m4a_dec_lib.text)
#if defined CONFIG_ASR_ALGORITHM
        *(.libaisp.rodata)
        *(.libaisp.text)
#endif
#endif

#if !defined CONFIG_RTOS_AND_MM_LIB_CODE_SECTION_IN_SDRAM
        *(.dlmalloc_code)
        *(.dlmalloc_const)
        *(.mem_heap_code)
        *(.mem_heap_const)
        _os_begin = .;
        PROVIDE(os_begin = .);
        *(.os_code)
        *(.os_const)
        *(.os_str)
        *(.os_data)
        *(.os_critical_code)
        *(.os_port_code)
        *(.os_port_data)
        *(.os_port_const)
        _os_end = .;
        PROVIDE(os_end = .);
#endif

#if defined CONFIG_RF_TRIM_CODE_AT_RAM
        *(*.rf.text.cache.L2.rf_trim)
#endif
        #include "common/movable/movable_data.ld"

        . = ALIGN(4);
        _ram_text_rodata_end = .;
        PROVIDE(ram_text_rodata_end = .);

        *(.volatile_ram)
        *(.non_volatile_ram)
        *(.fft_data)
        *(.deepsleep_target)

#if defined CONFIG_BT_ENABLE
        . = ALIGN(4);
        #include "btctrler/btctler_lib_data.ld"
#endif

        . = ALIGN(4); // must at tail, make ram0_data size align 4
    } > ram0

    .ram0_bss ALIGN(4):
    {
#if defined CONFIG_BT_ENABLE
        #include "btctrler/btctler_lib_bss.ld"
#endif

        . = ALIGN(4);
        *(.the_debug_isr_stack_c0)
        *(.the_debug_isr_stack_c1)
        *(.stack_magic)
        *(.stack_magic0)

        . = ALIGN(4);
        _stack_info_begin = .;
        PROVIDE(stack_info_begin = .);
        *(.cpu0_ustack)
        _cpu0_sstack_begin = .;
        PROVIDE(cpu0_sstack_begin = .);
        *(.cpu0_sstack)
        _cpu0_sstack_end = .;
        PROVIDE(cpu0_sstack_end = .);
        *(.cpu1_ustack)
        _cpu1_sstack_begin = .;
        PROVIDE(cpu1_sstack_begin = .);
        *(.cpu1_sstack)
        _cpu1_sstack_end = .;
        PROVIDE(cpu1_sstack_end = .);
        _stack_info_end = .;
        PROVIDE(stack_info_end = .);

        *(.sram)

        . = ALIGN(4);
        #include "device/usb/usb_data.ld"

        . = ALIGN(4); // must at tail, make ram0_data size align 4
    } > ram0

    _RAM_HEAP_BEGIN = ( . + 31 ) / 32 * 32;
    PROVIDE(RAM_HEAP_BEGIN = (. + 31) / 32 * 32);
    _RAM_HEAP_END = 0x1c00000 + TLB_SIZE + RAM0_SIZE - 32;
    PROVIDE(RAM_HEAP_END = 0x1c00000 + TLB_SIZE + RAM0_SIZE - 32);
    _RAM_MALLOC_SIZE = _RAM_HEAP_END - _RAM_HEAP_BEGIN;
    PROVIDE(RAM_MALLOC_SIZE = _RAM_HEAP_END - _RAM_HEAP_BEGIN);


    . =ORIGIN(boot_info);
    .boot_info ALIGN(4):
    {
		*(.boot_info)
        . = ALIGN(4); // must at tail, make ram0_data size align 4
    } > boot_info


/********************************************/
    . =ORIGIN(cache_ram);
    .cache_ram_data ALIGN(4):
    {
        . = ALIGN(4); // must at tail, make ram0_data size align 4
    } > cache_ram

    .cache_ram_bss ALIGN(4):
    {
        . = ALIGN(4); // must at tail, make ram0_data size align 4
    } > cache_ram
/********************************************/

}

    text_begin = ADDR(.text);
    text_size = SIZEOF(.text);
    PROVIDE(text_size = SIZEOF(.text));
    bss_begin = ADDR(.bss);
    bss_size  = SIZEOF(.bss);
    PROVIDE(bss_size = SIZEOF(.bss));

    data_vma  = ADDR(.data);
    data_lma =  text_begin + SIZEOF(.text);
    data_size =  SIZEOF(.data);
    PROVIDE(data_size = SIZEOF(.data));

/********************************************/
    _ram0_bss_vma = ADDR(.ram0_bss);
    _ram0_bss_size = SIZEOF(.ram0_bss);
    PROVIDE(ram0_bss_size = SIZEOF(.ram0_bss));

    _ram0_data_vma = ADDR(.ram0_data);
    _ram0_data_lma = text_begin + SIZEOF(.text) + SIZEOF(.data);
    _ram0_data_size = SIZEOF(.ram0_data);
    PROVIDE(ram0_data_size = SIZEOF(.ram0_data));
/********************************************/

    PROVIDE(cache_ram_bss_vma = ADDR(.cache_ram_bss));
    PROVIDE(cache_ram_bss_size = SIZEOF(.cache_ram_bss));
    PROVIDE(cache_ram_data_vma = ADDR(.cache_ram_data));
    PROVIDE(cache_ram_data_lma = text_begin + SIZEOF(.text) + SIZEOF(.data) + SIZEOF(.ram0_data));
    PROVIDE(cache_ram_data_size = SIZEOF(.cache_ram_data));


#if defined CONFIG_BT_ENABLE
	/*代码统计 Code & RAM : 蓝牙协议栈*/
    BTCTLER_LE_RAM_TOTAL   = BTCTLER_LE_CONTROLLER_DATA_SIZE + BTCTLER_LE_CONTROLLER_BSS_SIZE;
    BTCTLER_LE_CODE_TOTAL  = BTCTLER_LE_CONTROLLER_CODE_SIZE;

    BTCTLER_CL_RAM_TOTAL   = BTCTLER_CL_DATA_SIZE + BTCTLER_CL_BSS_SIZE;
    BTCTLER_CL_CODE_TOTAL  = BTCTLER_CL_CODE_SIZE;

    BTCTLER_COMMON_RAM_TOTAL = BTCTLER_CONTROLLER_BSS_SIZE + BTCTLER_CONTROLLER_DATA_SIZE;
    BTCTLER_COMMON_CODE_TOTAL = BTCTLER_CONTROLLER_CODE_SIZE ;

    BTCTLER_RAM_TOTAL   = (btctler_data_end - btctler_data_start) + (btctler_bss_end - btctler_bss_start);
    BTCTLER_CODE_TOTAL  = (btctler_code_end - btctler_code_start);

    BTSTACK_LE_HOST_MESH_RAM_TOTAL = BTSTACK_LE_HOST_MESH_DATA_SIZE + BTSTACK_LE_HOST_MESH_BSS_SIZE;
    BTSTACK_LE_HOST_MESH_FLASH_TOTAL = BTSTACK_LE_HOST_MESH_CODE_SIZE;
#endif

// *INDENT-ON*

EXTERN(
    lib_audio_server_version
    lib_system_version
    lib_btstack_version
    lib_btctler_version
    lib_btmesh_version
    lib_cfg_tool_version
    lib_common_version
    lib_crcm_version
    lib_cryto_toolbox_version
    lib_event_version
    lib_finsh_version
    lib_fs_version
    lib_driver_version
    lib_video_dec_version
    lib_video_rec_version
    lib_update_version
    lib_ui_font_version
    lib_wifi_version
    lib_wl82_rf_version
    lib_httpcli_version
    lib_airplay_version
    lib_aliyun_version
    lib_cjson_version
    lib_dcl_version
    lib_wpa_supplicant_version
    lib_device_vad_version
    lib_dlna_version
    lib_dui_version
    lib_echocloud_version
    lib_ftp_client_version
    lib_hsm_version
    lib_http_server_version
    lib_profile_version
    lib_iperf_version
    lib_jl_cloud_version
    lib_json_c_version
    lib_kcp_version
    lib_coap_version
    lib_duer_version
    lib_mpeg_version
    lib_lwip_2_0_3_version
    lib_lwip_2_1_2_version
    lib_lwip_2_1_3_version
    lib_mbedtls_2_2_1_version
    lib_mbedtls_2_26_0_version
    lib_mdns_version
    lib_mongoose_version
    lib_mqtt_version
    lib_network_download_version
    lib_rtp_version
    lib_net_server_version
    lib_media_server_version
    lib_stupid_ftpd_version
    lib_ftpserver_version
    lib_telecom_version
    lib_tencent_version
    lib_tmallgenie_version
    lib_tunnel_version
    lib_turing_version
    lib_uc_httpc_version
    lib_uip_version
    lib_websocket_version
    lib_wolfmqtt_version
    lib_wolfssl_version
    lib_wt_version
    lib_zliblite_version
    app_wifi_story_machine_version
    app_wifi_camera_version
    app_scan_box_version
    lib_font_version
    lib_res_version
    lib_ui_draw_version
    lib_ui_version
)
