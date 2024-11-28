.section .sys.version, "ax"

.global lib_lwip_2_1_3_version

lib_lwip_2_1_3_version:

#ifdef CONFIG_RELEASE_ENABLE

#else

.asciz "LWIP_2_1_3-*modified*--@20241112-$8f49e90b"
#endif
