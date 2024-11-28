.section .sys.version, "ax"

.global lib_uip_version

lib_uip_version:

#ifdef CONFIG_RELEASE_ENABLE

#else

.asciz "UIP-*modified*--@20231213-$807643d8"
#endif
