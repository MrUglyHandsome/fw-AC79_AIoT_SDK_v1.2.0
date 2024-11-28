.section .sys.version, "ax"

.global lib_zliblite_version

lib_zliblite_version:

#ifdef CONFIG_RELEASE_ENABLE

#else

.asciz "ZLIBLITE-*modified*--@20231213-$807643d8"
#endif
