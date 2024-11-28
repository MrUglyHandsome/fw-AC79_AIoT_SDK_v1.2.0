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
 * File: lightduer_smart_config.c
 * Auth: Chen Xihao (chenxihao@baidu.com)
 * Desc: Duer smart config function file.
 */

#include "lightduer_smart_config.h"
#ifdef __linux__
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdint.h>
#include <unistd.h>
#else
/* #include <sockets.h> */
#include "lwip/sockets.h"
#endif
#include "lightduer_smart_config_decode.h"
#include "lightduer_log.h"
#include "lightduer_lib.h"
#include "lightduer_memory.h"
#include "lightduer_timestamp.h"
#include "lightduer_sleep.h"

#define DEFAULT_ACK_UDP_PORT    10001
#define SMART_CONFIG_ACK_TIMEOUT 10000 // ms
#define ACK_SEND_COUNTS         50

static const char *SMARTCONFIG_VERSION = "1.0";
static volatile int s_ack_stop_flag = 0;
duer_sc_status_t duer_smart_config_recv_packet(duer_sc_context_t context,
        uint8_t *packet, uint32_t len)
{
    duer_smartconfig_priv_t *priv = (duer_smartconfig_priv_t *)context;
    duer_sc_status_t status = DUER_SC_STATUS_IDLE;

    if (priv == NULL) {
        DUER_LOGE("invalid arguments");
        return status;
    }

    if (len < sizeof(duer_ieee80211_frame_t)) {
        DUER_LOGE("invalid packet, len %u", len);
        return status;
    }

    if (priv->status == DUER_SC_STATUS_COMPLETE) {
        return DUER_SC_STATUS_COMPLETE;
    }

    status = duer_sc_packet_decode(priv, packet, len);

    if (status == DUER_SC_STATUS_COMPLETE &&
        priv->status < DUER_SC_STATUS_COMPLETE) {
        DUER_LOGI("complete ssid: %s pwd: %s random: %d",
                  priv->result.ssid, priv->result.pwd, priv->result.random);
    } else if (status == DUER_SC_STATUS_LOCKED_CHAN && priv->status != status) {
        DUER_LOGI("channel locked");
        uint8_t *ap_mac = NULL;
        duer_ieee80211_frame_t *frame = (duer_ieee80211_frame_t *)packet;
        int dir = frame->frame_ctl[1] & DUER_IEEE80211_FC1_DIR_MASK;

        if (dir == DUER_IEEE80211_FC1_DIR_NODS) {
            ap_mac = frame->addr3;
        } else if (dir == DUER_IEEE80211_FC1_DIR_FROMDS) {
            ap_mac = frame->addr2;
        } else {
            ap_mac = frame->addr1;
        }

        DUER_MEMCPY(priv->ap_mac, ap_mac, sizeof(priv->ap_mac));

        priv->lock_channel_time = duer_timestamp();
#ifdef DUER_MODULE_DEVICE_STATUS_SUPPORT
        priv->log_lock_channel = 1;
#endif
    } else if (status == DUER_SC_STATUS_LOCKED_CHAN &&
               DUER_TIME_AFTER(duer_timestamp(),
                               priv->lock_channel_time + DUER_LOCK_CHANNEL_TIMEOUT)) {
        // TODO: we should search channel again
        DUER_LOGE("lock channel timeout");
        priv->lock_channel_time = duer_timestamp();
    }

    priv->status = status;

    return status;
}

int duer_smart_config_switch_channel(duer_sc_context_t context, int channel)
{
    duer_smartconfig_priv_t *priv = (duer_smartconfig_priv_t *)context;
    if (priv) {
        priv->cur_channel = channel;
    }

    return 0;
}

duer_sc_status_t duer_smart_config_get_status(duer_sc_context_t context)
{
    duer_smartconfig_priv_t *priv = (duer_smartconfig_priv_t *)context;

    if (priv == NULL) {
        return DUER_SC_STATUS_IDLE;
    }

    return priv->status;
}

duer_sc_context_t duer_smart_config_init(void)
{
    duer_smartconfig_priv_t *priv = NULL;
    int ret = 0;

    priv = (duer_smartconfig_priv_t *)DUER_MALLOC(sizeof(duer_smartconfig_priv_t));
    if (priv == NULL) {
        DUER_LOGE("malloc failed!");
        return NULL;
    }

    DUER_MEMSET(priv, 0, sizeof(duer_smartconfig_priv_t));

    priv->status = DUER_SC_STATUS_SEARCH_CHAN;

    return priv;
}

int duer_smart_config_deinit(duer_sc_context_t context)
{
    duer_smartconfig_priv_t *priv = (duer_smartconfig_priv_t *)context;

    if (priv == NULL) {
        DUER_LOGE("invalid arguments");
        return -1;
    }

#ifdef DUER_MODULE_DEVICE_STATUS_SUPPORT
    duer_ds_log_smart_cfg_finished(&priv->info);
#endif

    DUER_FREE(priv);

    return 0;
}

int duer_smart_config_get_result(duer_sc_context_t context,
                                 duer_wifi_config_result_t *result)
{
    duer_smartconfig_priv_t *priv = (duer_smartconfig_priv_t *)context;

    if (!result) {
        DUER_LOGE("invalid arguments");
        return -1;
    }

    if (priv == NULL) {
        DUER_LOGI("has already stoped!");
        return -1;
    }

    DUER_MEMCPY(result, &priv->result, sizeof(*result));

    return 0;
}

#ifdef DUER_MODULE_DEVICE_STATUS_SUPPORT
void duer_smart_config_send_log(duer_sc_context_t context)
{
    duer_smartconfig_priv_t *priv = (duer_smartconfig_priv_t *)context;
    if (priv) {
        if (priv->log_lock_channel) {
            duer_ds_log_smart_cfg(DUER_DS_LOG_SMART_CFG_LOCK_CHANNEL, NULL);
            priv->log_lock_channel = 0;
        }

        if (priv->log_lead_completed) {
            duer_ds_log_smart_cfg_lead_completed(priv->lead_code.data_len);
            priv->log_lead_completed = 0;
        }
    }
}
#endif

int duer_smart_config_get_ap_mac(duer_sc_context_t context, uint8_t *ap_mac)
{
    duer_smartconfig_priv_t *priv = (duer_smartconfig_priv_t *)context;

    if (priv == NULL || ap_mac == NULL) {
        DUER_LOGE("invalid arguments");
        return -1;
    }

    if (priv->status < DUER_SC_STATUS_LOCKED_CHAN) {
        DUER_LOGE("channel has not been locked");
        return -1;
    }

    DUER_MEMCPY(ap_mac, priv->ap_mac, sizeof(priv->ap_mac));

    return 0;
}

int duer_smart_config_ack(uint32_t port, uint8_t random)
{
    struct sockaddr_in addr;
    uint32_t end_time = 0;
    int ack_count = 0;
    int socketfd = -1;
    int tmp = 1;
    int ret = 0;
    int i = 0;

    DUER_LOGI("ack start");

    socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketfd < 0) {
        DUER_LOGE("create socket fail");
        return -1;
    }

    if (port == 0) {
        port = DEFAULT_ACK_UDP_PORT;
    }
    DUER_LOGI("ack port %d", port);

    DUER_MEMSET(&addr, 0, sizeof(addr));

    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    s_ack_stop_flag = 0;
    end_time = duer_timestamp() + SMART_CONFIG_ACK_TIMEOUT;

    ret = setsockopt(socketfd, SOL_SOCKET, SO_BROADCAST, &tmp, sizeof(tmp));
    if (ret < 0) {
        DUER_LOGE("setsockopt error");
        close(socketfd);
        return -1;
    }

    while (s_ack_stop_flag == 0 && DUER_TIME_BEFORE(duer_timestamp(), end_time)) {
        ret = sendto(socketfd, &random, 1, 0, (struct sockaddr *)&addr, sizeof(addr));
        if (ret > 0) {
            DUER_LOGD("ack send");
            if (ack_count++ >= ACK_SEND_COUNTS) {
                break;
            }
        } else {
            int err = 0;
            uint32_t optlen = sizeof(err);

            if (getsockopt(socketfd, SOL_SOCKET, SO_ERROR, &err, &optlen) < 0) {
                DUER_LOGE("getsockopt failed");
            }
            DUER_LOGE("udp send error, %d", err);
        }
        duer_sleep(20);
    }

    close(socketfd);

    DUER_LOGI("ack end");

    return 0;
}

void duer_smart_config_stop_ack(void)
{
    s_ack_stop_flag = 1;
}
