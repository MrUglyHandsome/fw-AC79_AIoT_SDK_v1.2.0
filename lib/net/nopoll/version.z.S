
.section .sys.version, "ax"

.global lib_nopoll_version

lib_nopoll_version:

#ifdef CONFIG_RELEASE_ENABLE

#else

.asciz "LIBNOPOLL-*modified*--@20231213-$807643d8"
#endif
