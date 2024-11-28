.section .sys.version, "ax"

.global lib_cjson_version

lib_cjson_version:

#ifdef CONFIG_RELEASE_ENABLE

#else

.asciz "CJSON-*modified*--@20231024-$8e8b52e"
#endif
