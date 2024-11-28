.section .sys.version, "ax"

.global lib_aliyun_version

lib_aliyun_version:

#ifdef CONFIG_RELEASE_ENABLE

#else

.asciz "ALIYUN-*modified*--@20231024-$8e8b52e"
#endif
