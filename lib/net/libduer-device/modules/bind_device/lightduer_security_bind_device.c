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
 * File: lightduer_security_bind_device.c
 * Auth: Chen Xihao (chenxihao@baidu.com)
 * Desc: Support App to bind device securely
 */
#ifdef ENABLE_SECURITY_BIND_DEVICE
#include "lightduer_security_bind_device.h"
#ifdef __linux__
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdint.h>
#include <unistd.h>
#else
#include <sockets.h>
#endif
#include <stdbool.h>
#include "lightduer_events.h"
#include "lightduer_log.h"
#include "lightduer_memory.h"
#include "lightduer_timestamp.h"
#include "lightduer_aes.h"
#include "lightduer_sleep.h"
#include "lightduer_ds_log_bind.h"

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/entropy.h>

// #define BIND_DEBUG
#define PUBLIC_POINT_LEN    (66)
#define SHARED_SECRET_LEN   (32)

typedef struct {
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
    mbedtls_ecdh_context ecdh_ctx;
    uint8_t public_point[PUBLIC_POINT_LEN];
    uint8_t shared_secret[SHARED_SECRET_LEN];
    uint8_t app_pub[PUBLIC_POINT_LEN];
} duer_ecdh_context_t;

/*
 * package format
 * |---------------------------Head-------------------------------|
 * |   2 bytes    | 1 bytes |   2 bytes   | 1 byte   |    1byte   |
 * | magic number | version | body length | msg type | crc8(body) |
 * |---------------------------Body-------------------------------|
 */
typedef enum {
    DUER_MSG_HEAD_MAGIC_NUMBER_OFFSET = 0,
    DUER_MSG_HEAD_VERSION_OFFSET = 2,
    DUER_MSG_HEAD_BODY_LEN_OFFSET = 3,
    DUER_MSG_HEAD_MSG_TYPE_OFFSET = 5,
    DUER_MSG_HEAD_CRC_OFFSET = 6,
} duer_msg_head_offset_t;

typedef enum {
    DUER_MSG_INVALID = -1,
    DUER_MSG_NOT_NEED = 0x0,
    DUER_MSG_DISCOVERY_REQ = 0x1,
    DUER_MSG_DISCOVERY_RESP = 0x2,
    DUER_MSG_DONE_NOTIFY = 0x3,
} duer_bind_msg_type_t;

typedef enum {
    DUER_TAG_APP_ECC_PUB = 0x11,
    DUER_TAG_DEVICE_ECC_PUB = 0x21,
    DUER_TAG_DEVICE_ID = 0x22,
    DUER_TAG_AEC_BIND_TOKEN = 0x23,
    DUER_TAG_BIND_END = 0x31,
} duer_bind_tag_t;

typedef enum {
    DUER_INVALID_REQUEST = -1,
    DUER_NEW_REQUEST = 0,
    DUER_REPETITIVE_REQUEST = 1,
} duer_handle_discovery_request_t;

static const size_t MIN_LIFE_CYCLE = 60; // 60 seconds
static const int UDP_PORT = 12456;
static const uint8_t MSG_HEAD_LEN = 7;
static const uint8_t MAGIC_NUMBER[2] = { 0xFA, 0xFB};
static const uint8_t VERSION = 0x1;

static const uint8_t CRC8_POLY = 0x31;
static const uint8_t BODY_SEG_HEAD_LEN = 2; // 1byte tag + 1byte length
static const size_t AES_KEY_LEN = 16;
static const size_t BIND_TOKEN_LEN = 32;
static const size_t STACK_SIZE = 3 * 1024;

static duer_events_handler s_events = NULL;
static size_t s_timeout = 0;
static volatile bool s_stop_send = false;

static duer_ecdh_context_t *s_ecdh_context = NULL;
static char *s_device_uuid = NULL;
static char *s_bind_token = NULL;
static duer_ds_log_bind_code_t s_log_code;

static uint8_t *duer_aes_cbc_encrypt_bind_token(const char *bind_token)
{
    duer_aes_context aes_ctx;
    uint8_t *enc_data = NULL;
    int ret = 0;

    if (bind_token == NULL || DUER_STRLEN(bind_token) != BIND_TOKEN_LEN) {
        DUER_LOGE("invalid bind token!");
        return NULL;
    }

    aes_ctx = duer_aes_context_init();
    if (aes_ctx == NULL) {
        DUER_LOGE("init aes context failed");
        return NULL;
    }
    // AES key is 128bits(16bytes)
    duer_aes_setkey(aes_ctx, s_ecdh_context->shared_secret, AES_KEY_LEN * 8);
    duer_aes_setiv(aes_ctx, s_ecdh_context->shared_secret + AES_KEY_LEN);

    do {
        enc_data = (uint8_t *)DUER_MALLOC(BIND_TOKEN_LEN);
        if (enc_data == NULL) {
            DUER_LOGE("no free memory");
            break;
        }

        ret = duer_aes_cbc_encrypt(aes_ctx, BIND_TOKEN_LEN, (uint8_t *)bind_token, enc_data);
        if (ret != DUER_OK) {
            DUER_LOGE("encrypt failed");
            DUER_FREE(enc_data);
            enc_data = NULL;
        }
    } while (0);

    duer_aes_context_destroy(aes_ctx);

    return enc_data;
}

// only use to test duer_aes_cbc_encrypt_bind_token
#if 0
static char *duer_aes_cbc_decrypt_bind_token(const uint8_t *enc_data)
{
    duer_aes_context aes_ctx;
    uint8_t *bind_token = NULL;
    int ret = 0;

    if (enc_data == NULL) {
        DUER_LOGE("invalid base64_data!");
        return NULL;
    }

    aes_ctx = duer_aes_context_init();
    if (aes_ctx == NULL) {
        DUER_LOGE("init aes context failed");
        return NULL;
    }

    // AES key is 128bits(16bytes)
    duer_aes_setkey(aes_ctx, s_ecdh_context->shared_secret, AES_KEY_LEN * 8);
    duer_aes_setiv(aes_ctx, s_ecdh_context->shared_secret + AES_KEY_LEN);

    bind_token = (uint8_t *)DUER_MALLOC(BIND_TOKEN_LEN + 1);
    if (bind_token == NULL) {
        DUER_LOGE("no free memory");
        return NULL;
    }
    bind_token[BIND_TOKEN_LEN] = 0;

    ret = duer_aes_cbc_decrypt(aes_ctx, BIND_TOKEN_LEN, enc_data, bind_token);
    if (ret != DUER_OK) {
        DUER_LOGE("decrypt failed");
        DUER_FREE(bind_token);
        bind_token = NULL;
    }

    duer_aes_context_destroy(aes_ctx);

    return (char *)bind_token;
}
#endif

static int duer_get_socket_errno(int fd)
{
    int sock_errno = 0;
    uint32_t optlen = sizeof(sock_errno);

    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &sock_errno, &optlen) < 0) {
        DUER_LOGE("getsockopt failed");
    }

    return sock_errno;
}

static uint8_t duer_cal_crc8(const uint8_t *data, int len)
{
    uint8_t crc = 0;
    int i = 0;
    int j = 0;

    for (i = 0; i < len; i++) {
        crc ^= data[i];
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

#ifdef BIND_DEBUG
static void duer_dump_bytes(const uint8_t *bytes, size_t len, const char *name)
{
    if (bytes == NULL || len == 0) {
        return;
    }

    printf("dump %s[%d]:\n", name, len);
    for (int i = 0; i < len; i++) {
        printf("%02x ", bytes[i]);
    }
    printf("\n");
}
#endif

static duer_bind_msg_type_t duer_bind_msg_recv(const uint8_t *msg, uint16_t len)
{
    uint16_t body_len = 0;
    duer_bind_msg_type_t type = DUER_MSG_INVALID;
    if (msg == NULL || len < MSG_HEAD_LEN) {
        DUER_LOGW("duer_bind_msg_recv invalid msg");
        return type;
    }

#ifdef BIND_DEBUG
    duer_dump_bytes(msg, len, "recv_msg");
#endif

    // check magic number
    if (DUER_MEMCMP(msg, MAGIC_NUMBER, sizeof(MAGIC_NUMBER)) != 0) {
        DUER_LOGW("not bind msg");
        return type;
    }

    // check version
    if (msg[DUER_MSG_HEAD_VERSION_OFFSET] != VERSION) {
        DUER_LOGW("invalid version");
        return type;
    }

    // check length
    body_len = msg[DUER_MSG_HEAD_BODY_LEN_OFFSET] << 8 | msg[DUER_MSG_HEAD_BODY_LEN_OFFSET + 1];
    if (body_len != (len - MSG_HEAD_LEN)) {
        DUER_LOGW("the length of msg is not match");
        return type;
    }

    type = (duer_bind_msg_type_t)msg[DUER_MSG_HEAD_MSG_TYPE_OFFSET];
    DUER_LOGI("msg type: %d", type);
    if (type != DUER_MSG_DISCOVERY_REQ && type != DUER_MSG_DONE_NOTIFY) {
        return DUER_MSG_NOT_NEED;
    }

    // check crc8
    if (msg[DUER_MSG_HEAD_CRC_OFFSET] != duer_cal_crc8(msg + MSG_HEAD_LEN, body_len)) {
        DUER_LOGW("check crc8 failed");
        return DUER_MSG_INVALID;
    }

    return type;
}

static void duer_ecdh_context_free(duer_ecdh_context_t *ctx);

static duer_ecdh_context_t *duer_ecdh_context_init()
{
    int ret = 0;
    size_t olen = 0;
    duer_ecdh_context_t *ctx = (duer_ecdh_context_t *)DUER_MALLOC(sizeof(*ctx));

    if (ctx == NULL) {
        DUER_LOGE("No free memory!");
        return NULL;
    }
    DUER_MEMSET(ctx, 0, sizeof(*ctx));

    mbedtls_ctr_drbg_init(&ctx->ctr_drbg);
    mbedtls_entropy_init(&ctx->entropy);
    mbedtls_ecdh_init(&ctx->ecdh_ctx);

    do {
        ret = mbedtls_ctr_drbg_seed(&ctx->ctr_drbg, mbedtls_entropy_func, &ctx->entropy, NULL, 0);
        if (ret != 0) {
            DUER_LOGE("mbedtls_ctr_drbg_seed failed");
            ret = -1;
            break;
        }

        ret = mbedtls_ecp_group_load(&ctx->ecdh_ctx.grp, MBEDTLS_ECP_DP_SECP256R1);
        if (ret != 0) {
            DUER_LOGE("mbedtls_ctr_drbg_seed failed");
            break;
        }

        ret = mbedtls_ecdh_make_public(&ctx->ecdh_ctx, &olen, ctx->public_point,
                                       sizeof(ctx->public_point), mbedtls_ctr_drbg_random, &ctx->ctr_drbg);
        if (ret != 0 || olen != sizeof(ctx->public_point)) {
            DUER_LOGE("mbedtls_ecdh_make_public failed");
            break;
        }

    } while (0);

    if (ret != 0) {
        duer_ecdh_context_free(ctx);
        ctx = NULL;
    }

    return ctx;
}

static void duer_ecdh_context_free(duer_ecdh_context_t *ctx)
{
    if (ctx != NULL) {
        mbedtls_ecdh_free(&ctx->ecdh_ctx);
        mbedtls_entropy_free(&ctx->entropy);
        mbedtls_ctr_drbg_free(&ctx->ctr_drbg);
        DUER_FREE(ctx);
        ctx = NULL;
    }
}

static uint8_t duer_write_data_to_msg(uint8_t *buf, uint8_t tag, uint8_t *data, uint8_t len)
{
    if (buf == NULL || data == NULL || len == 0) {
        DUER_LOGE("invalid arguments");
        return 0;
    }

    buf[0] = tag;
    buf[1] = len;
    DUER_MEMCPY(buf + BODY_SEG_HEAD_LEN, data, len);

    return len + BODY_SEG_HEAD_LEN;
}

/*
 * the format of discovery_resp_msg is
 * Tag length DeviceEccPubKey Tag length deviceUuid Tag length AES(bindToken)
 * 1Byte:0x21 1Byte Byte* 1Byte:0x22 1Byte Byte* 1Byte:0x23 1Byte Byte*
 */
static int duer_pack_discovery_resp_msg(uint8_t **msg, uint16_t *len)
{
    uint8_t *buf = NULL;
    uint8_t *enc_token = NULL;
    uint8_t id_len = 0;
    uint8_t rlen = 0;
    uint16_t body_len = 0;
    int ret = -1;

    if (msg == NULL || *msg != NULL || len == NULL) {
        DUER_LOGE("invalid arguments");
        return -1;
    }

    do {
        enc_token = duer_aes_cbc_encrypt_bind_token(s_bind_token);
        if (enc_token == NULL) {
            s_log_code = DUER_DS_LOG_BIND_ENCRYPT_FAILED;
            break;
        }

        // calculate the length of body
        body_len += BODY_SEG_HEAD_LEN;
        body_len += sizeof(s_ecdh_context->public_point);

        body_len += BODY_SEG_HEAD_LEN;

        id_len = DUER_STRLEN(s_device_uuid);
        body_len += id_len;

        body_len += BODY_SEG_HEAD_LEN;
        body_len += BIND_TOKEN_LEN;

        *len = body_len + MSG_HEAD_LEN;
        buf = (uint8_t *)DUER_MALLOC(*len);
        if (buf == NULL) {
            DUER_LOGE("No free memory");
            s_log_code = DUER_DS_LOG_BIND_NO_MEMORY;
            break;
        }
        *msg = buf;

        // write head
        // magic number
        DUER_MEMCPY(buf, MAGIC_NUMBER, sizeof(MAGIC_NUMBER));

        // version
        buf[DUER_MSG_HEAD_VERSION_OFFSET] = VERSION;

        // the length of body
        buf[DUER_MSG_HEAD_BODY_LEN_OFFSET] = (uint8_t)(body_len >> 8);
        buf[DUER_MSG_HEAD_BODY_LEN_OFFSET + 1] = (uint8_t)body_len;

        // msg type
        buf[DUER_MSG_HEAD_MSG_TYPE_OFFSET] = (uint8_t)DUER_MSG_DISCOVERY_RESP;

        // write body
        buf += MSG_HEAD_LEN;

        rlen = duer_write_data_to_msg(buf, DUER_TAG_DEVICE_ECC_PUB, s_ecdh_context->public_point,
                                      sizeof(s_ecdh_context->public_point));
        buf += rlen;

        rlen = duer_write_data_to_msg(buf, DUER_TAG_DEVICE_ID, (uint8_t *)s_device_uuid, id_len);
        buf += rlen;

        rlen = duer_write_data_to_msg(buf, DUER_TAG_AEC_BIND_TOKEN, enc_token, BIND_TOKEN_LEN);

        // crc8 of body
        (*msg)[DUER_MSG_HEAD_CRC_OFFSET] = duer_cal_crc8(*msg + MSG_HEAD_LEN, body_len);
        ret = 0;
    } while (0);

    return ret;
}

static duer_handle_discovery_request_t duer_handle_discovery_req(uint8_t *data, uint16_t len)
{
    size_t olen = 0;
    int ret = 0;

    if (data == NULL || len < BODY_SEG_HEAD_LEN) {
        return DUER_INVALID_REQUEST;
    }

    len -= BODY_SEG_HEAD_LEN;
    // check tag and length
    if (data[0] != DUER_TAG_APP_ECC_PUB || data[1] != len) {
        DUER_LOGE("invalid descovery request");
        return DUER_INVALID_REQUEST;
    }
    data += BODY_SEG_HEAD_LEN;

    if (DUER_MEMCMP(data, s_ecdh_context->app_pub, len) == 0) {
        DUER_LOGI("repetitive request");
        return DUER_REPETITIVE_REQUEST;
    }

    ret = mbedtls_ecdh_read_public(&s_ecdh_context->ecdh_ctx, data, len);
    if (0 != ret) {
        DUER_LOGE("read public failed");
        return DUER_INVALID_REQUEST;
    }

    ret = mbedtls_ecdh_calc_secret(&s_ecdh_context->ecdh_ctx, &olen, s_ecdh_context->shared_secret,
                                   sizeof(s_ecdh_context->shared_secret), mbedtls_ctr_drbg_random,
                                   &s_ecdh_context->ctr_drbg);
    if (0 != ret) {
        DUER_LOGE("mbedtls_ecdh_calc_secret failed");
        return DUER_INVALID_REQUEST;
    }

#ifdef BIND_DEBUG
    duer_dump_bytes(s_ecdh_context->shared_secret, SHARED_SECRET_LEN, "shared_secret");
#endif

    DUER_MEMCPY(s_ecdh_context->app_pub, data, len);

    return DUER_NEW_REQUEST;
}

static void duer_security_bind_device(int what, void *object)
{
    struct sockaddr_in local_addr;
    struct sockaddr_in remote_addr;
    socklen_t addr_len = sizeof(remote_addr);
    const uint16_t BUF_LEN = 200;
    uint16_t resp_len = 0;
    uint16_t recv_len = 0;
    uint8_t *buf = NULL;
    uint8_t *resp_buf = NULL;
    int ret = 0;
    int err = 0;
    int fd = -1;
    int optval = 1;
    int resp_sended = 0;
    size_t start = duer_timestamp();
    size_t now = start;
    size_t cost = 0;

    DUER_LOGI("duer_security_bind_device");
    duer_ds_log_bind(DUER_DS_LOG_BIND_TASK_START);
    s_log_code = DUER_DS_LOG_BIND_TASK_END;

    do {
        buf = (uint8_t *)DUER_MALLOC(BUF_LEN);
        if (buf == NULL) {
            DUER_LOGE("buf allocate fail");
            s_log_code = DUER_DS_LOG_BIND_NO_MEMORY;
            break;
        }

        s_ecdh_context = duer_ecdh_context_init();
        if (s_ecdh_context == NULL) {
            DUER_LOGE("init ecdh context failed");
            s_log_code = DUER_DS_LOG_BIND_INIT_AES_FAILED;
            break;
        }

        fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); //IPPROTO_UDP
        if (fd < 0) {
            DUER_LOGE("failed to create socket!");
            s_log_code = DUER_DS_LOG_BIND_INIT_SOCKET_FAILED;
            break;
        }

        ret = setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (void *)&optval, sizeof(optval));
        if (ret < 0) {
            DUER_LOGE("setsockopt failed");
            s_log_code = DUER_DS_LOG_BIND_INIT_SOCKET_FAILED;
            break;
        }

        DUER_MEMSET(&local_addr, 0, sizeof(local_addr));
        local_addr.sin_family = AF_INET;
        local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        local_addr.sin_port = htons(UDP_PORT);

        ret = bind(fd, (const struct sockaddr *)&local_addr, sizeof(local_addr));
        if (ret < 0) {
            err = duer_get_socket_errno(fd);
            DUER_LOGE("airkiss bind local port ERROR! errno %d", err);
            s_log_code = DUER_DS_LOG_BIND_BIND_SOCKET_FAILED;
            break;
        }

        while (!s_stop_send && s_timeout > cost) {
            struct timeval tv;
            fd_set rfds;

            FD_ZERO(&rfds);
            FD_SET(fd, &rfds);

            tv.tv_sec = 1;
            tv.tv_usec = 0;

            ret = select(fd + 1, &rfds, NULL, NULL, &tv);
            if (ret > 0) {
                recv_len = recvfrom(fd, buf, BUF_LEN, 0, (struct sockaddr *)&remote_addr,
                                    (socklen_t *)&addr_len);

                duer_bind_msg_type_t type = duer_bind_msg_recv(buf, recv_len);
                switch (type) {
                case DUER_MSG_DISCOVERY_REQ:
                    DUER_LOGD("DUER_MSG_DISCOVERY_REQ");
                    duer_handle_discovery_request_t req = duer_handle_discovery_req(
                            buf + MSG_HEAD_LEN, recv_len - MSG_HEAD_LEN);
                    if (req == DUER_INVALID_REQUEST) {
                        break;
                    }

                    if (ret == DUER_NEW_REQUEST) {
                        if (resp_buf) {
                            DUER_FREE(resp_buf);
                            resp_buf = NULL;
                        }

                        ret = duer_pack_discovery_resp_msg(&resp_buf, &resp_len);
                        if (ret < 0) {
                            DUER_LOGE("Pack device info packet error!");
                            break;
                        }
#ifdef BIND_DEBUG
                        duer_dump_bytes(resp_buf, resp_len, "resp_msg");
#endif
                    }

                    remote_addr.sin_port = htons(UDP_PORT);
                    ret = sendto(fd, resp_buf, resp_len, 0,
                                 (struct sockaddr *)&remote_addr, addr_len);
                    if (ret < 0) {
                        err = duer_get_socket_errno(fd);
                        DUER_LOGE("send resp msg ERROR! errno %d", err);
                        s_log_code = DUER_DS_LOG_BIND_SOCKET_ERROR;
                    } else {
                        if (!resp_sended) {
                            duer_ds_log_bind(DUER_DS_LOG_BIND_SEND_RESP);
                        }
                        resp_sended = 1;
                        DUER_LOGI("send resp msg OK!");
                    }
                    break;
                case DUER_MSG_DONE_NOTIFY:
                    if (resp_sended) {
                        DUER_LOGI("bind done:%d!", buf[MSG_HEAD_LEN + 2]);
                        duer_stop_security_bind_device_task();
                    }
                    break;
                default:
                    DUER_LOGD("Pack is not valid msg!");
                    break;
                }
            } else {
                DUER_LOGI("recv nothing!");
            }

            now = duer_timestamp();
            cost = now > start ? now - start : (UINT_MAX - start + now);
        }
    } while (0);

    if (fd > -1) {
        close(fd);
        fd = -1;
    }

    if (buf) {
        DUER_FREE(buf);
        buf = NULL;
    }

    if (resp_buf) {
        DUER_FREE(resp_buf);
        resp_buf = NULL;
    }

    if (s_ecdh_context) {
        duer_ecdh_context_free(s_ecdh_context);
        s_ecdh_context = NULL;
    }

    if (s_device_uuid != NULL) {
        DUER_FREE(s_device_uuid);
        s_device_uuid = NULL;
    }

    if (s_bind_token != NULL) {
        DUER_FREE(s_bind_token);
        s_bind_token = NULL;
    }

    duer_events_destroy(s_events);
    s_events = NULL;

    duer_ds_log_bind(s_log_code);
}

duer_status_t duer_start_security_bind_device_task(const char *uuid,
        const char *bind_token, size_t lifecycle)
{
    DUER_LOGI("duer_start_security_bind_device_task");
    int ret = DUER_ERR_FAILED;
    int len = 0;
    if (uuid == NULL || uuid[0] == 0 || bind_token == NULL || bind_token[0] == 0) {
        DUER_LOGE("invalid uuid or bind_token");
        duer_ds_log_bind(DUER_DS_LOG_BIND_INVALID_PARAMS);
        return DUER_ERR_FAILED;
    }

    if (s_events != NULL) {
        DUER_LOGE("a task has been started!");
        duer_ds_log_bind(DUER_DS_LOG_BIND_START_TASK_FAILED);
        return DUER_ERR_FAILED;
    }

    if (lifecycle < MIN_LIFE_CYCLE) {
        DUER_LOGW("the lifecycle must not less than %s seconds", MIN_LIFE_CYCLE);
        lifecycle = MIN_LIFE_CYCLE;
    }

    s_timeout = lifecycle * 1000;

    do {
        len = DUER_STRLEN(uuid) + 1;
        s_device_uuid = (char *)DUER_MALLOC(len);
        if (s_device_uuid == NULL) {
            DUER_LOGE("no free memory");
            s_log_code = DUER_DS_LOG_BIND_NO_MEMORY;
            break;
        }
        DUER_MEMCPY(s_device_uuid, uuid, len);

        len = DUER_STRLEN(bind_token) + 1;
        s_bind_token = (char *)DUER_MALLOC(len);
        if (s_bind_token == NULL) {
            DUER_LOGE("no free memory");
            s_log_code = DUER_DS_LOG_BIND_NO_MEMORY;
            break;
        }
        DUER_MEMCPY(s_bind_token, bind_token, len);

        s_events = duer_events_create("bind_device", STACK_SIZE, 1);
        if (s_events == NULL) {
            DUER_LOGE("create events failed");
            s_log_code = DUER_DS_LOG_BIND_START_TASK_FAILED;
            break;
        }

        s_stop_send = false;
        duer_events_call(s_events, duer_security_bind_device, 0, NULL);

        ret = DUER_OK;
    } while (0);

    if (ret != DUER_OK) {
        if (s_device_uuid != NULL) {
            DUER_FREE(s_device_uuid);
            s_device_uuid = NULL;
        }

        if (s_bind_token != NULL) {
            DUER_FREE(s_bind_token);
            s_bind_token = NULL;
        }
        duer_ds_log_bind(s_log_code);
    }

    return DUER_OK;
}

duer_status_t duer_stop_security_bind_device_task(void)
{
    s_stop_send = true;

    return DUER_OK;
}

#endif // ENABLE_SECURITY_BIND_DEVICE
