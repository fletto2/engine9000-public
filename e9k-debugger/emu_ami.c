/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debugger.h"
#include "debugger_input_bindings.h"
#include "debug.h"
#include "e9ui.h"
#include "range_bar.h"
#include "custom_log.h"
#include "custom_ui.h"
#include "amiga_uae_options.h"
#include "libretro.h"
#include "libretro_host.h"

#define EMU_AMI_BLITTER_VIS_POINTS_CAP_DEFAULT (2304u * 1620u)
#define EMU_AMI_BLITTER_VIS_LINE_TABLE_CAP_MAX (1u << 20)
#define EMU_AMI_BLITTER_VIS_WORD_SHIFT_PIXELS 16
#define EMU_AMI_BLITTER_VIS_Y_GAP_MAX 1024u
#define EMU_AMI_BLITTER_VIS_XINV_MAX_LOGS 32u
#define EMU_AMI_BLITTER_VIS_MODE_OVERLAY 0x2u
#define EMU_AMI_BLITTER_VIS_ALPHA_MAX 0xb0u
#define EMU_AMI_BLITTER_VIS_BLIT_TABLE_CAP_MIN 1024u
#define EMU_AMI_BLITTER_VIS_BLIT_TABLE_CAP_MAX (1u << 20)
typedef struct emu_ami_blitter_vis_line_stat {
    uint32_t blitId;
    uint32_t y;
    uint32_t minX;
    uint32_t maxX;
    uint32_t count;
    uint8_t used;
} emu_ami_blitter_vis_line_stat_t;

typedef struct emu_ami_blitter_vis_analysis {
    uint32_t lineCount;
    uint32_t droppedEntries;
    uint32_t comparedPairs;
    int maxAbsDelta;
    uint32_t maxAbsDeltaBlitId;
    uint32_t maxAbsDeltaY;
    uint32_t maxAbsDeltaPrevY;
    int maxAbsDeltaMin;
    int maxAbsDeltaMax;
    uint32_t maxPrevMinX;
    uint32_t maxPrevMaxX;
    uint32_t maxCurrMinX;
    uint32_t maxCurrMaxX;
    uint32_t blitsWithMinXVariance;
    uint32_t maxMinXSpread;
    uint32_t maxMinXSpreadBlitId;
    uint32_t maxMinXSpreadLowY;
    uint32_t maxMinXSpreadHighY;
    uint32_t maxMinXSpreadLowMinX;
    uint32_t maxMinXSpreadHighMinX;
    uint32_t wordShiftSameCount;
    uint32_t firstWordShiftBlitId;
    uint32_t firstWordShiftY;
    uint32_t firstWordShiftPrevY;
    int firstWordShiftMin;
    int firstWordShiftMax;
} emu_ami_blitter_vis_analysis_t;

typedef struct emu_ami_blitter_vis_cache {
    SDL_Texture *texture;
    SDL_Renderer *renderer;
    uint32_t *pixels;
    size_t pixelsCap;
    uint32_t *retainedBlitIds;
    size_t retainedCap;
    uint32_t *blitFrameIds;
    uint32_t *blitFrameValues;
    size_t blitFrameCap;
    size_t blitFrameCount;
    int texWidth;
    int texHeight;
    e9k_debug_ami_blitter_vis_point_t *points;
    size_t pointsCap;
    emu_ami_blitter_vis_line_stat_t *lineTable;
    size_t lineTableCap;
    emu_ami_blitter_vis_line_stat_t *lineList;
    size_t lineListCap;
    uint32_t overlayFrameCounter;
    int hasRetainedOverlay;
} emu_ami_blitter_vis_cache_t;

static emu_ami_blitter_vis_cache_t emu_ami_blitterVisCache = {0};
static int emu_ami_customLogCallbackBound = 0;

static const char *
emu_ami_mouseCaptureOptionKey(void)
{
    return "e9k_debugger_amiga_mouse_capture";
}

static int
emu_ami_percentToVideoLine(float percent, int lineCount)
{
    float clamped = percent;

    if (lineCount <= 1) {
        return 0;
    }
    if (clamped < 0.0f) {
        clamped = 0.0f;
    }
    if (clamped > 1.0f) {
        clamped = 1.0f;
    }
    return (int)(clamped * (float)(lineCount - 1) + 0.5f);
}

static float
emu_ami_videoLineToPercent(int videoLine, int lineCount)
{
    int clamped = videoLine;

    if (lineCount <= 1) {
        return 0.0f;
    }
    if (clamped < 0) {
        clamped = 0;
    }
    if (clamped >= lineCount) {
        clamped = lineCount - 1;
    }
    return (float)clamped / (float)(lineCount - 1);
}

static int
emu_ami_rangeBarGetCoreRangeFromPercent(float startPercent,
                                        float endPercent,
                                        int *outStartLine,
                                        int *outEndLine)
{
    int lineCount = 0;
    int startVideoLine = 0;
    int endVideoLine = 0;
    int startLine = -1;
    int endLine = -1;

    if (!libretro_host_debugAmiGetVideoLineCount(&lineCount) || lineCount <= 0) {
        return 0;
    }
    startVideoLine = emu_ami_percentToVideoLine(startPercent, lineCount);
    endVideoLine = emu_ami_percentToVideoLine(endPercent, lineCount);
    if (!libretro_host_debugAmiVideoLineToCoreLine(startVideoLine, &startLine)) {
        return 0;
    }
    if (!libretro_host_debugAmiVideoLineToCoreLine(endVideoLine, &endLine)) {
        return 0;
    }
    if (endLine < startLine) {
        int temp = startLine;
        startLine = endLine;
        endLine = temp;
    }
    if (outStartLine) {
        *outStartLine = startLine;
    }
    if (outEndLine) {
        *outEndLine = endLine;
    }
    return 1;
}

static int
emu_ami_rangeBarSetPercentFromCoreLines(e9ui_component_t *bar, int startLine, int endLine)
{
    int lineCount = 0;
    int startVideoLine = -1;
    int endVideoLine = -1;
    float startPercent = 0.0f;
    float endPercent = 0.0f;

    if (!bar) {
        return 0;
    }
    if (!libretro_host_debugAmiGetVideoLineCount(&lineCount) || lineCount <= 0) {
        return 0;
    }
    if (!libretro_host_debugAmiCoreLineToVideoLine(startLine, &startVideoLine)) {
        return 0;
    }
    if (!libretro_host_debugAmiCoreLineToVideoLine(endLine, &endVideoLine)) {
        return 0;
    }
    startPercent = emu_ami_videoLineToPercent(startVideoLine, lineCount);
    endPercent = emu_ami_videoLineToPercent(endVideoLine, lineCount);
    if (endPercent < startPercent) {
        float temp = startPercent;
        startPercent = endPercent;
        endPercent = temp;
    }
    range_bar_setRangePercent(bar, startPercent, endPercent);
    return 1;
}

int
emu_ami_mouseCaptureCanEnable(void)
{
    const char *overrideValue = NULL;
    const char *joyportMode = amiga_uaeGetPuaeOptionValue("puae_joyport");

    if (!target) {
        return 0;
    }
    if (target->romConfigGetActiveCustomOptionValue) {
        overrideValue = target->romConfigGetActiveCustomOptionValue(emu_ami_mouseCaptureOptionKey());
    }
    if (!overrideValue && target->coreOptionGetValue) {
        overrideValue = target->coreOptionGetValue(emu_ami_mouseCaptureOptionKey());
    }
    if (overrideValue && strcmp(overrideValue, "disabled") == 0) {
        return 0;
    }
    if (joyportMode &&
        (strcmp(joyportMode, "joystick") == 0 ||
         strcmp(joyportMode, "Joystick") == 0 ||
         strcmp(joyportMode, "Joystick (Port 1)") == 0)) {
        return 0;
    }
    return 1;
}

size_t
emu_ami_rangeBarCount(void)
{
    return 2;
}

int
emu_ami_rangeBarDescribe(size_t index, emu_range_bar_desc_t *outDesc)
{
    if (!outDesc) {
        return 0;
    }
    memset(outDesc, 0, sizeof(*outDesc));
    if (index == 0) {
        outDesc->metaKey = "range_bar_left";
        outDesc->side = (int)range_bar_sideLeft;
    } else if (index == 1) {
        outDesc->metaKey = "range_bar_right";
        outDesc->side = (int)range_bar_sideRight;
    } else {
        return 0;
    }
    outDesc->marginTop = 10;
    outDesc->marginBottom = 10;
    outDesc->marginSide = 10;
    outDesc->width = 12;
    outDesc->hoverMargin = 18;
    return 1;
}

void
emu_ami_rangeBarChanged(size_t index, float startPercent, float endPercent)
{
    int startLine = -1;
    int endLine = -1;

    if (!emu_ami_rangeBarGetCoreRangeFromPercent(startPercent, endPercent, &startLine, &endLine)) {
        return;
    }
    if (index == 1) {
        custom_ui_setCopperLimitRange(startLine, endLine);
        return;
    }
    if (index == 0) {
        custom_ui_setBplptrLineLimitRange(startLine, endLine);
    }
}

void
emu_ami_rangeBarDragging(size_t index, int dragging, float startPercent, float endPercent)
{
    (void)index;
    (void)dragging;
    (void)startPercent;
    (void)endPercent;
}

void
emu_ami_rangeBarTooltip(size_t index, float startPercent, float endPercent, char *out, size_t cap)
{
    int startLine = -1;
    int endLine = -1;
    const char *label = "BPLPTR";

    if (!out || cap == 0) {
        return;
    }
    if (!emu_ami_rangeBarGetCoreRangeFromPercent(startPercent, endPercent, &startLine, &endLine)) {
        return;
    }
    if (index == 1) {
        label = "Copper";
    }
    snprintf(out, cap, "%s %d..%d", label, startLine, endLine);
}

int
emu_ami_rangeBarSync(size_t index, e9ui_component_t *bar)
{
    int enabled = 0;
    int startLine = 0;
    int endLine = 0;

    if (!bar) {
        return 0;
    }
    if (!custom_ui_isOpen()) {
        e9ui_setHidden(bar, 1);
        return 0;
    }
    if (index == 1) {
        enabled = custom_ui_getCopperLimitEnabled();
        if (!custom_ui_getCopperLimitRange(&startLine, &endLine)) {
            enabled = 0;
        }
    } else if (index == 0) {
        enabled = custom_ui_getBplptrBlockEnabled();
        if (!custom_ui_getBplptrLineLimitRange(&startLine, &endLine)) {
            enabled = 0;
        }
    } else {
        enabled = 0;
    }
    if (!enabled) {
        e9ui_setHidden(bar, 1);
        return 0;
    }
    if (!emu_ami_rangeBarSetPercentFromCoreLines(bar, startLine, endLine)) {
        e9ui_setHidden(bar, 1);
        return 0;
    }
    return 1;
}

static void
emu_ami_onCustomLogFrame(const e9k_debug_ami_custom_log_entry_t *entries,
                         size_t count,
                         uint32_t dropped,
                         uint64_t frameNo,
                         void *user)
{
    (void)user;
    custom_log_captureFrame(entries, count, dropped, frameNo);
}

static void
emu_ami_tryBindCustomLogFrameCallback(void)
{
    if (emu_ami_customLogCallbackBound) {
        return;
    }
    if (libretro_host_setCustomLogFrameCallback(emu_ami_onCustomLogFrame, NULL)) {
        emu_ami_customLogCallbackBound = 1;
    }
}

static void
emu_ami_destroy(void)
{
    emu_ami_customLogCallbackBound = 0;
}

static uint32_t
emu_ami_blitterVisColorFromId(uint32_t blitId)
{
    uint32_t h = blitId ? blitId : 1u;
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;

    uint8_t r = (uint8_t)(64u + (h & 0x7fu));
    uint8_t g = (uint8_t)(64u + ((h >> 8) & 0x7fu));
    uint8_t b = (uint8_t)(64u + ((h >> 16) & 0x7fu));
    return (uint32_t)(0xb0u << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static int
emu_ami_blitterVisAbs(int value)
{
    if (value < 0) {
        return -value;
    }
    return value;
}

static uint32_t
emu_ami_blitterVisLineHash(uint32_t blitId, uint32_t y)
{
    uint32_t mixed = (blitId * 2654435761u) ^ (y * 2246822519u);
    return mixed;
}

static int
emu_ami_blitterVisLineCompare(const void *lhs, const void *rhs)
{
    const emu_ami_blitter_vis_line_stat_t *left = (const emu_ami_blitter_vis_line_stat_t *)lhs;
    const emu_ami_blitter_vis_line_stat_t *right = (const emu_ami_blitter_vis_line_stat_t *)rhs;
    if (left->blitId < right->blitId) {
        return -1;
    }
    if (left->blitId > right->blitId) {
        return 1;
    }
    if (left->y < right->y) {
        return -1;
    }
    if (left->y > right->y) {
        return 1;
    }
    return 0;
}

static uint32_t
emu_ami_blitterVisBlitHash(uint32_t blitId)
{
    return blitId * 2654435761u;
}

static uint8_t
emu_ami_blitterVisAlphaForAge(uint32_t age, uint32_t decayFrames)
{
    if (decayFrames <= 1u) {
        return (uint8_t)EMU_AMI_BLITTER_VIS_ALPHA_MAX;
    }
    if (age >= decayFrames) {
        return 0u;
    }
    uint32_t range = decayFrames - 1u;
    uint32_t alpha = (EMU_AMI_BLITTER_VIS_ALPHA_MAX * (range - age)) / range;
    return (uint8_t)alpha;
}

static int
emu_ami_blitterVisEnsureBlitFrameTable(emu_ami_blitter_vis_cache_t *cache, size_t neededEntries)
{
    if (!cache) {
        return 0;
    }
    size_t target = neededEntries;
    if (target < (size_t)EMU_AMI_BLITTER_VIS_BLIT_TABLE_CAP_MIN / 2u) {
        target = (size_t)EMU_AMI_BLITTER_VIS_BLIT_TABLE_CAP_MIN / 2u;
    }
    size_t desiredCap = cache->blitFrameCap;
    if (desiredCap < (size_t)EMU_AMI_BLITTER_VIS_BLIT_TABLE_CAP_MIN) {
        desiredCap = (size_t)EMU_AMI_BLITTER_VIS_BLIT_TABLE_CAP_MIN;
    }
    while ((target * 2u) > desiredCap && desiredCap < (size_t)EMU_AMI_BLITTER_VIS_BLIT_TABLE_CAP_MAX) {
        desiredCap <<= 1u;
    }
    if (desiredCap > (size_t)EMU_AMI_BLITTER_VIS_BLIT_TABLE_CAP_MAX) {
        desiredCap = (size_t)EMU_AMI_BLITTER_VIS_BLIT_TABLE_CAP_MAX;
    }
    if (cache->blitFrameCap >= desiredCap && cache->blitFrameIds && cache->blitFrameValues) {
        return 1;
    }

    uint32_t *nextIds = (uint32_t *)calloc(desiredCap, sizeof(*nextIds));
    uint32_t *nextValues = (uint32_t *)calloc(desiredCap, sizeof(*nextValues));
    if (!nextIds || !nextValues) {
        free(nextIds);
        free(nextValues);
        return 0;
    }

    size_t nextCount = 0;
    if (cache->blitFrameIds && cache->blitFrameValues && cache->blitFrameCap) {
        size_t nextMask = desiredCap - 1u;
        for (size_t i = 0; i < cache->blitFrameCap; ++i) {
            uint32_t blitId = cache->blitFrameIds[i];
            if (!blitId) {
                continue;
            }
            size_t index = (size_t)emu_ami_blitterVisBlitHash(blitId) & nextMask;
            for (size_t probe = 0; probe < desiredCap; ++probe) {
                if (!nextIds[index]) {
                    nextIds[index] = blitId;
                    nextValues[index] = cache->blitFrameValues[i];
                    nextCount++;
                    break;
                }
                index = (index + 1u) & nextMask;
            }
        }
    }

    free(cache->blitFrameIds);
    free(cache->blitFrameValues);
    cache->blitFrameIds = nextIds;
    cache->blitFrameValues = nextValues;
    cache->blitFrameCap = desiredCap;
    cache->blitFrameCount = nextCount;
    return 1;
}

static int
emu_ami_blitterVisSetBlitFrame(emu_ami_blitter_vis_cache_t *cache, uint32_t blitId, uint32_t frameCounter)
{
    if (!cache || !blitId) {
        return 0;
    }
    if (!emu_ami_blitterVisEnsureBlitFrameTable(cache, cache->blitFrameCount + 1u)) {
        return 0;
    }
    if ((cache->blitFrameCount * 4u) >= (cache->blitFrameCap * 3u)) {
        if (cache->blitFrameCap >= (size_t)EMU_AMI_BLITTER_VIS_BLIT_TABLE_CAP_MAX ||
            !emu_ami_blitterVisEnsureBlitFrameTable(cache, cache->blitFrameCount + 1u)) {
            return 0;
        }
    }

    size_t mask = cache->blitFrameCap - 1u;
    size_t index = (size_t)emu_ami_blitterVisBlitHash(blitId) & mask;
    for (size_t probe = 0; probe < cache->blitFrameCap; ++probe) {
        if (!cache->blitFrameIds[index]) {
            cache->blitFrameIds[index] = blitId;
            cache->blitFrameValues[index] = frameCounter;
            cache->blitFrameCount++;
            return 1;
        }
        if (cache->blitFrameIds[index] == blitId) {
            cache->blitFrameValues[index] = frameCounter;
            return 1;
        }
        index = (index + 1u) & mask;
    }
    return 0;
}

static int
emu_ami_blitterVisGetBlitFrame(const emu_ami_blitter_vis_cache_t *cache, uint32_t blitId, uint32_t *outFrame)
{
    if (outFrame) {
        *outFrame = 0u;
    }
    if (!cache || !blitId || !cache->blitFrameIds || !cache->blitFrameValues || !cache->blitFrameCap) {
        return 0;
    }
    size_t mask = cache->blitFrameCap - 1u;
    size_t index = (size_t)emu_ami_blitterVisBlitHash(blitId) & mask;
    for (size_t probe = 0; probe < cache->blitFrameCap; ++probe) {
        uint32_t id = cache->blitFrameIds[index];
        if (!id) {
            return 0;
        }
        if (id == blitId) {
            if (outFrame) {
                *outFrame = cache->blitFrameValues[index];
            }
            return 1;
        }
        index = (index + 1u) & mask;
    }
    return 0;
}

static size_t
emu_ami_blitterVisRecommendedLineTableCap(size_t fetchedCount)
{
    size_t target = fetchedCount;
    if (target < 1024u) {
        target = 1024u;
    }
    if (target > (size_t)EMU_AMI_BLITTER_VIS_LINE_TABLE_CAP_MAX / 2u) {
        target = (size_t)EMU_AMI_BLITTER_VIS_LINE_TABLE_CAP_MAX / 2u;
    }
    size_t cap = 1024u;
    while (cap < target * 2u && cap < (size_t)EMU_AMI_BLITTER_VIS_LINE_TABLE_CAP_MAX) {
        cap <<= 1;
    }
    if (cap > (size_t)EMU_AMI_BLITTER_VIS_LINE_TABLE_CAP_MAX) {
        cap = (size_t)EMU_AMI_BLITTER_VIS_LINE_TABLE_CAP_MAX;
    }
    return cap;
}

static int
emu_ami_blitterVisEnsureLineStorage(emu_ami_blitter_vis_cache_t *cache, size_t fetchedCount)
{
    if (!cache) {
        return 0;
    }
    size_t desiredCap = emu_ami_blitterVisRecommendedLineTableCap(fetchedCount);
    if (cache->lineTableCap < desiredCap) {
        emu_ami_blitter_vis_line_stat_t *nextTable =
            (emu_ami_blitter_vis_line_stat_t *)realloc(cache->lineTable, desiredCap * sizeof(*nextTable));
        if (!nextTable) {
            return 0;
        }
        cache->lineTable = nextTable;
        cache->lineTableCap = desiredCap;
    }
    if (cache->lineListCap < cache->lineTableCap) {
        emu_ami_blitter_vis_line_stat_t *nextList =
            (emu_ami_blitter_vis_line_stat_t *)realloc(cache->lineList, cache->lineTableCap * sizeof(*nextList));
        if (!nextList) {
            return 0;
        }
        cache->lineList = nextList;
        cache->lineListCap = cache->lineTableCap;
    }
    return 1;
}

static __attribute__((unused)) int
emu_ami_blitterVisAnalyzePoints(emu_ami_blitter_vis_cache_t *cache, size_t fetchedCount, emu_ami_blitter_vis_analysis_t *out)
{
    if (!cache || !out) {
        return 0;
    }
    memset(out, 0, sizeof(*out));
    out->firstWordShiftBlitId = 0u;

    if (!fetchedCount) {
        return 1;
    }
    if (!emu_ami_blitterVisEnsureLineStorage(cache, fetchedCount)) {
        return 0;
    }
    if (!cache->lineTable || !cache->lineList || !cache->lineTableCap) {
        return 0;
    }

    memset(cache->lineTable, 0, cache->lineTableCap * sizeof(*cache->lineTable));
    size_t mask = cache->lineTableCap - 1u;
    for (size_t i = 0; i < fetchedCount; ++i) {
        uint32_t blitId = cache->points[i].blitId;
        if (!blitId) {
            continue;
        }
        uint32_t y = (uint32_t)cache->points[i].y;
        uint32_t x = (uint32_t)cache->points[i].x;
        uint32_t hash = emu_ami_blitterVisLineHash(blitId, y);
        size_t index = (size_t)hash & mask;
        emu_ami_blitter_vis_line_stat_t *entry = NULL;
        for (size_t probe = 0; probe < cache->lineTableCap; ++probe) {
            emu_ami_blitter_vis_line_stat_t *candidate = &cache->lineTable[index];
            if (!candidate->used) {
                candidate->used = 1u;
                candidate->blitId = blitId;
                candidate->y = y;
                candidate->minX = x;
                candidate->maxX = x;
                candidate->count = 1u;
                entry = candidate;
                break;
            }
            if (candidate->blitId == blitId && candidate->y == y) {
                entry = candidate;
                break;
            }
            index = (index + 1u) & mask;
        }
        if (!entry) {
            out->droppedEntries++;
            continue;
        }
        if (entry->count != 1u || entry->minX != x || entry->maxX != x) {
            if (x < entry->minX) {
                entry->minX = x;
            }
            if (x > entry->maxX) {
                entry->maxX = x;
            }
            entry->count++;
        }
    }

    size_t emitCount = 0u;
    for (size_t i = 0; i < cache->lineTableCap; ++i) {
        if (!cache->lineTable[i].used) {
            continue;
        }
        cache->lineList[emitCount++] = cache->lineTable[i];
    }
    out->lineCount = (uint32_t)emitCount;
    if (emitCount == 0u) {
        return 1;
    }

    qsort(cache->lineList, emitCount, sizeof(cache->lineList[0]), emu_ami_blitterVisLineCompare);
    size_t groupStart = 0u;
    while (groupStart < emitCount) {
        uint32_t blitId = cache->lineList[groupStart].blitId;
        uint32_t lowMinX = cache->lineList[groupStart].minX;
        uint32_t highMinX = cache->lineList[groupStart].minX;
        uint32_t lowY = cache->lineList[groupStart].y;
        uint32_t highY = cache->lineList[groupStart].y;
        size_t groupEnd = groupStart + 1u;
        while (groupEnd < emitCount && cache->lineList[groupEnd].blitId == blitId) {
            const emu_ami_blitter_vis_line_stat_t *entry = &cache->lineList[groupEnd];
            if (entry->minX < lowMinX) {
                lowMinX = entry->minX;
                lowY = entry->y;
            }
            if (entry->minX > highMinX) {
                highMinX = entry->minX;
                highY = entry->y;
            }
            groupEnd++;
        }
        if (highMinX > lowMinX) {
            uint32_t spread = highMinX - lowMinX;
            out->blitsWithMinXVariance++;
            if (spread > out->maxMinXSpread) {
                out->maxMinXSpread = spread;
                out->maxMinXSpreadBlitId = blitId;
                out->maxMinXSpreadLowY = lowY;
                out->maxMinXSpreadHighY = highY;
                out->maxMinXSpreadLowMinX = lowMinX;
                out->maxMinXSpreadHighMinX = highMinX;
            }
        }
        groupStart = groupEnd;
    }

    for (size_t i = 1u; i < emitCount; ++i) {
        const emu_ami_blitter_vis_line_stat_t *prev = &cache->lineList[i - 1u];
        const emu_ami_blitter_vis_line_stat_t *curr = &cache->lineList[i];
        if (prev->blitId != curr->blitId) {
            continue;
        }
        if (curr->y <= prev->y) {
            continue;
        }
        uint32_t yGap = curr->y - prev->y;
        if (yGap > EMU_AMI_BLITTER_VIS_Y_GAP_MAX) {
            continue;
        }

        int minDelta = (int)curr->minX - (int)prev->minX;
        int maxDelta = (int)curr->maxX - (int)prev->maxX;
        int pairAbs = emu_ami_blitterVisAbs(minDelta);
        if (emu_ami_blitterVisAbs(maxDelta) > pairAbs) {
            pairAbs = emu_ami_blitterVisAbs(maxDelta);
        }
        out->comparedPairs++;
        if (pairAbs > out->maxAbsDelta) {
            out->maxAbsDelta = pairAbs;
            out->maxAbsDeltaBlitId = curr->blitId;
            out->maxAbsDeltaY = curr->y;
            out->maxAbsDeltaPrevY = prev->y;
            out->maxAbsDeltaMin = minDelta;
            out->maxAbsDeltaMax = maxDelta;
            out->maxPrevMinX = prev->minX;
            out->maxPrevMaxX = prev->maxX;
            out->maxCurrMinX = curr->minX;
            out->maxCurrMaxX = curr->maxX;
        }
        if (emu_ami_blitterVisAbs(minDelta) == EMU_AMI_BLITTER_VIS_WORD_SHIFT_PIXELS &&
            emu_ami_blitterVisAbs(maxDelta) == EMU_AMI_BLITTER_VIS_WORD_SHIFT_PIXELS &&
            ((minDelta < 0 && maxDelta < 0) || (minDelta > 0 && maxDelta > 0))) {
            out->wordShiftSameCount++;
            if (out->firstWordShiftBlitId == 0u) {
                out->firstWordShiftBlitId = curr->blitId;
                out->firstWordShiftY = curr->y;
                out->firstWordShiftPrevY = prev->y;
                out->firstWordShiftMin = minDelta;
                out->firstWordShiftMax = maxDelta;
            }
        }
    }
    return 1;
}

static __attribute__((unused)) void
emu_ami_blitterVisDumpRunsForLine(emu_ami_blitter_vis_cache_t *cache,
                                  size_t fetchedCount,
                                  uint32_t blitId,
                                  uint32_t y,
                                  uint32_t width,
                                  const char *label)
{
    if (!cache || !cache->points || !label || !width) {
        return;
    }
    uint8_t *mask = (uint8_t *)calloc(width, sizeof(*mask));
    if (!mask) {
        return;
    }

    uint32_t count = 0u;
    for (size_t i = 0; i < fetchedCount; ++i) {
        const e9k_debug_ami_blitter_vis_point_t *p = &cache->points[i];
        if (p->blitId != blitId || (uint32_t)p->y != y) {
            continue;
        }
        if ((uint32_t)p->x >= width) {
            continue;
        }
        if (!mask[p->x]) {
            mask[p->x] = 1u;
            count++;
        }
    }

    printf("E9K BLITTER VIS RUNS: %s blit=%u y=%u count=%u width=%u\n", label, blitId, y, count, width);
    uint32_t runCount = 0u;
    int inRun = 0;
    uint32_t runStart = 0u;
    for (uint32_t x = 0u; x < width; ++x) {
        if (!inRun && mask[x]) {
            inRun = 1;
            runStart = x;
            continue;
        }
        if (inRun && !mask[x]) {
            printf("E9K BLITTER VIS RUNS: %s run=%u [%u,%u]\n", label, runCount, runStart, x - 1u);
            runCount++;
            inRun = 0;
            if (runCount >= 24u) {
                printf("E9K BLITTER VIS RUNS: %s truncated_after=%u\n", label, runCount);
                break;
            }
        }
    }
    if (inRun && runCount < 24u) {
        printf("E9K BLITTER VIS RUNS: %s run=%u [%u,%u]\n", label, runCount, runStart, width - 1u);
    }
    free(mask);
}

static __attribute__((unused)) void
emu_ami_blitterVisDumpSpanIdsForLine(emu_ami_blitter_vis_cache_t *cache,
                                     size_t fetchedCount,
                                     uint32_t y,
                                     uint32_t width,
                                     uint32_t xStart,
                                     uint32_t xEnd,
                                     const char *label)
{
    if (!cache || !cache->points || !label || !width || xStart > xEnd || xStart >= width) {
        return;
    }
    if (xEnd >= width) {
        xEnd = width - 1u;
    }
    uint32_t *idByX = (uint32_t *)calloc(width, sizeof(*idByX));
    if (!idByX) {
        return;
    }
    for (size_t i = 0; i < fetchedCount; ++i) {
        const e9k_debug_ami_blitter_vis_point_t *p = &cache->points[i];
        if ((uint32_t)p->y != y) {
            continue;
        }
        uint32_t x = (uint32_t)p->x;
        if (x < xStart || x > xEnd || x >= width) {
            continue;
        }
        idByX[x] = p->blitId;
    }

    printf("E9K BLITTER VIS SPANIDS: %s y=%u span=[%u,%u]\n", label, y, xStart, xEnd);
    uint32_t runCount = 0u;
    uint32_t runStart = xStart;
    uint32_t runId = idByX[xStart];
    for (uint32_t x = xStart + 1u; x <= xEnd; ++x) {
        if (idByX[x] == runId) {
            continue;
        }
        printf("E9K BLITTER VIS SPANIDS: %s run=%u id=%u [%u,%u]\n", label, runCount, runId, runStart, x - 1u);
        runCount++;
        runStart = x;
        runId = idByX[x];
        if (runCount >= 48u) {
            printf("E9K BLITTER VIS SPANIDS: %s truncated_after=%u\n", label, runCount);
            free(idByX);
            return;
        }
    }
    printf("E9K BLITTER VIS SPANIDS: %s run=%u id=%u [%u,%u]\n", label, runCount, runId, runStart, xEnd);
    free(idByX);
}

static __attribute__((unused)) void
emu_ami_blitterVisDumpMinXVariance(const emu_ami_blitter_vis_cache_t *cache, const emu_ami_blitter_vis_analysis_t *analysis)
{
    if (!cache || !analysis || !cache->lineList || analysis->lineCount == 0u) {
        return;
    }
    uint32_t logged = 0u;
    size_t idx = 0u;
    while (idx < (size_t)analysis->lineCount) {
        const emu_ami_blitter_vis_line_stat_t *start = &cache->lineList[idx];
        uint32_t blitId = start->blitId;
        uint32_t lowMinX = start->minX;
        uint32_t highMinX = start->minX;
        uint32_t lowY = start->y;
        uint32_t highY = start->y;
        size_t lines = 1u;
        idx++;
        while (idx < (size_t)analysis->lineCount && cache->lineList[idx].blitId == blitId) {
            const emu_ami_blitter_vis_line_stat_t *entry = &cache->lineList[idx];
            if (entry->minX < lowMinX) {
                lowMinX = entry->minX;
                lowY = entry->y;
            }
            if (entry->minX > highMinX) {
                highMinX = entry->minX;
                highY = entry->y;
            }
            lines++;
            idx++;
        }
        if (highMinX > lowMinX) {
            printf("E9K BLITTER VIS XLTMIN: blit=%u lowMinX=%u lowY=%u highMinX=%u highY=%u spread=%u lines=%zu\n",
                   blitId,
                   lowMinX,
                   lowY,
                   highMinX,
                   highY,
                   highMinX - lowMinX,
                   lines);
            logged++;
            if (logged >= EMU_AMI_BLITTER_VIS_XINV_MAX_LOGS) {
                printf("E9K BLITTER VIS XLTMIN: truncated_after=%u totalBlits=%u\n",
                       logged,
                       analysis->blitsWithMinXVariance);
                break;
            }
        }
    }
}

static __attribute__((unused)) void
emu_ami_blitterVisDumpBlitLines(const emu_ami_blitter_vis_cache_t *cache,
                                const emu_ami_blitter_vis_analysis_t *analysis,
                                uint32_t blitId)
{
    if (!cache || !analysis || !cache->lineList || !blitId) {
        return;
    }
    printf("E9K BLITTER VIS BLITLINES: blit=%u begin\n", blitId);
    uint32_t logged = 0u;
    for (size_t i = 0u; i < (size_t)analysis->lineCount; ++i) {
        const emu_ami_blitter_vis_line_stat_t *entry = &cache->lineList[i];
        if (entry->blitId != blitId) {
            continue;
        }
        printf("E9K BLITTER VIS BLITLINES: blit=%u y=%u minX=%u maxX=%u count=%u\n",
               blitId,
               entry->y,
               entry->minX,
               entry->maxX,
               entry->count);
        logged++;
        if (logged >= 128u) {
            printf("E9K BLITTER VIS BLITLINES: blit=%u truncated_after=%u\n", blitId, logged);
            break;
        }
    }
    printf("E9K BLITTER VIS BLITLINES: blit=%u end lines=%u\n", blitId, logged);
}

static void
emu_ami_renderBlitterVisOverlay(e9ui_context_t *ctx, SDL_Rect *dst)
{
    if (!ctx || !ctx->renderer || !dst || dst->w <= 0 || dst->h <= 0) {
        return;
    }

    int enabled = 0;
    if (!libretro_host_debugAmiGetBlitterDebug(&enabled) || !enabled) {
        return;
    }

    uint32_t srcWidth = 0;
    uint32_t srcHeight = 0;
    if (!emu_ami_blitterVisCache.pointsCap) {
        emu_ami_blitterVisCache.pointsCap = EMU_AMI_BLITTER_VIS_POINTS_CAP_DEFAULT;
        emu_ami_blitterVisCache.points = (e9k_debug_ami_blitter_vis_point_t *)realloc(emu_ami_blitterVisCache.points,
                                                                                       emu_ami_blitterVisCache.pointsCap * sizeof(*emu_ami_blitterVisCache.points));
        if (!emu_ami_blitterVisCache.points) {
            emu_ami_blitterVisCache.pointsCap = 0u;
            return;
        }
    }

    size_t fetchedCount = libretro_host_debugAmiReadBlitterVisPoints(emu_ami_blitterVisCache.points,
                                                                      emu_ami_blitterVisCache.pointsCap,
                                                                      &srcWidth,
                                                                      &srcHeight);
    e9k_debug_ami_blitter_vis_stats_t stats;
    int hasStats = libretro_host_debugAmiReadBlitterVisStats(&stats) ? 1 : 0;
    uint32_t mode = 0u;
    uint32_t frameCounter = 0u;
    int overlayMode = 1;
    uint32_t decayFrames = 0u;
    if (hasStats) {
        mode = stats.mode;
        frameCounter = stats.frameCounter;
        overlayMode = ((mode & EMU_AMI_BLITTER_VIS_MODE_OVERLAY) != 0u) ? 1 : 0;
    } else {
        frameCounter = ++emu_ami_blitterVisCache.overlayFrameCounter;
    }
    if (overlayMode) {
        int uiDecay = custom_ui_getBlitterVisDecay();
        if (uiDecay < 0) {
            uiDecay = 0;
        }
        decayFrames = (uint32_t)uiDecay;
    }
    if (!srcWidth || !srcHeight) {
        if (emu_ami_blitterVisCache.hasRetainedOverlay && emu_ami_blitterVisCache.texture) {
            SDL_SetTextureBlendMode(emu_ami_blitterVisCache.texture, SDL_BLENDMODE_BLEND);
            SDL_RenderCopy(ctx->renderer, emu_ami_blitterVisCache.texture, NULL, dst);
        }
        return;
    }

    if (fetchedCount > emu_ami_blitterVisCache.pointsCap) {
        e9k_debug_ami_blitter_vis_point_t *nextPoints = (e9k_debug_ami_blitter_vis_point_t *)realloc(emu_ami_blitterVisCache.points,
                                                                                                       fetchedCount * sizeof(*nextPoints));
        if (!nextPoints) {
            return;
        }
        emu_ami_blitterVisCache.points = nextPoints;
        emu_ami_blitterVisCache.pointsCap = fetchedCount;
        fetchedCount = libretro_host_debugAmiReadBlitterVisPoints(emu_ami_blitterVisCache.points,
                                                                   emu_ami_blitterVisCache.pointsCap,
                                                                   &srcWidth,
                                                                   &srcHeight);
    }

    if (!srcWidth || !srcHeight) {
        if (emu_ami_blitterVisCache.hasRetainedOverlay && emu_ami_blitterVisCache.texture) {
            SDL_SetTextureBlendMode(emu_ami_blitterVisCache.texture, SDL_BLENDMODE_BLEND);
            SDL_RenderCopy(ctx->renderer, emu_ami_blitterVisCache.texture, NULL, dst);
        }
        return;
    }

    int textureWidth = (int)srcWidth;
    int textureHeight = (int)srcHeight;
    if (textureWidth <= 0 || textureHeight <= 0) {
        return;
    }

    if (emu_ami_blitterVisCache.renderer != ctx->renderer) {
        if (emu_ami_blitterVisCache.texture) {
            SDL_DestroyTexture(emu_ami_blitterVisCache.texture);
            emu_ami_blitterVisCache.texture = NULL;
        }
        emu_ami_blitterVisCache.renderer = ctx->renderer;
        emu_ami_blitterVisCache.texWidth = 0;
        emu_ami_blitterVisCache.texHeight = 0;
        emu_ami_blitterVisCache.hasRetainedOverlay = 0;
    }

    int textureRecreated = 0;
    if (!emu_ami_blitterVisCache.texture ||
        emu_ami_blitterVisCache.texWidth != textureWidth ||
        emu_ami_blitterVisCache.texHeight != textureHeight) {
        if (emu_ami_blitterVisCache.texture) {
            SDL_DestroyTexture(emu_ami_blitterVisCache.texture);
            emu_ami_blitterVisCache.texture = NULL;
        }
        emu_ami_blitterVisCache.texture = SDL_CreateTexture(ctx->renderer,
                                                            SDL_PIXELFORMAT_ARGB8888,
                                                            SDL_TEXTUREACCESS_STREAMING,
                                                            textureWidth,
                                                            textureHeight);
        if (!emu_ami_blitterVisCache.texture) {
            return;
        }
        emu_ami_blitterVisCache.texWidth = textureWidth;
        emu_ami_blitterVisCache.texHeight = textureHeight;
        emu_ami_blitterVisCache.hasRetainedOverlay = 0;
        textureRecreated = 1;
    }

    size_t pixelCount = (size_t)textureWidth * (size_t)textureHeight;
    if (pixelCount > emu_ami_blitterVisCache.pixelsCap) {
        uint32_t *nextPixels = (uint32_t *)realloc(emu_ami_blitterVisCache.pixels, pixelCount * sizeof(*nextPixels));
        if (!nextPixels) {
            return;
        }
        emu_ami_blitterVisCache.pixels = nextPixels;
        emu_ami_blitterVisCache.pixelsCap = pixelCount;
    }

    if (pixelCount > emu_ami_blitterVisCache.retainedCap) {
        uint32_t *nextRetainedBlitIds =
            (uint32_t *)realloc(emu_ami_blitterVisCache.retainedBlitIds, pixelCount * sizeof(*nextRetainedBlitIds));
        if (!nextRetainedBlitIds) {
            return;
        }
        emu_ami_blitterVisCache.retainedBlitIds = nextRetainedBlitIds;
        size_t oldRetainedCap = emu_ami_blitterVisCache.retainedCap;
        emu_ami_blitterVisCache.retainedCap = pixelCount;
        if (pixelCount > oldRetainedCap) {
            memset(emu_ami_blitterVisCache.retainedBlitIds + oldRetainedCap,
                   0,
                   (pixelCount - oldRetainedCap) * sizeof(*emu_ami_blitterVisCache.retainedBlitIds));
        }
    }

    if (textureRecreated &&
        emu_ami_blitterVisCache.retainedBlitIds &&
        emu_ami_blitterVisCache.retainedCap >= pixelCount) {
        memset(emu_ami_blitterVisCache.retainedBlitIds, 0, pixelCount * sizeof(*emu_ami_blitterVisCache.retainedBlitIds));
        if (emu_ami_blitterVisCache.blitFrameIds && emu_ami_blitterVisCache.blitFrameCap) {
            memset(emu_ami_blitterVisCache.blitFrameIds, 0, emu_ami_blitterVisCache.blitFrameCap * sizeof(*emu_ami_blitterVisCache.blitFrameIds));
        }
        emu_ami_blitterVisCache.blitFrameCount = 0u;
    }

    int hasVisiblePixels = 0;
    if (overlayMode &&
        emu_ami_blitterVisCache.retainedBlitIds &&
        emu_ami_blitterVisCache.retainedCap >= pixelCount) {
        for (size_t i = 0; i < fetchedCount; i++) {
            uint32_t x = emu_ami_blitterVisCache.points[i].x;
            uint32_t y = emu_ami_blitterVisCache.points[i].y;
            if (x >= srcWidth || y >= srcHeight) {
                continue;
            }
            uint32_t blitId = emu_ami_blitterVisCache.points[i].blitId;
            if (!blitId) {
                continue;
            }
            size_t pixelIndex = (size_t)y * (size_t)srcWidth + (size_t)x;
            emu_ami_blitterVisCache.retainedBlitIds[pixelIndex] = blitId;
            (void)emu_ami_blitterVisSetBlitFrame(&emu_ami_blitterVisCache, blitId, frameCounter);
        }

        for (size_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex) {
            uint32_t blitId = emu_ami_blitterVisCache.retainedBlitIds[pixelIndex];
            if (!blitId) {
                emu_ami_blitterVisCache.pixels[pixelIndex] = 0u;
                continue;
            }
            uint32_t blitFrame = 0u;
            if (!emu_ami_blitterVisGetBlitFrame(&emu_ami_blitterVisCache, blitId, &blitFrame)) {
                emu_ami_blitterVisCache.pixels[pixelIndex] = 0u;
                emu_ami_blitterVisCache.retainedBlitIds[pixelIndex] = 0u;
                continue;
            }
            uint32_t age = frameCounter - blitFrame;
            if (decayFrames > 0u && age < decayFrames) {
                uint32_t color = emu_ami_blitterVisColorFromId(blitId);
                uint32_t rgb = color & 0x00ffffffu;
                uint8_t alpha = emu_ami_blitterVisAlphaForAge(age, decayFrames);
                emu_ami_blitterVisCache.pixels[pixelIndex] = ((uint32_t)alpha << 24) | rgb;
                hasVisiblePixels = 1;
            } else {
                emu_ami_blitterVisCache.pixels[pixelIndex] = 0u;
                emu_ami_blitterVisCache.retainedBlitIds[pixelIndex] = 0u;
            }
        }
    } else {
        memset(emu_ami_blitterVisCache.pixels, 0, pixelCount * sizeof(*emu_ami_blitterVisCache.pixels));
        for (size_t i = 0; i < fetchedCount; i++) {
            uint32_t x = emu_ami_blitterVisCache.points[i].x;
            uint32_t y = emu_ami_blitterVisCache.points[i].y;
            if (x >= srcWidth || y >= srcHeight) {
                continue;
            }
            emu_ami_blitterVisCache.pixels[(size_t)y * (size_t)srcWidth + (size_t)x] =
                emu_ami_blitterVisColorFromId(emu_ami_blitterVisCache.points[i].blitId);
            hasVisiblePixels = 1;
        }
    }

    SDL_UpdateTexture(emu_ami_blitterVisCache.texture,
                      NULL,
                      emu_ami_blitterVisCache.pixels,
                      textureWidth * (int)sizeof(*emu_ami_blitterVisCache.pixels));
    emu_ami_blitterVisCache.hasRetainedOverlay = hasVisiblePixels ? 1 : 0;
    SDL_SetTextureBlendMode(emu_ami_blitterVisCache.texture, SDL_BLENDMODE_BLEND);
    SDL_RenderCopy(ctx->renderer, emu_ami_blitterVisCache.texture, NULL, dst);
}

static int
emu_ami_mapKeyToJoypad(SDL_Keycode key, unsigned *id)
{
    return debugger_input_bindings_mapKeyToJoypad(TARGET_AMIGA,
                                                  (target && target->coreOptionGetValue)
                                                      ? target->coreOptionGetValue
                                                      : NULL,
                                                  key,
                                                  id);
}

uint16_t
emu_ami_translateModifiers(SDL_Keymod mod)
{
    uint16_t out = 0;
    if (mod & KMOD_SHIFT) {
        out |= RETROKMOD_SHIFT;
    }
    if (mod & KMOD_CTRL) {
        out |= RETROKMOD_CTRL;
    }
    if (mod & KMOD_ALT) {
        out |= RETROKMOD_ALT;
    }
    if (mod & KMOD_GUI) {
        out |= RETROKMOD_META;
    }
    if (mod & KMOD_NUM) {
        out |= RETROKMOD_NUMLOCK;
    }
    if (mod & KMOD_CAPS) {
        out |= RETROKMOD_CAPSLOCK;
    }
    return out;
}

uint32_t
emu_ami_translateCharacter(SDL_Keycode key, SDL_Keymod mod)
{
    if (key < 32 || key >= 127) {
        return 0;
    }
    int shift = (mod & KMOD_SHIFT) ? 1 : 0;
    int caps = (mod & KMOD_CAPS) ? 1 : 0;
    if (key >= 'a' && key <= 'z') {
        if (shift ^ caps) {
            return (uint32_t)toupper((int)key);
        }
        return (uint32_t)key;
    }
    if (!shift) {
        return (uint32_t)key;
    }
    switch (key) {
    case '1': return '!';
    case '2': return '@';
    case '3': return '#';
    case '4': return '$';
    case '5': return '%';
    case '6': return '^';
    case '7': return '&';
    case '8': return '*';
    case '9': return '(';
    case '0': return ')';
    case '-': return '_';
    case '=': return '+';
    case '[': return '{';
    case ']': return '}';
    case '\\': return '|';
    case ';': return ':';
    case '\'': return '"';
    case ',': return '<';
    case '.': return '>';
    case '/': return '?';
    case '`': return '~';
    default:
        break;
    }
    return (uint32_t)key;
}

unsigned
emu_ami_translateKey(SDL_Keycode key)
{
    if (key >= 32 && key < 127) {
        if (key >= 'A' && key <= 'Z') {
            return (unsigned)tolower((int)key);
        }
        return (unsigned)key;
    }
    switch (key) {
    case SDLK_BACKSPACE: return RETROK_BACKSPACE;
    case SDLK_TAB: return RETROK_TAB;
    case SDLK_RETURN: return RETROK_RETURN;
    case SDLK_ESCAPE: return RETROK_ESCAPE;
    case SDLK_DELETE: return RETROK_DELETE;
    case SDLK_INSERT: return RETROK_INSERT;
    case SDLK_HOME: return RETROK_HOME;
    case SDLK_END: return RETROK_END;
    case SDLK_PAGEUP: return RETROK_PAGEUP;
    case SDLK_PAGEDOWN: return RETROK_PAGEDOWN;
    case SDLK_UP: return RETROK_UP;
    case SDLK_DOWN: return RETROK_DOWN;
    case SDLK_LEFT: return RETROK_LEFT;
    case SDLK_RIGHT: return RETROK_RIGHT;
    case SDLK_F1: return RETROK_F1;
    case SDLK_F2: return RETROK_F2;
    case SDLK_F3: return RETROK_F3;
    case SDLK_F4: return RETROK_F4;
    case SDLK_F5: return RETROK_F5;
    case SDLK_F6: return RETROK_F6;
    case SDLK_F7: return RETROK_F7;
    case SDLK_F8: return RETROK_F8;
    case SDLK_F9: return RETROK_F9;
    case SDLK_F10: return RETROK_F10;
    case SDLK_F11: return RETROK_F11;
    case SDLK_F12: return RETROK_F12;
    case SDLK_LSHIFT: return RETROK_LSHIFT;
    case SDLK_RSHIFT: return RETROK_RSHIFT;
    case SDLK_LCTRL: return RETROK_LCTRL;
    case SDLK_RCTRL: return RETROK_RCTRL;
    case SDLK_LALT: return RETROK_LALT;
    case SDLK_RALT: return RETROK_RALT;
    case SDLK_LGUI: return RETROK_LMETA;
    case SDLK_RGUI: return RETROK_RMETA;
    default:
        break;
    }
    return RETROK_UNKNOWN;
}

static void
emu_ami_cycleDebugDma(e9ui_context_t *ctx, void *user)
{
    (void)ctx;
    (void)user;
    int *debugDma = debugger.amigaDebug.debugDma;
    if (!debugDma) {
        return;
    }
    switch (*debugDma) {
    case 0: *debugDma = 2; break;
    case 2: *debugDma = 3; break;
    case 3: *debugDma = 4; break;
    default: *debugDma = 0; break;
    }
}

static void
emu_ami_toggleCustom(e9ui_context_t *ctx, void *user)
{
    (void)ctx;
    (void)user;
    if (custom_ui_isOpen()) {
        custom_ui_shutdown();
    } else {
        (void)custom_ui_init();
    }
}

static void
emu_ami_toggleCustomLog(e9ui_context_t *ctx, void *user)
{
    (void)ctx;
    (void)user;
    if (custom_log_isOpen()) {
        custom_log_shutdown();
    } else {
        (void)custom_log_init();
    }
}

static void
emu_ami_createOverlays(e9ui_component_t* comp, e9ui_component_t* button_stack)
{
    emu_ami_tryBindCustomLogFrameCallback();

    e9ui_component_t *btn = e9ui_button_make("DMA Debug", emu_ami_cycleDebugDma, comp);
    if (btn) {
        e9ui_button_setMini(btn, 1);
        e9ui_setFocusTarget(btn, comp);
        void* dmaDebugBtnMeta = alloc_strdup("dma_debug");
        if (button_stack) {
            e9ui_child_add(button_stack, btn, dmaDebugBtnMeta);
        } else {
            e9ui_child_add(comp, btn, dmaDebugBtnMeta);
        }
    }

    e9ui_component_t *btnCustom = e9ui_button_make("Custom Debug", emu_ami_toggleCustom, comp);
    if (btnCustom) {
        e9ui_button_setMini(btnCustom, 1);
        e9ui_setFocusTarget(btnCustom, comp);
        void *customBtnMeta = alloc_strdup("custom");
        if (button_stack) {
            e9ui_child_add(button_stack, btnCustom, customBtnMeta);
        } else {
            e9ui_child_add(comp, btnCustom, customBtnMeta);
        }
    }

    e9ui_component_t *btnCustomLog = e9ui_button_make("Custom Logger", emu_ami_toggleCustomLog, comp);
    if (btnCustomLog) {
        e9ui_button_setMini(btnCustomLog, 1);
        e9ui_setFocusTarget(btnCustomLog, comp);
        void *customLogBtnMeta = alloc_strdup("custom_log");
        if (button_stack) {
            e9ui_child_add(button_stack, btnCustomLog, customLogBtnMeta);
        } else {
            e9ui_child_add(comp, btnCustomLog, customLogBtnMeta);
        }
    }
}

static void
emu_ami_render(e9ui_context_t *ctx, SDL_Rect* dst)
{
    emu_ami_tryBindCustomLogFrameCallback();
    emu_ami_renderBlitterVisOverlay(ctx, dst);
}

const emu_system_iface_t emu_ami_iface = {
    .translateCharacter = emu_ami_translateCharacter,
    .translateModifiers = emu_ami_translateModifiers,
    .translateKey = emu_ami_translateKey,
    .mapKeyToJoypad = emu_ami_mapKeyToJoypad,
    .mouseCaptureCanEnable = emu_ami_mouseCaptureCanEnable,
    .rangeBarCount = emu_ami_rangeBarCount,
    .rangeBarDescribe = emu_ami_rangeBarDescribe,
    .rangeBarChanged = emu_ami_rangeBarChanged,
    .rangeBarDragging = emu_ami_rangeBarDragging,
    .rangeBarTooltip = emu_ami_rangeBarTooltip,
    .rangeBarSync = emu_ami_rangeBarSync,
    .createOverlays = emu_ami_createOverlays,
    .render = emu_ami_render,
    .destroy = emu_ami_destroy,
};
