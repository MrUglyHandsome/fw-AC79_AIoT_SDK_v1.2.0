// Copyright (2018) Baidu Inc. All rights reserved.
/**
 * File: lightduer_flash.c
 * Auth: Sijun Li(lisijun@baidu.com)
 * Desc: APIs used for flash strings module locally.
 */
#include "lightduer_flash.h"
#include <string.h>
#include <stdio.h>
#include "lightduer_log.h"
#include "lightduer_memory.h"

int duer_payload_write_flash(
    duer_flash_context_t *ctx,
    unsigned int addr_start,
    duer_flash_data_header *p_flash_header,
    const char *payload_string,
    int payload_len,
    int page_size)
{
    int part1_len = 0;
    int part2_len = 0;
    int i = 0;
    char *page_cache = NULL;

    page_cache = (char *)DUER_MALLOC(page_size);
    memset(page_cache, 0, page_size);

    // Put header into cache.
    if (payload_string) {
        if (page_size - sizeof(duer_flash_data_header) >= payload_len) {
            i = payload_len;
        } else {
            i = page_size - sizeof(duer_flash_data_header);
        }
        memcpy(page_cache + sizeof(duer_flash_data_header), payload_string, i);
    } else {
        duer_flash_read(ctx, addr_start, page_cache, page_size);
    }
    memcpy(page_cache, p_flash_header, sizeof(duer_flash_data_header));
    // write header and some of payload
    duer_flash_write(ctx, addr_start, page_cache, page_size);

    // write rest of payload string.
    if (payload_string && i < payload_len) {
        if (addr_start + sizeof(duer_flash_data_header) + payload_len > ctx->len) {
            part1_len = ctx->len - (addr_start + page_size);
            part2_len = addr_start + sizeof(duer_flash_data_header) + payload_len - ctx->len;
            if (part1_len > 0) {
                duer_flash_write(ctx, addr_start + page_size, (void *)(payload_string + i), part1_len);
            }
            duer_flash_write(ctx, 0, (void *)(payload_string + i + part1_len), part2_len);
        } else {
            duer_flash_write(ctx, addr_start + page_size, (void *)(payload_string + i), payload_len - i);
        }
    }

    DUER_FREE(page_cache);

    return DUER_OK;
}

