/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#include <stdint.h>
#include <string.h>

#include "state_wrap.h"

typedef struct state_wrap_header_v1 {
    char magic[8];
    uint32_t version;
    uint32_t headerSize;
    uint32_t payloadSize;
    uint32_t textBaseAddr;
    uint32_t dataBaseAddr;
    uint32_t bssBaseAddr;
} state_wrap_header_v1_t;

typedef struct state_wrap_header_v2 {
    char magic[8];
    uint32_t version;
    uint32_t headerSize;
    uint32_t payloadSize;
    uint32_t textBaseAddr;
    uint32_t dataBaseAddr;
    uint32_t bssBaseAddr;
    uint32_t baseMapMode;
    uint32_t baseMapStackCount;
} state_wrap_header_v2_t;

static const char state_wrap_magic[8] = { 'E', '9', 'K', 'S', 'T', 'A', 'T', 'E' };
static const uint32_t state_wrap_version = 2;

size_t
state_wrap_headerSize(void)
{
    size_t headerSize = sizeof(state_wrap_header_v2_t);
    if (base_map_getMode() == BASE_MAP_MODE_STACK) {
        headerSize += base_map_getStackCount() * sizeof(state_wrap_base_entry_t);
    }
    return headerSize;
}

size_t
state_wrap_wrappedSize(size_t payloadSize)
{
    return state_wrap_headerSize() + payloadSize;
}

int
state_wrap_writeHeader(uint8_t *dst, size_t dstCap, size_t payloadSize, const machine_t *machine)
{
    if (!dst) {
        return 0;
    }

    base_map_mode_t mode = base_map_getMode();
    size_t stackCount = 0;
    if (mode == BASE_MAP_MODE_STACK) {
        stackCount = base_map_getStackCount();
    }
    size_t headerSize = sizeof(state_wrap_header_v2_t) + (stackCount * sizeof(state_wrap_base_entry_t));
    if (dstCap < headerSize + payloadSize) {
        return 0;
    }

    state_wrap_header_v2_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    memcpy(hdr.magic, state_wrap_magic, sizeof(hdr.magic));
    hdr.version = state_wrap_version;
    hdr.headerSize = (uint32_t)headerSize;
    hdr.payloadSize = (uint32_t)payloadSize;
    hdr.textBaseAddr = machine ? machine->textBaseAddr : 0;
    hdr.dataBaseAddr = machine ? machine->dataBaseAddr : 0;
    hdr.bssBaseAddr = machine ? machine->bssBaseAddr : 0;
    hdr.baseMapMode = (uint32_t)mode;
    hdr.baseMapStackCount = (uint32_t)stackCount;

    memcpy(dst, &hdr, sizeof(hdr));
    uint8_t *entryPtr = dst + sizeof(hdr);
    for (size_t i = 0; i < stackCount; ++i) {
        base_map_section_t section = BASE_MAP_SECTION_TEXT;
        uint32_t base = 0;
        uint32_t size = BASE_MAP_INVALID_SIZE;
        if (!base_map_getStackEntry(i, &section, &base, &size)) {
            return 0;
        }
        state_wrap_base_entry_t entry;
        entry.section = (uint32_t)section;
        entry.base = base;
        entry.size = size;
        memcpy(entryPtr + i * sizeof(entry), &entry, sizeof(entry));
    }
    return 1;
}

int
state_wrap_wrap(uint8_t *dst, size_t dstCap, const uint8_t *payload, size_t payloadSize, const machine_t *machine)
{
    if (!dst || !payload || payloadSize == 0) {
        return 0;
    }
    if (!state_wrap_writeHeader(dst, dstCap, payloadSize, machine)) {
        return 0;
    }
    state_wrap_header_v1_t hdr;
    memcpy(&hdr, dst, sizeof(hdr));
    memcpy(dst + hdr.headerSize, payload, payloadSize);
    return 1;
}

int
state_wrap_parse(const uint8_t *buf, size_t bufSize, state_wrap_info_t *out)
{
    if (!buf || !out) {
        return 0;
    }
    memset(out, 0, sizeof(*out));

    if (bufSize < sizeof(state_wrap_header_v1_t)) {
        return 0;
    }
    state_wrap_header_v1_t hdr;
    memcpy(&hdr, buf, sizeof(hdr));
    if (memcmp(hdr.magic, state_wrap_magic, sizeof(state_wrap_magic)) != 0) {
        return 0;
    }
    if (hdr.headerSize < sizeof(state_wrap_header_v1_t)) {
        return 0;
    }
    if ((size_t)hdr.headerSize > bufSize) {
        return 0;
    }
    if ((size_t)hdr.headerSize + (size_t)hdr.payloadSize > bufSize) {
        return 0;
    }

    out->version = hdr.version;
    out->textBaseAddr = hdr.textBaseAddr;
    out->dataBaseAddr = hdr.dataBaseAddr;
    out->bssBaseAddr = hdr.bssBaseAddr;
    out->mode = BASE_MAP_MODE_BASIC;
    out->baseMapStackCount = 0;
    out->baseMapStackData = NULL;

    if (hdr.version >= 2 && hdr.headerSize >= sizeof(state_wrap_header_v2_t)) {
        state_wrap_header_v2_t hdr2;
        memcpy(&hdr2, buf, sizeof(hdr2));
        if (hdr2.baseMapMode == (uint32_t)BASE_MAP_MODE_STACK) {
            out->mode = BASE_MAP_MODE_STACK;
        } else {
            out->mode = BASE_MAP_MODE_BASIC;
        }
        size_t stackCount = (size_t)hdr2.baseMapStackCount;
        size_t entriesBytes = stackCount * sizeof(state_wrap_base_entry_t);
        if ((size_t)hdr2.headerSize < sizeof(state_wrap_header_v2_t) + entriesBytes) {
            return 0;
        }
        if (out->mode == BASE_MAP_MODE_STACK && stackCount > 0) {
            out->baseMapStackCount = stackCount;
            out->baseMapStackData = buf + sizeof(state_wrap_header_v2_t);
        }
    }

    out->payload = buf + hdr.headerSize;
    out->payloadSize = (size_t)hdr.payloadSize;
    return 1;
}

int
state_wrap_getBaseMapStackEntry(const state_wrap_info_t *info, size_t index, state_wrap_base_entry_t *outEntry)
{
    if (!info || !outEntry || !info->baseMapStackData || index >= info->baseMapStackCount) {
        return 0;
    }
    const uint8_t *ptr = info->baseMapStackData + index * sizeof(*outEntry);
    memcpy(outEntry, ptr, sizeof(*outEntry));
    return 1;
}
