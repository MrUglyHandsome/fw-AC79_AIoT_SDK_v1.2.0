.section .sys.version, "ax"

.global lib_coap_version

lib_coap_version:

#ifdef CONFIG_RELEASE_ENABLE

#else

.asciz "COAP-*modified*--@20231213-$807643d8"
#endif
