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
 * File: lightduer_smart_config_decode.c
 * Auth: Chen Xihao (chenxihao@baidu.com)
 * Desc: Duer smart config decode function file.
 */

#include "lightduer_smart_config_decode.h"
#include "lightduer_smart_config.h"
#include "lightduer_log.h"
#include "lightduer_lib.h"

typedef enum {
    LEAD_CODE_NONE = 0,             /* 0, not used */
    LEAD_CODE_GET_CHANNEL,          /* 1 */
    LEAD_CODE_GET_DATA_SIZE,        /* 2 */
    LEAD_CODE_GET_DATA_CRC,         /* 3 */
    LEAD_CODE_MAX,                  /* 4, data start index */
} LEADCODE_SEQ;

#define DUER_SC_MAGIC_ADD0      0x01
#define DUER_SC_MAGIC_ADD1      0x00
#define DUER_SC_MAGIC_ADD2      0X5E
#define DUER_SC_MAX_INDEX_NUM   127
#define DUER_SC_RETRY_PKG_TIMES 5
#define DUER_SC_IOS_CHANNEL     0xff

#define DUER_MAC_ADDR_0         0
#define DUER_MAC_ADDR_1         1
#define DUER_MAC_ADDR_2         2
#define DUER_MAC_ADDR_3         3
#define DUER_MAC_ADDR_4         4
#define DUER_MAC_ADDR_5         5

/* if data already exist return 0, else return 1; if invalid arguments return -1 */
static int duer_valid_packet_index(uint8_t *bitmaps, uint8_t index)
{
    if (bitmaps == NULL || index > DUER_SC_MAX_INDEX_NUM) {
        return -1;
    }

    if (((*(bitmaps + index / 8)) & (1 << (8 - (index % 8) - 1))) > 0) {
        return 0;
    }

    *(bitmaps + index / 8) |= 1 << (8 - (index % 8) - 1);

    return 1;
}

static LEADCODE_SEQ duer_get_lead_code(duer_ieee80211_frame_t *iframe, uint32_t len,
                                       duer_sc_lead_code_t *lead_code, uint8_t packet_num,
                                       uint8_t cur_channel, uint8_t is_from_ap,
                                       duer_sc_status_t *status)
{
    uint8_t *data = NULL;
    uint8_t *crc = NULL;
    uint8_t crc8 = 0;
    LEADCODE_SEQ code;

    if (is_from_ap) {
        data = &iframe->addr1[DUER_MAC_ADDR_4];
        crc = &iframe->addr1[DUER_MAC_ADDR_5];
    } else {
        data = &iframe->addr3[DUER_MAC_ADDR_4];
        crc = &iframe->addr3[DUER_MAC_ADDR_5];
    }

    if ((packet_num != LEAD_CODE_GET_CHANNEL && *status < DUER_SC_STATUS_LOCKED_CHAN)
        || (packet_num == LEAD_CODE_GET_CHANNEL && *status >= DUER_SC_STATUS_LOCKED_CHAN)) {
        DUER_LOGD("need not to handle this lead code:%d", packet_num);
        return LEAD_CODE_NONE;
    }

    if (packet_num == LEAD_CODE_NONE || packet_num > LEAD_CODE_GET_DATA_CRC) {
        DUER_LOGE("packet_num:%d", packet_num);
        return LEAD_CODE_NONE;
    }

    // check length
    if (packet_num > LEAD_CODE_GET_CHANNEL && len != lead_code->base_len) {
        DUER_LOGD("the len of frame does not equal to base_len");
        return LEAD_CODE_NONE;
    }

    /* crc check the leadcode, crc8(index + data) */
    crc8 = duer_cal_crc8(data - 1, 2);
    if (crc8 != *crc) {
        return LEAD_CODE_NONE;
    }

    DUER_LOGI("get leadcode %d", packet_num);

    code = (LEADCODE_SEQ)packet_num;
    switch (code) {
    case LEAD_CODE_GET_CHANNEL:
        // ios can't get channel of wifi, set channel = 0xff
        if (*data != cur_channel && *data != DUER_SC_IOS_CHANNEL) {
            DUER_LOGE("invalid channel %d, cur_channel = %d", *data, cur_channel);
            break;
        }
        lead_code->channel = *data;
        lead_code->base_len = len;
        *status = DUER_SC_STATUS_LOCKED_CHAN;
        DUER_LOGD("locked channel : %d", lead_code->channel);
        break;
    case LEAD_CODE_GET_DATA_SIZE:
        if ((*data % 2) == 1) {
            DUER_LOGE("invalid data size packet");
            break;
        }
        lead_code->data_len = *data;
        // one packet has 2 bytes data
        lead_code->packet_total = lead_code->data_len / 2;
        DUER_LOGD("packet_total %d", lead_code->packet_total);
        break;
    case LEAD_CODE_GET_DATA_CRC:
        lead_code->crc = *data;
        lead_code->crc_flag = 1;
        break;
    default:
        code = LEAD_CODE_NONE;
        break;
    }

    if ((*status == DUER_SC_STATUS_LOCKED_CHAN) && (lead_code->data_len != 0)
        && (lead_code->crc_flag != 0)) {
        DUER_LOGI("lead complete data len:%d crc:%d channel:%d", lead_code->data_len,
                  lead_code->crc, lead_code->channel);
        *status = DUER_SC_STATUS_LEAD_CODE_COMPLETE;
    }

    return code;
}

static int duer_decode_wifi_data(duer_smartconfig_priv_t *priv)
{
    if (priv == NULL) {
        DUER_LOGE("invalid channel packet");
        return -1;
    }

    return duer_wifi_config_parse_data(priv->src_data_buff, priv->lead_code.data_len,
                                       priv->lead_code.crc, 1, &priv->result);
}

/* save wifi data */
static int duer_push_wifi_data(duer_sc_lead_code_t *lead_code, uint8_t packet_num,
                               uint8_t *src_data_buff, uint8_t data[2], int *updated)
{
    int d = 0;
    uint8_t *src = NULL;
    int ret = 0;

    packet_num -= LEAD_CODE_MAX;
    src = src_data_buff + (packet_num << 1);

    ret = duer_valid_packet_index(lead_code->bitmaps, packet_num);
    if (ret == 1) {
        lead_code->packet_count++;
        *updated = 1;
    } else if (ret == 0) {
        *updated = (src[0] != data[0]) || (src[1] != data[1]);
    } else {
        *updated = 0;
        return -1;
    }

    d = lead_code->packet_count - lead_code->packet_total;
    src[0] = data[0];
    src[1] = data[1];

    DUER_LOGI("push packet_num:%d, data:%02x%02x, %d", packet_num, data[0], data[1], d);

    return d;
}

static void duer_sc_reset_lead_code(duer_sc_lead_code_t *lead_code)
{
    DUER_MEMSET(lead_code, 0, sizeof(duer_sc_lead_code_t));
}

duer_sc_status_t duer_sc_packet_decode(
    duer_smartconfig_priv_t *priv, uint8_t *data, uint32_t len)
{
    duer_ieee80211_frame_t *iframe = (duer_ieee80211_frame_t *)data;
    uint8_t packet_num = 0;
    int ret = 0;
    duer_sc_status_t status = DUER_SC_STATUS_IDLE;
    duer_sc_lead_code_t *lead_code = NULL;
    uint8_t is_from_ap = 0;
    int updated = 0;

    if (priv == NULL || iframe == NULL) {
        return status;
    }

    status = priv->status;
    lead_code = &priv->lead_code;

    // for debug
#if 0
    printf("framecontrol:0x%x, 0x%x\n", iframe->frame_ctl[0], iframe->frame_ctl[1]);
    printf("addr1:%x:%x:%x:%x:%x:%x\n", iframe->addr1[0], iframe->addr1[1], iframe->addr1[2], iframe->addr1[3], iframe->addr1[4], iframe->addr1[5]);
    printf("addr2:%x:%x:%x:%x:%x:%x\n", iframe->addr2[0], iframe->addr2[1], iframe->addr2[2], iframe->addr2[3], iframe->addr2[4], iframe->addr2[5]);
    printf("addr3:%x:%x:%x:%x:%x:%x\n", iframe->addr3[0], iframe->addr3[1], iframe->addr3[2], iframe->addr3[3], iframe->addr3[4], iframe->addr3[5]);
#endif

    uint8_t *arrdata = NULL;
    if ((iframe->addr1[DUER_MAC_ADDR_0] == DUER_SC_MAGIC_ADD0) &&
        (iframe->addr1[DUER_MAC_ADDR_1] == DUER_SC_MAGIC_ADD1) &&
        (iframe->addr1[DUER_MAC_ADDR_2] == DUER_SC_MAGIC_ADD2)) {
        is_from_ap = 1;
        packet_num = iframe->addr1[DUER_MAC_ADDR_3];
        arrdata = &iframe->addr1[DUER_MAC_ADDR_3];
    } else if ((iframe->addr3[DUER_MAC_ADDR_0] == DUER_SC_MAGIC_ADD0) &&
               (iframe->addr3[DUER_MAC_ADDR_1] == DUER_SC_MAGIC_ADD1) &&
               (iframe->addr3[DUER_MAC_ADDR_2] == DUER_SC_MAGIC_ADD2)) {
        packet_num = iframe->addr3[DUER_MAC_ADDR_3];
        arrdata = &iframe->addr3[DUER_MAC_ADDR_3];
    } else {
        return status;
    }

    DUER_LOGD("arrdata:%d:%d:%d, from_ap:%d\n", arrdata[0], arrdata[1], arrdata[2], is_from_ap);

#ifdef DUER_MODULE_DEVICE_STATUS_SUPPORT
    priv->info.packet_counts++;
#endif

    // handle lead code
    if (packet_num > LEAD_CODE_NONE && packet_num < LEAD_CODE_MAX) {
        if (status < DUER_SC_STATUS_LEAD_CODE_COMPLETE) {
            duer_get_lead_code(iframe, len, lead_code, packet_num,
                               priv->cur_channel, is_from_ap, &status);
#ifdef DUER_MODULE_DEVICE_STATUS_SUPPORT
            if (status == DUER_SC_STATUS_LEAD_CODE_COMPLETE) {
                priv->log_lead_completed = 1;
            }
#endif
        }

        return status;
    }

    if (status < DUER_SC_STATUS_LEAD_CODE_COMPLETE) {
        return status;
    }

    if (packet_num <= (lead_code->packet_total + LEAD_CODE_MAX)) {
        uint8_t *data = 0;
        uint8_t crc = len - lead_code->base_len;
        if (is_from_ap) {
            data = &iframe->addr1[DUER_MAC_ADDR_3];
        } else {
            data = &iframe->addr3[DUER_MAC_ADDR_3];
        }

#ifdef DUER_MODULE_DEVICE_STATUS_SUPPORT
        priv->info.data_counts++;
#endif

        // check data crc8, crc8(1byte index + 2bytes data)
        if (duer_cal_crc8(data, 3) != crc) {
#ifdef DUER_MODULE_DEVICE_STATUS_SUPPORT
            priv->info.data_invalids++;
#endif
            return status;
        }

        priv->recv_pkg_count++;

        if (duer_push_wifi_data(lead_code, packet_num, priv->src_data_buff,
                                data + 1, &updated) != 0) {
            return status;
        }
    }

    if (priv->status < DUER_SC_STATUS_COMPLETE &&
        lead_code->packet_total == lead_code->packet_count) {
        if (updated) {
            DUER_LOGI("DATA IS ALL RECEIVE, data_decode");
            ret = duer_decode_wifi_data(priv);
            if (ret) {
                DUER_LOGE("decode err");
            } else {
                DUER_LOGI("decode success");
                status = DUER_SC_STATUS_COMPLETE;
            }
#ifdef DUER_MODULE_DEVICE_STATUS_SUPPORT
            priv->info.decode_counts++;
#endif
        }

        if (status != DUER_SC_STATUS_COMPLETE
            && priv->recv_pkg_count > (lead_code->packet_total * DUER_SC_RETRY_PKG_TIMES)) {
            duer_sc_reset_lead_code(&priv->lead_code);
            priv->recv_pkg_count = 0;
            priv->src_data_buff[0] = 0;
            status = DUER_SC_STATUS_IDLE;
        }
    }

    return status;
}
