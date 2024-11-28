/**
 * Copyright (2017) Baidu Inc. All rights reserved.
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
// Author: Su Hao (suhao@baidu.com)
//
// Description: Encode & decode the int8 into base16/32/64.

#include "lightduer_base_coding.h"
#include "lightduer_log.h"

#define DUER_BITS_MAX       (8)
#define DUER_BITS_MASK      (-1)    // 0xffffffff

DUER_INT duer_size_t duer_base_encoded_length(duer_size_t dbits, duer_size_t sbits, duer_size_t slen)
{
    if (sbits != DUER_BITS_MAX) {
        slen = (slen * sbits) / DUER_BITS_MAX;
    }
    return (slen * DUER_BITS_MAX + dbits - 1) / dbits;
}

DUER_INT_IMPL duer_status_t duer_base_coding(char *dst, duer_size_t dlen, duer_size_t dbits, const char *src, duer_size_t slen, duer_size_t sbits)
{
    int rs = duer_base_encoded_length(dbits, sbits, slen);
    char dleft = dbits;
    char sleft;
    char temp;

    if (slen == 0 || src == NULL || dst == NULL || dlen < rs) {
        return DUER_ERR_FAILED;
    }

    while (slen > 0) {
        sleft = sbits;

        while (sleft > 0) {
            temp = *src & (~(DUER_BITS_MASK << sleft));
            DUER_LOGD("temp = %x, sleft = %d, dleft = %d", temp, sleft, dleft);

            if (sleft < dleft) {
                dleft -= sleft;

                *dst |= temp << dleft;
                DUER_LOGD("*dst = %x", *dst);

                sleft = 0;
            } else {
                sleft -= dleft;

                *dst |= (temp >> sleft) & (~(DUER_BITS_MASK << dleft));
                DUER_LOGD("*dst = %x", *dst);

                dst++;
                dleft = dbits;
            }
        }

        src++;
        slen--;
    }

    return rs;
}
