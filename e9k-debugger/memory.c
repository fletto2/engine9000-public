/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#include <SDL.h>
#include <SDL_ttf.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#include "memory.h"
#include "e9ui_context.h"
#include "e9ui_scale.h"
#include "e9ui_stack.h"
#include "e9ui_hstack.h"
#include "e9ui_textbox.h"
#include "e9ui_text_cache.h"
#include "e9ui_step_buttons.h"
#include "e9ui_scrollbar.h"
#include "debugger.h"
#include "libretro_host.h"

#define MEMORY_SEARCH_MAX_PATTERN 128
#define MEMORY_SEARCH_MAX_RANGES 64
#define MEMORY_SEARCH_CHUNK 4096

typedef struct memory_search_pattern {
    uint8_t ascii[MEMORY_SEARCH_MAX_PATTERN];
    int asciiLen;
    uint8_t hex[MEMORY_SEARCH_MAX_PATTERN];
    int hexLen;
} memory_search_pattern_t;

typedef struct memory_view_state {
    unsigned int   base;
    unsigned int   size;
    unsigned char *data;
    e9ui_component_t *addressBox;
    e9ui_component_t *searchBox;
    uint32_t       searchMatchAddr;
    int            searchMatchLen;
    int            searchMatchValid;
    char           error[128];
    e9ui_step_buttons_state_t stepButtons;
    e9ui_scrollbar_state_t hScrollbar;
    int            scrollX;
    int            contentPixelWidth;
} memory_view_state_t;

static memory_view_state_t *g_memory_view_state = NULL;

static void
memory_fillFromram(memory_view_state_t *st, unsigned int base);

static int
memory_buildSearchPattern(const char *text, memory_search_pattern_t *outPattern);

static int
memory_findNextMatch(memory_view_state_t *st, const memory_search_pattern_t *pattern,
                     uint32_t startAddr, uint32_t *outAddr, int *outLen);

static int
memory_findPrevMatch(memory_view_state_t *st, const memory_search_pattern_t *pattern,
                     uint32_t startAddr, uint32_t *outAddr, int *outLen);

static int
memory_parseU64SmartHex(const char *text, unsigned long long *outValue, char **outEnd)
{
    if (outValue) {
        *outValue = 0;
    }
    if (outEnd) {
        *outEnd = NULL;
    }
    if (!text || !outValue) {
        return 0;
    }
    const char *p = text;
    while (*p && isspace((unsigned char)*p)) {
        ++p;
    }
    int base = 0;
    const char *parseStart = p;
    if (*parseStart == '$') {
        ++parseStart;
        base = 16;
    } else if (!(parseStart[0] == '0' && (parseStart[1] == 'x' || parseStart[1] == 'X'))) {
        for (const char *q = parseStart; *q; ++q) {
            if (isspace((unsigned char)*q)) {
                break;
            }
            if (((*q >= 'a') && (*q <= 'f')) || ((*q >= 'A') && (*q <= 'F'))) {
                base = 16;
                break;
            }
        }
    }
    errno = 0;
    char *end = NULL;
    unsigned long long value = strtoull(parseStart, &end, base);
    if (outEnd) {
        *outEnd = end;
    }
    if (!end || end == parseStart || errno != 0) {
        return 0;
    }
    *outValue = value;
    return 1;
}

static int
memory_getAddressLimits(uint32_t *outMinAddr, uint32_t *outMaxAddr)
{
    if (outMinAddr) {
        *outMinAddr = 0;
    }
    if (outMaxAddr) {
        *outMaxAddr = 0x00ffffffu;
    }
    if (target && target->memoryGetLimits) {
        return target->memoryGetLimits(outMinAddr, outMaxAddr);
    }
    return 0;
}

static void
memory_syncTextboxFromBase(memory_view_state_t *st)
{
    if (!st || !st->addressBox) {
        return;
    }
    char addrText[32];
    snprintf(addrText, sizeof(addrText), "0x%06X", st->base & 0x00ffffffu);
    e9ui_textbox_setText(st->addressBox, addrText);
}

static uint32_t
memory_clampBaseForView(memory_view_state_t *st, uint32_t base)
{
    if (!st) {
        return base & 0x00ffffffu;
    }
    uint32_t minAddr = 0;
    uint32_t maxAddr = 0x00ffffffu;
    if (!memory_getAddressLimits(&minAddr, &maxAddr)) {
        minAddr = 0;
        maxAddr = 0x00ffffffu;
    }
    uint64_t span = st->size > 0 ? (uint64_t)(st->size - 1u) : 0ull;
    uint32_t maxBase = maxAddr;
    if ((uint64_t)maxAddr >= span) {
        maxBase = (uint32_t)((uint64_t)maxAddr - span);
    } else {
        maxBase = minAddr;
    }
    if (maxBase < minAddr) {
        maxBase = minAddr;
    }
    uint32_t base24 = base & 0x00ffffffu;
    if (base24 < minAddr) {
        base24 = minAddr;
    }
    if (base24 > maxBase) {
        base24 = maxBase;
    }
    return base24;
}

static void
memory_scrollRows(memory_view_state_t *st, int rows)
{
    if (!st || rows == 0) {
        return;
    }
    int64_t delta = (int64_t)rows * 16ll;
    int64_t rawBase = (int64_t)(uint32_t)(st->base & 0x00ffffffu) + delta;
    if (rawBase < 0) {
        rawBase = 0;
    }
    uint32_t clamped = memory_clampBaseForView(st, (uint32_t)rawBase);
    if (clamped == st->base) {
        return;
    }
    st->base = clamped;
    memory_syncTextboxFromBase(st);
    memory_fillFromram(st, st->base);
}

static void
memory_setError(memory_view_state_t *panel, const char *fmt, ...)
{
    if (!panel) {
        return;
    }
    panel->error[0] = '\0';
    if (!fmt || !*fmt) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(panel->error, sizeof(panel->error), fmt, ap);
    va_end(ap);
    panel->error[sizeof(panel->error) - 1] = '\0';
}

static void
memory_clearData(memory_view_state_t *panel)
{
    if (!panel || !panel->data) {
        return;
    }
    memset(panel->data, 0, panel->size);
}

static int
memory_parseAddress(memory_view_state_t *st, unsigned int *out_addr)
{
    if (!st || !st->addressBox || !out_addr) {
        return 0;
    }
    const char *t = e9ui_textbox_getText(st->addressBox);
    if (!t || !*t) {
        memory_setError(st, "Invalid address: empty input");
        return 0;
    }
    char *end = NULL;
    unsigned long long val = 0;
    if (!memory_parseU64SmartHex(t, &val, &end)) {
        memory_setError(st, "Invalid address: \"%s\"", t);
        return 0;
    }
    while (*end && isspace((unsigned char)*end)) {
        ++end;
    }
    if (*end) {
        memory_setError(st, "Invalid address: \"%s\"", t);
        return 0;
    }
    if (val > 0x00ffffffull) {
        memory_setError(st, "Address outside 24-bit range (0x000000-0xFFFFFF)");
        return 0;
    }
    uint32_t minAddr = 0;
    uint32_t maxAddr = 0x00ffffffu;
    int hasLimits = memory_getAddressLimits(&minAddr, &maxAddr);
    if (hasLimits && ((uint32_t)val < minAddr || (uint32_t)val > maxAddr)) {
        memory_setError(st, "Address outside range (0x%06X-0x%06X)", minAddr, maxAddr);
        return 0;
    }
    *out_addr = (unsigned int)val;
    return 1;
}

static void
memory_fillFromram(memory_view_state_t *st, unsigned int base)
{
    if (!st || !st->data) {
        return;
    }
    uint32_t minAddr = 0;
    uint32_t maxAddr = 0x00ffffffu;
    int hasLimits = memory_getAddressLimits(&minAddr, &maxAddr);
    memory_setError(st, NULL);
    int range_error = 0;
    for (unsigned int i = 0; i < st->size; ++i) {
        uint32_t addr = base + i;
        if (addr > 0x00ffffffu) {
            st->data[i] = 0;
            range_error = 1;
            continue;
        }
        if (hasLimits && (addr < minAddr || addr > maxAddr)) {
            st->data[i] = 0;
            range_error = 1;
            continue;
        }
        if (!libretro_host_debugReadMemory(addr, &st->data[i], 1)) {
            st->data[i] = 0;
            range_error = 1;
        }
    }
    if (range_error) {
        if (hasLimits) {
            memory_setError(st, "Range exceeds limits (0x%06X-0x%06X)", minAddr, maxAddr);
        } else {
            memory_setError(st, "Range exceeds 24-bit address space (0x000000-0xFFFFFF)");
        }
    }
}

static void
memory_onAddressSubmit(e9ui_context_t *ctx, void *user)
{
    (void)ctx;
    memory_view_state_t *st = (memory_view_state_t*)user;
    if (!st || !st->addressBox) {
        return;
    }
    unsigned int addr = 0;
    if (!memory_parseAddress(st, &addr)) {
        return;
    }
    st->base = memory_clampBaseForView(st, addr);
    memory_syncTextboxFromBase(st);
    memory_fillFromram(st, st->base);
}

static int
memory_collectSearchRanges(target_memory_range_t *outRanges, size_t cap, size_t *outCount)
{
    if (!outRanges || cap == 0 || !outCount) {
        return 0;
    }
    *outCount = 0;
    if (target && target->memoryTrackGetRanges) {
        size_t count = 0;
        if (target->memoryTrackGetRanges(outRanges, cap, &count) && count > 0) {
            size_t write = 0;
            for (size_t i = 0; i < count && write < cap; ++i) {
                if (outRanges[i].size == 0) {
                    continue;
                }
                outRanges[write++] = outRanges[i];
            }
            if (write > 0) {
                *outCount = write;
                return 1;
            }
        }
    }
    uint32_t minAddr = 0;
    uint32_t maxAddr = 0x00ffffffu;
    if (!memory_getAddressLimits(&minAddr, &maxAddr)) {
        minAddr = 0;
        maxAddr = 0x00ffffffu;
    }
    if (maxAddr < minAddr) {
        return 0;
    }
    outRanges[0].baseAddr = minAddr;
    outRanges[0].size = (maxAddr - minAddr) + 1u;
    *outCount = 1;
    return 1;
}

static int
memory_patternMatchesAt(const uint8_t *bytes, int avail, const memory_search_pattern_t *pattern, int *outLen)
{
    int bestLen = 0;
    if (!bytes || !pattern || avail <= 0) {
        return 0;
    }
    if (pattern->asciiLen > 0 && pattern->asciiLen <= avail) {
        if (memcmp(bytes, pattern->ascii, (size_t)pattern->asciiLen) == 0) {
            bestLen = pattern->asciiLen;
        }
    }
    if (pattern->hexLen > 0 && pattern->hexLen <= avail) {
        if (memcmp(bytes, pattern->hex, (size_t)pattern->hexLen) == 0) {
            if (pattern->hexLen > bestLen) {
                bestLen = pattern->hexLen;
            }
        }
    }
    if (bestLen <= 0) {
        return 0;
    }
    if (outLen) {
        *outLen = bestLen;
    }
    return 1;
}

static int
memory_scanRangeForMatch(uint32_t scanStart, uint32_t scanEndInclusive,
                         const memory_search_pattern_t *pattern,
                         uint32_t *outAddr, int *outLen)
{
    if (!pattern || scanEndInclusive < scanStart) {
        return 0;
    }
    int maxPatternLen = pattern->asciiLen > pattern->hexLen ? pattern->asciiLen : pattern->hexLen;
    if (maxPatternLen <= 0) {
        return 0;
    }
    uint8_t chunk[MEMORY_SEARCH_CHUNK];
    uint32_t addr = scanStart;
    while (addr <= scanEndInclusive) {
        uint32_t remaining = scanEndInclusive - addr + 1u;
        size_t want = remaining < MEMORY_SEARCH_CHUNK ? (size_t)remaining : (size_t)MEMORY_SEARCH_CHUNK;
        if (want < (size_t)maxPatternLen && remaining >= (uint32_t)maxPatternLen) {
            want = (size_t)maxPatternLen;
        }
        if (want > MEMORY_SEARCH_CHUNK) {
            want = MEMORY_SEARCH_CHUNK;
        }
        if (!libretro_host_debugReadMemory(addr, chunk, want)) {
            uint32_t step = (uint32_t)want;
            if (step == 0) {
                step = 1;
            }
            if (addr > scanEndInclusive - step + 1u) {
                break;
            }
            addr += step;
            continue;
        }
        for (int i = 0; i < (int)want; ++i) {
            int matchLen = 0;
            if (memory_patternMatchesAt(chunk + i, (int)want - i, pattern, &matchLen)) {
                if (outAddr) {
                    *outAddr = addr + (uint32_t)i;
                }
                if (outLen) {
                    *outLen = matchLen;
                }
                return 1;
            }
        }
        if (remaining <= (uint32_t)maxPatternLen) {
            break;
        }
        uint32_t step = (uint32_t)want - (uint32_t)maxPatternLen + 1u;
        if (step == 0) {
            step = 1;
        }
        if (addr > scanEndInclusive - step + 1u) {
            break;
        }
        addr += step;
    }
    return 0;
}

static int
memory_scanRangeForLastMatch(uint32_t scanStart, uint32_t scanEndInclusive,
                             const memory_search_pattern_t *pattern,
                             uint32_t *outAddr, int *outLen)
{
    if (!pattern || scanEndInclusive < scanStart) {
        return 0;
    }
    int maxPatternLen = pattern->asciiLen > pattern->hexLen ? pattern->asciiLen : pattern->hexLen;
    if (maxPatternLen <= 0) {
        return 0;
    }
    uint8_t chunk[MEMORY_SEARCH_CHUNK];
    uint32_t addr = scanStart;
    uint32_t lastAddr = 0;
    int lastLen = 0;
    int found = 0;
    while (addr <= scanEndInclusive) {
        uint32_t remaining = scanEndInclusive - addr + 1u;
        size_t want = remaining < MEMORY_SEARCH_CHUNK ? (size_t)remaining : (size_t)MEMORY_SEARCH_CHUNK;
        if (want < (size_t)maxPatternLen && remaining >= (uint32_t)maxPatternLen) {
            want = (size_t)maxPatternLen;
        }
        if (want > MEMORY_SEARCH_CHUNK) {
            want = MEMORY_SEARCH_CHUNK;
        }
        if (!libretro_host_debugReadMemory(addr, chunk, want)) {
            uint32_t step = (uint32_t)want;
            if (step == 0) {
                step = 1;
            }
            if (addr > scanEndInclusive - step + 1u) {
                break;
            }
            addr += step;
            continue;
        }
        for (int i = 0; i < (int)want; ++i) {
            int matchLen = 0;
            if (memory_patternMatchesAt(chunk + i, (int)want - i, pattern, &matchLen)) {
                lastAddr = addr + (uint32_t)i;
                lastLen = matchLen;
                found = 1;
            }
        }
        if (remaining <= (uint32_t)maxPatternLen) {
            break;
        }
        uint32_t step = (uint32_t)want - (uint32_t)maxPatternLen + 1u;
        if (step == 0) {
            step = 1;
        }
        if (addr > scanEndInclusive - step + 1u) {
            break;
        }
        addr += step;
    }
    if (!found) {
        return 0;
    }
    if (outAddr) {
        *outAddr = lastAddr;
    }
    if (outLen) {
        *outLen = lastLen;
    }
    return 1;
}

static int
memory_findNextMatch(memory_view_state_t *st, const memory_search_pattern_t *pattern,
                     uint32_t startAddr, uint32_t *outAddr, int *outLen)
{
    (void)st;
    target_memory_range_t ranges[MEMORY_SEARCH_MAX_RANGES];
    size_t rangeCount = 0;
    if (!memory_collectSearchRanges(ranges, MEMORY_SEARCH_MAX_RANGES, &rangeCount) || rangeCount == 0) {
        return 0;
    }
    for (int pass = 0; pass < 2; ++pass) {
        for (size_t i = 0; i < rangeCount; ++i) {
            uint32_t base = ranges[i].baseAddr & 0x00ffffffu;
            uint32_t size = ranges[i].size;
            if (size == 0) {
                continue;
            }
            uint32_t end = base + size - 1u;
            if (end < base) {
                end = 0x00ffffffu;
            }
            uint32_t scanStart = base;
            uint32_t scanEnd = end;
            if (pass == 0) {
                if (startAddr > end) {
                    continue;
                }
                if (startAddr > base) {
                    scanStart = startAddr;
                }
            } else {
                if (startAddr <= base) {
                    continue;
                }
                if (startAddr <= end) {
                    scanEnd = startAddr - 1u;
                }
            }
            if (scanEnd < scanStart) {
                continue;
            }
            if (memory_scanRangeForMatch(scanStart, scanEnd, pattern, outAddr, outLen)) {
                return 1;
            }
        }
    }
    return 0;
}

static int
memory_findPrevMatch(memory_view_state_t *st, const memory_search_pattern_t *pattern,
                     uint32_t startAddr, uint32_t *outAddr, int *outLen)
{
    (void)st;
    target_memory_range_t ranges[MEMORY_SEARCH_MAX_RANGES];
    size_t rangeCount = 0;
    if (!memory_collectSearchRanges(ranges, MEMORY_SEARCH_MAX_RANGES, &rangeCount) || rangeCount == 0) {
        return 0;
    }
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = (int)rangeCount - 1; i >= 0; --i) {
            uint32_t base = ranges[i].baseAddr & 0x00ffffffu;
            uint32_t size = ranges[i].size;
            if (size == 0) {
                continue;
            }
            uint32_t end = base + size - 1u;
            if (end < base) {
                end = 0x00ffffffu;
            }
            uint32_t scanStart = base;
            uint32_t scanEnd = end;
            if (pass == 0) {
                if (startAddr < base) {
                    continue;
                }
                if (startAddr < end) {
                    scanEnd = startAddr;
                }
            } else {
                if (startAddr >= end) {
                    continue;
                }
                if (startAddr >= base) {
                    scanStart = startAddr + 1u;
                }
            }
            if (scanEnd < scanStart) {
                continue;
            }
            if (memory_scanRangeForLastMatch(scanStart, scanEnd, pattern, outAddr, outLen)) {
                return 1;
            }
        }
    }
    return 0;
}

static int
memory_buildSearchPattern(const char *text, memory_search_pattern_t *outPattern)
{
    if (!outPattern) {
        return 0;
    }
    memset(outPattern, 0, sizeof(*outPattern));
    if (!text) {
        return 0;
    }
    int textLen = (int)strlen(text);
    if (textLen <= 0) {
        return 0;
    }
    int asciiLen = textLen;
    if (asciiLen > MEMORY_SEARCH_MAX_PATTERN) {
        asciiLen = MEMORY_SEARCH_MAX_PATTERN;
    }
    if (asciiLen > 0) {
        memcpy(outPattern->ascii, text, (size_t)asciiLen);
        outPattern->asciiLen = asciiLen;
    }

    char hexDigits[MEMORY_SEARCH_MAX_PATTERN * 2 + 1];
    int hexCount = 0;
    for (int i = 0; text[i] && hexCount < (int)sizeof(hexDigits) - 1; ++i) {
        if (isxdigit((unsigned char)text[i])) {
            hexDigits[hexCount++] = text[i];
        }
    }
    if (hexCount >= 2 && (hexCount % 2) == 0) {
        int bytes = hexCount / 2;
        if (bytes > MEMORY_SEARCH_MAX_PATTERN) {
            bytes = MEMORY_SEARCH_MAX_PATTERN;
        }
        for (int i = 0; i < bytes; ++i) {
            char pair[3];
            pair[0] = hexDigits[i * 2];
            pair[1] = hexDigits[i * 2 + 1];
            pair[2] = '\0';
            outPattern->hex[i] = (uint8_t)strtoul(pair, NULL, 16);
        }
        outPattern->hexLen = bytes;
    }
    return outPattern->asciiLen > 0 || outPattern->hexLen > 0;
}

static void
memory_runSearch(memory_view_state_t *st, int direction, int advance)
{
    if (!st || !st->searchBox) {
        return;
    }
    const char *text = e9ui_textbox_getText(st->searchBox);
    memory_search_pattern_t pattern;
    if (!memory_buildSearchPattern(text, &pattern)) {
        st->searchMatchValid = 0;
        st->searchMatchAddr = 0;
        st->searchMatchLen = 0;
        return;
    }
    uint32_t startAddr = st->base & 0x00ffffffu;
    if (advance && st->searchMatchValid) {
        if (direction >= 0) {
            startAddr = (st->searchMatchAddr + 1u) & 0x00ffffffu;
        } else {
            startAddr = (st->searchMatchAddr - 1u) & 0x00ffffffu;
        }
    }
    uint32_t hitAddr = 0;
    int hitLen = 0;
    int found = 0;
    if (direction >= 0) {
        found = memory_findNextMatch(st, &pattern, startAddr, &hitAddr, &hitLen);
    } else {
        found = memory_findPrevMatch(st, &pattern, startAddr, &hitAddr, &hitLen);
    }
    if (!found) {
        st->searchMatchValid = 0;
        st->searchMatchAddr = 0;
        st->searchMatchLen = 0;
        return;
    }
    st->searchMatchValid = 1;
    st->searchMatchAddr = hitAddr & 0x00ffffffu;
    st->searchMatchLen = hitLen;

    uint32_t wantBase = memory_clampBaseForView(st, hitAddr & ~0x0fu);
    if (wantBase != st->base) {
        st->base = wantBase;
        memory_syncTextboxFromBase(st);
    }
    memory_fillFromram(st, st->base);
}

static void
memory_onSearchChange(e9ui_context_t *ctx, void *user)
{
    (void)ctx;
    memory_view_state_t *st = (memory_view_state_t*)user;
    memory_runSearch(st, 1, 0);
}

static void
memory_onSearchSubmit(e9ui_context_t *ctx, void *user)
{
    (void)ctx;
    memory_view_state_t *st = (memory_view_state_t*)user;
    memory_runSearch(st, 1, 1);
}

static int
memory_onSearchKey(e9ui_context_t *ctx, SDL_Keycode key, SDL_Keymod mods, void *user)
{
    (void)ctx;
    memory_view_state_t *st = (memory_view_state_t*)user;
    if (!st) {
        return 0;
    }
    if ((key == SDLK_RETURN || key == SDLK_KP_ENTER) && (mods & KMOD_SHIFT) != 0) {
        memory_runSearch(st, -1, 1);
        return 1;
    }
    return 0;
}

static int
memory_stepButtonsGutterWidth(e9ui_context_t *ctx, e9ui_component_t *self)
{
    if (!ctx || !self) {
        return 0;
    }
    int thickness = e9ui_scale_px(ctx, 8);
    if (thickness < 4) {
        thickness = 4;
    }
    if (self->bounds.w > 0 && thickness >= self->bounds.w) {
        thickness = self->bounds.w > 1 ? self->bounds.w - 1 : 1;
    }
    if (thickness <= 0) {
        return 0;
    }
    int buttonW = thickness * 2;
    if (buttonW > self->bounds.w) {
        buttonW = self->bounds.w;
    }
    int margin = e9ui_scale_px(ctx, 4);
    if (margin < 0) {
        margin = 0;
    }
    int gutter = buttonW + margin;
    if (gutter > self->bounds.w) {
        gutter = self->bounds.w;
    }
    return gutter > 0 ? gutter : 0;
}

static int
memory_hscrollContentWidthEstimate(memory_view_state_t *st, TTF_Font *font)
{
    const int contentPad = 8;
    if (!font) {
        return 0;
    }
    int w = 0;
    int sampleLen = 0;
    char sample[256];
    sampleLen = snprintf(sample, sizeof(sample),
                         "00FFFFFF: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................");
    if (sampleLen < 0) {
        sample[0] = '\0';
    }
    (void)TTF_SizeText(font, sample, &w, NULL);
    if (st && st->error[0]) {
        int ew = 0;
        (void)TTF_SizeText(font, st->error, &ew, NULL);
        if (ew > w) {
            w = ew;
        }
    }
    return w + contentPad * 2;
}

static e9ui_rect_t
memory_hscrollBounds(e9ui_context_t *ctx, e9ui_component_t *self)
{
    e9ui_rect_t bounds = {0, 0, 0, 0};
    if (!ctx || !self) {
        return bounds;
    }
    int rightGutter = memory_stepButtonsGutterWidth(ctx, self);
    if (rightGutter < 0) {
        rightGutter = 0;
    }
    if (rightGutter > self->bounds.w) {
        rightGutter = self->bounds.w;
    }
    bounds.x = self->bounds.x;
    bounds.y = self->bounds.y;
    bounds.w = self->bounds.w - rightGutter;
    bounds.h = self->bounds.h;
    if (bounds.w < 0) {
        bounds.w = 0;
    }
    return bounds;
}

typedef struct memory_step_buttons_action_ctx {
    memory_view_state_t *st;
} memory_step_buttons_action_ctx_t;

static int
memory_stepButtonsOnAction(void *user, e9ui_step_buttons_action_t action)
{
    memory_step_buttons_action_ctx_t *actionCtx = (memory_step_buttons_action_ctx_t*)user;
    if (!actionCtx || !actionCtx->st) {
        return 0;
    }
    int rows = 0;
    switch (action) {
    case e9ui_step_buttons_action_page_up:
        rows = -32;
        break;
    case e9ui_step_buttons_action_line_up:
        rows = -1;
        break;
    case e9ui_step_buttons_action_line_down:
        rows = 1;
        break;
    case e9ui_step_buttons_action_page_down:
        rows = 32;
        break;
    default:
        break;
    }
    if (rows == 0) {
        return 0;
    }
    memory_scrollRows(actionCtx->st, rows);
    return 1;
}

static int
memory_handleEvent(e9ui_component_t *self, e9ui_context_t *ctx, const e9ui_event_t *ev)
{
    if (!self || !ctx || !ev) {
        return 0;
    }
    memory_view_state_t *st = (memory_view_state_t*)self->state;
    if (!st) {
        return 0;
    }

    if (ctx &&
        (ev->type == SDL_MOUSEMOTION || ev->type == SDL_MOUSEBUTTONDOWN || ev->type == SDL_MOUSEBUTTONUP)) {
        e9ui_rect_t sbBounds = memory_hscrollBounds(ctx, self);
        int viewW = sbBounds.w;
        if (viewW < 1) {
            viewW = 1;
        }
        TTF_Font *font = e9ui->theme.text.source ? e9ui->theme.text.source : ctx->font;
        int contentW = st->contentPixelWidth > 0 ? st->contentPixelWidth : memory_hscrollContentWidthEstimate(st, font);
        if (contentW < 0) {
            contentW = 0;
        }
        int scrollX = st->scrollX;
        int scrollY = 0;
        if (e9ui_scrollbar_handleEvent(self,
                                       ctx,
                                       ev,
                                       sbBounds,
                                       viewW,
                                       1,
                                       contentW,
                                       1,
                                       &scrollX,
                                       &scrollY,
                                       &st->hScrollbar)) {
            st->scrollX = scrollX;
            return 1;
        }
    }

    if (ctx) {
        memory_step_buttons_action_ctx_t actionCtx = { st };
        int stepEnabled = machine_getRunning(debugger.machine) ? 0 : 1;
        if (e9ui_step_buttons_handleEvent(ctx,
                                          ev,
                                          self->bounds,
                                          0,
                                          stepEnabled,
                                          &st->stepButtons,
                                          &actionCtx,
                                          memory_stepButtonsOnAction)) {
            return 1;
        }
    }

    if (ev->type == SDL_MOUSEWHEEL) {
        int mx = ctx->mouseX;
        int my = ctx->mouseY;
        if (mx < self->bounds.x || mx >= self->bounds.x + self->bounds.w ||
            my < self->bounds.y || my >= self->bounds.y + self->bounds.h) {
            return 0;
        }
        int wheelX = ev->wheel.x;
        int wheelY = ev->wheel.y;
        if (ev->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
            wheelX = -wheelX;
            wheelY = -wheelY;
        }
        if (wheelX != 0) {
            e9ui_rect_t sbBounds = memory_hscrollBounds(ctx, self);
            int viewW = sbBounds.w > 0 ? sbBounds.w : 1;
            int scrollY = 0;
            st->scrollX -= wheelX * e9ui_scale_px(ctx, 24);
            e9ui_scrollbar_clamp(viewW, 1, st->contentPixelWidth, 1, &st->scrollX, &scrollY);
        }
        if (wheelY != 0) {
            memory_scrollRows(st, -wheelY * 3);
        }
        if (wheelX != 0 || wheelY != 0) {
            return 1;
        }
        return 0;
    }
    if (ev->type == SDL_KEYDOWN && ctx && e9ui_getFocus(ctx) == self) {
        SDL_Keycode kc = ev->key.keysym.sym;
        if (kc == SDLK_PAGEUP) {
            memory_scrollRows(st, -32);
            return 1;
        }
        if (kc == SDLK_PAGEDOWN) {
            memory_scrollRows(st, 32);
            return 1;
        }
        if (kc == SDLK_UP) {
            memory_scrollRows(st, -1);
            return 1;
        }
        if (kc == SDLK_DOWN) {
            memory_scrollRows(st, 1);
            return 1;
        }
    }
    return 0;
}

static int
memory_preferredHeight(e9ui_component_t *self, e9ui_context_t *ctx, int availW)
{
    (void)self; (void)ctx; (void)availW;
    return 0;
}

static void
memory_layout(e9ui_component_t *self, e9ui_context_t *ctx, e9ui_rect_t bounds)
{
    (void)ctx;
    self->bounds = bounds;
}

static int
memory_measureSegment(TTF_Font *font, const char *line, int start, int len)
{
    if (!font || !line || len <= 0) {
        return 0;
    }
    int lineLen = (int)strlen(line);
    if (start < 0) {
        start = 0;
    }
    if (start > lineLen) {
        start = lineLen;
    }
    if (start + len > lineLen) {
        len = lineLen - start;
    }
    if (len <= 0) {
        return 0;
    }
    char tmp[512];
    if (len >= (int)sizeof(tmp)) {
        len = (int)sizeof(tmp) - 1;
    }
    memcpy(tmp, line + start, (size_t)len);
    tmp[len] = '\0';
    int w = 0;
    (void)TTF_SizeText(font, tmp, &w, NULL);
    return w;
}

static void
memory_markVisibleMatches(memory_view_state_t *st, const memory_search_pattern_t *pattern,
                          uint8_t *allMask, uint8_t *currentMask)
{
    if (!st || !pattern || !allMask || !currentMask) {
        return;
    }
    memset(allMask, 0, st->size);
    memset(currentMask, 0, st->size);
    int maxLen = pattern->asciiLen > pattern->hexLen ? pattern->asciiLen : pattern->hexLen;
    if (maxLen <= 0) {
        return;
    }
    for (unsigned int i = 0; i < st->size; ++i) {
        int matchLen = 0;
        if (!memory_patternMatchesAt(st->data + i, (int)(st->size - i), pattern, &matchLen)) {
            continue;
        }
        for (int j = 0; j < matchLen && i + (unsigned int)j < st->size; ++j) {
            allMask[i + (unsigned int)j] = 1;
        }
    }
    if (st->searchMatchValid && st->searchMatchLen > 0) {
        uint32_t viewStart = st->base & 0x00ffffffu;
        uint32_t viewEnd = viewStart + st->size - 1u;
        if (st->searchMatchAddr >= viewStart && st->searchMatchAddr <= viewEnd) {
            unsigned int start = (unsigned int)(st->searchMatchAddr - viewStart);
            for (int i = 0; i < st->searchMatchLen && start + (unsigned int)i < st->size; ++i) {
                currentMask[start + (unsigned int)i] = 1;
            }
        }
    }
}

static void
memory_render(e9ui_component_t *self, e9ui_context_t *ctx)
{
    if (!self || !ctx || !ctx->renderer) {
        return;
    }
    memory_view_state_t *st = (memory_view_state_t*)self->state;
    if (!st) {
        return;
    }
    memory_step_buttons_action_ctx_t actionCtx = { st };
    {
        int stepEnabled = machine_getRunning(debugger.machine) ? 0 : 1;
        e9ui_step_buttons_tick(ctx,
                               self->bounds,
                               0,
                               stepEnabled,
                               &st->stepButtons,
                               &actionCtx,
                               memory_stepButtonsOnAction);
    }
    SDL_Rect r = { self->bounds.x, self->bounds.y, self->bounds.w, self->bounds.h };
    SDL_SetRenderDrawColor(ctx->renderer, 20, 22, 20, 255);
    SDL_RenderFillRect(ctx->renderer, &r);
    TTF_Font *font = e9ui->theme.text.source ? e9ui->theme.text.source : ctx->font;
    if (!font || !st->data) {
        return;
    }
    int stepEnabled = machine_getRunning(debugger.machine) ? 0 : 1;
    int rightGutter = memory_stepButtonsGutterWidth(ctx, self);
    if (rightGutter < 0) {
        rightGutter = 0;
    }
    if (rightGutter > r.w) {
        rightGutter = r.w;
    }
    SDL_bool hadClip = SDL_RenderIsClipEnabled(ctx->renderer);
    SDL_Rect prevClip = {0};
    if (hadClip) {
        SDL_RenderGetClipRect(ctx->renderer, &prevClip);
    }
    SDL_Rect contentClip = r;
    contentClip.w -= rightGutter;
    if (contentClip.w < 0) {
        contentClip.w = 0;
    }
    if (contentClip.w > 0 && contentClip.h > 0) {
        if (hadClip) {
            SDL_Rect clipped;
            if (SDL_IntersectRect(&prevClip, &contentClip, &clipped)) {
                SDL_RenderSetClipRect(ctx->renderer, &clipped);
            } else {
                SDL_RenderSetClipRect(ctx->renderer, &contentClip);
            }
        } else {
            SDL_RenderSetClipRect(ctx->renderer, &contentClip);
        }
    } else {
        SDL_RenderSetClipRect(ctx->renderer, NULL);
    }
    st->contentPixelWidth = memory_hscrollContentWidthEstimate(st, font);
    if (st->contentPixelWidth < 0) {
        st->contentPixelWidth = 0;
    }
    {
        int scrollY = 0;
        int viewW = contentClip.w > 0 ? contentClip.w : 1;
        e9ui_scrollbar_clamp(viewW, 1, st->contentPixelWidth, 1, &st->scrollX, &scrollY);
    }
    int lh = TTF_FontHeight(font);
    if (lh <= 0) {
        lh = 16;
    }
    unsigned int addr = st->base;
    int pad = 8;
    int y = r.y + pad;
    uint8_t allMask[16u * 32u];
    uint8_t currentMask[16u * 32u];
    memset(allMask, 0, sizeof(allMask));
    memset(currentMask, 0, sizeof(currentMask));
    if (st->searchBox) {
        memory_search_pattern_t pattern;
        const char *searchText = e9ui_textbox_getText(st->searchBox);
        if (memory_buildSearchPattern(searchText, &pattern)) {
            memory_markVisibleMatches(st, &pattern, allMask, currentMask);
        }
    }
    if (st->error[0]) {
        SDL_Color err = {220, 80, 80, 255};
        int tw = 0, th = 0;
        SDL_Texture *t = e9ui_text_cache_getText(ctx->renderer, font, st->error, err, &tw, &th);
        if (t) {
            int drawX = r.x + pad - st->scrollX;
            SDL_Rect tr = { drawX, y, tw, th };
            SDL_RenderCopy(ctx->renderer, t, NULL, &tr);
            if (tw + pad * 2 > st->contentPixelWidth) {
                st->contentPixelWidth = tw + pad * 2;
            }
        }
        y += lh;
    }
    char line[256];
    for (unsigned int off = 0; off < st->size; off += 16) {
        int n = snprintf(line, sizeof(line), "%08X: ", addr + off);
        for (unsigned int i=0; i<16; ++i) {
            if (off + i < st->size) {
                n += snprintf(line + n, sizeof(line) - n, "%02X ", st->data[off + i]);
            } else {
                n += snprintf(line + n, sizeof(line) - n, "   ");
            }
        }
        n += snprintf(line + n, sizeof(line) - n, " ");
        for (unsigned int i=0; i<16 && off + i < st->size; ++i) {
            unsigned char c = st->data[off + i];
            line[n++] = (c >= 32 && c <= 126) ? (char)c : '.';
        }
        line[n] = '\0';
        int baseX = r.x + pad - st->scrollX;
        for (unsigned int i = 0; i < 16 && off + i < st->size; ++i) {
            unsigned int idx = off + i;
            if (!allMask[idx]) {
                continue;
            }
            int hexCol = 10 + (int)i * 3;
            int asciiCol = 59 + (int)i;
            int hexX = baseX + memory_measureSegment(font, line, 0, hexCol);
            int asciiX = baseX + memory_measureSegment(font, line, 0, asciiCol);
            int hexW = memory_measureSegment(font, line, hexCol, 2);
            int asciiW = memory_measureSegment(font, line, asciiCol, 1);
            SDL_Color hl = currentMask[idx] ? (SDL_Color){64, 112, 188, 220}
                                            : (SDL_Color){120, 100, 48, 170};
            SDL_SetRenderDrawColor(ctx->renderer, hl.r, hl.g, hl.b, hl.a);
            SDL_Rect hexRect = { hexX, y - 1, hexW, lh + 2 };
            SDL_Rect asciiRect = { asciiX, y - 1, asciiW, lh + 2 };
            SDL_RenderFillRect(ctx->renderer, &hexRect);
            SDL_RenderFillRect(ctx->renderer, &asciiRect);
        }
        SDL_Color col = {200,220,200,255};
        int tw = 0, th = 0;
        SDL_Texture *t = e9ui_text_cache_getText(ctx->renderer, font, line, col, &tw, &th);
        if (t) {
            SDL_Rect tr = { r.x + pad - st->scrollX, y, tw, th };
            SDL_RenderCopy(ctx->renderer, t, NULL, &tr);
            if (tw + pad * 2 > st->contentPixelWidth) {
                st->contentPixelWidth = tw + pad * 2;
            }
        }
        y += lh;
        if (y > r.y + r.h - pad) {
            break;
        }
    }
    if (hadClip) {
        SDL_RenderSetClipRect(ctx->renderer, &prevClip);
    } else {
        SDL_RenderSetClipRect(ctx->renderer, NULL);
    }
    {
        e9ui_rect_t sbBounds = memory_hscrollBounds(ctx, self);
        int viewW = sbBounds.w > 0 ? sbBounds.w : 1;
        e9ui_scrollbar_render(self,
                              ctx,
                              sbBounds,
                              viewW,
                              1,
                              st->contentPixelWidth,
                              1,
                              st->scrollX,
                              0);
    }
    e9ui_step_buttons_render(ctx,
                             self->bounds,
                             0,
                             stepEnabled,
                             &st->stepButtons);
}

static void
memory_dtor(e9ui_component_t *self, e9ui_context_t *ctx)
{
  (void)ctx;
  if (!self) {
    return;
  }
  memory_view_state_t *st = (memory_view_state_t*)self->state;
  if (st) {
    alloc_free(st->data);
  }
}

e9ui_component_t *
memory_makeComponent(void)
{
    e9ui_component_t *stack = e9ui_stack_makeVertical();
    e9ui_component_t *row = e9ui_hstack_make();
    e9ui_component_t *c = (e9ui_component_t*)alloc_calloc(1, sizeof(*c));
    if (!c || !row) {
        alloc_free(c);
        return NULL;
    }
    memory_view_state_t *st = (memory_view_state_t*)alloc_calloc(1, sizeof(*st));
    if (!st) {
        alloc_free(c);
        return NULL;
    }
    c->name = "memory_view";
    c->state = st;
    c->preferredHeight = memory_preferredHeight;
    c->layout = memory_layout;
    c->render = memory_render;
    c->handleEvent = memory_handleEvent;
    c->dtor = memory_dtor;
    c->focusable = 1;
    
    uint32_t minAddr = 0;
    uint32_t maxAddr = 0x00ffffffu;
    if (memory_getAddressLimits(&minAddr, &maxAddr)) {
        st->base = minAddr;
    } else {
        st->base = 0;
    }
    st->base = memory_clampBaseForView(st, st->base);
    st->size = 16u * 32u;
    st->data = (unsigned char*)alloc_alloc(st->size);
    memory_clearData(st);
    memory_setError(st, NULL);

    st->addressBox = e9ui_textbox_make(32, memory_onAddressSubmit, NULL, st);
    st->searchBox = e9ui_textbox_make(128, memory_onSearchSubmit, memory_onSearchChange, st);
    if (!st->addressBox || !st->searchBox) {
        if (st->addressBox) {
            e9ui_childDestroy(st->addressBox, NULL);
            st->addressBox = NULL;
        }
        if (st->searchBox) {
            e9ui_childDestroy(st->searchBox, NULL);
            st->searchBox = NULL;
        }
        e9ui_childDestroy(row, NULL);
        alloc_free(st->data);
        alloc_free(st);
        alloc_free(c);
        return NULL;
    }
    e9ui_setDisableVariable(st->addressBox, machine_getRunningState(debugger.machine), 1);
    e9ui_setDisableVariable(st->searchBox, machine_getRunningState(debugger.machine), 1);
    e9ui_textbox_setFocusBorderVisible(st->addressBox, 0);
    e9ui_textbox_setFocusBorderVisible(st->searchBox, 0);
    e9ui_textbox_setKeyHandler(st->searchBox, memory_onSearchKey, st);

    e9ui_textbox_setPlaceholder(st->addressBox, "Base address (hex)");
    e9ui_textbox_setPlaceholder(st->searchBox, "Search (hex/ascii)");
    memory_syncTextboxFromBase(st);

    e9ui_hstack_addFlex(row, st->addressBox);
    e9ui_hstack_addFlex(row, st->searchBox);

    e9ui_stack_addFixed(stack, row);

    e9ui_stack_addFlex(stack, c);

    g_memory_view_state = st;

    return stack;
}

void
memory_refreshOnBreak(void)
{
    if (!g_memory_view_state) {
        return;
    }
    unsigned int addr = 0;
    if (!memory_parseAddress(g_memory_view_state, &addr)) {
        return;
    }
    g_memory_view_state->base = memory_clampBaseForView(g_memory_view_state, addr);
    memory_syncTextboxFromBase(g_memory_view_state);
    memory_fillFromram(g_memory_view_state, g_memory_view_state->base);
    memory_runSearch(g_memory_view_state, 1, 0);
}
