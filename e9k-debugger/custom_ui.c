/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>

#include "config.h"
#include "alloc.h"
#include "custom_ui.h"
#include "debug.h"
#include "debugger.h"
#include "e9ui.h"
#include "e9ui_scroll.h"
#include "e9ui_seek_bar.h"
#include "libretro_host.h"

#define CUSTOM_UI_TITLE "ENGINE9000 DEBUGGER - CUSTOM"
#define CUSTOM_UI_AMIGA_SPRITE_COUNT 8
#define CUSTOM_UI_AMIGA_BITPLANE_COUNT 8
#define CUSTOM_UI_AMIGA_AUDIO_CHANNEL_COUNT 4
#define CUSTOM_UI_AMIGA_BPLPTR_COUNT 6
#define CUSTOM_UI_BLITTER_VIS_MODE_SOLID 0x1
#define CUSTOM_UI_BLITTER_VIS_MODE_COLLECT 0x2
#define CUSTOM_UI_BLITTER_VIS_MODE_PATTERN 0x4
#define CUSTOM_UI_BLITTER_VIS_MODE_STYLE_MASK (CUSTOM_UI_BLITTER_VIS_MODE_SOLID | CUSTOM_UI_BLITTER_VIS_MODE_PATTERN)
#define CUSTOM_UI_BLITTER_VIS_DECAY_MAX 64
#define CUSTOM_UI_COPPER_LINE_MAX 2047

typedef struct custom_ui_state {
    int open;
    int closeRequested;
    int blitterEnabled;
    int blitterDebugEnabled;
    int suppressBlitterDebugCallbacks;
    int blitterVisMode;
    int suppressBlitterVisModeCallbacks;
    int blitterVisBlink;
    int blitterVisDecay;
    int copperLimitEnabled;
    int copperLimitStart;
    int copperLimitEnd;
    int bplptrBlockAllEnabled;
    int bplptrBlockEnabled[CUSTOM_UI_AMIGA_BPLPTR_COUNT];
    int bplptrLineLimitStart;
    int bplptrLineLimitEnd;
    int suppressBplptrBlockCallbacks;
    int bplcon1DelayScrollEnabled;
    int spritesEnabled;
    int spriteEnabled[CUSTOM_UI_AMIGA_SPRITE_COUNT];
    int suppressSpriteCallbacks;
    int bitplanesEnabled;
    int bitplaneEnabled[CUSTOM_UI_AMIGA_BITPLANE_COUNT];
    int suppressBitplaneCallbacks;
    int audiosEnabled;
    int suppressAudioCallbacks;
    int audioEnabled[CUSTOM_UI_AMIGA_AUDIO_CHANNEL_COUNT];
    int warnedMissingOption;
    e9ui_window_t *windowHost;
    int winX;
    int winY;
    int winW;
    int winH;
    int winHasSaved;
    e9ui_context_t ctx;
    e9ui_component_t *root;
    e9ui_component_t *fullscreen;
    e9ui_component_t *pendingRemove;
    e9ui_component_t *spritesCheckbox;
    e9ui_component_t *spriteCheckboxes[CUSTOM_UI_AMIGA_SPRITE_COUNT];
    e9ui_component_t *bitplanesCheckbox;
    e9ui_component_t *bitplaneCheckboxes[CUSTOM_UI_AMIGA_BITPLANE_COUNT];
    e9ui_component_t *audiosCheckbox;
    e9ui_component_t *blitterDebugCheckbox;
    e9ui_component_t *blitterVisPatternCheckbox;
    e9ui_component_t *blitterVisModeCheckbox;
    e9ui_component_t *blitterVisCollectCheckbox;
    e9ui_component_t *blitterVisBlinkCheckbox;
    e9ui_component_t *blitterVisDecayRow;
    e9ui_component_t *blitterVisDecayTextbox;
    e9ui_component_t *blitterVisDecaySeekRow;
    e9ui_component_t *blitterVisDecaySeekBar;
    int blitterVisDecayTextboxHadFocus;
    e9ui_component_t *copperLimitCheckbox;
    e9ui_component_t *copperLimitStartRow;
    e9ui_component_t *copperLimitStartTextbox;
    int copperLimitStartTextboxHadFocus;
    e9ui_component_t *copperLimitEndRow;
    e9ui_component_t *copperLimitEndTextbox;
    int copperLimitEndTextboxHadFocus;
    e9ui_component_t *bplptrBlockAllCheckbox;
    e9ui_component_t *bplptrLineLimitStartRow;
    e9ui_component_t *bplptrLineLimitStartTextbox;
    int bplptrLineLimitStartTextboxHadFocus;
    e9ui_component_t *bplptrLineLimitEndRow;
    e9ui_component_t *bplptrLineLimitEndTextbox;
    int bplptrLineLimitEndTextboxHadFocus;
    e9ui_component_t *bplptrBlockCheckboxes[CUSTOM_UI_AMIGA_BPLPTR_COUNT];
    e9ui_component_t *audioCheckboxes[CUSTOM_UI_AMIGA_AUDIO_CHANNEL_COUNT];
    struct custom_ui_sprite_cb {
        struct custom_ui_state *ui;
        int spriteIndex;
    } spriteCb[CUSTOM_UI_AMIGA_SPRITE_COUNT];
    struct custom_ui_bitplane_cb {
        struct custom_ui_state *ui;
        int bitplaneIndex;
    } bitplaneCb[CUSTOM_UI_AMIGA_BITPLANE_COUNT];
    struct custom_ui_audio_cb {
        struct custom_ui_state *ui;
        int audioChannelIndex;
    } audioCb[CUSTOM_UI_AMIGA_AUDIO_CHANNEL_COUNT];
    struct custom_ui_bplptr_cb {
        struct custom_ui_state *ui;
        int bplptrIndex;
    } bplptrCb[CUSTOM_UI_AMIGA_BPLPTR_COUNT];
} custom_ui_state_t;

typedef struct custom_ui_seek_row_state {
    e9ui_component_t *bar;
    int leftInset;
    int rightInset;
    int barHeight;
    int rowPadding;
} custom_ui_seek_row_state_t;

typedef struct custom_ui_overlay_body_state {
    custom_ui_state_t *ui;
} custom_ui_overlay_body_state_t;

static custom_ui_state_t custom_ui_state = {
    .blitterEnabled = 1,
    .blitterDebugEnabled = 0,
    .blitterVisMode = CUSTOM_UI_BLITTER_VIS_MODE_COLLECT,
    .blitterVisBlink = 1,
    .blitterVisDecay = 5,
    .copperLimitEnabled = 0,
    .copperLimitStart = 52,
    .copperLimitEnd = 308,
    .bplptrBlockAllEnabled = 0,
    .bplptrBlockEnabled = { 0, 0, 0, 0, 0, 0 },
    .bplptrLineLimitStart = 52,
    .bplptrLineLimitEnd = 308,
    .bplcon1DelayScrollEnabled = 1,
    .spritesEnabled = 1,
    .spriteEnabled = { 1, 1, 1, 1, 1, 1, 1, 1 },
    .bitplanesEnabled = 1,
    .bitplaneEnabled = { 1, 1, 1, 1, 1, 1, 1, 1 },
    .audiosEnabled = 1,
    .audioEnabled = { 1, 1, 1, 1 }
};

static int
custom_ui_parseInt(const char *value, int *out)
{
    if (!value || !out) {
        return 0;
    }
    char *end = NULL;
    long parsed = strtol(value, &end, 10);
    if (!end || end == value) {
        return 0;
    }
    if (parsed < INT_MIN || parsed > INT_MAX) {
        return 0;
    }
    *out = (int)parsed;
    return 1;
}

static int
custom_ui_clampCopperLine(int line);

static e9ui_window_backend_t
custom_ui_windowBackend(void)
{
    return e9ui_window_backend_overlay;
}

static e9ui_rect_t
custom_ui_windowDefaultRect(const e9ui_context_t *ctx)
{
    e9ui_rect_t rect = {
        e9ui_scale_px(ctx, 64),
        e9ui_scale_px(ctx, 64),
        e9ui_scale_px(ctx, 560),
        e9ui_scale_px(ctx, 560)
    };
    return rect;
}

static float
custom_ui_blitterVisDecayToPercent(int decay)
{
    int clamped = decay;
    if (clamped < 1) {
        clamped = 1;
    }
    if (clamped > CUSTOM_UI_BLITTER_VIS_DECAY_MAX) {
        clamped = CUSTOM_UI_BLITTER_VIS_DECAY_MAX;
    }
    if (CUSTOM_UI_BLITTER_VIS_DECAY_MAX <= 1) {
        return 1.0f;
    }
    return (float)(clamped - 1) / (float)(CUSTOM_UI_BLITTER_VIS_DECAY_MAX - 1);
}

static int
custom_ui_blitterVisDecayFromPercent(float percent)
{
    float clamped = percent;
    if (clamped < 0.0f) {
        clamped = 0.0f;
    }
    if (clamped > 1.0f) {
        clamped = 1.0f;
    }
    int decay = 1 + (int)(clamped * (float)(CUSTOM_UI_BLITTER_VIS_DECAY_MAX - 1) + 0.5f);
    if (decay < 1) {
        decay = 1;
    }
    if (decay > CUSTOM_UI_BLITTER_VIS_DECAY_MAX) {
        decay = CUSTOM_UI_BLITTER_VIS_DECAY_MAX;
    }
    return decay;
}

static void
custom_ui_syncBlitterVisDecaySeekBar(custom_ui_state_t *ui)
{
    if (!ui || !ui->blitterVisDecaySeekBar) {
        return;
    }
    e9ui_seek_bar_setPercent(ui->blitterVisDecaySeekBar, custom_ui_blitterVisDecayToPercent(ui->blitterVisDecay));
}

static int
custom_ui_seekRowPreferredHeight(e9ui_component_t *self, e9ui_context_t *ctx, int availW)
{
    (void)availW;
    if (!self || !self->state || !ctx) {
        return 0;
    }
    custom_ui_seek_row_state_t *st = (custom_ui_seek_row_state_t*)self->state;
    int barH = e9ui_scale_px(ctx, st->barHeight);
    int pad = e9ui_scale_px(ctx, st->rowPadding);
    if (barH <= 0) {
        barH = 10;
    }
    return barH + pad * 2;
}

static void
custom_ui_seekRowLayout(e9ui_component_t *self, e9ui_context_t *ctx, e9ui_rect_t bounds)
{
    if (!self || !self->state || !ctx) {
        return;
    }
    custom_ui_seek_row_state_t *st = (custom_ui_seek_row_state_t*)self->state;
    self->bounds = bounds;
    if (!st->bar) {
        return;
    }
    int leftInset = e9ui_scale_px(ctx, st->leftInset);
    int rightInset = e9ui_scale_px(ctx, st->rightInset);
    int barH = e9ui_scale_px(ctx, st->barHeight);
    if (barH <= 0) {
        barH = 10;
    }
    int barW = bounds.w - leftInset - rightInset;
    if (barW < 1) {
        barW = 1;
    }
    st->bar->bounds.x = bounds.x + leftInset;
    st->bar->bounds.w = barW;
    st->bar->bounds.h = barH;
    st->bar->bounds.y = bounds.y + (bounds.h - barH) / 2;
}

static void
custom_ui_seekRowRender(e9ui_component_t *self, e9ui_context_t *ctx)
{
    if (!self || !self->state || !ctx) {
        return;
    }
    custom_ui_seek_row_state_t *st = (custom_ui_seek_row_state_t*)self->state;
    if (st->bar && st->bar->render) {
        st->bar->render(st->bar, ctx);
    }
}

static void
custom_ui_seekRowDtor(e9ui_component_t *self, e9ui_context_t *ctx)
{
    (void)ctx;
    if (!self) {
        return;
    }
    if (self->state) {
        alloc_free(self->state);
        self->state = NULL;
    }
}

static e9ui_component_t *
custom_ui_blitterVisDecaySeekRowMake(e9ui_component_t **outBar)
{
    e9ui_component_t *row = (e9ui_component_t*)alloc_calloc(1, sizeof(*row));
    custom_ui_seek_row_state_t *st = (custom_ui_seek_row_state_t*)alloc_calloc(1, sizeof(*st));
    if (!row || !st) {
        if (row) {
            alloc_free(row);
        }
        if (st) {
            alloc_free(st);
        }
        return NULL;
    }
    st->leftInset = 92;
    st->rightInset = 14;
    st->barHeight = 10;
    st->rowPadding = 3;
    st->bar = e9ui_seek_bar_make();
    if (st->bar) {
        e9ui_seek_bar_setMargins(st->bar, 0, 0, 0);
        e9ui_seek_bar_setHeight(st->bar, 10);
        e9ui_seek_bar_setHoverMargin(st->bar, 6);
    }
    row->name = "custom_ui_seek_row";
    row->state = st;
    row->preferredHeight = custom_ui_seekRowPreferredHeight;
    row->layout = custom_ui_seekRowLayout;
    row->render = custom_ui_seekRowRender;
    row->dtor = custom_ui_seekRowDtor;
    if (st->bar) {
        e9ui_child_add(row, st->bar, NULL);
    }
    if (outBar) {
        *outBar = st->bar;
    }
    return row;
}

static void
custom_ui_applyOption(e9k_debug_option_t option, uint32_t argument)
{
    custom_ui_state_t *ui = &custom_ui_state;
    if (libretro_host_debugSetDebugOption(option, argument, NULL)) {
        ui->warnedMissingOption = 0;
        return;
    }
    if (!ui->warnedMissingOption) {
        debug_error("custom ui: core does not expose debug option API");
        ui->warnedMissingOption = 1;
    }
}

static e9k_debug_option_t
custom_ui_spriteOptionForIndex(int spriteIndex)
{
    switch (spriteIndex) {
        case 0: return E9K_DEBUG_OPTION_AMIGA_SPRITE0;
        case 1: return E9K_DEBUG_OPTION_AMIGA_SPRITE1;
        case 2: return E9K_DEBUG_OPTION_AMIGA_SPRITE2;
        case 3: return E9K_DEBUG_OPTION_AMIGA_SPRITE3;
        case 4: return E9K_DEBUG_OPTION_AMIGA_SPRITE4;
        case 5: return E9K_DEBUG_OPTION_AMIGA_SPRITE5;
        case 6: return E9K_DEBUG_OPTION_AMIGA_SPRITE6;
        case 7: return E9K_DEBUG_OPTION_AMIGA_SPRITE7;
        default: break;
    }
    return e9k_debug_option_none;
}

static e9k_debug_option_t
custom_ui_bitplaneOptionForIndex(int bitplaneIndex)
{
    switch (bitplaneIndex) {
        case 0: return E9K_DEBUG_OPTION_AMIGA_BITPLANE0;
        case 1: return E9K_DEBUG_OPTION_AMIGA_BITPLANE1;
        case 2: return E9K_DEBUG_OPTION_AMIGA_BITPLANE2;
        case 3: return E9K_DEBUG_OPTION_AMIGA_BITPLANE3;
        case 4: return E9K_DEBUG_OPTION_AMIGA_BITPLANE4;
        case 5: return E9K_DEBUG_OPTION_AMIGA_BITPLANE5;
        case 6: return E9K_DEBUG_OPTION_AMIGA_BITPLANE6;
        case 7: return E9K_DEBUG_OPTION_AMIGA_BITPLANE7;
        default: break;
    }
    return e9k_debug_option_none;
}

static e9k_debug_option_t
custom_ui_audioOptionForIndex(int audioChannelIndex)
{
    switch (audioChannelIndex) {
        case 0: return E9K_DEBUG_OPTION_AMIGA_AUDIO0;
        case 1: return E9K_DEBUG_OPTION_AMIGA_AUDIO1;
        case 2: return E9K_DEBUG_OPTION_AMIGA_AUDIO2;
        case 3: return E9K_DEBUG_OPTION_AMIGA_AUDIO3;
        default: break;
    }
    return e9k_debug_option_none;
}

static e9k_debug_option_t
custom_ui_bplptrOptionForIndex(int bplptrIndex)
{
    switch (bplptrIndex) {
        case 0: return E9K_DEBUG_OPTION_AMIGA_BPLPTR1_BLOCK;
        case 1: return E9K_DEBUG_OPTION_AMIGA_BPLPTR2_BLOCK;
        case 2: return E9K_DEBUG_OPTION_AMIGA_BPLPTR3_BLOCK;
        case 3: return E9K_DEBUG_OPTION_AMIGA_BPLPTR4_BLOCK;
        case 4: return E9K_DEBUG_OPTION_AMIGA_BPLPTR5_BLOCK;
        case 5: return E9K_DEBUG_OPTION_AMIGA_BPLPTR6_BLOCK;
        default: break;
    }
    return e9k_debug_option_none;
}

static void
custom_ui_applyBlitterOption(void)
{
    custom_ui_state_t *ui = &custom_ui_state;
    custom_ui_applyOption(E9K_DEBUG_OPTION_AMIGA_BLITTER, ui->blitterEnabled ? 1u : 0u);
}

static void
custom_ui_applyBlitterVisDecayOption(void)
{
    custom_ui_state_t *ui = &custom_ui_state;
    custom_ui_applyOption(E9K_DEBUG_OPTION_AMIGA_BLITTER_VIS_DECAY, (uint32_t)ui->blitterVisDecay);
}

static void
custom_ui_applyBlitterVisModeOption(void)
{
    custom_ui_state_t *ui = &custom_ui_state;
    custom_ui_applyOption(E9K_DEBUG_OPTION_AMIGA_BLITTER_VIS_MODE, (uint32_t)ui->blitterVisMode);
}

static void
custom_ui_applyBlitterVisBlinkOption(void)
{
    custom_ui_state_t *ui = &custom_ui_state;
    custom_ui_applyOption(E9K_DEBUG_OPTION_AMIGA_BLITTER_VIS_BLINK, ui->blitterVisBlink ? 1u : 0u);
}

static void
custom_ui_applyBlitterDebugOption(void)
{
    custom_ui_state_t *ui = &custom_ui_state;
    (void)libretro_host_debugAmiSetBlitterDebug(ui->blitterDebugEnabled ? 1 : 0);
}

static void
custom_ui_applyBplcon1DelayScrollOption(void)
{
    custom_ui_state_t *ui = &custom_ui_state;
    custom_ui_applyOption(E9K_DEBUG_OPTION_AMIGA_BPLCON1_DELAY_SCROLL, ui->bplcon1DelayScrollEnabled ? 1u : 0u);
}

static void
custom_ui_applyCopperLimitEnabledOption(void)
{
    custom_ui_state_t *ui = &custom_ui_state;
    custom_ui_applyOption(E9K_DEBUG_OPTION_AMIGA_COPPER_LINE_LIMIT_ENABLED, ui->copperLimitEnabled ? 1u : 0u);
}

static void
custom_ui_applyCopperLimitStartOption(void)
{
    custom_ui_state_t *ui = &custom_ui_state;
    ui->copperLimitStart = custom_ui_clampCopperLine(ui->copperLimitStart);
    custom_ui_applyOption(E9K_DEBUG_OPTION_AMIGA_COPPER_LINE_LIMIT_START, (uint32_t)ui->copperLimitStart);
}

static void
custom_ui_applyCopperLimitEndOption(void)
{
    custom_ui_state_t *ui = &custom_ui_state;
    ui->copperLimitEnd = custom_ui_clampCopperLine(ui->copperLimitEnd);
    custom_ui_applyOption(E9K_DEBUG_OPTION_AMIGA_COPPER_LINE_LIMIT_END, (uint32_t)ui->copperLimitEnd);
}

static void
custom_ui_applyBplptrBlockAllOption(void)
{
    custom_ui_state_t *ui = &custom_ui_state;
    custom_ui_applyOption(E9K_DEBUG_OPTION_AMIGA_BPLPTR_BLOCK_ALL, ui->bplptrBlockAllEnabled ? 1u : 0u);
}

static void
custom_ui_applyBplptrLineLimitStartOption(void)
{
    custom_ui_state_t *ui = &custom_ui_state;
    ui->bplptrLineLimitStart = custom_ui_clampCopperLine(ui->bplptrLineLimitStart);
    custom_ui_applyOption(E9K_DEBUG_OPTION_AMIGA_BPLPTR_LINE_LIMIT_START, (uint32_t)ui->bplptrLineLimitStart);
}

static void
custom_ui_applyBplptrLineLimitEndOption(void)
{
    custom_ui_state_t *ui = &custom_ui_state;
    ui->bplptrLineLimitEnd = custom_ui_clampCopperLine(ui->bplptrLineLimitEnd);
    custom_ui_applyOption(E9K_DEBUG_OPTION_AMIGA_BPLPTR_LINE_LIMIT_END, (uint32_t)ui->bplptrLineLimitEnd);
}

static void
custom_ui_applyBplptrBlockOption(int bplptrIndex)
{
    custom_ui_state_t *ui = &custom_ui_state;
    if (bplptrIndex < 0 || bplptrIndex >= CUSTOM_UI_AMIGA_BPLPTR_COUNT) {
        return;
    }
    e9k_debug_option_t option = custom_ui_bplptrOptionForIndex(bplptrIndex);
    if (option == e9k_debug_option_none) {
        return;
    }
    custom_ui_applyOption(option, ui->bplptrBlockEnabled[bplptrIndex] ? 1u : 0u);
}

static void
custom_ui_applySpriteOption(int spriteIndex)
{
    custom_ui_state_t *ui = &custom_ui_state;
    if (spriteIndex < 0 || spriteIndex >= CUSTOM_UI_AMIGA_SPRITE_COUNT) {
        return;
    }
    e9k_debug_option_t option = custom_ui_spriteOptionForIndex(spriteIndex);
    if (option == e9k_debug_option_none) {
        return;
    }
    custom_ui_applyOption(option, ui->spriteEnabled[spriteIndex] ? 1u : 0u);
}

static void
custom_ui_applyBitplaneOption(int bitplaneIndex)
{
    custom_ui_state_t *ui = &custom_ui_state;
    if (bitplaneIndex < 0 || bitplaneIndex >= CUSTOM_UI_AMIGA_BITPLANE_COUNT) {
        return;
    }
    e9k_debug_option_t option = custom_ui_bitplaneOptionForIndex(bitplaneIndex);
    if (option == e9k_debug_option_none) {
        return;
    }
    custom_ui_applyOption(option, ui->bitplaneEnabled[bitplaneIndex] ? 1u : 0u);
}

static void
custom_ui_applyAudioOption(int audioChannelIndex)
{
    custom_ui_state_t *ui = &custom_ui_state;
    if (audioChannelIndex < 0 || audioChannelIndex >= CUSTOM_UI_AMIGA_AUDIO_CHANNEL_COUNT) {
        return;
    }
    e9k_debug_option_t option = custom_ui_audioOptionForIndex(audioChannelIndex);
    if (option == e9k_debug_option_none) {
        return;
    }
    custom_ui_applyOption(option, ui->audioEnabled[audioChannelIndex] ? 1u : 0u);
}

static void
custom_ui_applyAllOptions(void)
{
    custom_ui_applyBlitterOption();
    custom_ui_applyBlitterVisDecayOption();
    custom_ui_applyBlitterVisModeOption();
    custom_ui_applyBlitterVisBlinkOption();
    custom_ui_applyBplcon1DelayScrollOption();
    custom_ui_applyCopperLimitEnabledOption();
    custom_ui_applyCopperLimitStartOption();
    custom_ui_applyCopperLimitEndOption();
    custom_ui_applyBplptrBlockAllOption();
    custom_ui_applyBplptrLineLimitStartOption();
    custom_ui_applyBplptrLineLimitEndOption();
    for (int bplptrIndex = 0; bplptrIndex < CUSTOM_UI_AMIGA_BPLPTR_COUNT; ++bplptrIndex) {
        custom_ui_applyBplptrBlockOption(bplptrIndex);
    }
    for (int spriteIndex = 0; spriteIndex < CUSTOM_UI_AMIGA_SPRITE_COUNT; ++spriteIndex) {
        custom_ui_applySpriteOption(spriteIndex);
    }
    for (int bitplaneIndex = 0; bitplaneIndex < CUSTOM_UI_AMIGA_BITPLANE_COUNT; ++bitplaneIndex) {
        custom_ui_applyBitplaneOption(bitplaneIndex);
    }
    for (int audioChannelIndex = 0; audioChannelIndex < CUSTOM_UI_AMIGA_AUDIO_CHANNEL_COUNT; ++audioChannelIndex) {
        custom_ui_applyAudioOption(audioChannelIndex);
    }
}

static void
custom_ui_setComponentDisabled(e9ui_component_t *comp, int disabled)
{
    if (!comp) {
        return;
    }
    comp->disabled = disabled ? 1 : 0;
}

static void
custom_ui_syncBlitterDebugSuboptions(custom_ui_state_t *ui)
{
    if (!ui) {
        return;
    }
    int disabled = ui->blitterDebugEnabled ? 0 : 1;
    custom_ui_setComponentDisabled(ui->blitterVisCollectCheckbox, disabled);
    custom_ui_setComponentDisabled(ui->blitterVisDecayRow, disabled);
    custom_ui_setComponentDisabled(ui->blitterVisDecayTextbox, disabled);
    custom_ui_setComponentDisabled(ui->blitterVisDecaySeekRow, disabled);
    custom_ui_setComponentDisabled(ui->blitterVisDecaySeekBar, disabled);
    custom_ui_setComponentDisabled(ui->blitterVisBlinkCheckbox, disabled);
    custom_ui_setComponentDisabled(ui->blitterVisPatternCheckbox, disabled);
    custom_ui_setComponentDisabled(ui->blitterVisModeCheckbox, disabled);
}

static void
custom_ui_syncCopperLimitSuboptions(custom_ui_state_t *ui)
{
    if (!ui) {
        return;
    }
    int disabled = ui->copperLimitEnabled ? 0 : 1;
    custom_ui_setComponentDisabled(ui->copperLimitStartRow, disabled);
    custom_ui_setComponentDisabled(ui->copperLimitStartTextbox, disabled);
    custom_ui_setComponentDisabled(ui->copperLimitEndRow, disabled);
    custom_ui_setComponentDisabled(ui->copperLimitEndTextbox, disabled);
}

static void
custom_ui_syncBlitterDebugCheckbox(custom_ui_state_t *ui)
{
    if (!ui) {
        return;
    }
    int enabled = 0;
    if (!libretro_host_debugAmiGetBlitterDebug(&enabled)) {
        return;
    }
    ui->blitterDebugEnabled = enabled ? 1 : 0;
    if (ui->blitterDebugCheckbox) {
        ui->suppressBlitterDebugCallbacks = 1;
        e9ui_checkbox_setSelected(ui->blitterDebugCheckbox, ui->blitterDebugEnabled, &ui->ctx);
        ui->suppressBlitterDebugCallbacks = 0;
    }
    custom_ui_syncBlitterDebugSuboptions(ui);
}

static int
custom_ui_areAllSpritesEnabled(const custom_ui_state_t *ui)
{
    if (!ui) {
        return 0;
    }
    for (int spriteIndex = 0; spriteIndex < CUSTOM_UI_AMIGA_SPRITE_COUNT; ++spriteIndex) {
        if (!ui->spriteEnabled[spriteIndex]) {
            return 0;
        }
    }
    return 1;
}

static int
custom_ui_areAllBitplanesEnabled(const custom_ui_state_t *ui)
{
    if (!ui) {
        return 0;
    }
    for (int bitplaneIndex = 0; bitplaneIndex < CUSTOM_UI_AMIGA_BITPLANE_COUNT; ++bitplaneIndex) {
        if (!ui->bitplaneEnabled[bitplaneIndex]) {
            return 0;
        }
    }
    return 1;
}

static int
custom_ui_areAllAudiosEnabled(const custom_ui_state_t *ui)
{
    if (!ui) {
        return 0;
    }
    for (int audioChannelIndex = 0; audioChannelIndex < CUSTOM_UI_AMIGA_AUDIO_CHANNEL_COUNT; ++audioChannelIndex) {
        if (!ui->audioEnabled[audioChannelIndex]) {
            return 0;
        }
    }
    return 1;
}

static int
custom_ui_areAllBplptrBlocked(const custom_ui_state_t *ui)
{
    if (!ui) {
        return 0;
    }
    for (int bplptrIndex = 0; bplptrIndex < CUSTOM_UI_AMIGA_BPLPTR_COUNT; ++bplptrIndex) {
        if (!ui->bplptrBlockEnabled[bplptrIndex]) {
            return 0;
        }
    }
    return 1;
}

static void
custom_ui_syncSpritesMasterCheckbox(custom_ui_state_t *ui)
{
    if (!ui) {
        return;
    }
    ui->spritesEnabled = custom_ui_areAllSpritesEnabled(ui) ? 1 : 0;
    if (!ui->spritesCheckbox) {
        return;
    }
    ui->suppressSpriteCallbacks = 1;
    e9ui_checkbox_setSelected(ui->spritesCheckbox, ui->spritesEnabled, &ui->ctx);
    ui->suppressSpriteCallbacks = 0;
}

static void
custom_ui_syncBitplanesMasterCheckbox(custom_ui_state_t *ui)
{
    if (!ui) {
        return;
    }
    ui->bitplanesEnabled = custom_ui_areAllBitplanesEnabled(ui) ? 1 : 0;
    if (!ui->bitplanesCheckbox) {
        return;
    }
    ui->suppressBitplaneCallbacks = 1;
    e9ui_checkbox_setSelected(ui->bitplanesCheckbox, ui->bitplanesEnabled, &ui->ctx);
    ui->suppressBitplaneCallbacks = 0;
}

static void
custom_ui_syncAudiosMasterCheckbox(custom_ui_state_t *ui)
{
    if (!ui) {
        return;
    }
    ui->audiosEnabled = custom_ui_areAllAudiosEnabled(ui) ? 1 : 0;
    if (!ui->audiosCheckbox) {
        return;
    }
    ui->suppressAudioCallbacks = 1;
    e9ui_checkbox_setSelected(ui->audiosCheckbox, ui->audiosEnabled, &ui->ctx);
    ui->suppressAudioCallbacks = 0;
}

static void
custom_ui_syncBplptrBlockMasterCheckbox(custom_ui_state_t *ui)
{
    if (!ui) {
        return;
    }
    ui->bplptrBlockAllEnabled = custom_ui_areAllBplptrBlocked(ui) ? 1 : 0;
    if (!ui->bplptrBlockAllCheckbox) {
        return;
    }
    ui->suppressBplptrBlockCallbacks = 1;
    e9ui_checkbox_setSelected(ui->bplptrBlockAllCheckbox, ui->bplptrBlockAllEnabled, &ui->ctx);
    ui->suppressBplptrBlockCallbacks = 0;
}

static void
custom_ui_blitterChanged(e9ui_component_t *self, e9ui_context_t *ctx, int selected, void *user)
{
    (void)self;
    (void)ctx;
    custom_ui_state_t *ui = (custom_ui_state_t*)user;
    if (!ui) {
        return;
    }
    ui->blitterEnabled = selected ? 1 : 0;
    custom_ui_applyBlitterOption();
}

static void
custom_ui_copperLimitChanged(e9ui_component_t *self, e9ui_context_t *ctx, int selected, void *user)
{
    (void)self;
    (void)ctx;
    custom_ui_state_t *ui = (custom_ui_state_t*)user;
    if (!ui) {
        return;
    }
    ui->copperLimitEnabled = selected ? 1 : 0;
    custom_ui_applyCopperLimitEnabledOption();
    custom_ui_syncCopperLimitSuboptions(ui);
}

static void
custom_ui_bplptrBlockAllChanged(e9ui_component_t *self, e9ui_context_t *ctx, int selected, void *user)
{
    (void)self;
    (void)ctx;
    custom_ui_state_t *ui = (custom_ui_state_t*)user;
    if (!ui) {
        return;
    }
    if (ui->suppressBplptrBlockCallbacks) {
        return;
    }
    int nextValue = selected ? 1 : 0;
    ui->bplptrBlockAllEnabled = nextValue;
    custom_ui_applyBplptrBlockAllOption();
    for (int bplptrIndex = 0; bplptrIndex < CUSTOM_UI_AMIGA_BPLPTR_COUNT; ++bplptrIndex) {
        ui->bplptrBlockEnabled[bplptrIndex] = nextValue;
        custom_ui_applyBplptrBlockOption(bplptrIndex);
        if (ui->bplptrBlockCheckboxes[bplptrIndex]) {
            ui->suppressBplptrBlockCallbacks = 1;
            e9ui_checkbox_setSelected(ui->bplptrBlockCheckboxes[bplptrIndex], nextValue, &ui->ctx);
            ui->suppressBplptrBlockCallbacks = 0;
        }
    }
}

static void
custom_ui_bplptrBlockChanged(e9ui_component_t *self, e9ui_context_t *ctx, int selected, void *user)
{
    (void)self;
    (void)ctx;
    if (!user) {
        return;
    }
    struct custom_ui_bplptr_cb *cb = (struct custom_ui_bplptr_cb *)user;
    if (!cb->ui) {
        return;
    }
    if (cb->bplptrIndex < 0 || cb->bplptrIndex >= CUSTOM_UI_AMIGA_BPLPTR_COUNT) {
        return;
    }
    if (cb->ui->suppressBplptrBlockCallbacks) {
        return;
    }
    cb->ui->bplptrBlockEnabled[cb->bplptrIndex] = selected ? 1 : 0;
    custom_ui_applyBplptrBlockOption(cb->bplptrIndex);
    custom_ui_syncBplptrBlockMasterCheckbox(cb->ui);
    custom_ui_applyBplptrBlockAllOption();
}

static void
custom_ui_blitterDebugChanged(e9ui_component_t *self, e9ui_context_t *ctx, int selected, void *user)
{
    (void)self;
    (void)ctx;
    custom_ui_state_t *ui = (custom_ui_state_t*)user;
    if (!ui) {
        return;
    }
    if (ui->suppressBlitterDebugCallbacks) {
        return;
    }
    ui->blitterDebugEnabled = selected ? 1 : 0;
    custom_ui_applyBlitterDebugOption();
    custom_ui_syncBlitterDebugSuboptions(ui);
}

static void
custom_ui_blitterVisPatternChanged(e9ui_component_t *self, e9ui_context_t *ctx, int selected, void *user)
{
    (void)self;
    (void)ctx;
    custom_ui_state_t *ui = (custom_ui_state_t*)user;
    if (!ui) {
        return;
    }
    if (!ui->blitterDebugEnabled) {
        return;
    }
    if (ui->suppressBlitterVisModeCallbacks) {
        return;
    }
    if (selected) {
        ui->blitterVisMode |= CUSTOM_UI_BLITTER_VIS_MODE_PATTERN;
        ui->blitterVisMode &= ~CUSTOM_UI_BLITTER_VIS_MODE_SOLID;
        if (ui->blitterVisModeCheckbox) {
            ui->suppressBlitterVisModeCallbacks = 1;
            e9ui_checkbox_setSelected(ui->blitterVisModeCheckbox, 0, &ui->ctx);
            ui->suppressBlitterVisModeCallbacks = 0;
        }
    } else {
        ui->blitterVisMode &= ~CUSTOM_UI_BLITTER_VIS_MODE_PATTERN;
    }
    custom_ui_applyBlitterVisModeOption();
}

static void
custom_ui_blitterVisModeChanged(e9ui_component_t *self, e9ui_context_t *ctx, int selected, void *user)
{
    (void)self;
    (void)ctx;
    custom_ui_state_t *ui = (custom_ui_state_t*)user;
    if (!ui) {
        return;
    }
    if (!ui->blitterDebugEnabled) {
        return;
    }
    if (ui->suppressBlitterVisModeCallbacks) {
        return;
    }
    if (selected) {
        ui->blitterVisMode |= CUSTOM_UI_BLITTER_VIS_MODE_SOLID;
        ui->blitterVisMode &= ~CUSTOM_UI_BLITTER_VIS_MODE_PATTERN;
        if (ui->blitterVisPatternCheckbox) {
            ui->suppressBlitterVisModeCallbacks = 1;
            e9ui_checkbox_setSelected(ui->blitterVisPatternCheckbox, 0, &ui->ctx);
            ui->suppressBlitterVisModeCallbacks = 0;
        }
    } else {
        ui->blitterVisMode &= ~CUSTOM_UI_BLITTER_VIS_MODE_SOLID;
    }
    custom_ui_applyBlitterVisModeOption();
}

static void
custom_ui_blitterVisCollectChanged(e9ui_component_t *self, e9ui_context_t *ctx, int selected, void *user)
{
    (void)self;
    (void)ctx;
    custom_ui_state_t *ui = (custom_ui_state_t*)user;
    if (!ui) {
        return;
    }
    if (!ui->blitterDebugEnabled) {
        return;
    }
    if (ui->suppressBlitterVisModeCallbacks) {
        return;
    }
    if (selected) {
        ui->blitterVisMode |= CUSTOM_UI_BLITTER_VIS_MODE_COLLECT;
    } else {
        ui->blitterVisMode &= ~CUSTOM_UI_BLITTER_VIS_MODE_COLLECT;
    }
    custom_ui_applyBlitterVisModeOption();
}

static void
custom_ui_blitterVisBlinkChanged(e9ui_component_t *self, e9ui_context_t *ctx, int selected, void *user)
{
    (void)self;
    (void)ctx;
    custom_ui_state_t *ui = (custom_ui_state_t*)user;
    if (!ui) {
        return;
    }
    if (!ui->blitterDebugEnabled) {
        return;
    }
    ui->blitterVisBlink = selected ? 1 : 0;
    custom_ui_applyBlitterVisBlinkOption();
}

static void
custom_ui_blitterVisDecayChanged(e9ui_context_t *ctx, e9ui_component_t *comp, const char *value, void *user)
{
    (void)ctx;
    (void)comp;
    (void)value;
    (void)user;
}

static void
custom_ui_blitterVisDecaySeekChanged(float percent, void *user)
{
    custom_ui_state_t *ui = (custom_ui_state_t*)user;
    if (!ui || !ui->blitterDebugEnabled) {
        return;
    }
    int nextDecay = custom_ui_blitterVisDecayFromPercent(percent);
    if (nextDecay == ui->blitterVisDecay) {
        return;
    }
    ui->blitterVisDecay = nextDecay;
    custom_ui_applyBlitterVisDecayOption();
    if (ui->blitterVisDecayRow) {
        char decayText[16];
        snprintf(decayText, sizeof(decayText), "%d", ui->blitterVisDecay);
        e9ui_labeled_textbox_setText(ui->blitterVisDecayRow, decayText);
    }
    custom_ui_syncBlitterVisDecaySeekBar(ui);
}

static void
custom_ui_blitterVisDecaySeekTooltip(float percent, char *out, size_t cap, void *user)
{
    (void)user;
    if (!out || cap == 0) {
        return;
    }
    int decay = custom_ui_blitterVisDecayFromPercent(percent);
    snprintf(out, cap, "Decay %d", decay);
}

static void
custom_ui_copperLimitStartChanged(e9ui_context_t *ctx, e9ui_component_t *comp, const char *value, void *user)
{
    (void)ctx;
    (void)comp;
    (void)value;
    (void)user;
}

static void
custom_ui_copperLimitEndChanged(e9ui_context_t *ctx, e9ui_component_t *comp, const char *value, void *user)
{
    (void)ctx;
    (void)comp;
    (void)value;
    (void)user;
}

static void
custom_ui_bplptrLineLimitStartChanged(e9ui_context_t *ctx, e9ui_component_t *comp, const char *value, void *user)
{
    (void)ctx;
    (void)comp;
    (void)value;
    (void)user;
}

static void
custom_ui_bplptrLineLimitEndChanged(e9ui_context_t *ctx, e9ui_component_t *comp, const char *value, void *user)
{
    (void)ctx;
    (void)comp;
    (void)value;
    (void)user;
}

static int
custom_ui_clampCopperLine(int line)
{
    if (line < 0) {
        return 0;
    }
    if (line > CUSTOM_UI_COPPER_LINE_MAX) {
        return CUSTOM_UI_COPPER_LINE_MAX;
    }
    return line;
}

static void
custom_ui_commitBlitterVisDecayTextbox(custom_ui_state_t *ui)
{
    if (!ui || !ui->blitterVisDecayTextbox) {
        return;
    }
    if (!ui->blitterDebugEnabled) {
        return;
    }

    const char *value = e9ui_textbox_getText(ui->blitterVisDecayTextbox);
    int nextDecay = 0;
    if (!value || sscanf(value, "%d", &nextDecay) != 1) {
        return;
    }
    if (nextDecay <= 0 || nextDecay > CUSTOM_UI_BLITTER_VIS_DECAY_MAX) {
        return;
    }
    if (nextDecay == ui->blitterVisDecay) {
        return;
    }
    ui->blitterVisDecay = nextDecay;
    custom_ui_applyBlitterVisDecayOption();
    custom_ui_syncBlitterVisDecaySeekBar(ui);
}

static int
custom_ui_blitterVisDecayTextboxKey(e9ui_context_t *ctx, SDL_Keycode key, SDL_Keymod mods, void *user)
{
    (void)ctx;
    (void)mods;
    custom_ui_state_t *ui = (custom_ui_state_t*)user;
    if (!ui || !ui->blitterVisDecayTextbox) {
        return 0;
    }
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        custom_ui_commitBlitterVisDecayTextbox(ui);
    }
    return 0;
}

static void
custom_ui_tickBlitterVisDecayTextbox(custom_ui_state_t *ui)
{
    if (!ui || !ui->blitterVisDecayTextbox) {
        return;
    }
    int hasFocus = e9ui_getFocus(&ui->ctx) == ui->blitterVisDecayTextbox ? 1 : 0;
    if (ui->blitterVisDecayTextboxHadFocus && !hasFocus) {
        custom_ui_commitBlitterVisDecayTextbox(ui);
    }
    ui->blitterVisDecayTextboxHadFocus = hasFocus;
}

static void
custom_ui_commitCopperLimitStartTextbox(custom_ui_state_t *ui)
{
    if (!ui || !ui->copperLimitStartTextbox) {
        return;
    }
    if (!ui->copperLimitEnabled) {
        return;
    }

    const char *value = e9ui_textbox_getText(ui->copperLimitStartTextbox);
    int nextStart = 0;
    if (!value || sscanf(value, "%d", &nextStart) != 1) {
        return;
    }
    nextStart = custom_ui_clampCopperLine(nextStart);
    if (nextStart == ui->copperLimitStart) {
        return;
    }
    ui->copperLimitStart = nextStart;
    custom_ui_applyCopperLimitStartOption();
}

static void
custom_ui_commitCopperLimitEndTextbox(custom_ui_state_t *ui)
{
    if (!ui || !ui->copperLimitEndTextbox) {
        return;
    }
    if (!ui->copperLimitEnabled) {
        return;
    }

    const char *value = e9ui_textbox_getText(ui->copperLimitEndTextbox);
    int nextEnd = 0;
    if (!value || sscanf(value, "%d", &nextEnd) != 1) {
        return;
    }
    nextEnd = custom_ui_clampCopperLine(nextEnd);
    if (nextEnd == ui->copperLimitEnd) {
        return;
    }
    ui->copperLimitEnd = nextEnd;
    custom_ui_applyCopperLimitEndOption();
}

static int
custom_ui_copperLimitStartTextboxKey(e9ui_context_t *ctx, SDL_Keycode key, SDL_Keymod mods, void *user)
{
    (void)ctx;
    (void)mods;
    custom_ui_state_t *ui = (custom_ui_state_t*)user;
    if (!ui || !ui->copperLimitStartTextbox) {
        return 0;
    }
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        custom_ui_commitCopperLimitStartTextbox(ui);
    }
    return 0;
}

static int
custom_ui_copperLimitEndTextboxKey(e9ui_context_t *ctx, SDL_Keycode key, SDL_Keymod mods, void *user)
{
    (void)ctx;
    (void)mods;
    custom_ui_state_t *ui = (custom_ui_state_t*)user;
    if (!ui || !ui->copperLimitEndTextbox) {
        return 0;
    }
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        custom_ui_commitCopperLimitEndTextbox(ui);
    }
    return 0;
}

static void
custom_ui_tickCopperLimitTextboxes(custom_ui_state_t *ui)
{
    if (!ui) {
        return;
    }
    int startHasFocus = e9ui_getFocus(&ui->ctx) == ui->copperLimitStartTextbox ? 1 : 0;
    if (ui->copperLimitStartTextboxHadFocus && !startHasFocus) {
        custom_ui_commitCopperLimitStartTextbox(ui);
    }
    ui->copperLimitStartTextboxHadFocus = startHasFocus;

    int endHasFocus = e9ui_getFocus(&ui->ctx) == ui->copperLimitEndTextbox ? 1 : 0;
    if (ui->copperLimitEndTextboxHadFocus && !endHasFocus) {
        custom_ui_commitCopperLimitEndTextbox(ui);
    }
    ui->copperLimitEndTextboxHadFocus = endHasFocus;
}

static void
custom_ui_commitBplptrLineLimitStartTextbox(custom_ui_state_t *ui)
{
    if (!ui || !ui->bplptrLineLimitStartTextbox) {
        return;
    }

    const char *value = e9ui_textbox_getText(ui->bplptrLineLimitStartTextbox);
    int nextStart = 0;
    if (!value || sscanf(value, "%d", &nextStart) != 1) {
        return;
    }
    nextStart = custom_ui_clampCopperLine(nextStart);
    if (nextStart == ui->bplptrLineLimitStart) {
        return;
    }
    ui->bplptrLineLimitStart = nextStart;
    custom_ui_applyBplptrLineLimitStartOption();
}

static void
custom_ui_commitBplptrLineLimitEndTextbox(custom_ui_state_t *ui)
{
    if (!ui || !ui->bplptrLineLimitEndTextbox) {
        return;
    }

    const char *value = e9ui_textbox_getText(ui->bplptrLineLimitEndTextbox);
    int nextEnd = 0;
    if (!value || sscanf(value, "%d", &nextEnd) != 1) {
        return;
    }
    nextEnd = custom_ui_clampCopperLine(nextEnd);
    if (nextEnd == ui->bplptrLineLimitEnd) {
        return;
    }
    ui->bplptrLineLimitEnd = nextEnd;
    custom_ui_applyBplptrLineLimitEndOption();
}

static int
custom_ui_bplptrLineLimitStartTextboxKey(e9ui_context_t *ctx, SDL_Keycode key, SDL_Keymod mods, void *user)
{
    (void)ctx;
    (void)mods;
    custom_ui_state_t *ui = (custom_ui_state_t*)user;
    if (!ui || !ui->bplptrLineLimitStartTextbox) {
        return 0;
    }
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        custom_ui_commitBplptrLineLimitStartTextbox(ui);
    }
    return 0;
}

static int
custom_ui_bplptrLineLimitEndTextboxKey(e9ui_context_t *ctx, SDL_Keycode key, SDL_Keymod mods, void *user)
{
    (void)ctx;
    (void)mods;
    custom_ui_state_t *ui = (custom_ui_state_t*)user;
    if (!ui || !ui->bplptrLineLimitEndTextbox) {
        return 0;
    }
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        custom_ui_commitBplptrLineLimitEndTextbox(ui);
    }
    return 0;
}

static void
custom_ui_tickBplptrLineLimitTextboxes(custom_ui_state_t *ui)
{
    if (!ui) {
        return;
    }
    int startHasFocus = e9ui_getFocus(&ui->ctx) == ui->bplptrLineLimitStartTextbox ? 1 : 0;
    if (ui->bplptrLineLimitStartTextboxHadFocus && !startHasFocus) {
        custom_ui_commitBplptrLineLimitStartTextbox(ui);
    }
    ui->bplptrLineLimitStartTextboxHadFocus = startHasFocus;

    int endHasFocus = e9ui_getFocus(&ui->ctx) == ui->bplptrLineLimitEndTextbox ? 1 : 0;
    if (ui->bplptrLineLimitEndTextboxHadFocus && !endHasFocus) {
        custom_ui_commitBplptrLineLimitEndTextbox(ui);
    }
    ui->bplptrLineLimitEndTextboxHadFocus = endHasFocus;
}

static void
custom_ui_bplcon1DelayScrollChanged(e9ui_component_t *self, e9ui_context_t *ctx, int selected, void *user)
{
    (void)self;
    (void)ctx;
    custom_ui_state_t *ui = (custom_ui_state_t*)user;
    if (!ui) {
        return;
    }
    ui->bplcon1DelayScrollEnabled = selected ? 1 : 0;
    custom_ui_applyBplcon1DelayScrollOption();
}

static void
custom_ui_spritesChanged(e9ui_component_t *self, e9ui_context_t *ctx, int selected, void *user)
{
    (void)self;
    (void)ctx;
    custom_ui_state_t *ui = (custom_ui_state_t*)user;
    if (!ui) {
        return;
    }
    if (ui->suppressSpriteCallbacks) {
        return;
    }
    int nextValue = selected ? 1 : 0;
    ui->spritesEnabled = nextValue;
    for (int spriteIndex = 0; spriteIndex < CUSTOM_UI_AMIGA_SPRITE_COUNT; ++spriteIndex) {
        ui->spriteEnabled[spriteIndex] = nextValue;
        custom_ui_applySpriteOption(spriteIndex);
        if (ui->spriteCheckboxes[spriteIndex]) {
            ui->suppressSpriteCallbacks = 1;
            e9ui_checkbox_setSelected(ui->spriteCheckboxes[spriteIndex], nextValue, &ui->ctx);
            ui->suppressSpriteCallbacks = 0;
        }
    }
}

static void
custom_ui_spriteChanged(e9ui_component_t *self, e9ui_context_t *ctx, int selected, void *user)
{
    (void)self;
    (void)ctx;
    if (!user) {
        return;
    }
    struct custom_ui_sprite_cb *cb = (struct custom_ui_sprite_cb *)user;
    if (!cb->ui) {
        return;
    }
    if (cb->spriteIndex < 0 || cb->spriteIndex >= CUSTOM_UI_AMIGA_SPRITE_COUNT) {
        return;
    }
    if (cb->ui->suppressSpriteCallbacks) {
        return;
    }
    cb->ui->spriteEnabled[cb->spriteIndex] = selected ? 1 : 0;
    custom_ui_applySpriteOption(cb->spriteIndex);
    custom_ui_syncSpritesMasterCheckbox(cb->ui);
}

static void
custom_ui_bitplanesChanged(e9ui_component_t *self, e9ui_context_t *ctx, int selected, void *user)
{
    (void)self;
    (void)ctx;
    custom_ui_state_t *ui = (custom_ui_state_t*)user;
    if (!ui) {
        return;
    }
    if (ui->suppressBitplaneCallbacks) {
        return;
    }
    int nextValue = selected ? 1 : 0;
    ui->bitplanesEnabled = nextValue;
    for (int bitplaneIndex = 0; bitplaneIndex < CUSTOM_UI_AMIGA_BITPLANE_COUNT; ++bitplaneIndex) {
        ui->bitplaneEnabled[bitplaneIndex] = nextValue;
        custom_ui_applyBitplaneOption(bitplaneIndex);
        if (ui->bitplaneCheckboxes[bitplaneIndex]) {
            ui->suppressBitplaneCallbacks = 1;
            e9ui_checkbox_setSelected(ui->bitplaneCheckboxes[bitplaneIndex], nextValue, &ui->ctx);
            ui->suppressBitplaneCallbacks = 0;
        }
    }
}

static void
custom_ui_bitplaneChanged(e9ui_component_t *self, e9ui_context_t *ctx, int selected, void *user)
{
    (void)self;
    (void)ctx;
    if (!user) {
        return;
    }
    struct custom_ui_bitplane_cb *cb = (struct custom_ui_bitplane_cb *)user;
    if (!cb->ui) {
        return;
    }
    if (cb->bitplaneIndex < 0 || cb->bitplaneIndex >= CUSTOM_UI_AMIGA_BITPLANE_COUNT) {
        return;
    }
    if (cb->ui->suppressBitplaneCallbacks) {
        return;
    }
    cb->ui->bitplaneEnabled[cb->bitplaneIndex] = selected ? 1 : 0;
    custom_ui_applyBitplaneOption(cb->bitplaneIndex);
    custom_ui_syncBitplanesMasterCheckbox(cb->ui);
}

static void
custom_ui_audiosChanged(e9ui_component_t *self, e9ui_context_t *ctx, int selected, void *user)
{
    (void)self;
    (void)ctx;
    custom_ui_state_t *ui = (custom_ui_state_t*)user;
    if (!ui) {
        return;
    }
    if (ui->suppressAudioCallbacks) {
        return;
    }
    int nextValue = selected ? 1 : 0;
    ui->audiosEnabled = nextValue;
    for (int audioChannelIndex = 0; audioChannelIndex < CUSTOM_UI_AMIGA_AUDIO_CHANNEL_COUNT; ++audioChannelIndex) {
        ui->audioEnabled[audioChannelIndex] = nextValue;
        custom_ui_applyAudioOption(audioChannelIndex);
        if (ui->audioCheckboxes[audioChannelIndex]) {
            ui->suppressAudioCallbacks = 1;
            e9ui_checkbox_setSelected(ui->audioCheckboxes[audioChannelIndex], nextValue, &ui->ctx);
            ui->suppressAudioCallbacks = 0;
        }
    }
}

static void
custom_ui_audioChanged(e9ui_component_t *self, e9ui_context_t *ctx, int selected, void *user)
{
    (void)self;
    (void)ctx;
    if (!user) {
        return;
    }
    struct custom_ui_audio_cb *cb = (struct custom_ui_audio_cb *)user;
    if (!cb->ui) {
        return;
    }
    if (cb->audioChannelIndex < 0 || cb->audioChannelIndex >= CUSTOM_UI_AMIGA_AUDIO_CHANNEL_COUNT) {
        return;
    }
    if (cb->ui->suppressAudioCallbacks) {
        return;
    }
    cb->ui->audioEnabled[cb->audioChannelIndex] = selected ? 1 : 0;
    custom_ui_applyAudioOption(cb->audioChannelIndex);
    custom_ui_syncAudiosMasterCheckbox(cb->ui);
}

static e9ui_component_t *
custom_ui_buildRoot(custom_ui_state_t *ui)
{
    e9ui_component_t *rootStack = e9ui_stack_makeVertical();
    if (!rootStack) {
        return NULL;
    }
    e9ui_stack_addFixed(rootStack, e9ui_vspacer_make(12));

    e9ui_component_t *columns = e9ui_hstack_make();
    if (!columns) {
        e9ui_childDestroy(rootStack, &ui->ctx);
        return NULL;
    }

    e9ui_component_t *leftColumn = e9ui_stack_makeVertical();
    if (!leftColumn) {
        e9ui_childDestroy(rootStack, &ui->ctx);
        return NULL;
    }

    e9ui_component_t *cbBplcon1DelayScroll = e9ui_checkbox_make("BPLCON1 Scroll",
                                                                 ui->bplcon1DelayScrollEnabled,
                                                                 custom_ui_bplcon1DelayScrollChanged,
                                                                 ui);
    if (!cbBplcon1DelayScroll) {
        e9ui_childDestroy(rootStack, &ui->ctx);
        return NULL;
    }
    e9ui_checkbox_setLeftMargin(cbBplcon1DelayScroll, 12);
    e9ui_stack_addFixed(leftColumn, cbBplcon1DelayScroll);
    e9ui_stack_addFixed(leftColumn, e9ui_vspacer_make(8));

    e9ui_component_t *cbSprites = e9ui_checkbox_make("Sprites",
                                                     ui->spritesEnabled,
                                                     custom_ui_spritesChanged,
                                                     ui);
    if (!cbSprites) {
        e9ui_childDestroy(rootStack, &ui->ctx);
        return NULL;
    }
    ui->spritesCheckbox = cbSprites;
    e9ui_checkbox_setLeftMargin(cbSprites, 12);
    e9ui_stack_addFixed(leftColumn, cbSprites);
    e9ui_stack_addFixed(leftColumn, e9ui_vspacer_make(6));

    for (int spriteIndex = 0; spriteIndex < CUSTOM_UI_AMIGA_SPRITE_COUNT; ++spriteIndex) {
        char label[32];
        snprintf(label, sizeof(label), "Sprite %d", spriteIndex);
        ui->spriteCb[spriteIndex].ui = ui;
        ui->spriteCb[spriteIndex].spriteIndex = spriteIndex;
        e9ui_component_t *cbSprite = e9ui_checkbox_make(label,
                                                        ui->spriteEnabled[spriteIndex],
                                                        custom_ui_spriteChanged,
                                                        &ui->spriteCb[spriteIndex]);
        if (!cbSprite) {
            e9ui_childDestroy(rootStack, &ui->ctx);
            return NULL;
        }
        ui->spriteCheckboxes[spriteIndex] = cbSprite;
        e9ui_checkbox_setLeftMargin(cbSprite, 28);
        e9ui_stack_addFixed(leftColumn, cbSprite);
    }
    e9ui_stack_addFixed(leftColumn, e9ui_vspacer_make(8));

    e9ui_component_t *cbBitplanes = e9ui_checkbox_make("Bitplanes",
                                                       ui->bitplanesEnabled,
                                                       custom_ui_bitplanesChanged,
                                                       ui);
    if (!cbBitplanes) {
        e9ui_childDestroy(rootStack, &ui->ctx);
        return NULL;
    }
    ui->bitplanesCheckbox = cbBitplanes;
    e9ui_checkbox_setLeftMargin(cbBitplanes, 12);
    e9ui_stack_addFixed(leftColumn, cbBitplanes);
    e9ui_stack_addFixed(leftColumn, e9ui_vspacer_make(6));

    for (int bitplaneIndex = 0; bitplaneIndex < CUSTOM_UI_AMIGA_BITPLANE_COUNT; ++bitplaneIndex) {
        char label[32];
        snprintf(label, sizeof(label), "Bitplane %d", bitplaneIndex);
        ui->bitplaneCb[bitplaneIndex].ui = ui;
        ui->bitplaneCb[bitplaneIndex].bitplaneIndex = bitplaneIndex;
        e9ui_component_t *cbBitplane = e9ui_checkbox_make(label,
                                                          ui->bitplaneEnabled[bitplaneIndex],
                                                          custom_ui_bitplaneChanged,
                                                          &ui->bitplaneCb[bitplaneIndex]);
        if (!cbBitplane) {
            e9ui_childDestroy(rootStack, &ui->ctx);
            return NULL;
        }
        ui->bitplaneCheckboxes[bitplaneIndex] = cbBitplane;
        e9ui_checkbox_setLeftMargin(cbBitplane, 28);
        e9ui_stack_addFixed(leftColumn, cbBitplane);
    }
    e9ui_stack_addFixed(leftColumn, e9ui_vspacer_make(8));

    e9ui_component_t *cbBplptrBlockAll = e9ui_checkbox_make("Bitplane Ptr Block",
                                                            ui->bplptrBlockAllEnabled,
                                                            custom_ui_bplptrBlockAllChanged,
                                                            ui);
    if (!cbBplptrBlockAll) {
        e9ui_childDestroy(rootStack, &ui->ctx);
        return NULL;
    }
    ui->bplptrBlockAllCheckbox = cbBplptrBlockAll;
    e9ui_checkbox_setLeftMargin(cbBplptrBlockAll, 12);
    e9ui_stack_addFixed(leftColumn, cbBplptrBlockAll);
    e9ui_stack_addFixed(leftColumn, e9ui_vspacer_make(6));

    char bplptrLineLimitStartText[16];
    snprintf(bplptrLineLimitStartText, sizeof(bplptrLineLimitStartText), "%d", custom_ui_clampCopperLine(ui->bplptrLineLimitStart));
    e9ui_component_t *bplptrLineLimitStartRow = e9ui_labeled_textbox_make("Start",
                                                                           78,
                                                                           0,
                                                                           custom_ui_bplptrLineLimitStartChanged,
                                                                           ui);
    if (!bplptrLineLimitStartRow) {
        e9ui_childDestroy(rootStack, &ui->ctx);
        return NULL;
    }
    e9ui_labeled_textbox_setText(bplptrLineLimitStartRow, bplptrLineLimitStartText);
    e9ui_component_t *bplptrLineLimitStartTextbox = e9ui_labeled_textbox_getTextbox(bplptrLineLimitStartRow);
    if (bplptrLineLimitStartTextbox) {
        e9ui_textbox_setNumericOnly(bplptrLineLimitStartTextbox, 1);
        e9ui_textbox_setKeyHandler(bplptrLineLimitStartTextbox, custom_ui_bplptrLineLimitStartTextboxKey, ui);
    }
    ui->bplptrLineLimitStartRow = bplptrLineLimitStartRow;
    ui->bplptrLineLimitStartTextbox = bplptrLineLimitStartTextbox;
    e9ui_stack_addFixed(leftColumn, bplptrLineLimitStartRow);
    e9ui_stack_addFixed(leftColumn, e9ui_vspacer_make(6));

    char bplptrLineLimitEndText[16];
    snprintf(bplptrLineLimitEndText, sizeof(bplptrLineLimitEndText), "%d", custom_ui_clampCopperLine(ui->bplptrLineLimitEnd));
    e9ui_component_t *bplptrLineLimitEndRow = e9ui_labeled_textbox_make("End",
                                                                         78,
                                                                         0,
                                                                         custom_ui_bplptrLineLimitEndChanged,
                                                                         ui);
    if (!bplptrLineLimitEndRow) {
        e9ui_childDestroy(rootStack, &ui->ctx);
        return NULL;
    }
    e9ui_labeled_textbox_setText(bplptrLineLimitEndRow, bplptrLineLimitEndText);
    e9ui_component_t *bplptrLineLimitEndTextbox = e9ui_labeled_textbox_getTextbox(bplptrLineLimitEndRow);
    if (bplptrLineLimitEndTextbox) {
        e9ui_textbox_setNumericOnly(bplptrLineLimitEndTextbox, 1);
        e9ui_textbox_setKeyHandler(bplptrLineLimitEndTextbox, custom_ui_bplptrLineLimitEndTextboxKey, ui);
    }
    ui->bplptrLineLimitEndRow = bplptrLineLimitEndRow;
    ui->bplptrLineLimitEndTextbox = bplptrLineLimitEndTextbox;
    e9ui_stack_addFixed(leftColumn, bplptrLineLimitEndRow);
    e9ui_stack_addFixed(leftColumn, e9ui_vspacer_make(6));

    for (int bplptrIndex = 0; bplptrIndex < CUSTOM_UI_AMIGA_BPLPTR_COUNT; ++bplptrIndex) {
        char label[32];
        snprintf(label, sizeof(label), "BPL%dPT", bplptrIndex + 1);
        ui->bplptrCb[bplptrIndex].ui = ui;
        ui->bplptrCb[bplptrIndex].bplptrIndex = bplptrIndex;
        e9ui_component_t *cbBplptrBlock = e9ui_checkbox_make(label,
                                                             ui->bplptrBlockEnabled[bplptrIndex],
                                                             custom_ui_bplptrBlockChanged,
                                                             &ui->bplptrCb[bplptrIndex]);
        if (!cbBplptrBlock) {
            e9ui_childDestroy(rootStack, &ui->ctx);
            return NULL;
        }
        ui->bplptrBlockCheckboxes[bplptrIndex] = cbBplptrBlock;
        e9ui_checkbox_setLeftMargin(cbBplptrBlock, 28);
        e9ui_stack_addFixed(leftColumn, cbBplptrBlock);
    }

    e9ui_component_t *rightColumn = e9ui_stack_makeVertical();
    if (!rightColumn) {
        e9ui_childDestroy(rootStack, &ui->ctx);
        return NULL;
    }

    e9ui_component_t *cbBlitter = e9ui_checkbox_make("Blitter",
                                                     ui->blitterEnabled,
                                                     custom_ui_blitterChanged,
                                                     ui);
    if (!cbBlitter) {
        e9ui_childDestroy(rootStack, &ui->ctx);
        return NULL;
    }
    e9ui_checkbox_setLeftMargin(cbBlitter, 12);
    e9ui_stack_addFixed(rightColumn, cbBlitter);
    e9ui_stack_addFixed(rightColumn, e9ui_vspacer_make(8));

    e9ui_component_t *cbBlitterDebug = e9ui_checkbox_make("Blitter Visualiser",
                                                           ui->blitterDebugEnabled,
                                                           custom_ui_blitterDebugChanged,
                                                           ui);
    if (!cbBlitterDebug) {
        e9ui_childDestroy(rootStack, &ui->ctx);
        return NULL;
    }
    ui->blitterDebugCheckbox = cbBlitterDebug;
    e9ui_checkbox_setLeftMargin(cbBlitterDebug, 12);
    e9ui_stack_addFixed(rightColumn, cbBlitterDebug);
    e9ui_stack_addFixed(rightColumn, e9ui_vspacer_make(8));

    e9ui_component_t *cbBlitterVisCollect = e9ui_checkbox_make("Overlay",
                                                               (ui->blitterVisMode & CUSTOM_UI_BLITTER_VIS_MODE_COLLECT) != 0,
                                                               custom_ui_blitterVisCollectChanged,
                                                               ui);
    if (!cbBlitterVisCollect) {
        e9ui_childDestroy(rootStack, &ui->ctx);
        return NULL;
    }
    ui->blitterVisCollectCheckbox = cbBlitterVisCollect;
    e9ui_checkbox_setLeftMargin(cbBlitterVisCollect, 28);
    /* Hidden (kept for state/default compatibility):
     * e9ui_stack_addFixed(rightColumn, cbBlitterVisCollect);
     * e9ui_stack_addFixed(rightColumn, e9ui_vspacer_make(8));
     */

    char decayText[16];
    snprintf(decayText, sizeof(decayText), "%d", ui->blitterVisDecay);
    e9ui_component_t *blitterVisDecayTextboxRow = e9ui_labeled_textbox_make("Decay",
                                                                             78,
                                                                             0,
                                                                             custom_ui_blitterVisDecayChanged,
                                                                             ui);
    if (!blitterVisDecayTextboxRow) {
        e9ui_childDestroy(rootStack, &ui->ctx);
        return NULL;
    }
    e9ui_labeled_textbox_setText(blitterVisDecayTextboxRow, decayText);
    e9ui_component_t *blitterVisDecayTextbox = e9ui_labeled_textbox_getTextbox(blitterVisDecayTextboxRow);
    if (blitterVisDecayTextbox) {
        e9ui_textbox_setKeyHandler(blitterVisDecayTextbox, custom_ui_blitterVisDecayTextboxKey, ui);
    }
    ui->blitterVisDecayRow = blitterVisDecayTextboxRow;
    ui->blitterVisDecayTextbox = blitterVisDecayTextbox;
    e9ui_stack_addFixed(rightColumn, blitterVisDecayTextboxRow);
    e9ui_stack_addFixed(rightColumn, e9ui_vspacer_make(4));

    e9ui_component_t *blitterVisDecaySeekBar = NULL;
    e9ui_component_t *blitterVisDecaySeekRow = custom_ui_blitterVisDecaySeekRowMake(&blitterVisDecaySeekBar);
    if (!blitterVisDecaySeekRow) {
        e9ui_childDestroy(rootStack, &ui->ctx);
        return NULL;
    }
    if (blitterVisDecaySeekBar) {
        e9ui_seek_bar_setCallback(blitterVisDecaySeekBar, custom_ui_blitterVisDecaySeekChanged, ui);
        e9ui_seek_bar_setTooltipCallback(blitterVisDecaySeekBar, custom_ui_blitterVisDecaySeekTooltip, ui);
    }
    ui->blitterVisDecaySeekRow = blitterVisDecaySeekRow;
    ui->blitterVisDecaySeekBar = blitterVisDecaySeekBar;
    custom_ui_syncBlitterVisDecaySeekBar(ui);
    e9ui_stack_addFixed(rightColumn, blitterVisDecaySeekRow);
    e9ui_stack_addFixed(rightColumn, e9ui_vspacer_make(8));

    e9ui_component_t *cbBlitterVisBlink = e9ui_checkbox_make("Core Blink",
                                                             ui->blitterVisBlink,
                                                             custom_ui_blitterVisBlinkChanged,
                                                             ui);
    if (!cbBlitterVisBlink) {
        e9ui_childDestroy(rootStack, &ui->ctx);
        return NULL;
    }
    ui->blitterVisBlinkCheckbox = cbBlitterVisBlink;
    e9ui_checkbox_setLeftMargin(cbBlitterVisBlink, 28);
    /* Hidden (kept for state/default compatibility):
     * e9ui_stack_addFixed(rightColumn, cbBlitterVisBlink);
     * e9ui_stack_addFixed(rightColumn, e9ui_vspacer_make(8));
     */

    e9ui_component_t *cbBlitterVisPattern = e9ui_checkbox_make("Core Pattern",
                                                               (ui->blitterVisMode & CUSTOM_UI_BLITTER_VIS_MODE_PATTERN) != 0,
                                                               custom_ui_blitterVisPatternChanged,
                                                               ui);
    if (!cbBlitterVisPattern) {
        e9ui_childDestroy(rootStack, &ui->ctx);
        return NULL;
    }
    ui->blitterVisPatternCheckbox = cbBlitterVisPattern;
    e9ui_checkbox_setLeftMargin(cbBlitterVisPattern, 28);
    /* Hidden (kept for state/default compatibility):
     * e9ui_stack_addFixed(rightColumn, cbBlitterVisPattern);
     * e9ui_stack_addFixed(rightColumn, e9ui_vspacer_make(8));
     */

    e9ui_component_t *cbBlitterVisMode = e9ui_checkbox_make("Core Solid",
                                                            (ui->blitterVisMode & CUSTOM_UI_BLITTER_VIS_MODE_SOLID) != 0,
                                                            custom_ui_blitterVisModeChanged,
                                                            ui);
    if (!cbBlitterVisMode) {
        e9ui_childDestroy(rootStack, &ui->ctx);
        return NULL;
    }
    ui->blitterVisModeCheckbox = cbBlitterVisMode;
    e9ui_checkbox_setLeftMargin(cbBlitterVisMode, 28);
    /* Hidden (kept for state/default compatibility):
     * e9ui_stack_addFixed(rightColumn, cbBlitterVisMode);
     * e9ui_stack_addFixed(rightColumn, e9ui_vspacer_make(8));
     */

    custom_ui_syncBlitterDebugSuboptions(ui);

    e9ui_component_t *cbCopperLimit = e9ui_checkbox_make("Copper Block",
                                                         ui->copperLimitEnabled,
                                                         custom_ui_copperLimitChanged,
                                                         ui);
    if (!cbCopperLimit) {
        e9ui_childDestroy(rootStack, &ui->ctx);
        return NULL;
    }
    ui->copperLimitCheckbox = cbCopperLimit;
    e9ui_checkbox_setLeftMargin(cbCopperLimit, 12);
    e9ui_stack_addFixed(rightColumn, cbCopperLimit);
    e9ui_stack_addFixed(rightColumn, e9ui_vspacer_make(6));

    char copperLimitStartText[16];
    snprintf(copperLimitStartText, sizeof(copperLimitStartText), "%d", custom_ui_clampCopperLine(ui->copperLimitStart));
    e9ui_component_t *copperLimitStartRow = e9ui_labeled_textbox_make("Start",
                                                                       78,
                                                                       0,
                                                                       custom_ui_copperLimitStartChanged,
                                                                       ui);
    if (!copperLimitStartRow) {
        e9ui_childDestroy(rootStack, &ui->ctx);
        return NULL;
    }
    e9ui_labeled_textbox_setText(copperLimitStartRow, copperLimitStartText);
    e9ui_component_t *copperLimitStartTextbox = e9ui_labeled_textbox_getTextbox(copperLimitStartRow);
    if (copperLimitStartTextbox) {
        e9ui_textbox_setNumericOnly(copperLimitStartTextbox, 1);
        e9ui_textbox_setKeyHandler(copperLimitStartTextbox, custom_ui_copperLimitStartTextboxKey, ui);
    }
    ui->copperLimitStartRow = copperLimitStartRow;
    ui->copperLimitStartTextbox = copperLimitStartTextbox;
    e9ui_stack_addFixed(rightColumn, copperLimitStartRow);
    e9ui_stack_addFixed(rightColumn, e9ui_vspacer_make(6));

    char copperLimitEndText[16];
    snprintf(copperLimitEndText, sizeof(copperLimitEndText), "%d", custom_ui_clampCopperLine(ui->copperLimitEnd));
    e9ui_component_t *copperLimitEndRow = e9ui_labeled_textbox_make("End",
                                                                     78,
                                                                     0,
                                                                     custom_ui_copperLimitEndChanged,
                                                                     ui);
    if (!copperLimitEndRow) {
        e9ui_childDestroy(rootStack, &ui->ctx);
        return NULL;
    }
    e9ui_labeled_textbox_setText(copperLimitEndRow, copperLimitEndText);
    e9ui_component_t *copperLimitEndTextbox = e9ui_labeled_textbox_getTextbox(copperLimitEndRow);
    if (copperLimitEndTextbox) {
        e9ui_textbox_setNumericOnly(copperLimitEndTextbox, 1);
        e9ui_textbox_setKeyHandler(copperLimitEndTextbox, custom_ui_copperLimitEndTextboxKey, ui);
    }
    ui->copperLimitEndRow = copperLimitEndRow;
    ui->copperLimitEndTextbox = copperLimitEndTextbox;
    e9ui_stack_addFixed(rightColumn, copperLimitEndRow);
    e9ui_stack_addFixed(rightColumn, e9ui_vspacer_make(8));

    custom_ui_syncCopperLimitSuboptions(ui);

    e9ui_component_t *cbAudios = e9ui_checkbox_make("Audio",
                                                    ui->audiosEnabled,
                                                    custom_ui_audiosChanged,
                                                    ui);
    if (!cbAudios) {
        e9ui_childDestroy(rootStack, &ui->ctx);
        return NULL;
    }
    ui->audiosCheckbox = cbAudios;
    e9ui_checkbox_setLeftMargin(cbAudios, 12);
    e9ui_stack_addFixed(rightColumn, cbAudios);
    e9ui_stack_addFixed(rightColumn, e9ui_vspacer_make(6));

    for (int audioChannelIndex = 0; audioChannelIndex < CUSTOM_UI_AMIGA_AUDIO_CHANNEL_COUNT; ++audioChannelIndex) {
        char label[32];
        snprintf(label, sizeof(label), "Audio %d", audioChannelIndex);
        ui->audioCb[audioChannelIndex].ui = ui;
        ui->audioCb[audioChannelIndex].audioChannelIndex = audioChannelIndex;
        e9ui_component_t *cbAudio = e9ui_checkbox_make(label,
                                                       ui->audioEnabled[audioChannelIndex],
                                                       custom_ui_audioChanged,
                                                       &ui->audioCb[audioChannelIndex]);
        if (!cbAudio) {
            e9ui_childDestroy(rootStack, &ui->ctx);
            return NULL;
        }
        ui->audioCheckboxes[audioChannelIndex] = cbAudio;
        e9ui_checkbox_setLeftMargin(cbAudio, 28);
        e9ui_stack_addFixed(rightColumn, cbAudio);
    }

    e9ui_stack_addFixed(rightColumn, e9ui_vspacer_make(8));

    e9ui_hstack_addFlex(columns, leftColumn);
    e9ui_hstack_addFixed(columns, e9ui_spacer_make(16), 16);
    e9ui_hstack_addFlex(columns, rightColumn);
    e9ui_stack_addFlex(rootStack, columns);

    e9ui_component_t *scrollRoot = e9ui_scroll_make(rootStack);
    if (!scrollRoot) {
        e9ui_childDestroy(rootStack, &ui->ctx);
        return NULL;
    }
    return scrollRoot;
}

static void
custom_ui_prepareFrame(custom_ui_state_t *ui, const e9ui_context_t *frameCtx)
{
    if (!ui) {
        return;
    }
    if (frameCtx) {
        ui->ctx = *frameCtx;
    }
    if (ui->pendingRemove && ui->root) {
        e9ui_childRemove(ui->root, ui->pendingRemove, &ui->ctx);
        ui->pendingRemove = NULL;
    }
    custom_ui_syncBlitterDebugCheckbox(ui);
    custom_ui_tickBlitterVisDecayTextbox(ui);
    custom_ui_tickCopperLimitTextboxes(ui);
    custom_ui_tickBplptrLineLimitTextboxes(ui);
}

static int
custom_ui_overlayBodyPreferredHeight(e9ui_component_t *self, e9ui_context_t *ctx, int availW)
{
    (void)self;
    (void)ctx;
    (void)availW;
    return 0;
}

static void
custom_ui_overlayBodyLayout(e9ui_component_t *self, e9ui_context_t *ctx, e9ui_rect_t bounds)
{
    if (!self) {
        return;
    }
    self->bounds = bounds;
    custom_ui_overlay_body_state_t *st = (custom_ui_overlay_body_state_t *)self->state;
    if (!st || !st->ui || !st->ui->root || !st->ui->root->layout) {
        return;
    }
    st->ui->root->layout(st->ui->root, ctx, bounds);
}

static void
custom_ui_overlayBodyRender(e9ui_component_t *self, e9ui_context_t *ctx)
{
    if (!self || !ctx) {
        return;
    }
    custom_ui_overlay_body_state_t *st = (custom_ui_overlay_body_state_t *)self->state;
    if (!st || !st->ui || !st->ui->root) {
        return;
    }
    custom_ui_prepareFrame(st->ui, ctx);
    if (st->ui->root->render) {
        st->ui->root->render(st->ui->root, ctx);
    }
}

static e9ui_component_t *
custom_ui_makeOverlayBodyHost(custom_ui_state_t *ui)
{
    if (!ui || !ui->root) {
        return NULL;
    }
    e9ui_component_t *host = (e9ui_component_t *)alloc_calloc(1, sizeof(*host));
    if (!host) {
        return NULL;
    }
    custom_ui_overlay_body_state_t *st = (custom_ui_overlay_body_state_t *)alloc_calloc(1, sizeof(*st));
    if (!st) {
        alloc_free(host);
        return NULL;
    }
    st->ui = ui;
    host->name = "custom_ui_overlay_body";
    host->state = st;
    host->preferredHeight = custom_ui_overlayBodyPreferredHeight;
    host->layout = custom_ui_overlayBodyLayout;
    host->render = custom_ui_overlayBodyRender;
    e9ui_child_add(host, ui->root, alloc_strdup("custom_ui_root"));
    return host;
}

static void
custom_ui_overlayWindowCloseRequested(e9ui_window_t *window, void *user)
{
    (void)window;
    custom_ui_state_t *ui = (custom_ui_state_t *)user;
    if (!ui) {
        return;
    }
    ui->closeRequested = 1;
}

int
custom_ui_init(void)
{
    custom_ui_state_t *ui = &custom_ui_state;
    if (ui->open) {
        return 1;
    }

    ui->windowHost = e9ui_windowCreate(custom_ui_windowBackend());
    if (!ui->windowHost) {
        return 0;
    }
    ui->closeRequested = 0;
    ui->warnedMissingOption = 0;
    ui->suppressBlitterDebugCallbacks = 0;
    ui->suppressBlitterVisModeCallbacks = 0;
    ui->suppressSpriteCallbacks = 0;
    ui->suppressBitplaneCallbacks = 0;
    ui->suppressAudioCallbacks = 0;
    ui->suppressBplptrBlockCallbacks = 0;
    ui->spritesCheckbox = NULL;
    ui->bitplanesCheckbox = NULL;
    ui->bplptrBlockAllCheckbox = NULL;
    ui->audiosCheckbox = NULL;
    ui->copperLimitCheckbox = NULL;
    ui->copperLimitStartRow = NULL;
    ui->copperLimitStartTextbox = NULL;
    ui->copperLimitStartTextboxHadFocus = 0;
    ui->copperLimitEndRow = NULL;
    ui->copperLimitEndTextbox = NULL;
    ui->copperLimitEndTextboxHadFocus = 0;
    ui->bplptrLineLimitStartRow = NULL;
    ui->bplptrLineLimitStartTextbox = NULL;
    ui->bplptrLineLimitStartTextboxHadFocus = 0;
    ui->bplptrLineLimitEndRow = NULL;
    ui->bplptrLineLimitEndTextbox = NULL;
    ui->bplptrLineLimitEndTextboxHadFocus = 0;
    ui->blitterDebugCheckbox = NULL;
    ui->blitterVisPatternCheckbox = NULL;
    ui->blitterVisModeCheckbox = NULL;
    ui->blitterVisCollectCheckbox = NULL;
    ui->blitterVisBlinkCheckbox = NULL;
    ui->blitterVisDecayRow = NULL;
    ui->blitterVisDecayTextbox = NULL;
    ui->blitterVisDecaySeekRow = NULL;
    ui->blitterVisDecaySeekBar = NULL;
    ui->blitterVisDecayTextboxHadFocus = 0;
    for (int spriteIndex = 0; spriteIndex < CUSTOM_UI_AMIGA_SPRITE_COUNT; ++spriteIndex) {
        ui->spriteCheckboxes[spriteIndex] = NULL;
    }
    for (int bitplaneIndex = 0; bitplaneIndex < CUSTOM_UI_AMIGA_BITPLANE_COUNT; ++bitplaneIndex) {
        ui->bitplaneCheckboxes[bitplaneIndex] = NULL;
    }
    for (int bplptrIndex = 0; bplptrIndex < CUSTOM_UI_AMIGA_BPLPTR_COUNT; ++bplptrIndex) {
        ui->bplptrBlockCheckboxes[bplptrIndex] = NULL;
    }
    for (int audioChannelIndex = 0; audioChannelIndex < CUSTOM_UI_AMIGA_AUDIO_CHANNEL_COUNT; ++audioChannelIndex) {
        ui->audioCheckboxes[audioChannelIndex] = NULL;
    }
    custom_ui_syncSpritesMasterCheckbox(ui);
    custom_ui_syncBitplanesMasterCheckbox(ui);
    custom_ui_syncBplptrBlockMasterCheckbox(ui);
    custom_ui_syncAudiosMasterCheckbox(ui);
    custom_ui_syncBlitterDebugCheckbox(ui);

    ui->root = custom_ui_buildRoot(ui);
    if (!ui->root) {
        custom_ui_shutdown();
        return 0;
    }
    {
        e9ui_rect_t rect = e9ui_windowResolveOpenRect(&e9ui->ctx,
                                                               custom_ui_windowDefaultRect(&e9ui->ctx),
                                                               420,
                                                               420,
                                                               1,
                                                               ui->winHasSaved ? 1 : 0,
                                                               (ui->winHasSaved && ui->winW > 0 && ui->winH > 0) ? 1 : 0,
                                                               ui->winX,
                                                               ui->winY,
                                                               ui->winW,
                                                               ui->winH);
        e9ui_component_t *overlayBodyHost = custom_ui_makeOverlayBodyHost(ui);
        if (!overlayBodyHost) {
            custom_ui_shutdown();
            return 0;
        }
        if (!e9ui_windowOpen(ui->windowHost,
                                     CUSTOM_UI_TITLE,
                                     rect,
                                     overlayBodyHost,
                                     custom_ui_overlayWindowCloseRequested,
                                     ui,
                                     &e9ui->ctx)) {
            e9ui_childDestroy(overlayBodyHost, &e9ui->ctx);
            custom_ui_shutdown();
            return 0;
        }
        ui->ctx = e9ui->ctx;
    }

    custom_ui_applyAllOptions();
    ui->open = 1;
    return 1;
}

void
custom_ui_shutdown(void)
{
    custom_ui_state_t *ui = &custom_ui_state;
    if (!ui->open) {
        return;
    }
    if (ui->windowHost) {
        e9ui_windowDestroy(ui->windowHost);
        ui->windowHost = NULL;
    }
    ui->root = NULL;
    ui->fullscreen = NULL;
    ui->open = 0;
    ui->closeRequested = 0;
    ui->warnedMissingOption = 0;
    ui->pendingRemove = NULL;
    memset(&ui->ctx, 0, sizeof(ui->ctx));
}

void
custom_ui_toggle(void)
{
    if (custom_ui_isOpen()) {
        custom_ui_shutdown();
        return;
    }
    (void)custom_ui_init();
}

int
custom_ui_isOpen(void)
{
    return custom_ui_state.open ? 1 : 0;
}

uint32_t
custom_ui_getWindowId(void)
{
    return 0;
}

void
custom_ui_setMainWindowFocused(int focused)
{
    (void)focused;
}

int
custom_ui_getBlitterVisDecay(void)
{
    return custom_ui_state.blitterVisDecay;
}

int
custom_ui_getCopperLimitEnabled(void)
{
    return custom_ui_state.copperLimitEnabled ? 1 : 0;
}

int
custom_ui_getCopperLimitRange(int *outStart, int *outEnd)
{
    custom_ui_state_t *ui = &custom_ui_state;
    if (outStart) {
        *outStart = ui->copperLimitStart;
    }
    if (outEnd) {
        *outEnd = ui->copperLimitEnd;
    }
    return 1;
}

void
custom_ui_setCopperLimitRange(int start, int end)
{
    custom_ui_state_t *ui = &custom_ui_state;
    int nextStart = custom_ui_clampCopperLine(start);
    int nextEnd = custom_ui_clampCopperLine(end);
    if (nextEnd < nextStart) {
        int temp = nextStart;
        nextStart = nextEnd;
        nextEnd = temp;
    }
    int changedStart = (nextStart != ui->copperLimitStart) ? 1 : 0;
    int changedEnd = (nextEnd != ui->copperLimitEnd) ? 1 : 0;
    if (!changedStart && !changedEnd) {
        return;
    }
    ui->copperLimitStart = nextStart;
    ui->copperLimitEnd = nextEnd;
    custom_ui_applyCopperLimitStartOption();
    custom_ui_applyCopperLimitEndOption();
    if (ui->copperLimitStartRow) {
        char text[16];
        snprintf(text, sizeof(text), "%d", ui->copperLimitStart);
        e9ui_labeled_textbox_setText(ui->copperLimitStartRow, text);
    }
    if (ui->copperLimitEndRow) {
        char text[16];
        snprintf(text, sizeof(text), "%d", ui->copperLimitEnd);
        e9ui_labeled_textbox_setText(ui->copperLimitEndRow, text);
    }
}

int
custom_ui_getBplptrBlockEnabled(void)
{
    custom_ui_state_t *ui = &custom_ui_state;
    for (int bplptrIndex = 0; bplptrIndex < CUSTOM_UI_AMIGA_BPLPTR_COUNT; ++bplptrIndex) {
        if (ui->bplptrBlockEnabled[bplptrIndex]) {
            return 1;
        }
    }
    return ui->bplptrBlockAllEnabled ? 1 : 0;
}

int
custom_ui_getBplptrLineLimitRange(int *outStart, int *outEnd)
{
    custom_ui_state_t *ui = &custom_ui_state;
    if (outStart) {
        *outStart = ui->bplptrLineLimitStart;
    }
    if (outEnd) {
        *outEnd = ui->bplptrLineLimitEnd;
    }
    return 1;
}

void
custom_ui_setBplptrLineLimitRange(int start, int end)
{
    custom_ui_state_t *ui = &custom_ui_state;
    int nextStart = custom_ui_clampCopperLine(start);
    int nextEnd = custom_ui_clampCopperLine(end);
    if (nextEnd < nextStart) {
        int temp = nextStart;
        nextStart = nextEnd;
        nextEnd = temp;
    }
    int changedStart = (nextStart != ui->bplptrLineLimitStart) ? 1 : 0;
    int changedEnd = (nextEnd != ui->bplptrLineLimitEnd) ? 1 : 0;
    if (!changedStart && !changedEnd) {
        return;
    }
    ui->bplptrLineLimitStart = nextStart;
    ui->bplptrLineLimitEnd = nextEnd;
    custom_ui_applyBplptrLineLimitStartOption();
    custom_ui_applyBplptrLineLimitEndOption();
    if (ui->bplptrLineLimitStartRow) {
        char text[16];
        snprintf(text, sizeof(text), "%d", ui->bplptrLineLimitStart);
        e9ui_labeled_textbox_setText(ui->bplptrLineLimitStartRow, text);
    }
    if (ui->bplptrLineLimitEndRow) {
        char text[16];
        snprintf(text, sizeof(text), "%d", ui->bplptrLineLimitEnd);
        e9ui_labeled_textbox_setText(ui->bplptrLineLimitEndRow, text);
    }
}

void
custom_ui_handleEvent(SDL_Event *ev)
{
    (void)ev;
}

void
custom_ui_render(void)
{
    custom_ui_state_t *ui = &custom_ui_state;
    if (!ui->open || !ui->root) {
        return;
    }
    if (ui->closeRequested) {
        custom_ui_shutdown();
        return;
    }
    if (e9ui_windowCaptureRectChanged(ui->windowHost,
                                      (e9ui ? &e9ui->ctx : &ui->ctx),
                                      &ui->winHasSaved,
                                      &ui->winX,
                                      &ui->winY,
                                      &ui->winW,
                                      &ui->winH)) {
        config_saveConfig();
    }
}

void
custom_ui_persistConfig(FILE *file)
{
    custom_ui_state_t *ui = &custom_ui_state;
    if (!file) {
        return;
    }
    if (ui->open) {
        (void)e9ui_windowCaptureRectSnapshot(ui->windowHost,
                                                (e9ui ? &e9ui->ctx : &ui->ctx),
                                                &ui->winHasSaved,
                                                &ui->winX,
                                                &ui->winY,
                                                &ui->winW,
                                                &ui->winH);
    }
    if (!ui->winHasSaved) {
        return;
    }
    fprintf(file, "comp.custom_ui.win_x=%d\n", ui->winX);
    fprintf(file, "comp.custom_ui.win_y=%d\n", ui->winY);
    fprintf(file, "comp.custom_ui.win_w=%d\n", ui->winW);
    fprintf(file, "comp.custom_ui.win_h=%d\n", ui->winH);
}

int
custom_ui_loadConfigProperty(const char *prop, const char *value)
{
    custom_ui_state_t *ui = &custom_ui_state;
    if (!prop || !value) {
        return 0;
    }
    int intValue = 0;
    if (strcmp(prop, "win_x") == 0) {
        if (!custom_ui_parseInt(value, &intValue)) {
            return 0;
        }
        ui->winX = intValue;
    } else if (strcmp(prop, "win_y") == 0) {
        if (!custom_ui_parseInt(value, &intValue)) {
            return 0;
        }
        ui->winY = intValue;
    } else if (strcmp(prop, "win_w") == 0) {
        if (!custom_ui_parseInt(value, &intValue)) {
            return 0;
        }
        ui->winW = intValue;
    } else if (strcmp(prop, "win_h") == 0) {
        if (!custom_ui_parseInt(value, &intValue)) {
            return 0;
        }
        ui->winH = intValue;
    } else {
        return 0;
    }
    ui->winHasSaved = 1;
    return 1;
}
