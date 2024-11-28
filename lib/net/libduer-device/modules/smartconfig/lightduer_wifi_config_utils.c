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
/**
 * File: lightduer_wifi_config_utils.c
 * Auth: Chen Xihao (chenxihao@baidu.com)
 * Desc: Duer wifi config utility function file.
 */

#include "lightduer_wifi_config_utils.h"
#include "lightduer_lib.h"
#include "lightduer_log.h"
#include "lightduer_memory.h"
#include "lightduer_aes.h"

#if DUER_WC_CRYPT_ENABLE
#define DUER_WC_AES_KEY         "ba4df7466b584f26"
#endif

#define WIFI_DATA_VERSION       1
#define CRC8_INIT               0x0
#define CRC8_POLY               0x31

typedef enum duer_wifi_data_option_type_e {
    OP_SSID = 0,
    OP_PASSWORD,
    OP_UTOKEN,
    OP_PORT,
    OP_RANDOM,
    OP_CLIENT,
    OP_AUTHTYPE
} duer_wifi_data_option_type_t;

uint8_t duer_cal_crc8(const uint8_t *in, int num)
{
    uint8_t crc = CRC8_INIT;
    int i = 0;
    int j = 0;

    for (i = 0; i < num; i++) {
        crc ^= in[i];
        for (j = 0; j < 8; j++) {
            if (crc & 0x1) {
                crc = (crc >> 1) ^ CRC8_POLY;
            } else {
                crc = crc >> 1;
            }
        }

    }

    return crc;
}

#if DUER_WC_CRYPT_ENABLE
static int duer_aes_decrypt(const uint8_t *enc_data, uint32_t enc_data_len, uint8_t *dec_buf,
                            uint8_t key[16], uint8_t iv[16])
{
    int ret = 0;
    duer_aes_context ctx = duer_aes_context_init();
    if (ctx == NULL) {
        return -1;
    }

    if (duer_aes_setkey(ctx, key, 128) != 0) {
        return -1;
    }

    if (duer_aes_setiv(ctx, iv) != 0) {
        return -1;
    }

    ret = duer_aes_cbc_decrypt(ctx, enc_data_len, enc_data, dec_buf);

    duer_aes_context_destroy(ctx);

    return 0;
}
#endif

static int duer_wifi_config_parse_data_internal(uint8_t *src_data, uint8_t data_len, uint8_t crc,
        int check_crc, uint8_t key[16], uint8_t iv[16], duer_wifi_config_result_t *result)
{
    uint8_t *data = NULL;
    uint8_t offset = 0;
    int ret = 0;

    if (src_data == NULL || data_len == 0 || key == NULL || iv == NULL || result == NULL) {
        DUER_LOGE("parse wifi data, invalid arugments");
        return -1;
    }

    DUER_MEMSET(result, 0, sizeof(*result));

#if DUER_WC_CRYPT_ENABLE
    data = (uint8_t *)DUER_MALLOC(data_len);
    if (data == NULL) {
        DUER_LOGE("memory overflow");
        return -1;
    }

    if (duer_aes_decrypt(src_data, data_len, data, key, iv) == 0) {
        int padding_len = *(data + data_len - 1);
        DUER_LOGD("padding_len: %d", padding_len);
        // check padding
        for (int i = 1; i < padding_len; i++) {
            int value = data[data_len - i - 1];
            if (value != padding_len) {
                DUER_LOGE("aes decrypt err, value:%d", value);
                return -1;
            }
        }
        data_len = data_len - padding_len;
    } else {
        DUER_LOGE("aes decrypt err");
        return -1;
    }
#else
    data = src_data;
#endif // DUER_WC_CRYPT_ENABLE

    DUER_DUMPD("wifi data", data, data_len);

    // check crc8, sonic config does not need to check crc8
    if (check_crc != 0 && duer_cal_crc8(data, data_len) != crc) {
        DUER_LOGE("check data crc fail");
        return -1;
    }

    // check version
    if (data[offset] != WIFI_DATA_VERSION) {
        DUER_LOGE("invalid wifi data version");
        return -1;
    }
    offset++;

    // parse data
    while (offset < data_len - 1 && ret == 0) {
        duer_wifi_data_option_type_t type = (duer_wifi_data_option_type_t)data[offset];
        uint8_t len = data[offset + 1];
        offset += 2;
        DUER_LOGD("parse type = %d", type);
        switch (type) {
        case OP_SSID:
            if (len == 0 || len > (data_len - offset) || len > DUER_WC_SSID_MAX_LEN) {
                ret = -1;
                break;
            }
            DUER_MEMCPY(result->ssid, data + offset, len);
            result->ssid_len = len;
            offset += len;
            DUER_LOGD("ssid = %s", (char *)result->ssid);
            break;
        case OP_PASSWORD:
            if (len == 0 || len > (data_len - offset) || len > DUER_WC_PASSWOAD_MAX_LEN) {
                ret = -1;
                break;
            }
            DUER_MEMCPY(result->pwd, data + offset, len);
            result->pwd_len = len;
            offset += len;
            DUER_LOGD("pwd = %s", (char *)result->pwd);
            break;
        case OP_UTOKEN:
            if (len == 0 || len > (data_len - offset) || len > DUER_WC_UTOKEN_MAX_LEN) {
                ret = -1;
                break;
            }
            DUER_MEMCPY(result->utoken, data + offset, len);
            offset += len;
            DUER_LOGD("utoken = %s", (char *)result->utoken);
            break;
        case OP_PORT:
            if (len != sizeof(uint16_t) || len > (data_len - offset)) {
                ret = -1;
                break;
            }
            result->port = data[offset] | data[offset + 1] << 8;
            offset += len;
            break;
        case OP_RANDOM:
            result->random = len;
            break;
        case OP_CLIENT:
            DUER_LOGW("unhandle client type:%0x", len);
            break;
        case OP_AUTHTYPE:
            DUER_LOGW("unhandle auth type:%0x", len);
            break;
        default:
            DUER_LOGE("unknown wifi data type %d", type);
            break;
        }

        if (ret != 0) {
            DUER_LOGE("parse type %d fail", type);
        }
    }

    if (data != src_data) {
        DUER_FREE(data);
        data = NULL;
    }

    if (ret == 0 && (result->ssid_len == 0 || result->utoken[0] == 0)) {
        DUER_LOGE("wifi data does not include necessary option");
        ret = -1;
    }

    return ret;
}


int duer_wifi_config_parse_data(uint8_t *src_data, uint8_t data_len, uint8_t crc,
                                int check_crc, duer_wifi_config_result_t *result)
{
    return duer_wifi_config_parse_data_internal(src_data, data_len, crc, check_crc,
            DUER_WC_AES_KEY, DUER_WC_AES_KEY, result);
}

int duer_wifi_config_parse_data_ex(uint8_t *src_data, uint8_t data_len, uint8_t key[16],
                                   uint8_t iv[16], duer_wifi_config_result_t *result)
{
    return duer_wifi_config_parse_data_internal(src_data, data_len, 0, 0, key, iv, result);
}
