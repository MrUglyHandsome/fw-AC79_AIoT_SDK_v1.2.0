// Copyright (2018) Baidu Inc. All rights reserved.
/**
 * File: lightduer_flash_strings.c
 * Auth: Gang Chen(chengang12@baidu.com)
 * Desc: APIs of store string list to flash.
 */
#include "lightduer_flash_ring_buf.h"
#include <string.h>
#include <stdio.h>
#include "lightduer_log.h"
#include "lightduer_memory.h"

#define PAGE_SIZE                (1 << s_flash_config.page_align_bits)
static duer_flash_config_t s_flash_config = {0};
static duer_bool s_is_flash_config_set = DUER_FALSE;

void duer_flash_ring_buf_set_config(const duer_flash_config_t *config)
{
    if (!s_is_flash_config_set) {
        memcpy(&s_flash_config, config, sizeof(s_flash_config));
        s_is_flash_config_set = DUER_TRUE;
    }
}

void duer_flash_ring_buf_load(duer_flash_ring_buf_context_t *ctx)
{
    duer_flash_data_header data = {0, 0, 0};
    unsigned int addr = 0;
    int head_sequence = 0;

    if (!s_is_flash_config_set) {
        DUER_LOGE("Please set flash config first");
        return;
    }

    if (!ctx) {
        DUER_LOGE("Flash module haven't init, call duer_flash_init first.");
        return;
    }

    ctx->ele_count = 0;
    ctx->ele_header = 0;
    ctx->ele_tail = 0;

    for (addr = 0; addr < ctx->ctx.len; addr += PAGE_SIZE) {
        duer_flash_read(&ctx->ctx, addr, (char *)&data, sizeof(data));
        if (data.magic == FLASH_MAGIC) {
            DUER_LOGD("\nele_count=%d\n", ctx->ele_count);
            if (ctx->ele_count == 0) {
                ctx->ele_header = addr;
                head_sequence = data.sequence_num;
            }
            if (head_sequence > data.sequence_num) {
                ctx->ele_header = addr;
                head_sequence = data.sequence_num;
            }
            if (ctx->sequence_num <= data.sequence_num) {
                ctx->ele_tail = addr;
                ctx->sequence_num = data.sequence_num + 1;
            }
            ctx->ele_count++;
        }
    }

    DUER_LOGD("ele_header=%d, ele_tail=%d, sequence_num=%d", ctx->ele_header, ctx->ele_tail, ctx->sequence_num);
}

void duer_read_ele_from_flash(duer_flash_ring_buf_context_t *ctx,
                              unsigned int addr,
                              char *buf,
                              int len)
{
    int first_part_len = 0;

    if (addr + sizeof(duer_flash_data_header) + len > ctx->ctx.len) {
        first_part_len = addr + sizeof(duer_flash_data_header) + len - ctx->ctx.len;
        duer_flash_read(&ctx->ctx, addr + sizeof(duer_flash_data_header), buf, first_part_len);
        duer_flash_read(&ctx->ctx, 0, buf + first_part_len, len - first_part_len);
    } else {
        duer_flash_read(&ctx->ctx, addr + sizeof(duer_flash_data_header), buf, len);
    }
}

char *duer_flash_ring_buf_top(duer_flash_ring_buf_context_t *ctx)
{
    int rs = 0;
    char *top = NULL;
    duer_flash_data_header data;

    if (ctx->ele_count == 0) {
        return NULL;
    }

    rs = duer_flash_read(&ctx->ctx, ctx->ele_header, (char *)&data, sizeof(data));
    if (rs < 0 || data.magic != FLASH_MAGIC) {
        DUER_LOGE("Failed to read flash");
        return NULL;
    }

    top = DUER_MALLOC(data.len);
    if (!top) {
        DUER_LOGE("Memory too low");
        return NULL;
    }

    duer_read_ele_from_flash(ctx, ctx->ele_header, top, data.len);

    return top;
}

duer_status_t duer_flash_ring_buf_header_remove(duer_flash_ring_buf_context_t *ctx)
{
    duer_flash_data_header data;
    unsigned int addr = 0;
    unsigned int new_header = 0;
    char *empty_data = NULL;

    if (ctx->ele_count == 0) {
        DUER_LOGW("No data in flash");
        return DUER_ERR_FAILED;
    }

    duer_flash_read(&ctx->ctx, ctx->ele_header, (char *)&data, sizeof(data));
    if (data.magic != FLASH_MAGIC) {
        return DUER_ERR_FAILED;
    }

    new_header = ctx->ele_header + sizeof(data) + data.len;
    new_header = flash_addr_align_ceil(new_header, s_flash_config.page_align_bits);
    new_header = flash_addr_cycle(new_header, ctx->ctx.len);
    DUER_LOGD("\nnew_header:%d\n", new_header);

    empty_data = DUER_MALLOC(PAGE_SIZE);
    if (!empty_data) {
        return DUER_ERR_MEMORY_OVERFLOW;
    }
    memset(empty_data, 0, PAGE_SIZE);
    duer_flash_write(&ctx->ctx, ctx->ele_header, empty_data, PAGE_SIZE);
    DUER_FREE(empty_data);

    ctx->ele_header = new_header;
    ctx->ele_count--;
    DUER_LOGI("ele_count:%d, ctx->sequence_num=%d", ctx->ele_count, ctx->sequence_num);

    if (ctx->ele_count == 0) {
        ctx->ele_header = 0;
        ctx->ele_tail = 0;
        ctx->sequence_num = 0;
        return DUER_OK;
    }

    for (addr = ctx->ele_header; addr < ctx->ctx.len; addr += PAGE_SIZE) {
        duer_flash_read(&ctx->ctx, addr, (char *)&data, sizeof(data));
        if (data.magic == FLASH_MAGIC) {
            ctx->ele_header = addr;
            return DUER_OK;
        }
    }

    for (addr = 0; addr < ctx->ctx.len; addr += PAGE_SIZE) {
        duer_flash_read(&ctx->ctx, addr, (char *)&data, sizeof(data));
        if (data.magic == FLASH_MAGIC) {
            ctx->ele_header = addr;
            return DUER_OK;
        }
    }

    return DUER_ERR_FAILED;
}

static void duer_flash_erase_used_sector(duer_flash_ring_buf_context_t *ctx,
        unsigned int addr_start,
        unsigned int addr_end)
{
    unsigned int sector_start = 0;
    unsigned int sector_end = 0;
    unsigned int addr = 0;
    unsigned int addr_read = 0;
    unsigned int limit = 0;
    unsigned int tail = 0;
    duer_flash_data_header data = {0, 0, 0};

    tail = (addr_start == 0) ? ctx->ctx.len : addr_start;
    sector_start = flash_addr_align_floor(addr_start, s_flash_config.sector_align_bits);
    if (addr_start > sector_start) {
        sector_start += (1 << s_flash_config.sector_align_bits);
    }
    sector_start = flash_addr_cycle(sector_start, ctx->ctx.len);
    sector_end = flash_addr_align_ceil(addr_end, s_flash_config.sector_align_bits);
    if (sector_end > ctx->ctx.len) {
        sector_end = flash_addr_cycle(sector_end, ctx->ctx.len);
    }

    // the valid data storaged in flash might be erased
    if ((sector_end > ctx->ele_header) && (sector_end <= tail)) {
        limit = tail;
    }

    if (ctx->ele_header >= tail) {
        if ((sector_end > ctx->ele_header) || (sector_end <= tail)) {
            limit = tail + ctx->ctx.len;
        }
    }

    if (limit != 0) {
        // update the ele_count
        addr = sector_end;
        while (addr < limit) {
            addr_read = flash_addr_cycle(addr, ctx->ctx.len);
            duer_flash_read(&ctx->ctx, addr_read, (char *)&data, sizeof(data));
            if (data.magic == FLASH_MAGIC) {
                DUER_LOGD("data.sequence_num=%d", data.sequence_num);
                ctx->ele_header = addr_read;
                if (ctx->sequence_num > data.sequence_num) {
                    ctx->ele_count = ctx->sequence_num - data.sequence_num;
                }
                break;
            }
            addr += PAGE_SIZE;
        }

        // all of the valid data would be erased
        if (addr >= limit) {
            ctx->ele_count = 0;
        }
    }

    DUER_LOGD("sector_start=%d, sector_end=%d", sector_start, sector_end);
    if (sector_start > sector_end) {
        duer_flash_erase(&ctx->ctx, sector_start, ctx->ctx.len - sector_start);
        if (sector_end > 0) {
            duer_flash_erase(&ctx->ctx, 0, sector_end);
        }
    } else if (sector_end > sector_start) {
        duer_flash_erase(&ctx->ctx, sector_start, sector_end - sector_start);
    }
}

duer_status_t duer_flash_ring_buf_append(duer_flash_ring_buf_context_t *ctx, const char *msg)
{
    unsigned int addr_start = 0;
    unsigned int addr_end = 0;
    unsigned int offset_in_sector = 0;
    duer_flash_data_header data_add = {0, 0, 0};
    duer_flash_data_header data = {0, 0, 0};
    int payload_len = 0;
    int rs = DUER_OK;
    int sector_size = 1 << s_flash_config.sector_align_bits;

    if (!msg || !ctx) {
        DUER_LOGE("Param wrong!");
        rs = DUER_ERR_INVALID_PARAMETER;
        goto RET;
    }

    data_add.magic = FLASH_MAGIC;
    payload_len = flash_addr_align_ceil(strlen(msg) + 1, s_flash_config.word_align_bits);

    addr_start = ctx->ele_tail;

    if (ctx->ele_count > 0) {
        duer_flash_read(&ctx->ctx, ctx->ele_tail, (char *)&data, sizeof(data));
        if (data.magic == FLASH_MAGIC) {
            DUER_LOGD("data.sequence_num=%d", data.sequence_num);
            addr_start = ctx->ele_tail + sizeof(data) + data.len;
            addr_start = flash_addr_align_ceil(addr_start, s_flash_config.page_align_bits);
            addr_start = flash_addr_cycle(addr_start, ctx->ctx.len);
        }
    }

    offset_in_sector = addr_start & (sector_size - 1);
    if (payload_len + sizeof(duer_flash_data_header) + offset_in_sector > ctx->ctx.len) {
        DUER_LOGE("Data is too long");
        rs = DUER_ERR_FAILED;
        goto RET;
    }

    addr_end = addr_start + sizeof(duer_flash_data_header) + payload_len;
    if (addr_end > ctx->ctx.len) {
        addr_end = flash_addr_cycle(addr_end, ctx->ctx.len);
    }

    DUER_LOGD("addr_start=%d, %d", addr_start, (1 << s_flash_config.sector_align_bits));
    DUER_LOGD("ctx->sequence_num=%d", ctx->sequence_num);

    if ((payload_len + sizeof(duer_flash_data_header) + offset_in_sector > sector_size)
        || offset_in_sector == 0) {
        // Erase next used sectors.
        duer_flash_erase_used_sector(ctx, addr_start, addr_end);
    }

    // Write data into flash.
    data_add.len = payload_len;
    data_add.sequence_num = ctx->sequence_num;
    duer_payload_write_flash(&ctx->ctx, addr_start, &data_add, msg, payload_len, PAGE_SIZE);
    DUER_LOGD("write payload to flash[%08x]: %s", addr_start, msg);

RET:
    if (rs == DUER_OK) {
        ctx->ele_tail = addr_start;
        ctx->ele_count++;
        ctx->sequence_num++;
    }

    if (ctx) {
        DUER_LOGI("ele_count:%d, ctx->sequence_num=%d", ctx->ele_count, ctx->sequence_num);
    }

    return rs;
}

