/**
 * Copyright (2019) Baidu Inc. All rights reserved.
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
 * File: lightduer_ecdh.c
 * Auth: Chen Xihao(chenxihao@baidu.com)
 * Desc: Provide the ecdh API.
 */

#ifdef DUER_MBEDTLS_ECDH
#include "lightduer_ecdh.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lightduer_memory.h"
#include "lightduer_lib.h"
#include "lightduer_log.h"

duer_ecdh_context_t *duer_ecdh_context_init()
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
        }

    } while (0);

    if (ret != 0) {
        duer_ecdh_context_free(ctx);
        ctx = NULL;
    }

    return ctx;
}

int duer_ecdh_calc_secret(duer_ecdh_context_t *ctx, uint8_t *app_pub, uint16_t len)
{
    size_t olen = 0;
    int ret = 0;

    if (ctx == NULL || app_pub == NULL || len == 0) {
        DUER_LOGE("duer_ecdh_calc_secret invalid arguments");
        return -1;
    }

    ret = mbedtls_ecdh_read_public(&ctx->ecdh_ctx, app_pub, len);
    if (0 != ret) {
        DUER_LOGE("read public failed");
        return -1;
    }

    ret = mbedtls_ecdh_calc_secret(&ctx->ecdh_ctx, &olen, ctx->shared_secret,
                                   sizeof(ctx->shared_secret), mbedtls_ctr_drbg_random, &ctx->ctr_drbg);
    if (0 != ret) {
        DUER_LOGE("mbedtls_ecdh_calc_secret failed");
        return -1;
    }

    return 0;
}

int duer_ecdh_make_public(duer_ecdh_context_t *ctx)
{
    size_t olen = 0;
    int ret = 0;

    if (ctx == NULL) {
        DUER_LOGE("duer_ecdh_make_public invalid arguments");
        return -1;
    }

    ret = mbedtls_ecdh_make_public(&ctx->ecdh_ctx, &olen, ctx->public_point,
                                   sizeof(ctx->public_point), mbedtls_ctr_drbg_random, &ctx->ctr_drbg);
    if (ret != 0 || olen != sizeof(ctx->public_point)) {
        DUER_LOGE("mbedtls_ecdh_make_public failed");
        return -1;
    }

    return 0;
}

void duer_ecdh_context_free(duer_ecdh_context_t *ctx)
{
    if (ctx != NULL) {
        mbedtls_ecdh_free(&ctx->ecdh_ctx);
        mbedtls_entropy_free(&ctx->entropy);
        mbedtls_ctr_drbg_free(&ctx->ctr_drbg);
        DUER_FREE(ctx);
    }
}
#endif
