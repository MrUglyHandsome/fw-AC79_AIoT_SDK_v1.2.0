.section .sys.version, "ax"

.global lib_echocloud_version

lib_echocloud_version:

#ifdef CONFIG_RELEASE_ENABLE

#else

.asciz "ECHOCLOUD-*modified*--@20231213-$807643d8"
#endif
