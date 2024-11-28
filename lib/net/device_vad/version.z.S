.section .sys.version, "ax"

.global lib_device_vad_version

lib_device_vad_version:

#ifdef CONFIG_RELEASE_ENABLE

#else

.asciz "LIBVAD-*modified*--@20231213-$807643d8"
#endif
