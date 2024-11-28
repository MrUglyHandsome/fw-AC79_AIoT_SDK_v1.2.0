.section .sys.version, "ax"

.global lib_tencent_version

lib_tencent_version:

#ifdef CONFIG_RELEASE_ENABLE

#else

.asciz "TENCENT-*modified*--@20231213-$807643d8"
#endif
