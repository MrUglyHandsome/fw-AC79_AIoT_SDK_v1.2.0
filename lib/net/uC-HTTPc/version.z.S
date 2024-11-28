.section .sys.version, "ax"

.global lib_uc_httpc_version

lib_uc_httpc_version:

#ifdef CONFIG_RELEASE_ENABLE

#else

.asciz "UC_HTTPC-*modified*--@20231213-$807643d8"
#endif
