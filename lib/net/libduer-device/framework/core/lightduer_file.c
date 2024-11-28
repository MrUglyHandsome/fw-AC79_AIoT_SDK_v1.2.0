/**
 * Copyright (2018) Baidu Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
// Author: Chen Yun (chenyun08@baidu.com)
//
// Description: Wrapper for file opeation.

#include "lightduer_file.h"

typedef struct _duer_file_cbs_s {
    duer_fs_open f_open;
    duer_fs_read f_read;
    duer_fs_write f_write;
    duer_fs_size f_size;
    duer_fs_seek f_seek;
    duer_fs_tell f_tell;
    duer_fs_close f_close;
} duer_file_cbs;

DUER_LOC_IMPL duer_file_cbs s_duer_file_cbs = {NULL};

DUER_EXT_IMPL void duer_file_init(duer_fs_open f_open,
                                  duer_fs_read f_read,
                                  duer_fs_write f_write,
                                  duer_fs_size f_size,
                                  duer_fs_seek f_seek,
                                  duer_fs_tell f_tell,
                                  duer_fs_close f_close)
{
    s_duer_file_cbs.f_open = f_open;
    s_duer_file_cbs.f_read = f_read;
    s_duer_file_cbs.f_write = f_write;
    s_duer_file_cbs.f_size = f_size;
    s_duer_file_cbs.f_seek = f_seek;
    s_duer_file_cbs.f_tell = f_tell;
    s_duer_file_cbs.f_close = f_close;
}

DUER_EXT_IMPL duer_file_t duer_file_open(const char *filename, const char *mode)
{
    return s_duer_file_cbs.f_open ? s_duer_file_cbs.f_open(filename, mode) : NULL;
}

DUER_EXT_IMPL size_t duer_file_read(void *buffer, size_t size, size_t count, duer_file_t stream)
{
    return s_duer_file_cbs.f_read ? s_duer_file_cbs.f_read(buffer, size, count, stream) : 0;
}

DUER_EXT_IMPL size_t duer_file_write(const void *buffer, size_t size, size_t count,
                                     duer_file_t stream)
{
    return s_duer_file_cbs.f_write ? s_duer_file_cbs.f_write(buffer, size, count, stream) : 0;
}

DUER_EXT_IMPL long duer_file_size(duer_file_t stream)
{
    return s_duer_file_cbs.f_size ? s_duer_file_cbs.f_size(stream) : 0;
}

DUER_EXT_IMPL int duer_file_seek(duer_file_t stream, long offset, int origin)
{
    return s_duer_file_cbs.f_seek ? s_duer_file_cbs.f_seek(stream, offset, origin) : 0;
}

DUER_EXT_IMPL long duer_file_tell(duer_file_t stream)
{
    return s_duer_file_cbs.f_tell ? s_duer_file_cbs.f_tell(stream) : 0;
}

DUER_EXT_IMPL int duer_file_close(duer_file_t stream)
{
    return s_duer_file_cbs.f_close ? s_duer_file_cbs.f_close(stream) : 0;
}
