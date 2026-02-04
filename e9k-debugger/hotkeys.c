/*
 * COPYRIGHT (C) 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#include <string.h>

#include "hotkeys.h"
#include "e9ui.h"
#include "e9ui_text_select.h"
#include "e9ui_textbox.h"
#include "help.h"
#include "core_options.h"
#include "settings.h"
#include "debugger.h"
#include "sprite_debug.h"
#include "ui.h"
#include "crt.h"
#include "state_buffer.h"
#include "prompt.h"
#include "input_record.h"
#include "config.h"

static int hotkeys_enabled = 1;

static void
hotkeys_toggleCoreSystemAndRestart(void)
{
    target_nextTarget();
    char message[64];
  
    snprintf(message, sizeof(message), "RESTARTING AS %s", target->name);
    config_saveConfig();
    debugger.restartRequested = 1;
}

int
hotkeys_registerHotkey(e9ui_context_t *ctx, SDL_Keycode key, SDL_Keymod modMask, SDL_Keymod modValue,
                        void (*cb)(e9ui_context_t *ctx, void *user), void *user)
{
    (void)ctx;
    if (!cb) {
        return -1;
    }
    e9k_hotkey_registry_t *hk = &e9ui->hotkeys;
    if (hk->count == hk->cap) {
        int nc = hk->cap ? hk->cap * 2 : 16;
        hk->entries = (e9k_hotkey_entry_t*)alloc_realloc(hk->entries, (size_t)nc * sizeof(e9k_hotkey_entry_t));
        hk->cap = nc;
    }
    int id = (hk->next_id ? hk->next_id : 1);
    hk->next_id = id + 1;
    hk->entries[hk->count++] = (e9k_hotkey_entry_t){ id, (int)key, (int)modMask, (int)modValue, cb, user, 1 };
    return id;
}

void
hotkeys_unregisterHotkey(e9ui_context_t *ctx, int id)
{
    (void)ctx;
    e9k_hotkey_registry_t *hk = &e9ui->hotkeys;
    for (int i = 0; i < hk->count; i++) {
        if (hk->entries[i].id == id) {
            hk->entries[i] = hk->entries[hk->count - 1];
            hk->count--;
            break;
        }
    }
}

int
hotkeys_dispatchHotkey(e9ui_context_t *ctx, const SDL_KeyboardEvent *kev)
{
    (void)ctx;
    if (!hotkeys_enabled) {
        return 0;
    }
    if (!kev) {
        return 0;
    }
    if (kev->repeat != 0) {
        return 0;
    }
    SDL_Keycode key = kev->keysym.sym;
    SDL_Keymod rawMods = kev->keysym.mod;
    SDL_Keymod mods = 0;
    if (rawMods & KMOD_CTRL) {
        mods = (SDL_Keymod)(mods | KMOD_CTRL);
    }
    if (rawMods & KMOD_SHIFT) {
        mods = (SDL_Keymod)(mods | KMOD_SHIFT);
    }
    if (rawMods & KMOD_ALT) {
        mods = (SDL_Keymod)(mods | KMOD_ALT);
    }
    if (rawMods & KMOD_GUI) {
        mods = (SDL_Keymod)(mods | KMOD_GUI);
    }
    e9ui_component_t *focus = ctx ? e9ui_getFocus(ctx) : NULL;
    int focusIsTextbox = 0;
    if (focus && focus->name && strcmp(focus->name, "e9ui_textbox") == 0) {
        focusIsTextbox = 1;
    }
    int allowPrintableHotkey = (mods == 0 && (key == SDLK_f || key == SDLK_b || key == SDLK_g) && !focusIsTextbox) ? 1 : 0;
    if (focus) {
        SDL_Keymod noShiftMods = (SDL_Keymod)(mods & (KMOD_CTRL|KMOD_ALT|KMOD_GUI));
        int printable = (key >= 32 && key <= 126);
        if (noShiftMods == 0 && printable && !allowPrintableHotkey) {
            return 0;
        }
    }
    if (key == SDLK_TAB && ctx) {
        if (prompt_isFocused(ctx, e9ui->prompt)) {
            return 0;
        }
        e9ui_component_t *focus = e9ui_getFocus(ctx);
        if (focus && focus->name && strcmp(focus->name, "e9ui_textbox") == 0) {
            if (e9ui_textbox_getCompletionMode(focus) != e9ui_textbox_completion_none) {
                return 0;
            }
        }
    }
    e9k_hotkey_registry_t *hk = &e9ui->hotkeys;
    for (int i = 0; i < hk->count; i++) {
        e9k_hotkey_entry_t *e = &hk->entries[i];
        if (!e->active) {
            continue;
        }
        if ((SDL_Keycode)e->key == key) {
            if ((mods & (SDL_Keymod)e->mask) == (SDL_Keymod)e->value) {
                if (e->cb) {
                    e->cb(ctx, e->user);
                }
                return 1;
            }
        }
    }
    return 0;
}

int
hotkeys_handleKeydown(e9ui_context_t *ctx, const SDL_KeyboardEvent *kev)
{
    if (!ctx || !kev) {
        return 0;
    }
    SDL_Keycode key = kev->keysym.sym;
    if (key == SDLK_F11 && kev->repeat == 0) {
        hotkeys_enabled = hotkeys_enabled ? 0 : 1;
        e9ui_setFocus(ctx, NULL);
        e9ui_showTransientMessage(hotkeys_enabled ? "HOTKEYS ON" : "HOTKEYS OFF");
        return 1;
    }
    if (!hotkeys_enabled && key != SDLK_F12) {
        return 0;
    }
    if (key == SDLK_ESCAPE) {
        if (sprite_debug_is_window_id(kev->windowID)) {
            if (sprite_debug_is_open()) {
                sprite_debug_toggle();
            }
            return 1;
        }
        if (e9ui->helpModal) {
            help_cancelModal();
            return 1;
        }
        if (e9ui->coreOptionsModal) {
            core_options_cancelModal();
            return 1;
        }
        if (e9ui->settingsModal) {
            debugger_cancelSettingsModal();
            return 1;
        }
        return 1;
    }
    if (key == SDLK_F1) {
        e9ui_setFocus(ctx, NULL);
        if (e9ui->helpModal) {
            help_cancelModal();
        } else {
            help_showModal(ctx);
        }
        return 1;
    }
    if (key == SDLK_F2) {
        e9ui_setFocus(ctx, NULL);
        ui_copyFramebufferToClipboard();
        return 1;
    }
    if (key == SDLK_F3) {
        e9ui_setFocus(ctx, NULL);
        hotkeys_toggleCoreSystemAndRestart();
        return 1;
    }
    if (key == SDLK_F4) {
      if (0) {
        int enabled = e9ui_getFpsEnabled();
        e9ui_setFpsEnabled(!enabled);
        e9ui_setFocus(ctx, NULL);
        e9ui_showTransientMessage(!enabled ? "FPS ON" : "FPS OFF");
        return 1;
      }
      int paused = state_buffer_isRollingPaused() ? 0 : 1;
      state_buffer_setRollingPaused(paused);
      e9ui_setFocus(ctx, NULL);
      e9ui_showTransientMessage(paused ? "ROLLING SAVE PAUSED" : "ROLLING SAVE RESUMED");      
    }
    if (key == SDLK_F12 && kev->repeat == 0) {
        if (e9ui->fullscreen) {
            e9ui_clearFullscreenComponent();
        } else {
            e9ui_component_t *geo_box = e9ui_findById(e9ui->root, "libretro_box");
            if (geo_box) {
                e9ui_setFullscreenComponent(geo_box);
            } else {
                e9ui_component_t *geo_view = e9ui_findById(e9ui->root, "geo_view");
                if (geo_view) {
                    e9ui_setFullscreenComponent(geo_view);
                }
            }
        }
      
        return 1;
    }
    if (key == SDLK_c) {
        SDL_Keymod mods = (SDL_Keymod)(kev->keysym.mod & (KMOD_CTRL|KMOD_GUI));
        if (mods != 0 && e9ui_text_select_hasSelection()) {
            e9ui_component_t *focus = e9ui_getFocus(ctx);
            if (!focus || !focus->name || strcmp(focus->name, "e9ui_textbox") != 0) {
                e9ui_text_select_copyToClipboard();
                return 1;
            }
        }
    }
    if (key == SDLK_COMMA || key == SDLK_PERIOD || key == SDLK_SLASH) {
        SDL_Keymod mods = (SDL_Keymod)(kev->keysym.mod & (KMOD_CTRL|KMOD_ALT|KMOD_GUI|KMOD_SHIFT));
        int has_focus = (e9ui_getFocus(ctx) != NULL);
        if (mods == 0 && !has_focus) {
            if (!input_record_isPlayback()) {
                input_record_recordUiKey(debugger.frameCounter + 1, (unsigned)key, 1);
                input_record_handleUiKey((unsigned)key, 1);
            }
            return 1;
        }
    }
    if (ctx->dispatchHotkey) {
        if (ctx->dispatchHotkey(ctx, kev)) {
            return 1;
        }
    }
    return 0;
}

void
hotkeys_shutdown(void)
{
    if (e9ui->hotkeys.entries) {
        alloc_free(e9ui->hotkeys.entries);
        e9ui->hotkeys.entries = NULL;
        e9ui->hotkeys.count = e9ui->hotkeys.cap =
            e9ui->hotkeys.next_id = 0;
    }
}
