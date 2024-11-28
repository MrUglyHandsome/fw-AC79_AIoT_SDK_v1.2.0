.section .sys.version, "ax"

.global lib_tmallgenie_version

lib_tmallgenie_version:

#ifdef CONFIG_RELEASE_ENABLE

#else

.asciz "TMALLGENIE-*modified*--@20231213-$807643d8"
#endif
