/* Copyright (2018) Baidu Inc. All rights reserved.
 *
 * Auth: Chen Yun(chenyun08@baidu.com)
 * Desc: Provide the file system adapter interface.
 */

#include <string.h>
#if defined(DUER_PLATFORM_A113L) || defined(DUER_PLATFORM_SV32WB0X)
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#else
/* #include "freertos/FreeRTOS.h" */
/* #include "freertos/task.h" */
/* #include "freertos/queue.h" */
#include "os/os_api.h"
#endif

#include "lightduer_types.h"
#include "lightduer_file.h"
#include "lightduer_log.h"
#include "lightduer_memory.h"

#ifdef DUER_PLATFORM_XR871
#include "fs/fatfs/ff.h"
#include "common/framework/fs_ctrl.h"

duer_file_t duer_file_open_impl(const char *filename, const char *mode)
{
    int rs = 0;
    FIL *fp = NULL;
    uint8_t tmp_mode = FA_READ;

    if (!strcmp("r", mode)) {
        tmp_mode = FA_READ;
    } else if (!strcmp("r+", mode)) {
        tmp_mode = FA_READ | FA_WRITE;
    } else if (!strcmp("w", mode)) {
        tmp_mode = FA_CREATE_ALWAYS | FA_WRITE;
    } else if (!strcmp("w+", mode)) {
        tmp_mode = FA_CREATE_ALWAYS | FA_WRITE | FA_READ;
    } else if (!strcmp("a", mode)) {
        tmp_mode = FA_OPEN_APPEND | FA_WRITE;
    } else if (!strcmp("a+", mode)) {
        tmp_mode = FA_OPEN_APPEND | FA_WRITE | FA_READ;
    } else if (!strcmp("wx", mode)) {
        tmp_mode = FA_CREATE_NEW | FA_WRITE;
    } else if (!strcmp("w+x", mode)) {
        tmp_mode = FA_CREATE_NEW | FA_WRITE | FA_READ;
    } else {
        DUER_LOGW("fs mode error");
        return NULL;
    }

    fp = (FIL *)DUER_MALLOC(sizeof(FIL));
    if (fp == NULL) {
        DUER_LOGE("No free memory");
        return NULL;
    }

    memset(fp, 0x0, sizeof(FIL));
    rs = f_open(fp, filename, tmp_mode);

    if (rs != FR_OK) {
        DUER_LOGW("f_open fail:%d", rs);
        DUER_FREE(fp);
        fp = NULL;
    }

    return fp;
}

size_t duer_file_read_impl(void *buffer, size_t size, size_t count, duer_file_t stream)
{
    int rs = 0;
    uint32_t read_length = 0;

    rs = f_read((FIL *)stream, buffer, count, &read_length);

    if (rs != FR_OK) {
        DUER_LOGW("f_read fail:%d", rs);
        read_length = 0;
    }

    return read_length;
}

size_t duer_file_write_impl(const void *buffer, size_t size, size_t count, duer_file_t stream)
{
    int rs = 0;
    uint32_t write_length = 0;
    rs = f_write((FIL *)stream, buffer, count, &write_length);

    if (rs != FR_OK) {
        DUER_LOGW("f_write fail:%d", rs);
        write_length = 0;
    }

    return write_length;
}

long duer_file_size_impl(duer_file_t stream)
{
    long size = f_size((FIL *)stream);
    return size;
}

int duer_file_seek_impl(duer_file_t stream, long offset, int origin)
{
    int rs = FR_INVALID_PARAMETER;
    long size = f_size((FIL *)stream);

    switch (origin) {
    case SEEK_SET:
        rs = f_lseek((FIL *)stream, offset);
        break;

    case SEEK_END:
        rs = f_lseek((FIL *)stream, size);
        break;

    case SEEK_CUR:
        rs = f_lseek((FIL *)stream, f_tell((FIL *)stream) + offset);
        break;

    default:
        break;
    }

    if (rs != FR_OK) {
        DUER_LOGW("f_seek fail:%d", rs);
        return -1;
    }

    return 0;
}

long duer_file_tell_impl(duer_file_t stream)
{
    long pos = f_tell((FIL *)stream);
    return pos;
}

int duer_file_close_impl(duer_file_t stream)
{
    int rs = 0;
    int ret = 0;

    rs = f_close((FIL *)stream);

    if (rs != FR_OK) {
        DUER_LOGW("f_close fail:%d", rs);
        ret = -1;
    }

    DUER_FREE(stream);

    return ret;
}

#else
#include <stdio.h>
duer_file_t duer_file_open_impl(const char *filename, const char *mode)
{
    return fopen(filename, mode);
}

size_t duer_file_read_impl(void *buffer, size_t size, size_t count, duer_file_t stream)
{
    return fread(buffer, size, count, stream);
}

size_t duer_file_write_impl(const void *buffer, size_t size, size_t count, duer_file_t stream)
{
    return fwrite(buffer, size, count, stream);
}

long duer_file_size_impl(duer_file_t stream)
{
    long size = 0;
    fseek(stream, 0, SEEK_END);
    size = ftell(stream);
    fseek(stream, 0, SEEK_SET);
    return size;
}

int duer_file_seek_impl(duer_file_t stream, long offset, int origin)
{
    return fseek(stream, offset, origin);
}

long duer_file_tell_impl(duer_file_t stream)
{
    return ftell(stream);
}

int duer_file_close_impl(duer_file_t stream)
{
    return fclose(stream);
}

#endif

void baidu_fs_adapter_initialize(void)
{
    duer_file_init(duer_file_open_impl,
                   duer_file_read_impl,
                   duer_file_write_impl,
                   duer_file_size_impl,
                   duer_file_seek_impl,
                   duer_file_tell_impl,
                   duer_file_close_impl);
}
