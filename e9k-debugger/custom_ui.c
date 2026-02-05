/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#include <string.h>
#include <stdio.h>
#include <SDL.h>

#include "custom_ui.h"
#include "debug.h"
#include "debugger.h"
#include "e9ui.h"
#include "libretro_host.h"

#define CUSTOM_UI_TITLE "ENGINE9000 DEBUGGER - CUSTOM"
#define CUSTOM_UI_AMIGA_SPRITE_COUNT 8
#define CUSTOM_UI_AMIGA_BITPLANE_COUNT 8
#define CUSTOM_UI_AMIGA_AUDIO_CHANNEL_COUNT 4

typedef struct custom_ui_state {
    int open;
    int closeRequested;
    int blitterEnabled;
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
    SDL_Window *window;
    SDL_Renderer *renderer;
    uint32_t windowId;
    e9ui_context_t ctx;
    e9ui_component_t *root;
    e9ui_component_t *fullscreen;
    e9ui_component_t *pendingRemove;
    e9ui_component_t *spritesCheckbox;
    e9ui_component_t *spriteCheckboxes[CUSTOM_UI_AMIGA_SPRITE_COUNT];
    e9ui_component_t *bitplanesCheckbox;
    e9ui_component_t *bitplaneCheckboxes[CUSTOM_UI_AMIGA_BITPLANE_COUNT];
    e9ui_component_t *audiosCheckbox;
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
} custom_ui_state_t;

static custom_ui_state_t custom_ui_state = {
    .blitterEnabled = 1,
    .spritesEnabled = 1,
    .spriteEnabled = { 1, 1, 1, 1, 1, 1, 1, 1 },
    .bitplanesEnabled = 1,
    .bitplaneEnabled = { 1, 1, 1, 1, 1, 1, 1, 1 },
    .audiosEnabled = 1,
    .audioEnabled = { 1, 1, 1, 1 }
};

static float
custom_ui_computeDpiScale(const e9ui_context_t *ctx)
{
    if (!ctx || !ctx->window || !ctx->renderer) {
        return 1.0f;
    }
    int winW = 0;
    int winH = 0;
    int renW = 0;
    int renH = 0;
    SDL_GetWindowSize(ctx->window, &winW, &winH);
    SDL_GetRendererOutputSize(ctx->renderer, &renW, &renH);
    if (winW <= 0 || winH <= 0) {
        return 1.0f;
    }
    float scaleX = (float)renW / (float)winW;
    float scaleY = (float)renH / (float)winH;
    float scale = scaleX > scaleY ? scaleX : scaleY;
    return scale < 1.0f ? 1.0f : scale;
}

static void
custom_ui_refocusMain(void)
{
    SDL_Window *mainWin = e9ui->ctx.window;
    if (!mainWin) {
        return;
    }
    SDL_ShowWindow(mainWin);
    SDL_RaiseWindow(mainWin);
    SDL_SetWindowInputFocus(mainWin);
    e9ui_component_t *geo = e9ui_findById(e9ui->root, "geo_view");
    if (geo) {
        e9ui_setFocus(&e9ui->ctx, geo);
    }
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

static void
custom_ui_applyBlitterOption(void)
{
    custom_ui_state_t *ui = &custom_ui_state;
    custom_ui_applyOption(E9K_DEBUG_OPTION_AMIGA_BLITTER, ui->blitterEnabled ? 1u : 0u);
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

    e9ui_component_t *cbBlitter = e9ui_checkbox_make("Blitter",
                                                     ui->blitterEnabled,
                                                     custom_ui_blitterChanged,
                                                     ui);
    if (!cbBlitter) {
        e9ui_childDestroy(rootStack, &ui->ctx);
        return NULL;
    }
    e9ui_checkbox_setLeftMargin(cbBlitter, 12);
    e9ui_stack_addFixed(leftColumn, cbBlitter);
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

    e9ui_component_t *rightColumn = e9ui_stack_makeVertical();
    if (!rightColumn) {
        e9ui_childDestroy(rootStack, &ui->ctx);
        return NULL;
    }
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

    e9ui_hstack_addFlex(columns, leftColumn);
    e9ui_hstack_addFixed(columns, e9ui_spacer_make(16), 16);
    e9ui_hstack_addFlex(columns, rightColumn);
    e9ui_stack_addFlex(rootStack, columns);
    return rootStack;
}

int
custom_ui_init(void)
{
    custom_ui_state_t *ui = &custom_ui_state;
    if (ui->open) {
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow(CUSTOM_UI_TITLE,
                                       SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       560, 560,
                                       SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!win) {
        debug_error("custom ui: SDL_CreateWindow failed: %s", SDL_GetError());
        return 0;
    }
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren) {
        SDL_DestroyWindow(win);
        debug_error("custom ui: SDL_CreateRenderer failed: %s", SDL_GetError());
        return 0;
    }
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    ui->window = win;
    ui->renderer = ren;
    ui->windowId = SDL_GetWindowID(win);
    ui->ctx.window = win;
    ui->ctx.renderer = ren;
    ui->ctx.font = e9ui->ctx.font;
    ui->ctx.dpiScale = custom_ui_computeDpiScale(&ui->ctx);
    ui->closeRequested = 0;
    ui->warnedMissingOption = 0;
    ui->suppressSpriteCallbacks = 0;
    ui->suppressBitplaneCallbacks = 0;
    ui->suppressAudioCallbacks = 0;
    ui->spritesCheckbox = NULL;
    ui->bitplanesCheckbox = NULL;
    ui->audiosCheckbox = NULL;
    for (int spriteIndex = 0; spriteIndex < CUSTOM_UI_AMIGA_SPRITE_COUNT; ++spriteIndex) {
        ui->spriteCheckboxes[spriteIndex] = NULL;
    }
    for (int bitplaneIndex = 0; bitplaneIndex < CUSTOM_UI_AMIGA_BITPLANE_COUNT; ++bitplaneIndex) {
        ui->bitplaneCheckboxes[bitplaneIndex] = NULL;
    }
    for (int audioChannelIndex = 0; audioChannelIndex < CUSTOM_UI_AMIGA_AUDIO_CHANNEL_COUNT; ++audioChannelIndex) {
        ui->audioCheckboxes[audioChannelIndex] = NULL;
    }
    custom_ui_syncSpritesMasterCheckbox(ui);
    custom_ui_syncBitplanesMasterCheckbox(ui);
    custom_ui_syncAudiosMasterCheckbox(ui);

    ui->root = custom_ui_buildRoot(ui);
    if (!ui->root) {
        custom_ui_shutdown();
        return 0;
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
    if (ui->root) {
        e9ui_childDestroy(ui->root, &ui->ctx);
        ui->root = NULL;
    }
    e9ui_text_cache_clearRenderer(ui->renderer);
    if (ui->renderer) {
        SDL_DestroyRenderer(ui->renderer);
        ui->renderer = NULL;
    }
    if (ui->window) {
        SDL_DestroyWindow(ui->window);
        ui->window = NULL;
    }
    ui->open = 0;
    ui->closeRequested = 0;
    ui->windowId = 0;
    ui->warnedMissingOption = 0;
    ui->pendingRemove = NULL;
    memset(&ui->ctx, 0, sizeof(ui->ctx));
    custom_ui_refocusMain();
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
    return custom_ui_state.windowId;
}

void
custom_ui_handleEvent(SDL_Event *ev)
{
    if (!ev || !custom_ui_state.open) {
        return;
    }
    custom_ui_state_t *ui = &custom_ui_state;
    if (ui->closeRequested) {
        return;
    }

    e9ui_component_t *root = ui->fullscreen ? ui->fullscreen : ui->root;
    ui->ctx.focusClickHandled = 0;
    ui->ctx.cursorOverride = 0;

    if (ev->type == SDL_WINDOWEVENT && ev->window.event == SDL_WINDOWEVENT_CLOSE) {
        ui->closeRequested = 1;
        return;
    }

    if (ev->type == SDL_MOUSEMOTION) {
        int prevX = ui->ctx.mouseX;
        int prevY = ui->ctx.mouseY;
        ui->ctx.mousePrevX = prevX;
        ui->ctx.mousePrevY = prevY;
        int scaledX = e9ui_scale_coord(&ui->ctx, ev->motion.x);
        int scaledY = e9ui_scale_coord(&ui->ctx, ev->motion.y);
        ev->motion.x = scaledX;
        ev->motion.y = scaledY;
        ev->motion.xrel = scaledX - prevX;
        ev->motion.yrel = scaledY - prevY;
        ui->ctx.mouseX = scaledX;
        ui->ctx.mouseY = scaledY;
    } else if (ev->type == SDL_MOUSEBUTTONDOWN || ev->type == SDL_MOUSEBUTTONUP) {
        int scaledX = e9ui_scale_coord(&ui->ctx, ev->button.x);
        int scaledY = e9ui_scale_coord(&ui->ctx, ev->button.y);
        ev->button.x = scaledX;
        ev->button.y = scaledY;
        ui->ctx.mouseX = scaledX;
        ui->ctx.mouseY = scaledY;
    } else if (ev->type == SDL_MOUSEWHEEL) {
        int mouseX = 0;
        int mouseY = 0;
        SDL_GetMouseState(&mouseX, &mouseY);
        ui->ctx.mouseX = e9ui_scale_coord(&ui->ctx, mouseX);
        ui->ctx.mouseY = e9ui_scale_coord(&ui->ctx, mouseY);
    } else if (ev->type == SDL_WINDOWEVENT) {
        if (ev->window.event == SDL_WINDOWEVENT_RESIZED ||
            ev->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            ui->ctx.dpiScale = custom_ui_computeDpiScale(&ui->ctx);
        }
    } else if (ev->type == SDL_KEYDOWN) {
        if (ev->key.keysym.sym == SDLK_ESCAPE) {
            ui->closeRequested = 1;
            return;
        }
    }

    if (root) {
        e9ui_event_process(root, &ui->ctx, ev);
    }
    if (ev->type == SDL_MOUSEBUTTONDOWN &&
        ev->button.button == SDL_BUTTON_LEFT &&
        !ui->ctx.focusClickHandled) {
        e9ui_setFocus(&ui->ctx, NULL);
    }
}

void
custom_ui_render(void)
{
    custom_ui_state_t *ui = &custom_ui_state;
    if (!ui->open || !ui->renderer || !ui->root) {
        return;
    }
    if (ui->closeRequested) {
        custom_ui_shutdown();
        return;
    }
    if (ui->pendingRemove && ui->root) {
        e9ui_childRemove(ui->root, ui->pendingRemove, &ui->ctx);
        ui->pendingRemove = NULL;
    }

    ui->ctx.font = e9ui->ctx.font;
    ui->ctx.window = ui->window;
    ui->ctx.renderer = ui->renderer;

    SDL_SetRenderDrawColor(ui->renderer, 12, 12, 12, 255);
    SDL_RenderClear(ui->renderer);
    int winW = 0;
    int winH = 0;
    SDL_GetRendererOutputSize(ui->renderer, &winW, &winH);
    ui->ctx.winW = winW;
    ui->ctx.winH = winH;

    e9ui_component_t *root = ui->fullscreen ? ui->fullscreen : ui->root;
    if (root && root->layout) {
        e9ui_rect_t full = { 0, 0, winW, winH };
        root->layout(root, &ui->ctx, full);
    }
    if (root && root->render) {
        root->render(root, &ui->ctx);
    }
    SDL_RenderPresent(ui->renderer);
}
