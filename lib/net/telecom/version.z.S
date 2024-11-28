.section .sys.version, "ax"

.global lib_telecom_version

lib_telecom_version:

#ifdef CONFIG_RELEASE_ENABLE

#else

.asciz "TELECOM-*modified*--@20231213-$807643d8"
#endif
